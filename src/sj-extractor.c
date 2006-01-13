/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-extractor.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <glib/gerror.h>
#include <glib/gtypes.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <profiles/gnome-media-profiles.h>
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"

/* Properties */
enum {
	PROP_0,
	PROP_PROFILE,
};

/* Signals */
enum {
  PROGRESS,
  COMPLETION,
  ERROR,
  LAST_SIGNAL
};

static guint sje_table_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_AUDIO_PROFILE_NAME "cdlossy"

struct SjExtractorPrivate {
  /** The current audio profile */
  GMAudioProfile *profile;
  /** If the pipeline needs to be re-created */
  gboolean rebuild_pipeline;
  /* The gstreamer pipeline elements */
  GstElement *pipeline, *cdparanoia, *queue, *thread, *encoder, *filesink;
  GstFormat track_format;
  GstPad *source_pad;
  char *device_path;
  int paranoia_mode;
  int track_start;
  int seconds;
  GError *construct_error;
  /* Variables used by the thread to push signals into the main thread */
  guint idle_id;
  GMutex *idle_mutex;
  GAsyncQueue *idle_queue;
};

static void build_pipeline (SjExtractor *extractor);

/*
 * GObject methods
 */

static void sj_extractor_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec);
static void sj_extractor_get_property (GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec);
static void sj_extractor_finalize (GObject *object);

G_DEFINE_TYPE(SjExtractor, sj_extractor, G_TYPE_OBJECT);

static void sj_extractor_class_init (SjExtractorClass *klass)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) klass;

  /* GObject */
  object_class->set_property = sj_extractor_set_property;
  object_class->get_property = sj_extractor_get_property;
  object_class->finalize = sj_extractor_finalize;

  /* Properties */
  g_object_class_install_property (object_class, PROP_PROFILE,
				   g_param_spec_pointer("profile", _("GNOME Audio Profile"), _("The GNOME Audio Profile used for encoding audio"), G_PARAM_READWRITE));
							
  /* Signals */
  sje_table_signals[PROGRESS] =
    g_signal_new ("progress",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SjExtractorClass, progress),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__INT,
                  G_TYPE_NONE, 1, G_TYPE_INT);
  sje_table_signals[COMPLETION] =
    g_signal_new ("completion",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SjExtractorClass, completion),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  sje_table_signals[ERROR] =
    g_signal_new ("error",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (SjExtractorClass, error),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);
}


static void sj_extractor_init (SjExtractor *extractor)
{
  extractor->priv = g_new0 (SjExtractorPrivate, 1);
  extractor->priv->profile = gm_audio_profile_lookup(DEFAULT_AUDIO_PROFILE_NAME);
  extractor->priv->rebuild_pipeline = TRUE;
  extractor->priv->idle_id = 0;
  extractor->priv->idle_mutex = g_mutex_new ();
  extractor->priv->idle_queue = g_async_queue_new ();
}

