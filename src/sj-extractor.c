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

#include <glib/gerror.h>
#include <glib/gtypes.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <libgnome/gnome-i18n.h>
#include "sj-extractor.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"

GType
encoding_format_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { VORBIS, "VORBIS", "Ogg Vorbis" },
      { MPEG, "MPEG", "MPEG" },
      { FLAC, "FLAC", "FLAC" },
      { WAVE, "WAVE", "Wave" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("EncodingFormat", values);
  }
  return etype;
}

/* Properties */
enum {
	PROP_0,
	PROP_FORMAT,
};

/* Signals */
enum {
  PROGRESS,
  COMPLETION,
  LAST_SIGNAL
};

static int sje_table_signals[LAST_SIGNAL] = { 0 };

#define DEFAULT_ENCODING_FORMAT VORBIS

struct SjExtractorPrivate {
  EncoderFormat format;
  gboolean rebuild_pipeline;
  GstElement *pipeline;
  GstElement *cdparanoia, *encoder, *filesink;
  GstFormat track_format;
  GstPad *source_pad;
  const char *encoder_name;
  int track_start;
  int seconds;
  const TrackDetails *track_details;
  GError *construct_error;
};

static void build_pipeline (SjExtractor *extractor);

/*
 * GObject methods
 */

static void sj_extractor_class_init (SjExtractorClass *klass);
static void sj_extractor_instance_init (SjExtractor *bcs);
static void sj_extractor_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec);
static void sj_extractor_get_property (GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec);
static void sj_extractor_finalize (GObject *object);


GType sj_extractor_get_type (void)
{
  static GType sj_extractor_type = 0;
  
  if (!sj_extractor_type) {
    static const GTypeInfo sj_extractor_info = {
      sizeof (SjExtractorClass),
      (GBaseInitFunc) NULL,
      (GBaseFinalizeFunc) NULL,
      (GClassInitFunc) sj_extractor_class_init,
      (GClassFinalizeFunc) NULL,
      NULL /* class_data */,
      sizeof (SjExtractor),
      0 /* n_preallocs */,
      (GInstanceInitFunc) sj_extractor_instance_init,
      NULL
    };
    
    sj_extractor_type = g_type_register_static (G_TYPE_OBJECT,
                                                "SjExtractor", &sj_extractor_info,
                                                (GTypeFlags)0);
  }
  return sj_extractor_type;
}

static void sj_extractor_class_init (SjExtractorClass *klass)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *) klass;

  /* GObject */
  object_class->set_property = sj_extractor_set_property;
  object_class->get_property = sj_extractor_get_property;
  object_class->finalize = sj_extractor_finalize;

  /* Properties */
  g_object_class_install_property (object_class, PROP_FORMAT,
                                   g_param_spec_enum ("format", _("Encoding format"), _("The format to write the encoded audio in"),
                                                      encoding_format_get_type(),
                                                      DEFAULT_ENCODING_FORMAT, G_PARAM_READWRITE));

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

  /* TODO: should probably move this elsewhere */
  gst_init (NULL, NULL);
}


static void sj_extractor_instance_init (SjExtractor *extractor)
{
  extractor->priv = g_new0 (SjExtractorPrivate, 1);
  extractor->priv->format = DEFAULT_ENCODING_FORMAT;
  extractor->priv->rebuild_pipeline = TRUE;
  build_pipeline(extractor);
}

static void sj_extractor_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec)
{
  SjExtractorPrivate *priv = (SjExtractorPrivate*)SJ_EXTRACTOR(object)->priv;
  switch (property_id) {
  case PROP_FORMAT:
    priv->format = g_value_get_enum (value);
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
  case PROP_FORMAT:
    g_value_set_enum (value, priv->format);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void sj_extractor_finalize (GObject *object)
{
  SjExtractor *extractor = (SjExtractor *)object;
  gst_element_set_state (extractor->priv->pipeline, GST_STATE_NULL);
  /* TODO: free the members of priv */
  g_free (extractor->priv);
  /* TODO: SJ_EXTRACTOR_GET_CLASS (extractor)->parent_class->finalize (object);*/
  /* g_object_finalize (object); */
  extractor->priv = NULL;
  extractor = NULL;
}

/*
 * Private Methods
 */
static void eos_cb (GstElement *gstelement, SjExtractor *extractor)
{
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));
  gst_element_set_state (extractor->priv->pipeline, GST_STATE_NULL);
  g_signal_emit (G_OBJECT (extractor),
                 sje_table_signals[COMPLETION],
                 0);
}

