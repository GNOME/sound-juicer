/*
 * sj-metadata-cdtext.c
 * Copyright (C) 2005 Ross Burton <ross@burtonini.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <glib/gerror.h>
#include <glib/glist.h>
#include <glib/gstrfuncs.h>
#include <glib/gmessages.h>
#include <cdio/cdio.h>
#include <cdio/cdtext.h>

#include "sj-metadata-cdtext.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "sj-util.h"

struct SjMetadataCdtextPrivate {
  char *cdrom;
  GList *albums;
  GError *error;
};

#define GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SJ_TYPE_METADATA_CDTEXT, SjMetadataCdtextPrivate))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_PROXY_HOST,
  PROP_PROXY_PORT,
};

static void metadata_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SjMetadataCdtext, sj_metadata_cdtext,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SJ_TYPE_METADATA, metadata_iface_init));


/**
 * Private methods
 */

static gboolean
fire_signal_idle (SjMetadataCdtext *m)
{
  g_return_val_if_fail (SJ_IS_METADATA_CDTEXT (m), FALSE);
  g_signal_emit_by_name (G_OBJECT (m), "metadata", m->priv->albums, m->priv->error);
  return FALSE;
}

static void
cdtext_list_albums (SjMetadata *metadata, GError **error)
{
  /* TODO: put in a thread */
  SjMetadataCdtextPrivate *priv;
  AlbumDetails *album;
  CdIo *cdio;
  track_t cdtrack, last_cdtrack;
  const cdtext_t *cdtext;

  g_return_if_fail (SJ_IS_METADATA_CDTEXT (metadata));

  priv = SJ_METADATA_CDTEXT (metadata)->priv;

  cdio = cdio_open (priv->cdrom, DRIVER_UNKNOWN);
  if (!cdio) {
    g_warning ("Cannot open CD");
    priv->error = g_error_new (SJ_ERROR, SJ_ERROR_INTERNAL_ERROR, _("Cannot read CD"));
    priv->albums = NULL;
    g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
    return;
  }

#if 0
  /* Why can I not read this here?  If I do, I get broken track data */
  cdtext = cdio_get_cdtext(cdio, 0);
  if (!cdtext) {
    g_printerr("Cannot read CD-TEXT\n");
    /* TODO: return no metadata somehow*/
    return;
  }
#endif

  album = g_new0(AlbumDetails, 1);

  cdtrack = cdio_get_first_track_num(cdio);
  last_cdtrack = cdtrack + cdio_get_num_tracks(cdio);

  for ( ; cdtrack < last_cdtrack; cdtrack++ ) {
    TrackDetails *track;
    track = g_new0 (TrackDetails, 1);
    track->album = album;
    track->number = cdtrack;
    cdtext = cdio_get_cdtext(cdio, cdtrack);
    if (cdtext) {
      track->title = g_strdup (cdtext_get (CDTEXT_TITLE, cdtext));
      track->artist = g_strdup (cdtext_get (CDTEXT_PERFORMER, cdtext));
    } else {
      g_print ("No CD-TEXT for track %u\n", cdtrack);
    }
    track->duration = cdio_get_track_sec_count (cdio, cdtrack) / CDIO_CD_FRAMES_PER_SEC;

    album->tracks = g_list_append (album->tracks, track);
    album->number++;
  }

  /* TODO: why can't I do this first? */
  cdtext = cdio_get_cdtext(cdio, 0);
  album->title = g_strdup (cdtext_get (CDTEXT_TITLE, cdtext));
  album->artist = g_strdup (cdtext_get (CDTEXT_PERFORMER, cdtext));
  album->genre = g_strdup (cdtext_get (CDTEXT_GENRE, cdtext));

  album->metadata_source = SOURCE_CDTEXT;

  priv->error = NULL;
  priv->albums = g_list_append (NULL, album);
  g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
}


/**
 * GObject methods
 */

static void
sj_metadata_cdtext_get_property (GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  SjMetadataCdtextPrivate *priv = SJ_METADATA_CDTEXT (object)->priv;
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    g_value_set_string (value, priv->cdrom);
    break;
  case PROP_PROXY_HOST:
    /* Do nothing */
    g_value_set_string (value, "");
    break;
  case PROP_PROXY_PORT:
    /* Do nothing */
    g_value_set_int (value, 0);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_cdtext_set_property (GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  SjMetadataCdtextPrivate *priv = SJ_METADATA_CDTEXT (object)->priv;
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    if (priv->cdrom)
      g_free (priv->cdrom);
    priv->cdrom = g_value_dup_string (value);
    break;
  case PROP_PROXY_HOST:
  case PROP_PROXY_PORT:
    /* Do nothing */
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_cdtext_finalize (GObject *object)
{
  SjMetadataCdtextPrivate *priv = SJ_METADATA_CDTEXT (object)->priv;
  g_free (priv->cdrom);
  g_list_deep_free (priv->albums, (GFunc)album_details_free);
  if (priv->error)
    g_error_free (priv->error);
}

static void
sj_metadata_cdtext_init (SjMetadataCdtext *cdtext)
{
  cdtext->priv = GET_PRIVATE (cdtext);
}

static void
metadata_iface_init (gpointer g_iface, gpointer iface_data)
{
  SjMetadataClass *klass = (SjMetadataClass*)g_iface;
  
  klass->list_albums = cdtext_list_albums;
}

static void
sj_metadata_cdtext_class_init (SjMetadataCdtextClass *class)
{
  GObjectClass *object_class = (GObjectClass*) class;

  g_type_class_add_private (class, sizeof (SjMetadataCdtextPrivate));

  object_class->get_property = sj_metadata_cdtext_get_property;
  object_class->set_property = sj_metadata_cdtext_set_property;
  object_class->finalize = sj_metadata_cdtext_finalize;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
  g_object_class_override_property (object_class, PROP_PROXY_HOST, "proxy-host");
  g_object_class_override_property (object_class, PROP_PROXY_PORT, "proxy-port");
}


/*
 * Public methods
 */

GObject *
sj_metadata_cdtext_new (void)
{
  return g_object_new (sj_metadata_cdtext_get_type (), NULL);
}
