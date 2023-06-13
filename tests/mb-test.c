#include "config.h"
#include <locale.h>
#include <udisks/udisks.h>
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

static const char *
release_type_to_id (const char *type)
{
	/* CD 2 of a multi-CD set
	 * Beastie Boys - Anthology: The Sounds of Science (disc 2)
	 * http://musicbrainz.org/release/0f15fc15-9538-44b6-aa19-4074934fc5ba.html */
	if (g_str_equal (type, "commercial"))
	  return "rg4F4Od5EgOwfDaI0niQ2TnCaxk-";
	/* Non-existant CD
	 * http://musicbrainz.org/bare/cdlookup.html?discid=aaaaaaaaaaaaaaaaaaaaaaaaaaaa */
	if (g_str_equal (type, "fake"))
	  return "aaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	/* Audio book
	 * Harry Potter and the Sorcerer's Stone (feat. narrator: Jim Dale) (disc 1)
	 * http://musicbrainz.org/release/947c6cdd-1188-4e3e-a53b-21bb3a49b79e.html */
	if (g_str_equal (type, "audiobook"))
	  return "VJ0lpdqHGE7r8wr.N8D6Q0G.pCs-";

	return NULL;
}

G_GNUC_NORETURN static void
metadata_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
  SjMetadataGetter* metadata = SJ_METADATA_GETTER (object);
  GError *error = NULL;
  char *url;
  GList *albums, *album_list;

  albums = sj_metadata_getter_list_albums_finish (metadata, result, &url, &error);

  if (error != NULL) {
    g_print ("Error: %s\n", error->message);
    g_object_unref (metadata);
    exit (1);
  }

  g_print ("Submit URL: %s\n", url);
  g_free (url);

  album_list = albums;
  while (albums) {
    AlbumDetails *album;
    char *disc_number;
    char *release_date = NULL;

    album = (AlbumDetails*)albums->data;
    g_print ("Source: %s\n", source_to_str(album->metadata_source));
    if (album->metadata_source == SOURCE_MUSICBRAINZ)
      g_print ("Album ID: %s\n", album->album_id);
    if (album->asin != NULL)
      g_print ("ASIN: %s\n", album->asin);
    if (album->discogs != NULL)
      g_print ("Discogs: %s\n", album->discogs);
    if (album->wikipedia != NULL)
      g_print ("Wikipedia: %s\n", album->wikipedia);
    if (album->is_spoken_word)
      g_print ("Is spoken word\n");
    disc_number = g_strdup_printf (" (Disc %d)", album->disc_number);
    if (album->release_date) {
      gchar *date;
      date = gst_date_time_to_iso8601_string (album->release_date);
      release_date = g_strdup_printf (", released %s\n", date);
      g_free (date);
    }
    g_print ("'%s', by %s%s%s\n", album->title, album->artist,
             album->disc_number ? disc_number : "",
             release_date ? release_date : "");
    g_free (release_date);
    g_free (disc_number);
    while (album->tracks) {
      TrackDetails *track = (TrackDetails*)album->tracks->data;
      gchar *composer;
      composer = track->composer ? g_strdup_printf (" Composer: %s;", track->composer) : g_strdup ("");
      g_print (" Track %d; Title: %s; Artist: %s;%s Duration: %d sec\n",
               track->number, track->title, track->artist, composer, track->duration);
      g_free (composer);
      album->tracks = g_list_next (album->tracks);
    }
    albums = g_list_next (albums);
  }

  g_list_free_full (album_list, (GDestroyNotify) album_details_free);
  g_object_unref (metadata);
  exit (0);
}

int main (int argc, char** argv)
{
  SjMetadataGetter *metadata;
  GMainLoop *loop;
  g_autoptr (UDisksClient) client;

  setlocale (LC_ALL, "");
  gst_init (&argc, &argv);

  /* Make sure probing of the various media have settled before going on */
  client = udisks_client_new_sync (NULL, NULL);
  g_assert_nonnull (client);
  udisks_client_settle (client);


  metadata = sj_metadata_getter_new ();

  if (argc == 2) {
    sj_metadata_getter_set_cdrom (metadata, argv[1]);
  } else if (argc == 3) {
    const char *id;
    sj_metadata_getter_set_cdrom (metadata, argv[1]);
    id = release_type_to_id (argv[2]);
    g_message ("argv %s id %s", argv[2], id);
    if (id == NULL) {
      g_print ("The faked type of disc must be one of: commercial, fake, audiobook\n");
      exit(1);
    }
    g_setenv ("MUSICBRAINZ_FORCE_DISC_ID", id, TRUE);
  } else {
    g_print ("Usage: %s [CD device] [commercial|fake|audiobook]\n", argv[0]);
    exit (1);
  }

  sj_metadata_getter_list_albums_async (metadata, NULL, metadata_cb, NULL);
  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  return 0;
}