static const char* get_encoder_name (SjExtractor *extractor)
{
  SjExtractorPrivate *priv;
  g_return_val_if_fail (SJ_IS_EXTRACTOR (extractor), NULL);
  priv = (SjExtractorPrivate*)extractor->priv;
  return priv->encoder_name;
}

static GstElement* build_encoder (SjExtractor *extractor)
{
  SjExtractorPrivate *priv;
  GstElement *element = NULL;
  g_return_val_if_fail (SJ_IS_EXTRACTOR (extractor), NULL);
  priv = (SjExtractorPrivate*)extractor->priv;
  switch (priv->format) {
  case VORBIS:
    element = gst_element_factory_make ("vorbisenc", "encoder");
    g_object_set (G_OBJECT (element), "quality", 0.6, NULL);
    priv->encoder_name = "vorbis";
    break;
  case MPEG:
    element = gst_element_factory_make ("lame", "encoder");
    if (!element) {
      element = gst_element_factory_make ("mpegaudio", "encoder");
      priv->encoder_name = "mpegaudio";
    } else {
      priv->encoder_name = "lame";
    }
    break;
  case FLAC:
    element = gst_element_factory_make ("flacenc", "encoder");
    priv->encoder_name = "flacenc";
    break;
  case WAVE:
    element = gst_element_factory_make ("wavenc", "encoder");
    priv->encoder_name = "wavenc";
    break;
  default:
    g_return_val_if_reached (NULL);
  }
  return element;
}

