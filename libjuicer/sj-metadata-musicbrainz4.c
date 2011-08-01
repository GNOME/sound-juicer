/*
 * sj-metadata-musicbrainz4.c
 * Copyright (C) 2008 Ross Burton <ross@burtonini.com>
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
 * Copyright (C) 2011 Christophe Fergeau <cfergeau@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <gconf/gconf-client.h>
#include <discid/discid.h>
#include <musicbrainz4/mb4_c.h>

#include "sj-metadata-musicbrainz4.h"
#include "sj-structures.h"
#include "sj-error.h"

#define GET(field, function, obj) {						\
	function (obj, buffer, sizeof (buffer));				\
	if (field)								\
		g_free (field);							\
	if (*buffer == '\0')							\
		field = NULL;							\
	else									\
		field = g_strdup (buffer);					\
}

#define GCONF_MUSICBRAINZ_SERVER "/apps/sound-juicer/musicbrainz_server"
#define GCONF_PROXY_USE_PROXY "/system/http_proxy/use_http_proxy"
#define GCONF_PROXY_HOST "/system/http_proxy/host"
#define GCONF_PROXY_PORT "/system/http_proxy/port"
#define GCONF_PROXY_USE_AUTHENTICATION "/system/http_proxy/use_authentication"
#define GCONF_PROXY_USERNAME "/system/http_proxy/authentication_user"
#define GCONF_PROXY_PASSWORD "/system/http_proxy/authentication_password"
#define SJ_MUSICBRAINZ_USER_AGENT "libjuicer-"VERSION

typedef struct {
  Mb4Query mb;
  DiscId *disc;
  char *cdrom;
  /* Proxy */
  char *http_proxy;
  int http_proxy_port;
} SjMetadataMusicbrainz4Private;

#define GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SJ_TYPE_METADATA_MUSICBRAINZ4, SjMetadataMusicbrainz4Private))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_USE_PROXY,
  PROP_PROXY_HOST,
  PROP_PROXY_PORT,
};

static void metadata_interface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SjMetadataMusicbrainz4,
                         sj_metadata_musicbrainz4,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SJ_TYPE_METADATA,
                                                metadata_interface_init));


/*
 * Private methods
 */


struct _SjMb4ArtistDetails {
  char *id;
  char *name;
  char *sortname;
  char *disambiguation;
  char *gender;
  char *country;

  /* doesn't belong in here, prevent sharing the artist structure between
   * distinct ReleaseGroups - more convenient for now */
  char *joinphrase;
};
typedef struct _SjMb4ArtistDetails SjMb4ArtistDetails;

struct _SjMb4AlbumDetails {
    AlbumDetails parent;
    GList *artists;
    char *status;
    char *quality;
    char *disambiguation;
    char *packaging;
    char *country;
    char *barcode;
    char *type;
    char *comment;
    char *format;
};
typedef struct _SjMb4AlbumDetails SjMb4AlbumDetails;

struct _SjMb4TrackDetails {
    TrackDetails parent;
    GList *artists;
};
typedef struct _SjMb4TrackDetails SjMb4TrackDetails;

static void
sj_mb4_artist_details_free (SjMb4ArtistDetails *details)
{
  g_free (details->id);
  g_free (details->name);
  g_free (details->sortname);
  g_free (details->disambiguation);
  g_free (details->gender);
  g_free (details->country);
  g_free (details->joinphrase);
  g_free (details);
}

static void
sj_mb4_track_details_free (SjMb4TrackDetails *details)
{
  g_list_foreach (details->artists, (GFunc)sj_mb4_artist_details_free, NULL);
  g_list_free (details->artists);
  track_details_free ((TrackDetails *)details);
}

