#include "config.h"
#include <glib.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus.h>
#include <stdlib.h>
#include "sj-structures.h"
#include "sj-metadata.h"
#include "sj-metadata-getter.h"

#define GCONF_ROOT "/apps/sound-juicer"

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

static void
metadata_cb (SjMetadataGetter *metadata, GList *albums, const GError *error)
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
    if (album->is_spoken_word)
      g_print ("Is spoken word\n");
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
  GConfClient *gconf_client;
  GError *error = NULL;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  dbus_threads_init_default ();

  g_type_init ();
  g_thread_init (NULL);

  gconf_client = gconf_client_get_default ();
  if (gconf_client == NULL) {
    g_warning (_("Could not create GConf client.\n"));
    exit (1);
  }
  gconf_client_add_dir (gconf_client, GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
  
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
