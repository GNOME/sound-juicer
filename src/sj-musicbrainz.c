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
#include "sj-musicbrainz.h"
#include "sj-structures.h"
#include "sj-error.h"

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
  g_return_if_fail (proxy != NULL);
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
    /* TODO: track duration */
    album->tracks = g_list_append (album->tracks, track);
  }
  return g_list_append (list, album);
}

GList* sj_musicbrainz_list_albums(GError **error) {
  GList *albums = NULL;
  char data[256];
  int num_albums, i, j;

  g_return_val_if_fail (mb != NULL, NULL);

  if (!mb_Query (mb, MBQ_GetCDInfo)) {
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
    }

    albums = g_list_append (albums, album);
  }
  return albums;
}
