#include <glib.h>
#include <stdlib.h>
#include "sj-structures.h"
#include "sj-metadata.h"
#include "sj-metadata-musicbrainz.h"

static void
metadata_cb (SjMetadata *metadata, GList *albums, GError *error)
{
  if (error != NULL) {
    g_print ("Error: %s\n", error->message);
    g_error_free (error);
    g_object_unref (metadata);
    exit (1);
  }

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
  exit (0);
}

int main (int argc, char** argv)
{
  SjMetadata *metadata;
  GMainLoop *loop;

  g_type_init ();
  g_thread_init (NULL);
  
  metadata = (SjMetadata*)sj_metadata_musicbrainz_new ();

  if (argc == 2) {
    sj_metadata_set_cdrom (metadata, argv[1]);
  } else {
    g_print ("Usage: %s [CD device]\n", argv[0]);
    exit (1);
  }

  g_signal_connect (G_OBJECT (metadata), "metadata",
		    G_CALLBACK (metadata_cb), NULL);
  sj_metadata_list_albums (metadata, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
