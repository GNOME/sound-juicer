#include <glib/glist.h>
#include <glib/gerror.h>
#include <glib/gstrfuncs.h>
#include "sj-musicbrainz.h"
#include "sj-structures.h"

void sj_musicbrainz_init (void) {}

void sj_musicbrainz_set_cdrom (const char* device) {}

GList* sj_musicbrainz_list_albums (GError **error) {
  GList *albums = NULL;
  AlbumDetails *album;
  TrackDetails *track;
  int i;

  album = g_new0 (AlbumDetails, 1);
  album->title = "Some Title";
  album->artist = "Some Artist";

  for (i = 1; i < 10; i++) {
    track = g_new0 (TrackDetails, 1);
    track->number = i;
    track->title = g_strdup_printf ("Track %d", i);
    track->artist = album->artist;
    track->duration = 60;
    album->tracks = g_list_append (album->tracks, track);
  }
  albums = g_list_append (albums, album);

  album = g_new0 (AlbumDetails, 1);
  album->title = "Another Album";
  album->artist = "Someone Else";

  for (i = 1; i < 10; i++) {
    track = g_new0 (TrackDetails, 1);
    track->number = i;
    track->title = g_strdup_printf ("Song %d", i);
    track->artist = album->artist;
    track->duration = 120;
    album->tracks = g_list_append (album->tracks, track);
  }
  albums = g_list_append (albums, album);
  return albums;
}