static void
sj_mb4_album_details_free (SjMb4AlbumDetails *details)
{
  g_list_foreach (details->artists, (GFunc)sj_mb4_artist_details_free, NULL);
  g_list_free (details->artists);
  g_free (details->status);
  g_free (details->quality);
  g_free (details->disambiguation);
  g_free (details->packaging);
  g_free (details->country);
  g_free (details->barcode);
  g_free (details->type);
  g_free (details->comment);
  g_free (details->format);
  g_list_foreach (details->parent.tracks, (GFunc)sj_mb4_track_details_free, NULL);
  g_list_free (details->parent.tracks);
  /* prevent album_details_free from double-freeing ::tracks */
  details->parent.tracks = NULL;
  album_details_free ((AlbumDetails *)details);
}

static GList *
get_artist_list (Mb4ArtistCredit credit)
{
  Mb4NameCreditList name_list;
  GList *artists;
  unsigned int i;
  char buffer[512]; /* for the GET macro */

  if (credit == NULL)
    return NULL;

  name_list = mb4_artistcredit_get_namecreditlist (credit);
  if (name_list == NULL) {
    return NULL;
  }

  artists = NULL;
  for (i = 0; i < mb4_namecredit_list_size (name_list); i++) {
    Mb4NameCredit name_credit;
    Mb4Artist artist;
    SjMb4ArtistDetails *details;

    name_credit = mb4_namecredit_list_item (name_list, i);
    details = g_new0 (SjMb4ArtistDetails, 1);
    GET (details->joinphrase, mb4_namecredit_get_joinphrase, name_credit);
    artists = g_list_prepend (artists, details);
    artist = mb4_namecredit_get_artist (name_credit);
    if (!artist) {
      g_warning ("no Mb4Artist associated with Mb4NameCredit, falling back to Mb4NameCredit::name");
      GET (details->name, mb4_namecredit_get_name, name_credit);
      mb4_namecredit_delete (name_credit);
      continue;
    }

    GET (details->id, mb4_artist_get_id, artist);
    GET (details->name, mb4_artist_get_name, artist);
    GET (details->sortname, mb4_artist_get_sortname, artist);
    GET (details->disambiguation, mb4_artist_get_disambiguation, artist);
    GET (details->gender, mb4_artist_get_gender, artist);
    GET (details->country, mb4_artist_get_country, artist);
    mb4_namecredit_delete (name_credit);
  }

  return g_list_reverse(artists);
}

static void
get_artist_info (GList *artists, char **name, char **sortname, char **id)
{
  GString *artist_name;
  GList *it;
  unsigned int artist_count;

  artist_name = g_string_new (NULL);
  artist_count = 0;
  for (it = artists; it != NULL; it = it->next) {
    SjMb4ArtistDetails *details = (SjMb4ArtistDetails *)it->data;
    artist_count++;
    g_string_append (artist_name, details->name);
    if (details->joinphrase != NULL)
      g_string_append (artist_name, details->joinphrase);
  }

  if (artist_count != 1) {
      g_warning ("multiple artists");
      if (sortname != NULL)
        *sortname = NULL;
      if (id != NULL)
        *id = NULL;
  } else {
      SjMb4ArtistDetails *details = (SjMb4ArtistDetails *)artists->data;
      if (sortname != NULL)
        *sortname = g_strdup (details->sortname);
      if (id != NULL)
        *id = g_strdup (details->id);
  }

  if (name != NULL)
    *name = artist_name->str;

  g_string_free (artist_name, FALSE);
}