static void sj_extractor_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec)
{
  SjExtractorPrivate *priv = (SjExtractorPrivate*)SJ_EXTRACTOR(object)->priv;
  switch (property_id) {
  case PROP_PROFILE:
    priv->profile = (GMAudioProfile *) g_value_get_pointer (value);
    priv->rebuild_pipeline = TRUE;
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void sj_extractor_get_property (GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec)
{
  SjExtractorPrivate *priv = (SjExtractorPrivate*)SJ_EXTRACTOR(object)->priv;
  switch (property_id) {
  case PROP_PROFILE:
    g_value_set_pointer (value, priv->profile);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void sj_extractor_finalize (GObject *object)
{
  SjExtractor *extractor = (SjExtractor *)object;
  if (extractor->priv->pipeline)
    gst_element_set_state (extractor->priv->pipeline, GST_STATE_NULL);
  /* TODO: free the members of priv */
  g_mutex_free (extractor->priv->idle_mutex);
  g_async_queue_unref (extractor->priv->idle_queue);
  g_free (extractor->priv);
  extractor->priv = NULL;
  G_OBJECT_CLASS (sj_extractor_parent_class)->finalize (object);
}

/*
 * Private Methods
 */

static void eos_cb (GstElement *gstelement, SjExtractor *extractor)
{
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));
  
  gst_element_set_state (extractor->priv->pipeline, GST_STATE_NULL);

  extractor->priv->rebuild_pipeline = TRUE;

  g_signal_emit (extractor, sje_table_signals [COMPLETION], 0);
}

/* Stolen from gst-plugins-good/ext/gconf/gconf.c */
static GstPad *
gst_bin_find_unconnected_pad (GstBin * bin, GstPadDirection direction)
{
  GstPad *pad = NULL;
  GList *elements = NULL;
  const GList *pads = NULL;
  GstElement *element = NULL;

  GST_OBJECT_LOCK (bin);
  elements = bin->children;
  /* traverse all elements looking for unconnected pads */
  while (elements && pad == NULL) {
    element = GST_ELEMENT (elements->data);
    GST_OBJECT_LOCK (element);
    pads = element->pads;
    while (pads) {
      GstPad *testpad = GST_PAD (pads->data);

      /* check if the direction matches */
      if (GST_PAD_DIRECTION (testpad) == direction) {
        GST_OBJECT_LOCK (testpad);
        if (GST_PAD_PEER (testpad) == NULL) {
          GST_OBJECT_UNLOCK (testpad);
          /* found it ! */
          pad = testpad;
          break;
        }
        GST_OBJECT_UNLOCK (testpad);
      }
      pads = g_list_next (pads);
    }
    GST_OBJECT_UNLOCK (element);
    elements = g_list_next (elements);
  }
  GST_OBJECT_UNLOCK (bin);

  return pad;
}

/* Stolen from gst-plugins-good/ext/gconf/gconf.c */
static GstElement *
gst_gconf_render_bin_from_description (const gchar * description)
{
  GstElement *bin = NULL;
  GstPad *pad = NULL;
  GError *error = NULL;
  gchar *desc = NULL;

  /* parse the pipeline to a bin */
  desc = g_strdup_printf ("bin.( %s )", description);
  bin = GST_ELEMENT (gst_parse_launch (desc, &error));
  g_free (desc);
  if (error) {
    GST_ERROR ("gstgconf: error parsing pipeline %s\n%s\n",
        description, error->message);
    g_error_free (error);
    return NULL;
  }

  /* find pads and ghost them if necessary */
  if ((pad = gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SRC))) {
    gst_element_add_pad (bin, gst_ghost_pad_new ("src", pad));
  }
  if ((pad = gst_bin_find_unconnected_pad (GST_BIN (bin), GST_PAD_SINK))) {
    gst_element_add_pad (bin, gst_ghost_pad_new ("sink", pad));
  }
  return bin;
}

static GstElement* build_encoder (SjExtractor *extractor)
{
  SjExtractorPrivate *priv;
  GstElement *element = NULL;
  char *pipeline;

  g_return_val_if_fail (SJ_IS_EXTRACTOR (extractor), NULL);
  priv = (SjExtractorPrivate*)extractor->priv;
  g_return_val_if_fail (priv->profile != NULL, NULL);
 
  pipeline = g_strdup_printf ("audioconvert ! %s", gm_audio_profile_get_pipeline (priv->profile));
  element = gst_gconf_render_bin_from_description (pipeline);
  g_free(pipeline);
  return element;
}

static void error_cb (GstBus *bus, GstMessage *message, gpointer user_data)
{
  SjExtractor *extractor = SJ_EXTRACTOR (user_data);
  GError *error = NULL;

  /* Make sure the pipeline is not running any more */
  gst_element_set_state (extractor->priv->pipeline, GST_STATE_NULL);

  gst_message_parse_error (message, &error, NULL);
  g_signal_emit (extractor, sje_table_signals[ERROR], 0, error);
}

/**
 * Callback from the gnomevfssink to say that its about to overwrite a file.
 * For now, Just Say Yes.  If this API will stay in 0.9, then rewrite
 * SjExtractor.
 */
static gboolean just_say_yes (GstElement *element, gpointer filename, gpointer user_data)
{
  return TRUE;
}

static void build_pipeline (SjExtractor *extractor)
{
  SjExtractorPrivate *priv;
  GstBus *bus;

  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  priv = extractor->priv;

  if (priv->pipeline != NULL) {
    gst_object_unref (GST_OBJECT (priv->pipeline));
  }
  priv->pipeline = gst_pipeline_new ("pipeline");
  bus = gst_element_get_bus (priv->pipeline);

  g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (error_cb), extractor);

  /* Read from CD */
  priv->cdparanoia = gst_element_factory_make ("cdparanoiasrc", "cdparanoiasrc");
  if (priv->cdparanoia == NULL) {
    g_set_error (&priv->construct_error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create GStreamer cdparanoia reader"));
    return;
  }

  g_object_set (G_OBJECT (priv->cdparanoia), "location", priv->device_path, NULL);
  g_object_set (G_OBJECT (priv->cdparanoia), "paranoia-mode", priv->paranoia_mode, NULL);

  /* Get the track format for seeking later */
  priv->track_format = gst_format_get_by_nick ("track");
  g_assert (priv->track_format != 0); /* TODO: GError */
  /* Get the source pad for seeking */
  priv->source_pad = gst_element_get_pad (priv->cdparanoia, "src");
  g_assert (priv->source_pad); /* TODO: GError */

  priv->queue = gst_element_factory_make ("queue", "queue");
  /* Nice big buffers... */
  g_object_set (priv->queue, "max-size-time", 20 * GST_SECOND, NULL);
  
  /* Encode */
  priv->encoder = build_encoder (extractor);
  if (priv->encoder == NULL) {
    g_set_error (&priv->construct_error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create GStreamer encoders for %s"), gm_audio_profile_get_name (priv->profile));
    return;
  }
  /* Connect to the eos so we know when its finished */
  g_signal_connect (priv->encoder, "eos", G_CALLBACK (eos_cb), extractor);

  /* Write to disk */
  priv->filesink = gst_element_factory_make ("gnomevfssink", "gnomevfssink");
  if (priv->filesink == NULL) {
    g_set_error (&priv->construct_error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create GStreamer file output"));
    return;
  }
  g_signal_connect (G_OBJECT (priv->filesink), "allow-overwrite", G_CALLBACK (just_say_yes), extractor);

  /* Add the elements to the pipeline */
  gst_bin_add_many (GST_BIN (priv->pipeline), priv->cdparanoia, priv->queue, priv->encoder, priv->filesink, NULL);

  /* Link it all together */
  if (!gst_element_link_many (priv->cdparanoia, priv->queue, priv->encoder, priv->filesink, NULL)) {
    /* TODO: need to produce a GError here */
    g_warning ("Cannot link pipeline, very bad!");
    g_object_unref (priv->pipeline);
    return;
  }

  priv->rebuild_pipeline = FALSE;
}

static gboolean tick_timeout_cb(SjExtractor *extractor)
{
  gint64 nanos;
  gint secs;
  static GstFormat format = GST_FORMAT_TIME;

  g_return_val_if_fail (extractor != NULL, FALSE);
  g_return_val_if_fail (SJ_IS_EXTRACTOR (extractor), FALSE);

#if 0
  if (gst_element_get_state (extractor->priv->pipeline) != GST_STATE_PLAYING) {
    extractor->priv->rebuild_pipeline = TRUE;
    return FALSE;
  }
#endif

  if (!gst_pad_query_position (extractor->priv->source_pad, &format, &nanos)) {
    g_warning (_("Could not get current track position"));
    return TRUE;
  }

  secs = nanos / GST_SECOND;
  if (secs != extractor->priv->seconds) {
    g_signal_emit (extractor, sje_table_signals[PROGRESS], 0, secs - extractor->priv->track_start);
  }

  return TRUE;
}

/*
 * Public Methods
 */

GObject *sj_extractor_new (void)
{
  return g_object_new (sj_extractor_get_type (), NULL);
}

GError *sj_extractor_get_new_error (SjExtractor *extractor)
{
  GError *error;
  if (extractor == NULL || extractor->priv == NULL) {
    g_set_error (&error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Extractor object is not valid. This is bad, check your console for errors."));
    return error;
  }
  return extractor->priv->construct_error;
}

void sj_extractor_set_device (SjExtractor *extractor, const char* device)
{
  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));
  g_return_if_fail (device != NULL);

  g_free (extractor->priv->device_path);
  extractor->priv->device_path = g_strdup (device);

  if (extractor->priv->cdparanoia != NULL)
    g_object_set (G_OBJECT (extractor->priv->cdparanoia), "location", device, NULL);
}

void sj_extractor_set_paranoia (SjExtractor *extractor, const int paranoia_mode)
{
  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  extractor->priv->paranoia_mode = paranoia_mode;
  if (extractor->priv->cdparanoia != NULL)
    g_object_set (G_OBJECT (extractor->priv->cdparanoia), "paranoia-mode", paranoia_mode, NULL);
}

void sj_extractor_extract_track (SjExtractor *extractor, const TrackDetails *track, const char* url, GError **error)
{
  SjExtractorPrivate *priv;
  static GstFormat format = GST_FORMAT_TIME;
  gint64 nanos;
  GstIterator *iter;
  GstTagSetter *tagger;
  gboolean done;

  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  g_return_if_fail (url != NULL);
  g_return_if_fail (track != NULL);

  priv = extractor->priv;

  /* See if we need to rebuild the pipeline */
  if (priv->rebuild_pipeline != FALSE) {
    build_pipeline (extractor);
    if (priv->construct_error != NULL) {
      *error = g_error_copy (priv->construct_error);
      g_error_free (priv->construct_error);
      priv->construct_error = NULL;
      return;
    }
  }

  /* Set extract speed */
  g_object_set (G_OBJECT (priv->cdparanoia), "read-speed", 0, NULL);

  /* Set the output filename */
  gst_element_set_state (priv->filesink, GST_STATE_NULL);
  g_object_set (G_OBJECT (priv->filesink), "location", url, NULL);

  /* Set the metadata */
  iter = gst_bin_iterate_all_by_interface (GST_BIN (priv->pipeline), GST_TYPE_TAG_SETTER);
  done = FALSE;
  
  while (!done) {
    switch (gst_iterator_next (iter, (gpointer)&tagger)) {
    case GST_ITERATOR_OK:
      /* TODO: generate this as a taglist once, and apply it to all elements */
      gst_tag_setter_add_tags (tagger,
                               GST_TAG_MERGE_REPLACE_ALL,
                               GST_TAG_TITLE, track->title,
                               GST_TAG_ARTIST, track->artist,
                               GST_TAG_TRACK_NUMBER, track->number,
                               GST_TAG_TRACK_COUNT, track->album->number,
                               GST_TAG_ALBUM, track->album->title,
                               GST_TAG_ENCODER, _("Sound Juicer"),
                               GST_TAG_ENCODER_VERSION, VERSION,
#if 0
                               GST_TAG_MUSICBRAINZ_ALBUMID, track->album->album_id,
                               GST_TAG_MUSICBRAINZ_ALBUMARTISTID, track->album->artist_id,
                               GST_TAG_MUSICBRAINZ_ARTISTID, track->artist_id,
                               GST_TAG_MUSICBRAINZ_TRACKID, track->track_id,
#endif
                               NULL);
      if (track->album->genre != NULL && strcmp (track->album->genre, "") != 0) {
        gst_tag_setter_add_tags (tagger,
                                 GST_TAG_MERGE_APPEND,
                                 GST_TAG_GENRE, track->album->genre,
                                 NULL);
      }
      if (track->album->release_date) {
        gst_tag_setter_add_tags (tagger,
                                 GST_TAG_MERGE_APPEND,
                                 GST_TAG_DATE, g_date_get_julian (track->album->release_date),
                                 NULL);
      }
      gst_object_unref (tagger);
      break;
    case GST_ITERATOR_RESYNC:
      // TODO?
      gst_iterator_resync (iter);
      break;
    case GST_ITERATOR_ERROR:
      done = TRUE;
      break;
    case GST_ITERATOR_DONE:
      done = TRUE;
      break;
    }
  }
  gst_iterator_free (iter);
  
  /* Let's get ready to rumble! */
  gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
  
  /* Seek to the right track */
  gst_element_seek (priv->cdparanoia, 1.0, priv->track_format, 0,
                    GST_SEEK_TYPE_SET, track->number-1,
                    GST_SEEK_TYPE_SET, track->number);

  if (!gst_pad_query_position (priv->source_pad, &format, &nanos)) {
    g_warning (_("Could not get track start position"));
    return;
  }
  priv->track_start = nanos / GST_SECOND;

  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  
  g_timeout_add (200, (GSourceFunc)tick_timeout_cb, extractor);
}

void sj_extractor_cancel_extract (SjExtractor *extractor)
{
  GstState state;

  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  gst_element_get_state (extractor->priv->pipeline, &state, NULL, GST_CLOCK_TIME_NONE);
  if (state != GST_STATE_PLAYING) {
    return;
  }
  gst_element_set_state (extractor->priv->pipeline, GST_STATE_NULL);

  extractor->priv->rebuild_pipeline = TRUE;
}

gboolean sj_extractor_supports_encoding (GError **error)
{
  GstElement *element = NULL;

  element = gst_element_factory_make ("cdparanoia", "cdparanoia");
  if (element == NULL) {
    g_set_error (error, SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("The plugin necessary for CD access was not found"));
    return FALSE;
  }
  g_object_unref (element);

  element = gst_element_factory_make ("filesink", "filesink");
  if (element == NULL) {
    g_set_error (error, SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("The plugin necessary for file access was not found"));
    return FALSE;
  }
  g_object_unref (element);

  return TRUE;
}

gboolean sj_extractor_supports_profile (GMAudioProfile *profile)
{
  GstElement *element = NULL;
  GError *error = NULL;
  char *pipeline;
  
  pipeline = g_strdup_printf ("audioconvert ! %s", gm_audio_profile_get_pipeline (profile));
  element = gst_parse_launch (pipeline, &error);
  /* TODO: check error */
  g_free(pipeline);
  if (element) {
    gst_object_unref (GST_OBJECT (element));
    return TRUE;
  } else {
    return FALSE;
  }
}

