#include "sj-structures.h"
#include <glib.h>

int main(int argc, char **argv)
{
  AlbumDetails* album;
  TrackDetails* track;
  int i;
  
  album = g_new0 (AlbumDetails, 1);
  album->title = "Some Title";
  album->artist = "Some Artist";

  for (i = 1; i < 10; i++) {
    track = g_new0 (TrackDetails, 1);
    track->number = i;
    track->title = g_strdup_printf ("Track %d", i);
    track->artist = album->artist;
    track->duration = 0;
    album->tracks = g_list_append (album->tracks, track);
  }

  g_print("Album Title: %s; Artist: %s\n", album->title, album->artist);
  /* This destroys the GList but I don't care */
  while (album->tracks) {
    TrackDetails *track = (TrackDetails*)album->tracks->data;
    g_print("Track %d; Title: %s; Artist: %s\n", track->number, track->title, track->artist);
    album->tracks = g_list_next (album->tracks);
  }
  return 0;
}