static void
fill_tracks_from_medium (Mb4Medium medium, AlbumDetails *album)
{
  Mb4TrackList track_list;
  GList *tracks;
  unsigned int i;
  char buffer[512]; /* for the GET() macro */

  track_list = mb4_medium_get_tracklist (medium);
  if (!track_list)
    return;

  album->number = mb4_track_list_size (track_list);

  tracks = NULL;

  for (i = 0; i < mb4_track_list_size (track_list); i++) {
    Mb4Track mbt;
    Mb4ArtistCredit credit;
    Mb4Recording recording;
    TrackDetails *track;
    SjMb4TrackDetails *mb4_track;

    mbt = mb4_track_list_item (track_list, i);
    if (!mbt)
      continue;

    mb4_track = g_new0 (SjMb4TrackDetails, 1);
    track = &mb4_track->parent;

    track->album = album;

    track->number = mb4_track_get_position (mbt);
    recording = mb4_track_get_recording (mbt);
    if (recording != NULL) {
      GET (track->title, mb4_recording_get_title, recording);
      GET (track->track_id, mb4_recording_get_id, recording);
      track->duration = mb4_recording_get_length (recording) / 1000;
      credit = mb4_recording_get_artistcredit (recording);
    } else {
      GET (track->title, mb4_track_get_title, mbt);
      track->duration = mb4_track_get_length (mbt) / 1000;
      credit = mb4_track_get_artistcredit (mbt);
    }

    if (credit) {
      GList *artists;
      artists = get_artist_list (credit);
      if (artists) {
        get_artist_info (artists, &track->artist,
                         &track->artist_sortname,
                         &track->artist_id);
      }
      mb4_track->artists = artists;
    }
    if (track->artist == NULL)
      track->artist = g_strdup (album->artist);
    if (track->artist_sortname == NULL)
      track->artist_sortname = g_strdup (album->artist_sortname);
    if (track->artist_id == NULL)
      track->artist_id = g_strdup (album->artist_id);

    tracks = g_list_prepend (tracks, track);
    mb4_track_delete (mbt);
  }
  album->tracks = g_list_reverse (tracks);
}

static AlbumDetails *
make_album_from_release (Mb4Release release, Mb4Medium medium)
{
  AlbumDetails *album;
  SjMb4AlbumDetails *mb4_album;
  Mb4ArtistCredit credit;
  Mb4ReleaseGroup group;
  GList *artists;
  char *date = NULL;
  char *new_title;
  char buffer[512]; /* for the GET macro */

  g_assert (release);
  g_return_val_if_fail (medium != NULL, NULL);

  mb4_album = g_new0 (SjMb4AlbumDetails, 1);
  album = &mb4_album->parent;

  GET (album->album_id, mb4_release_get_id, release);
  GET (album->title, mb4_medium_get_title, medium);
  if (album->title == NULL)
    GET (album->title, mb4_release_get_title, release);

  credit = mb4_release_get_artistcredit (release);

  artists = get_artist_list (credit);
  if (artists) {
    get_artist_info (artists, &album->artist,
                     &album->artist_sortname,
                     &album->artist_id);
  }
  mb4_album->artists = artists;

  GET (date, mb4_release_get_date, release);
  album->release_date = sj_metadata_helper_scan_date (date);
  g_free (date);

  GET (album->asin, mb4_release_get_asin, release);
  GET (mb4_album->status, mb4_release_get_status, release);
  GET (mb4_album->quality, mb4_release_get_quality, release);
  GET (mb4_album->disambiguation, mb4_release_get_disambiguation, release);
  GET (mb4_album->packaging, mb4_release_get_packaging, release);
  GET (mb4_album->country, mb4_release_get_country, release);
  GET (mb4_album->barcode, mb4_release_get_barcode, release);
  group = mb4_release_get_releasegroup (release);
  if (group) {
    GET (mb4_album->type, mb4_releasegroup_get_type, group);
    GET (mb4_album->comment, mb4_releasegroup_get_comment, group);
  }
  GET(mb4_album->format, mb4_medium_get_format, medium);

#if 0
  for (i = 0; i < mb_release_get_num_relations (release); i++) {
    MbRelation relation;
    char *type = NULL;

    relation = mb_release_get_relation (release, i);
    GET(type, mb_relation_get_type, relation);
    if (type && g_str_equal (type, "http://musicbrainz.org/ns/rel-1.0#Wikipedia")) {
      GET (album->wikipedia, mb_relation_get_target_id, relation);
    } else if (type && g_str_equal (type, "http://musicbrainz.org/ns/rel-1.0#Discogs")) {
      GET (album->discogs, mb_relation_get_target_id, relation);
      continue;
    }
    g_free (type);
  }
#else
  g_warning("Relations not handled");
#endif

#if 0
  for (i = 0; i < mb_release_get_num_types (release); i++) {
    mb_release_get_type (release, i, buffer, sizeof(buffer));

    if (g_str_has_suffix (buffer, "#Spokenword")
    	|| g_str_has_suffix (buffer, "#Interview")
    	|| g_str_has_suffix (buffer, "#Audiobook")) {
      album->is_spoken_word = TRUE;
      break;
    }
  }
#else
  /* If it ReleaseGroup::type that we want or something else? */
  g_warning("Recording type not handled");
#endif

  album->disc_number = mb4_medium_get_position (medium);
  fill_tracks_from_medium (medium, album);

  return album;
}

