#include <glib/gerror.h>
#include <glib/glist.h>
#include <glib/gstrfuncs.h>
#include <glib/gmessages.h>
#include <musicbrainz/queries.h>
#include <musicbrainz/mb_c.h>
#include <stdlib.h>
#include "sj-musicbrainz.h"
#include "sj-structures.h"

static char* cdrom;

void sj_musicbrainz_init (void) {
  cdrom = g_strdup ("/dev/cdrom");
}

void sj_musicbrainz_set_cdrom(const char* device) {
  g_return_if_fail (device != NULL);
  g_free (cdrom);
  cdrom = g_strdup (device);
}

static GList* get_offline_track_listing(musicbrainz_t mb, GError **error)
{
  GList* list = NULL;
  AlbumDetails *album;
  TrackDetails *track;
  int num_tracks, i;

  if (!mb_Query (mb, MBQ_GetCDTOC)) {
    char error[255];
    mb_GetQueryError (mb, error, 255);
    g_print("Cannot read CD: %s\n", error);
    return NULL; /* TODO: GError */
  }
  num_tracks = mb_GetResultInt (mb, MBE_TOCGetLastTrack);

  album = g_new0 (AlbumDetails, 1);
  album->artist = "Unknown Artist";
  album->title = "Unknown Title";
  for (i = 1; i <= num_tracks; i++) {
    track = g_new0 (TrackDetails, 1);
    track->album = album;
    track->number = i;
    track->title = g_strdup_printf ("Track %d", i);
    track->artist = album->artist;
    /* TODO: track duration */
    album->tracks = g_list_append (album->tracks, track);
  }
  return g_list_append (list, album);
}

/* TODO: GErrorify */
GList* sj_musicbrainz_list_albums(GError **error) {
  GList *albums = NULL;
  musicbrainz_t mb;
  char data[256];
  int num_albums, i, j;

  mb = mb_New ();
  if (!mb) {
    g_print ("Cannot get MusicBrainz connection\n");
    return NULL; /* TODO: GError */
  }
  mb_SetDevice (mb, cdrom);

  if (!mb_Query (mb, MBQ_GetCDInfo)) {
    char error[255];
    mb_GetQueryError (mb, error, 255);
    g_print("Cannot lookup CD: %s\n", error);
    return get_offline_track_listing (mb, NULL);
  }

  num_albums = mb_GetResultInt(mb, MBE_GetNumAlbums);
  if (num_albums < 1) {
    g_print("This CD was not found.\n");
    return NULL;
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

      track->number = j; /* TODO: replace with number lookup */

      if (mb_GetResultData1(mb, MBE_AlbumGetTrackName, data, 256, j)) {
        track->title = g_strdup (data);
      }

      if (mb_GetResultData1(mb, MBE_AlbumGetArtistName, data, 256, i)) {
        track->artist = g_strdup (data);
      }

      if (mb_GetResultData1(mb, MBE_AlbumGetTrackDuration, data, 256, j)) {
        track->duration = atoi (data) / 1000;
      }

      album->tracks = g_list_append (album->tracks, track);
    }

    albums = g_list_append (albums, album);
  }
  mb_Delete (mb);
  return albums;
}
