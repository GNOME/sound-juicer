/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-musicbrainz.c
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

#include "sound-juicer.h"

#include <glib/gerror.h>
#include <glib/glist.h>
#include <glib/gstrfuncs.h>
#include <glib/gmessages.h>
#include <musicbrainz/queries.h>
#include <musicbrainz/mb_c.h>
#include <stdlib.h>
#include <unistd.h>

#include "sj-musicbrainz.h"
#include "sj-structures.h"
#include "sj-error.h"
#include "cd-drive.h"

static musicbrainz_t mb;
static char* cdrom;
static char* http_proxy;
static int http_proxy_port;

void sj_musicbrainz_init (GError **error) {
  mb = mb_New ();
  if (!mb) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_CD_LOOKUP_ERROR,
                 _("Cannot create MusicBrainz client"));
    return;
  }
  mb_UseUTF8 (mb, TRUE);
  if (g_getenv("MUSICBRAINZ_DEBUG")) {
    mb_SetDebug (mb, TRUE);
  }
}

void sj_musicbrainz_destroy() {
  g_return_if_fail (mb != NULL);
  mb_Delete (mb);
}

void sj_musicbrainz_set_cdrom(const char* device) {
  g_return_if_fail (mb != NULL);
  g_return_if_fail (device != NULL);
  if (cdrom) {
    g_free (cdrom);
  }
  cdrom = g_strdup (device);
  mb_SetDevice (mb, cdrom);
}

void sj_musicbrainz_set_proxy (const char* proxy) {
  g_return_if_fail (mb != NULL);
  if (!proxy) {
    proxy = "";
  }
  if (http_proxy) {
    g_free (http_proxy);
  }
  http_proxy = g_strdup (proxy);
  mb_SetProxy (mb, http_proxy, http_proxy_port);
}

void sj_musicbrainz_set_proxy_port (int proxy_port) {
  g_return_if_fail (mb != NULL);
  http_proxy_port = proxy_port;
  mb_SetProxy (mb, http_proxy, http_proxy_port);
}

#define BYTES_PER_SECTOR 2352
#define BYTES_PER_SECOND (44100 / 8) / 16 / 2

static int sj_get_duration_from_sectors (int sectors)
{
  return (sectors * BYTES_PER_SECTOR / BYTES_PER_SECOND);
}

static GList* get_offline_track_listing(musicbrainz_t mb, GError **error)
{
  GList* list = NULL;
  AlbumDetails *album;
  TrackDetails *track;
  int num_tracks, i;

  if (!mb_Query (mb, MBQ_GetCDTOC)) {
    char message[255];
    mb_GetQueryError (mb, message, 255);
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_CD_LOOKUP_ERROR,
                 _("Cannot read CD: %s"), message);
    return NULL;
  }
  
  num_tracks = mb_GetResultInt (mb, MBE_TOCGetLastTrack);

  album = g_new0 (AlbumDetails, 1);
  album->artist = _("Unknown Artist");
  album->title = _("Unknown Title");
  for (i = 1; i <= num_tracks; i++) {
    track = g_new0 (TrackDetails, 1);
    track->album = album;
    track->number = i;
    track->title = g_strdup_printf (_("Track %d"), i);
    track->artist = album->artist;
    track->duration = sj_get_duration_from_sectors
      (mb_GetResultInt1 (mb, MBE_TOCGetTrackNumSectors, i+1));
    album->tracks = g_list_append (album->tracks, track);
    album->number++;
  }
  return g_list_append (list, album);
}

GList* sj_musicbrainz_list_albums(GError **error) {
  GList *albums = NULL;
  GList *al, *tl;
  char data[256];
  int num_albums, i, j;
  CDMediaType type;

  g_return_val_if_fail (mb != NULL, NULL);
  g_return_val_if_fail (cdrom != NULL, NULL);

  type = guess_media_type (cdrom);
  if (type == CD_MEDIA_TYPE_ERROR) {
    char *msg;
    SjError err;

    if (access (cdrom, W_OK) == 0) {
      msg = g_strdup_printf (_("Device '%s' does not contain a media"), cdrom);
      err = SJ_ERROR_CD_NO_MEDIA;
    } else {
      msg = g_strdup_printf (_("Device '%s' could not be opened. Check the access permissions on the device."), cdrom);
      err = SJ_ERROR_CD_PERMISSION_ERROR;
    }
    g_set_error (error, SJ_ERROR, err, _("Cannot read CD: %s"), msg);
    g_free (msg);

    return NULL;
  }

  if (!mb_Query (mb, MBQ_GetCDInfo)) {
    mb_GetQueryError (mb, data, 256);
    g_print(_("This CD could not be queried: %s\n"), data);
    return get_offline_track_listing (mb, error);
  }

  num_albums = mb_GetResultInt(mb, MBE_GetNumAlbums);
  if (num_albums < 1) {
    g_print(_("This CD was not found.\n"));
    return get_offline_track_listing (mb, error);
  }

  for (i = 1; i <= num_albums; i++) {
    int num_tracks;
    AlbumDetails *album;

    mb_Select1(mb, MBS_SelectAlbum, i);
    album = g_new0 (AlbumDetails, 1);

    mb_GetResultData(mb, MBE_AlbumGetAlbumName, data, 256);
    album->title = g_strdup (data);

    mb_GetResultData1(mb, MBE_AlbumGetArtistName, data, 256, 1);
    album->artist = g_strdup (data);

    num_tracks = mb_GetResultInt(mb, MBE_AlbumGetNumTracks);

    for (j = 1; j <= num_tracks; j++) {
      TrackDetails *track;
      track = g_new0 (TrackDetails, 1);

      track->album = album;

      track->number = j; /* replace with number lookup? */

      if (mb_GetResultData1(mb, MBE_AlbumGetTrackName, data, 256, j)) {
        track->title = g_strdup (data);
      }

      if (mb_GetResultData1(mb, MBE_AlbumGetArtistName, data, 256, j)) {
        track->artist = g_strdup (data);
      }

      if (mb_GetResultData1(mb, MBE_AlbumGetTrackDuration, data, 256, j)) {
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
  mb_Query (mb, MBQ_GetCDTOC);
  for (al = albums; al; al = al->next) {
    AlbumDetails *album = al->data;

    j = 1;
    for (tl = album->tracks; tl; tl = tl->next) {
      TrackDetails *track = tl->data;
      int sectors;

      if (track->duration == 0) {
        sectors = mb_GetResultInt1 (mb, MBE_TOCGetTrackNumSectors, j+1);
	track->duration = sj_get_duration_from_sectors (sectors);
      }
      j++;
    }
  }

  return albums;
}