static void build_pipeline (SjExtractor *extractor)
{
  SjExtractorPrivate *priv;

  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  priv = extractor->priv;

  if (priv->pipeline != NULL) {
    gst_object_unref (GST_OBJECT (priv->pipeline));
  }
  priv->pipeline = gst_pipeline_new ("pipeline");

  /* Read from CD */
  priv->cdparanoia = gst_element_factory_make ("cdparanoia", "cdparanoia");
  if (priv->cdparanoia == NULL) {
    g_set_error (&priv->construct_error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create GStreamer cdparanoia reader"));
    return;
  }

  /* Get the track format for seeking later */
  priv->track_format = gst_format_get_by_nick ("track");
  g_assert (priv->track_format != 0); /* TODO: GError */
  /* Get the source pad for seeking */
  priv->source_pad = gst_element_get_pad (priv->cdparanoia, "src");
  g_assert (priv->source_pad); /* TODO: GError */

  /* Encode */
  priv->encoder = build_encoder (extractor);
  if (priv->encoder == NULL) {
    g_set_error (&priv->construct_error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create GStreamer encoder (%s)"), get_encoder_name (extractor));
    return;
  }
  /* Connect to the eos so we know when its finished */
  g_signal_connect (priv->encoder, "eos", G_CALLBACK (eos_cb), extractor);

  /* Write to disk */
  priv->filesink = gst_element_factory_make ("filesink", "filesink");
  if (priv->filesink == NULL) {
    g_set_error (&priv->construct_error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create GStreamer file output"));
    return;
  }

  /* Add the elements to the pipeline */
  gst_bin_add (GST_BIN (priv->pipeline), priv->cdparanoia);
  gst_bin_add (GST_BIN (priv->pipeline), priv->encoder);
  gst_bin_add (GST_BIN (priv->pipeline), priv->filesink);

  /* Link it all together */
  gst_element_link_many (priv->cdparanoia, priv->encoder, priv->filesink, NULL);

  priv->rebuild_pipeline = FALSE;
}

static gboolean tick_timeout_cb(SjExtractor *extractor)
{
  gint64 nanos;
  gint secs;
  static GstFormat format = GST_FORMAT_TIME;

  g_return_val_if_fail (extractor != NULL, FALSE);
  g_return_val_if_fail (SJ_IS_EXTRACTOR (extractor), FALSE);

  if (gst_element_get_state (extractor->priv->pipeline) != GST_STATE_PLAYING) {
    return FALSE;
  }

  if (!gst_pad_query (extractor->priv->source_pad, GST_QUERY_POSITION, &format, &nanos)) {
    g_warning (_("Could not get current track position"));
    return TRUE;
  }

  secs = nanos / GST_SECOND;
  if (secs != extractor->priv->seconds) {
    extractor->priv->seconds = secs;
    g_signal_emit (G_OBJECT (extractor),
                   sje_table_signals[PROGRESS],
                   0, secs - extractor->priv->track_start);
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

  g_object_set (G_OBJECT (extractor->priv->cdparanoia), "location", device, NULL);
}

void sj_extractor_set_paranoia (SjExtractor *extractor, const int paranoia_mode)
{
  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  g_object_set (G_OBJECT (extractor->priv->cdparanoia), "paranoia-mode", paranoia_mode, NULL);
}

void sj_extractor_extract_track (SjExtractor *extractor, const TrackDetails *track, const char* path, GError **error)
{
  GstEvent *event;
  GstCaps *caps;
  char *tracknumber;
  static GstFormat format = GST_FORMAT_TIME;
  gint64 nanos;
  SjExtractorPrivate *priv;

  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  g_return_if_fail (path != NULL);
  g_return_if_fail (track != NULL);

  priv = extractor->priv;

  /* See if we need to rebuild the pipeline */
  if (priv->rebuild_pipeline) {
    build_pipeline (extractor);
    if (priv->construct_error != NULL) {
      *error = g_error_copy (priv->construct_error);
      g_error_free (priv->construct_error);
      priv->construct_error = NULL;
      return;
    }
  }

  /* Save a pointer to the track details */
  priv->track_details = track;

  /* Set the output filename */
  gst_element_set_state (priv->filesink, GST_STATE_NULL);
  g_object_set (G_OBJECT (priv->filesink), "location", path, NULL);

  /* Set the metadata */
  /* TODO; this works with Vorbis and MP3 and will work with FLAC when the
     property is added. Wave ... news not so good. */
  tracknumber = g_strdup_printf("%d", track->number);
  caps = GST_CAPS_NEW ("soundjuicer_metadata", 
                       "application/x-gst-metadata",
                       "title", GST_PROPS_STRING (track->title),
                       "artist", GST_PROPS_STRING (track->artist),
                       "tracknum", GST_PROPS_STRING (tracknumber),
                       "tracknumber", GST_PROPS_STRING (tracknumber),
                       "album", GST_PROPS_STRING (track->album->title),
                       "comment", GST_PROPS_STRING(_("Ripped with Sound Juicer"))
                       );
  g_object_set (G_OBJECT (priv->encoder), "metadata", caps, NULL);
  g_free (tracknumber);

  /* Let's get ready to rumble! */
  gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);

  /* Seek to the right track */
  event = gst_event_new_segment_seek (priv->track_format | GST_SEEK_METHOD_SET | GST_SEEK_FLAG_FLUSH,
			              track->number-1, track->number);
  if (!gst_pad_send_event (priv->source_pad, event)) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not seek to track"));
    return;
  }

  if (!gst_pad_query (priv->source_pad, GST_QUERY_POSITION, &format, &nanos)) {
    g_warning (_("Could not get track start position"));
    return;
  }
  priv->track_start = nanos / GST_SECOND;

  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
  g_idle_add ((GSourceFunc)gst_bin_iterate, priv->pipeline);
  g_timeout_add (200, (GSourceFunc)tick_timeout_cb, extractor);
}

void sj_extractor_cancel_extract (SjExtractor *extractor)
{
  g_return_if_fail (extractor != NULL);
  g_return_if_fail (SJ_IS_EXTRACTOR (extractor));

  if (gst_element_get_state (extractor->priv->pipeline) != GST_STATE_PLAYING) {
    return;
  }
  gst_element_set_state (extractor->priv->pipeline, GST_STATE_PAUSED);
  /*
   * TODO: I don't think I need to remove gst from the idle loop, as
   * it will call gst_bin_iterate(), which sees that it is PAUSED,
   * will return FALSE as it did nothing, and be removed. Correct?
   */
}

const TrackDetails *sj_extractor_get_track_details (SjExtractor *extractor)
{
  g_return_val_if_fail (extractor != NULL, NULL);
  g_return_val_if_fail (SJ_IS_EXTRACTOR (extractor), NULL);
  return extractor->priv->track_details;
}
