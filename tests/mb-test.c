#include <glib.h>
#include "sj-structures.h"
#include "sj-musicbrainz.h"

int main (int argc, char** argv)
{
  GList *albums;

  sj_musicbrainz_init (NULL);
  sj_musicbrainz_set_cdrom ("/dev/cdroms/cdrom1");
  albums = sj_musicbrainz_list_albums(NULL);

  while (albums) {
    AlbumDetails *album;
    album = (AlbumDetails*)albums->data;
    g_print ("'%s', by %s\n", album->title, album->artist);
    while (album->tracks) {
      TrackDetails *track = (TrackDetails*)album->tracks->data;
      g_print (" Track %d; Title: %s; Artist: %s\n", track->number, track->title, track->artist);
      album->tracks = g_list_next (album->tracks);
    }
    albums = g_list_next (albums);
  }
  sj_musicbrainz_destroy ();
  return 0;
}
