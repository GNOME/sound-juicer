/*
 * sj-metadata-musicbrainz.c
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
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

#include "sound-juicer.h"

#include <glib-object.h>
#include <glib/gerror.h>
#include <glib/glist.h>
#include <glib/gstrfuncs.h>
#include <glib/gmessages.h>
#include <musicbrainz/queries.h>
#include <musicbrainz/mb_c.h>
#include <stdlib.h>
#include <unistd.h>

#include "sj-metadata-musicbrainz.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "cd-drive.h"

struct SjMetadataMusicbrainzPrivate {
  GError *construct_error;
  musicbrainz_t mb;
  char *http_proxy;
  int http_proxy_port;
  char *cdrom;
};

static GError* mb_get_new_error (SjMetadata *metadata);
static void mb_set_cdrom (SjMetadata *metadata, const char* device);
static void mb_set_proxy (SjMetadata *metadata, const char* proxy);
static void mb_set_proxy_port (SjMetadata *metadata, const int port);
static GList* mb_list_albums (SjMetadata *metadata, GError **error);

/**
 * GObject methods
 */

static GObjectClass *parent_class = NULL;

static void
sj_metadata_musicbrainz_finalize (GObject *object)
{
  SjMetadataMusicbrainzPrivate *priv;
  g_return_if_fail (object != NULL);
  priv = SJ_METADATA_MUSICBRAINZ (object)->priv;

  g_free (priv->http_proxy);
  g_free (priv->cdrom);
  mb_Delete (priv->mb);
  g_free (priv);
}

static void
sj_metadata_musicbrainz_instance_init (GTypeInstance *instance, gpointer g_class)
{
  SjMetadataMusicbrainz *self = (SjMetadataMusicbrainz*)instance;
  self->priv = g_new0 (SjMetadataMusicbrainzPrivate, 1);
  self->priv->construct_error = NULL;
  self->priv->mb = mb_New ();
  // TODO
  if (!self->priv->mb) {
    g_set_error (&self->priv->construct_error,
                 SJ_ERROR, SJ_ERROR_CD_LOOKUP_ERROR,
                 _("Cannot create MusicBrainz client"));
    return;
  }
  mb_UseUTF8 (self->priv->mb, TRUE);
  if (g_getenv("MUSICBRAINZ_DEBUG")) {
    mb_SetDebug (self->priv->mb, TRUE);
  }
}

static void
metadata_interface_init (gpointer g_iface, gpointer iface_data)
{
  SjMetadataClass *klass = (SjMetadataClass*)g_iface;
  klass->get_new_error = mb_get_new_error;
  klass->set_cdrom = mb_set_cdrom;
  klass->set_proxy = mb_set_proxy;
  klass->set_proxy_port = mb_set_proxy_port;
  klass->list_albums = mb_list_albums;
}

static void
sj_metadata_musicbrainz_class_init (SjMetadataMusicbrainzClass *class)
{
  GObjectClass *object_class;
  parent_class = g_type_class_peek_parent (class);
  object_class = (GObjectClass*) class;
  object_class->finalize = sj_metadata_musicbrainz_finalize;
}

GType
sj_metadata_musicbrainz_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (SjMetadataMusicbrainzClass),
      NULL,
      NULL,
      (GClassInitFunc)sj_metadata_musicbrainz_class_init,
      NULL,
      NULL,
      sizeof (SjMetadataMusicbrainz),
      0,
      sj_metadata_musicbrainz_instance_init,
      NULL
    };
    static const GInterfaceInfo metadata_i_info = {
      (GInterfaceInitFunc) metadata_interface_init,
      NULL, NULL
    };
    type = g_type_register_static (G_TYPE_OBJECT, "SjMetadataMusicBrainzClass", &info, 0);
    g_type_add_interface_static (type, SJ_TYPE_METADATA, &metadata_i_info);
  }
  return type;
}

GObject *
sj_metadata_musicbrainz_new (void)
{
  return g_object_new (sj_metadata_musicbrainz_get_type (), NULL);
}

/**
 * Private methods
 */

#define BYTES_PER_SECTOR 2352
#define BYTES_PER_SECOND (44100 / 8) / 16 / 2

static int
get_duration_from_sectors (int sectors)
{
  return (sectors * BYTES_PER_SECTOR / BYTES_PER_SECOND);
}

static GList*
get_offline_track_listing(SjMetadata *metadata, GError **error)
{
  SjMetadataMusicbrainzPrivate *priv;
  GList* list = NULL;
  AlbumDetails *album;
  TrackDetails *track;
  int num_tracks, i;

  g_return_val_if_fail (metadata != NULL, NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

  if (!mb_Query (priv->mb, MBQ_GetCDTOC)) {
    char message[255];
    mb_GetQueryError (priv->mb, message, 255);
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_CD_LOOKUP_ERROR,
                 _("Cannot read CD: %s"), message);
    return NULL;
  }
  
  num_tracks = mb_GetResultInt (priv->mb, MBE_TOCGetLastTrack);

  album = g_new0 (AlbumDetails, 1);
  album->artist = g_strdup (_("Unknown Artist"));
  album->title = g_strdup (_("Unknown Title"));
  for (i = 1; i <= num_tracks; i++) {
    track = g_new0 (TrackDetails, 1);
    track->album = album;
    track->number = i;
    track->title = g_strdup_printf (_("Track %d"), i);
    track->artist = g_strdup (album->artist);
    track->duration = get_duration_from_sectors (mb_GetResultInt1 (priv->mb, MBE_TOCGetTrackNumSectors, i+1));
    album->tracks = g_list_append (album->tracks, track);
    album->number++;
  }
  return g_list_append (list, album);
}