/*
 * Virtual methods
 */
static GList *
mb4_list_albums (SjMetadata *metadata, char **url, GError **error)
{
  SjMetadataMusicbrainz4Private *priv;
  GList *albums = NULL;
  Mb4ReleaseList releases;
  Mb4Release release;
  const char *discid = NULL;
  char buffer[1024];
  int i;
  g_return_val_if_fail (SJ_IS_METADATA_MUSICBRAINZ4 (metadata), NULL);

  priv = GET_PRIVATE (metadata);

  if (sj_metadata_helper_check_media (priv->cdrom, error) == FALSE) {
    return NULL;
  }

  priv->disc = discid_new ();
  if (priv->disc == NULL)
    return NULL;
  if (discid_read (priv->disc, priv->cdrom) == 0)
    return NULL;

  if (url != NULL)
    *url = g_strdup (discid_get_submission_url (priv->disc));

  if (g_getenv("MUSICBRAINZ_FORCE_DISC_ID")) {
    discid = g_getenv("MUSICBRAINZ_FORCE_DISC_ID");
  } else {
    discid = discid_get_id (priv->disc);
  }

  releases = mb4_query_lookup_discid(priv->mb, discid);

  if (releases == NULL) {
    return NULL;
  }

  if (mb4_release_list_size (releases) == 0) {
    mb4_release_list_delete (releases);
    return NULL;
  }

  for (i = 0; i < mb4_release_list_size (releases); i++) {
    AlbumDetails *album;

    release = mb4_release_list_item (releases, i);
    if (release) {
      char *releaseid;
      Mb4Release full_release;

      releaseid = NULL;
      GET(releaseid, mb4_release_get_id, release);

      full_release = mb4_query_lookup_release (priv->mb, releaseid);
      g_free (releaseid);
      if (full_release) {
        Mb4MediumList media;
        unsigned int j;

        media = mb4_release_media_matching_discid (full_release, discid);
        for (j = 0; j < mb4_medium_list_size (media); j++) {
          Mb4Medium medium;
          medium = mb4_medium_list_item (media, j);
          if (medium) {
            album = make_album_from_release (full_release, medium);
            album->metadata_source = SOURCE_MUSICBRAINZ;
            albums = g_list_append (albums, album);
            mb4_medium_delete (medium);
          }
        }
        mb4_medium_list_delete (media);
        mb4_release_delete (full_release);
      }
      mb4_release_delete (release);
    }
  }
  mb4_release_list_delete (releases);
  return albums;
}

/*
 * GObject methods
 */

static void
metadata_interface_init (gpointer g_iface, gpointer iface_data)
{
  SjMetadataClass *klass = (SjMetadataClass*)g_iface;

  klass->list_albums = mb4_list_albums;
}

