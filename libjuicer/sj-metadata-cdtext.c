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

static GError* cdtext_get_new_error (SjMetadata *metadata);
static void cdtext_set_cdrom (SjMetadata *metadata, const char* device);
static void cdtext_set_proxy (SjMetadata *metadata, const char* proxy);
static void cdtext_set_proxy_port (SjMetadata *metadata, const int port);
static void cdtext_list_albums (SjMetadata *metadata, GError **error);

struct SjMetadataCdtextPrivate {
  GError *construct_error;
  char *cdrom;
  GList *albums;
  GError *error;
};

/**
 * GObject methods
 */

static void
sj_metadata_cdtext_finalize (GObject *object)
{
  SjMetadataCdtextPrivate *priv = SJ_METADATA_CDTEXT (object)->priv;
  g_error_free (priv->construct_error);
  g_free (priv->cdrom);
  g_list_deep_free (priv->albums, (GFunc)album_details_free);
  if (priv->error)
    g_error_free (priv->error);
}

static void
sj_metadata_cdtext_init (SjMetadataCdtext *cdtext)
{
  cdtext->priv = g_new0(SjMetadataCdtextPrivate, 1);
}

static void
metadata_iface_init (gpointer g_iface, gpointer iface_data)
{
  SjMetadataClass *klass = (SjMetadataClass*)g_iface;
  klass->get_new_error = cdtext_get_new_error;
  klass->set_cdrom = cdtext_set_cdrom;
  klass->set_proxy = cdtext_set_proxy;
  klass->set_proxy_port = cdtext_set_proxy_port;
  klass->list_albums = cdtext_list_albums;
}

static void
sj_metadata_cdtext_class_init (SjMetadataCdtextClass *class)
{
  GObjectClass *object_class = (GObjectClass*) class;
  object_class->finalize = sj_metadata_cdtext_finalize;
}

G_DEFINE_TYPE_EXTENDED (SjMetadataCdtext,
                        sj_metadata_cdtext,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (SJ_TYPE_METADATA, metadata_iface_init));

GObject *
sj_metadata_cdtext_new (void)
{
  return g_object_new (sj_metadata_cdtext_get_type (), NULL);
}

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

/**
 * Virtual methods
 */

static GError*
cdtext_get_new_error (SjMetadata *metadata)
{
  GError *error = NULL;
  if (metadata == NULL || SJ_METADATA_CDTEXT (metadata)->priv == NULL) {
    g_set_error (&error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("CD-TEXT metadata plugin failed to initialise."));
    return error;
  }
  return SJ_METADATA_CDTEXT (metadata)->priv->construct_error;
}

static void
cdtext_set_cdrom (SjMetadata *metadata, const char* device)
{
  SjMetadataCdtextPrivate *priv;
  g_return_if_fail (metadata != NULL);
  g_return_if_fail (device != NULL);
  priv = SJ_METADATA_CDTEXT (metadata)->priv;

  if (priv->cdrom) {
    g_free (priv->cdrom);
  }
  priv->cdrom = g_strdup (device);
}

static void
cdtext_set_proxy (SjMetadata *metadata, const char* proxy)
{
  /* No need to do anything here */
  return;
}

static void
cdtext_set_proxy_port (SjMetadata *metadata, const int port)
{
  /* No need to do anything here */
  return;
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

  priv->error = NULL;
  priv->albums = g_list_append (NULL, album);
  g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
}