/**
 * Virtual methods
 */

static GError*
mb_get_new_error (SjMetadata *metadata)
{
  GError *error;
  if (metadata == NULL || SJ_METADATA_MUSICBRAINZ (metadata)->priv == NULL) {
    g_set_error (&error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("MusicBrainz metadata object is not valid. This is bad, check your console for errors."));
    return error;
  }
  return SJ_METADATA_MUSICBRAINZ (metadata)->priv->construct_error;
}

static void
mb_set_cdrom (SjMetadata *metadata, const char* device)
{
  SjMetadataMusicbrainzPrivate *priv;
  g_return_if_fail (metadata != NULL);
  g_return_if_fail (device != NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

  if (priv->cdrom) {
    g_free (priv->cdrom);
  }
  priv->cdrom = g_strdup (device);
  mb_SetDevice (priv->mb, priv->cdrom);
}

static void
mb_set_proxy (SjMetadata *metadata, const char* proxy)
{
  SjMetadataMusicbrainzPrivate *priv;
  g_return_if_fail (metadata != NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

  if (proxy == NULL) {
    proxy = "";
  }
  if (priv->http_proxy) {
    g_free (priv->http_proxy);
  }
  priv->http_proxy = g_strdup (proxy);
  mb_SetProxy (priv->mb, priv->http_proxy, priv->http_proxy_port);
}

static void
mb_set_proxy_port (SjMetadata *metadata, const int port)
{
  SjMetadataMusicbrainzPrivate *priv;
  g_return_if_fail (metadata != NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

  priv->http_proxy_port = port;
  mb_SetProxy (priv->mb, priv->http_proxy, priv->http_proxy_port);
}

static GList*
mb_list_albums (SjMetadata *metadata, GError **error)
{
  SjMetadataMusicbrainzPrivate *priv;
  GList *albums = NULL;
  GList *al, *tl;
  char data[256];
  int num_albums, i, j;
  CDMediaType type;

  g_return_val_if_fail (metadata != NULL, NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;
  g_return_val_if_fail (priv->cdrom != NULL, NULL);

  type = guess_media_type (priv->cdrom);
  if (type == CD_MEDIA_TYPE_ERROR) {
    char *msg;
    SjError err;

    if (access (priv->cdrom, W_OK) == 0) {
      msg = g_strdup_printf (_("Device '%s' does not contain a media"), priv->cdrom);
      err = SJ_ERROR_CD_NO_MEDIA;
    } else {
      msg = g_strdup_printf (_("Device '%s' could not be opened. Check the access permissions on the device."), priv->cdrom);
      err = SJ_ERROR_CD_PERMISSION_ERROR;
    }
    g_set_error (error, SJ_ERROR, err, _("Cannot read CD: %s"), msg);
    g_free (msg);

    return NULL;
  }

  if (!mb_Query (priv->mb, MBQ_GetCDInfo)) {
    mb_GetQueryError (priv->mb, data, 256);
    g_print(_("This CD could not be queried: %s\n"), data);
    return get_offline_track_listing (metadata, error);
  }

  num_albums = mb_GetResultInt(priv->mb, MBE_GetNumAlbums);
  if (num_albums < 1) {
    g_print(_("This CD was not found.\n"));
    return get_offline_track_listing (priv->mb, error);
  }

  for (i = 1; i <= num_albums; i++) {
    int num_tracks;
    AlbumDetails *album;

    mb_Select1(priv->mb, MBS_SelectAlbum, i);
    album = g_new0 (AlbumDetails, 1);

    mb_GetResultData(priv->mb, MBE_AlbumGetAlbumName, data, 256);
    album->title = g_strdup (data);

    mb_GetResultData1(priv->mb, MBE_AlbumGetArtistName, data, 256, 1);
    album->artist = g_strdup (data);

    num_tracks = mb_GetResultInt(priv->mb, MBE_AlbumGetNumTracks);

    for (j = 1; j <= num_tracks; j++) {
      TrackDetails *track;
      track = g_new0 (TrackDetails, 1);

      track->album = album;

      track->number = j; /* replace with number lookup? */

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetTrackName, data, 256, j)) {
        track->title = g_strdup (data);
      }

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetArtistName, data, 256, j)) {
        track->artist = g_strdup (data);
      }

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetTrackDuration, data, 256, j)) {
        track->duration = atoi (data) / 1000;
      }

      album->tracks = g_list_append (album->tracks, track);
      album->number++;
    }

    albums = g_list_append (albums, album);
  }

  /* For each album, we need to insert the duration data if necessary
   * We need to query this here because otherwise we would flush the
   * data queried from the server */
  mb_Query (priv->mb, MBQ_GetCDTOC);
  for (al = albums; al; al = al->next) {
    AlbumDetails *album = al->data;
    
    j = 1;
    for (tl = album->tracks; tl; tl = tl->next) {
      TrackDetails *track = tl->data;
      int sectors;
      
      if (track->duration == 0) {
        sectors = mb_GetResultInt1 (priv->mb, MBE_TOCGetTrackNumSectors, j+1);
        track->duration = get_duration_from_sectors (sectors);
      }
      j++;
    }
  }
  
  return albums;
}
