#include <glib.h>
#include "sj-structures.h"
#include "sj-metadata.h"
#include "sj-metadata-musicbrainz.h"

int main (int argc, char** argv)
{
  GList *albums = NULL;
  SjMetadata *metadata;

  g_type_init ();
  metadata = (SjMetadata*)sj_metadata_musicbrainz_new ();
  sj_metadata_set_cdrom (metadata, "/dev/cdroms/cdrom0");
  //albums = sj_metadata_list_albums (metadata, NULL);

  while (albums) {
    AlbumDetails *album;
    album = (AlbumDetails*)albums->data;
    g_print ("'%s', by %s\n", album->title, album->artist);
    while (album->tracks) {
      TrackDetails *track = (TrackDetails*)album->tracks->data;
      g_print (" Track %d; Title: %s; Artist: %s Duration: %d sec\n", track->number, track->title, track->artist, track->duration);
      album->tracks = g_list_next (album->tracks);
    }
    albums = g_list_next (albums);
  }
  g_object_unref (metadata);
  return 0;
}