static void
sj_metadata_musicbrainz4_init (SjMetadataMusicbrainz4 *self)
{
  GConfClient *gconf_client;
  gchar *server_name;

  SjMetadataMusicbrainz4Private *priv;

  priv = GET_PRIVATE (self);

  gconf_client = gconf_client_get_default ();

  server_name = gconf_client_get_string (gconf_client, GCONF_MUSICBRAINZ_SERVER, NULL);

  if (server_name && (*server_name == '\0')) {
    g_free (server_name);
    server_name = NULL;
  }

  priv->mb = mb4_query_new (SJ_MUSICBRAINZ_USER_AGENT, server_name, 0);
  g_free (server_name);


  /* Set the HTTP proxy */
  if (gconf_client_get_bool (gconf_client, GCONF_PROXY_USE_PROXY, NULL)) {
    char *proxy_host;
    int port;

    proxy_host = gconf_client_get_string (gconf_client, GCONF_PROXY_HOST, NULL);
    mb4_query_set_proxyhost (priv->mb, proxy_host);
    g_free (proxy_host);

    port = gconf_client_get_int (gconf_client, GCONF_PROXY_PORT, NULL);
    mb4_query_set_proxyport (priv->mb, port);

    if (gconf_client_get_bool (gconf_client, GCONF_PROXY_USE_AUTHENTICATION, NULL)) {
      char *username, *password;

      username = gconf_client_get_string (gconf_client, GCONF_PROXY_USERNAME, NULL);
      mb4_query_set_proxyusername (priv->mb, username);
      g_free (username);

      password = gconf_client_get_string (gconf_client, GCONF_PROXY_PASSWORD, NULL);
      mb4_query_set_proxypassword (priv->mb, password);
      g_free (password);
    }
  }

  g_object_unref (gconf_client);
}

static void
sj_metadata_musicbrainz4_get_property (GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec)
{
  SjMetadataMusicbrainz4Private *priv = GET_PRIVATE (object);
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    g_value_set_string (value, priv->cdrom);
    break;
  case PROP_PROXY_HOST:
    g_value_set_string (value, priv->http_proxy);
    break;
  case PROP_PROXY_PORT:
    g_value_set_int (value, priv->http_proxy_port);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_musicbrainz4_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec)
{
  SjMetadataMusicbrainz4Private *priv = GET_PRIVATE (object);
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    if (priv->cdrom)
      g_free (priv->cdrom);
    priv->cdrom = g_value_dup_string (value);
    break;
  case PROP_PROXY_HOST:
    if (priv->http_proxy) {
      g_free (priv->http_proxy);
    }
    priv->http_proxy = g_value_dup_string (value);
    /* TODO: check this unsets the proxy if NULL, or should we pass "" ? */
    mb4_query_set_proxyhost (priv->mb, priv->http_proxy);
    break;
  case PROP_PROXY_PORT:
    priv->http_proxy_port = g_value_get_int (value);
    mb4_query_set_proxyport (priv->mb, priv->http_proxy_port);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_musicbrainz4_finalize (GObject *object)
{
  SjMetadataMusicbrainz4Private *priv;

  priv = GET_PRIVATE (object);

  if (priv->mb != NULL) {
    mb4_query_delete (priv->mb);
    priv->mb = NULL;
  }
  if (priv->disc != NULL) {
      discid_free (priv->disc);
      priv->disc = NULL;
  }
  g_free (priv->cdrom);

  G_OBJECT_CLASS (sj_metadata_musicbrainz4_parent_class)->finalize (object);
}

static void
sj_metadata_musicbrainz4_class_init (SjMetadataMusicbrainz4Class *class)
{
  GObjectClass *object_class = (GObjectClass*)class;

  g_type_class_add_private (class, sizeof (SjMetadataMusicbrainz4Private));

  object_class->get_property = sj_metadata_musicbrainz4_get_property;
  object_class->set_property = sj_metadata_musicbrainz4_set_property;
  object_class->finalize = sj_metadata_musicbrainz4_finalize;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
  g_object_class_override_property (object_class, PROP_PROXY_HOST, "proxy-host");
  g_object_class_override_property (object_class, PROP_PROXY_PORT, "proxy-port");
}


/*
 * Public methods.
 */

GObject *
sj_metadata_musicbrainz4_new (void)
{
  return g_object_new (SJ_TYPE_METADATA_MUSICBRAINZ4, NULL);
}
