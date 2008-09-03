#include <glib.h>
#include <stdlib.h>
#include "sj-structures.h"
#include "sj-metadata.h"
#include "sj-metadata-getter.h"

static const char *
source_to_str (MetadataSource source)
{
	const char * strs[] = {
		"Unknown",
		"CD-Text",
		"FreeDB",
		"MusicBrainz",
		"Fallback"
	};
	return strs[source];
}

static void
metadata_cb (SjMetadataGetter *metadata, GList *albums, GError *error)
{
  char *url;

  if (error != NULL) {
    g_print ("Error: %s\n", error->message);
    g_object_unref (metadata);
    exit (1);
  }

  url = sj_metadata_getter_get_submit_url (metadata);
  g_print ("Submit URL: %s\n", url);
  g_free (url);

  while (albums) {
    AlbumDetails *album;
    album = (AlbumDetails*)albums->data;
    char *disc_number;
    g_print ("Source: %s\n", source_to_str(album->metadata_source));
    if (album->metadata_source == SOURCE_MUSICBRAINZ)
      g_print ("Album ID: %s\n", album->album_id);
    if (album->asin != NULL)
      g_print ("ASIN: %s\n", album->asin);
    if (album->discogs != NULL)
      g_print ("Discogs: %s\n", album->discogs);
    if (album->wikipedia != NULL)
      g_print ("Wikipedia: %s\n", album->wikipedia);
    disc_number = g_strdup_printf (" (Disc %d)", album->disc_number);
    g_print ("'%s', by %s%s\n", album->title, album->artist, album->disc_number ? disc_number : "");
    g_free (disc_number);
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
  SjMetadataGetter *metadata;
  GMainLoop *loop;
  GError *error = NULL;

  g_type_init ();
  g_thread_init (NULL);
  
  metadata = sj_metadata_getter_new ();

  if (argc == 2) {
    sj_metadata_getter_set_cdrom (metadata, argv[1]);
  } else {
    g_print ("Usage: %s [CD device]\n", argv[0]);
    exit (1);
  }

  g_signal_connect (G_OBJECT (metadata), "metadata",
		    G_CALLBACK (metadata_cb), NULL);
  if (sj_metadata_getter_list_albums (metadata, &error) == FALSE) {
    g_warning ("Couldn't list tracks on album: %s", error->message);
    g_error_free (error);
    return 1;
  }

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
