/*
 * sj-metadata-musicbrainz5.c
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "sj-metadata"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <gdesktop-enums.h>
#include <discid/discid.h>
#include <musicbrainz5/mb5_c.h>

#include "sj-metadata-musicbrainz5.h"
#include "sj-structures.h"
#include "sj-error.h"

typedef char Mbid[40];

typedef struct {
  gint count;
  gchar *id;
  gchar *mcn;
  gchar *url;
  Mbid  *release_ids;
} DiscDetails;

static char language[3];

#define GET(field, function, obj) {						\
	function (obj, buffer, sizeof (buffer));				\
	if (field)								\
		g_free (field);							\
	if (*buffer == '\0')							\
		field = NULL;							\
	else									\
		field = g_strdup (buffer);					\
}

#ifndef DISCID_HAVE_SPARSE_READ
#define discid_read_sparse(disc, dev, i) discid_read(disc, dev)
#endif

#define SJ_MUSICBRAINZ_USER_AGENT "libjuicer-"VERSION

typedef struct {
  Mb5Query mb;
  char    *cdrom;
  GHashTable *artist_cache;
  /* Proxy */
  GDesktopProxyMode proxy_mode;
  char    *proxy_host;
  char    *proxy_username;
  char    *proxy_password;
  int      proxy_port;
  gboolean proxy_use_authentication;
} SjMetadataMusicbrainz5Private;

#define GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SJ_TYPE_METADATA_MUSICBRAINZ5, SjMetadataMusicbrainz5Private))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_PROXY_USE_AUTHENTICATION,
  PROP_PROXY_HOST,
  PROP_PROXY_PORT,
  PROP_PROXY_USERNAME,
  PROP_PROXY_PASSWORD,
  PROP_PROXY_MODE
};

static void metadata_interface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (SjMetadataMusicbrainz5,
                         sj_metadata_musicbrainz5,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (SJ_TYPE_METADATA,
                                                metadata_interface_init));


/*
 * Private methods
 */
#ifdef DUMP_DETAILS
static void
sj_mb5_album_details_dump (AlbumDetails *details)
{
  if (details->country)
    g_print ("Country: %s\n", details->country);
  if (details->type)
    g_print ("Type: %s\n", details->type);
  if (details->lyrics_url)
    g_print ("Lyrics URL: %s\n", details->lyrics_url);
}
#else
#define sj_mb5_album_details_dump(...)
#endif

static void
disc_details_free (DiscDetails *disc)
{
  g_free (disc->id);
  g_free (disc->mcn);
  g_free (disc->url);
  g_free (disc->release_ids);
  g_free (disc);
}

static void
parse_artist_aliases (Mb5Artist   artist,
                      char      **name,
                      char      **sortname)
{
  Mb5AliasList alias_list;
  int i;
  char buffer[512]; /* for the GET macro */
  char locale[512];

  GET (*name, mb5_artist_get_name, artist);
  GET (*sortname, mb5_artist_get_sortname, artist);

  if (*language == 0)
    return;

  alias_list = mb5_artist_get_aliaslist (artist);
  if (alias_list == NULL)
    return;

  for (i = 0; i < mb5_alias_list_size (alias_list); i++) {
    Mb5Alias alias = mb5_alias_list_item (alias_list, i);

    if (alias == NULL)
      continue;

    if (mb5_alias_get_locale (alias, locale, sizeof(locale)) > 0 &&
        strcmp (locale, language) == 0 &&
        mb5_alias_get_primary (alias, buffer, sizeof(buffer)) > 0 &&
        strcmp (buffer, "primary") == 0) {
      GET (*name, mb5_alias_get_text, alias);
      GET (*sortname, mb5_alias_get_sortname, alias);
    }
  }
}

static ArtistDetails*
make_artist_details (SjMetadataMusicbrainz5 *self,
                     Mb5Artist               artist)
{
  char buffer[512]; /* For the GET macro */
  ArtistDetails *details;
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (self);

  mb5_artist_get_id (artist, buffer, sizeof(buffer));
  details = g_hash_table_lookup (priv->artist_cache, buffer);
  if (details == NULL) {
    details = g_new0 (ArtistDetails, 1);
    details->id = g_strdup (buffer);
    parse_artist_aliases (artist, &details->name, &details->sortname);
    GET (details->disambiguation, mb5_artist_get_disambiguation, artist);
    GET (details->gender, mb5_artist_get_gender, artist);
    GET (details->country, mb5_artist_get_country, artist);
    g_hash_table_insert (priv->artist_cache, details->id, details);
  }
  return details;
}

static void
print_musicbrainz_query_error (SjMetadataMusicbrainz5 *self,
                               const char             *entity,
                               const char             *id)
{
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (self);
  int len;
  char *message;
  int code =  mb5_query_get_lasthttpcode (priv->mb);
  /* No error if the discid isn't found */
  if (strcmp (entity, "discid") == 0 && code == 404)
    return;
  len =  mb5_query_get_lasterrormessage (priv->mb, NULL, 0) + 1;
  message = g_malloc (len);
  mb5_query_get_lasterrormessage (priv->mb, message, len);
  g_info ("No Musicbrainz metadata for %s %s, http code %d, %s",
             entity, id, code, message);
  g_free (message);
}

static gint64 last_query_time;

static Mb5Metadata
query_musicbrainz (SjMetadataMusicbrainz5  *self,
                   const char              *entity,
                   const char              *id,
                   char                    *includes,
                   GCancellable            *cancellable,
                   GError                 **error)
{
  Mb5Metadata metadata;
  char *inc[] = { "inc" };
  gint64 delay = 4 * G_USEC_PER_SEC;
  int count = 0;
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (self);
  gint64 t;

  g_info ("Querying %s %s", entity, id);

  while (TRUE) {
    if (g_cancellable_set_error_if_cancelled (cancellable, error))
      return NULL;

    t = g_get_monotonic_time ();
    while (t - last_query_time < G_USEC_PER_SEC) {
      g_usleep (t - last_query_time + G_USEC_PER_SEC);
      t = g_get_monotonic_time ();
    }

    if (includes == NULL)
      metadata = mb5_query_query (priv->mb, entity, id, "",
                                  0, NULL, NULL);
    else
      metadata = mb5_query_query (priv->mb, entity, id, "",
                                  1, inc, &includes);

    if (metadata != NULL || ++count == 5 ||
        mb5_query_get_lasthttpcode (priv->mb) != 503)
      break;
    g_info ("Retrying %d sleeping for %.2g",
            count,
            (double)(delay)/(double)(G_USEC_PER_SEC));

    g_usleep (delay);
    if (delay < 30 * G_USEC_PER_SEC) {
      delay *= 45; /* Increase delay by ~2√2 */
      delay /= 16; /* Gives 4, 11, 30 */
    }
  }

  if (metadata == NULL)
    print_musicbrainz_query_error (self, entity, id);

  last_query_time = g_get_monotonic_time ();
  return metadata;
}

static Mb5Metadata
get_disc_md (SjMetadataMusicbrainz5  *self,
             DiscDetails             *disc_data,
             GCancellable            *cancellable,
             GError                 **error)
{
  const char *discid;
  DiscId disc;
  Mb5Metadata disc_md = NULL;
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (self);

  if (sj_metadata_helper_check_media (priv->cdrom, error) == FALSE) {
    return NULL;
  }

  disc = discid_new ();
  if (disc == NULL)
    return NULL;
  if (discid_read_sparse (disc, priv->cdrom, DISCID_FEATURE_MCN) == 0)
    goto out;

  if (g_getenv("MUSICBRAINZ_FORCE_DISC_ID")) {
    discid = g_getenv("MUSICBRAINZ_FORCE_DISC_ID");
  } else {
    discid = discid_get_id (disc);
  }
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    goto out;

  disc_md = query_musicbrainz (self, "discid", discid, NULL, cancellable, error);
  disc_data->url = g_strdup (discid_get_submission_url (disc));
  disc_data->mcn = g_strdup (discid_get_mcn (disc));
  disc_data->id = g_strdup (discid);
  g_info ("Disc id %s\nSubmission URL %s\nDisc MCN %s",
          disc_data->id,
          disc_data->url,
          disc_data->mcn);
 out:
  discid_free (disc);
  return disc_md;
}

static gboolean
mcn_matches_barcode (const char *mcn,
                     const char *barcode)
{
  if (mcn == NULL || barcode == NULL)
    return FALSE;

  /* The MCN should match an EAN barcode (13 digits)
   * or an UPC barcode (12 digits) with a leading '0'.
   */
  gsize len = strlen (barcode);
  if (len == 12) /* UPC barcode - skip leading '0' */
    return *mcn == '0' && strcmp (mcn + 1, barcode) == 0;
  else if (len == 13) /* EAN barcode */
    return strcmp (mcn, barcode) == 0;
  else /* Unknown/invalid barcode */
    return FALSE;
}

static void
filter_releases (Mb5Metadata  disc_md,
                 DiscDetails *disc)
{
  int i, j, count;
  Mb5ReleaseList releases;

  releases = mb5_disc_get_releaselist (mb5_metadata_get_disc (disc_md));
  count = disc->count = mb5_release_list_size (releases);
  if (disc->count == 0)
    return;

  disc->release_ids = g_new0 (Mbid, disc->count);
  for (i = 0, j = 0; i < count; i++) {
    Mb5Release release;
    gchar barcode[16];

    release = mb5_release_list_item (releases, i);
    if (release == NULL) {
      disc->count--;
      continue;
    }

    mb5_release_get_id (release, disc->release_ids[j], sizeof(Mbid));
    mb5_release_get_barcode (release, barcode, sizeof(barcode));
    if (mcn_matches_barcode (disc->mcn, barcode)) {
      disc->count = 1;
      strcpy (disc->release_ids[0], disc->release_ids[j]);
      break;
    }
    j++;
  }
}

static DiscDetails*
get_disc_details (SjMetadataMusicbrainz5  *self,
                  GCancellable            *cancellable,
                  GError                 **error)
{
  Mb5Metadata disc_md;
  DiscDetails *disc;

  disc = g_new0 (DiscDetails, 1);
  disc_md = get_disc_md (self, disc, cancellable, error);

  if (*error != NULL) {
    disc_details_free (disc);
    return NULL;
  }

  if (disc_md == NULL)
    return disc;

  filter_releases (disc_md, disc);

  mb5_metadata_delete (disc_md);

  return disc;
}

static ArtistDetails*
query_artist (SjMetadataMusicbrainz5  *self,
              const gchar             *id,
              GCancellable            *cancellable,
              GError                 **error)
{
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (self);
  ArtistDetails *details = g_hash_table_lookup (priv->artist_cache, id);

  if (details == NULL) {
    Mb5Artist artist;
    Mb5Metadata metadata;

    metadata = query_musicbrainz (self, "artist", id, "aliases", cancellable, error);
    if (metadata == NULL)
      return NULL;
    artist = mb5_metadata_get_artist (metadata);
    if (artist != NULL)
      details = make_artist_details (self, artist);
    mb5_metadata_delete (metadata);
  }
  return details;
}

static GList *
get_artist_list (SjMetadataMusicbrainz5 *self,
                 Mb5ArtistCredit         credit)
{
  Mb5NameCreditList name_list;
  GList *artists;
  unsigned int i;
  char buffer[512]; /* for the GET macro */

  if (credit == NULL)
    return NULL;

  name_list = mb5_artistcredit_get_namecreditlist (credit);
  if (name_list == NULL) {
    return NULL;
  }

  artists = NULL;
  for (i = 0; i < mb5_namecredit_list_size (name_list); i++) {
    Mb5NameCredit name_credit;
    Mb5Artist artist;
    ArtistCredit *artist_credit = g_slice_new0 (ArtistCredit);

    name_credit = mb5_namecredit_list_item (name_list, i);
    artist = mb5_namecredit_get_artist (name_credit);
    if (artist != NULL) {
      artist_credit->details = make_artist_details (self, artist);
    } else {
      g_info ("no Mb5Artist associated with Mb5NameCredit, falling back to Mb5NameCredit::name");
      artist_credit->details = g_new0 (ArtistDetails, 1);
      GET (artist_credit->details->name, mb5_namecredit_get_name, name_credit);
    }
    GET (artist_credit->joinphrase, mb5_namecredit_get_joinphrase, name_credit);
    artists = g_list_prepend (artists, artist_credit);
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
    ArtistCredit *artist_credit = (ArtistCredit *)it->data;
    artist_count++;
    g_string_append (artist_name, artist_credit->details->name);
    if (artist_credit->joinphrase != NULL)
      g_string_append (artist_name, artist_credit->joinphrase);
  }

  if (artist_count != 1) {
      g_info ("multiple artists");
      if (sortname != NULL)
        *sortname = NULL;
      if (id != NULL)
        *id = NULL;
  } else {
      ArtistDetails *details = ((ArtistCredit *)artists->data)->details;
      if (sortname != NULL)
        *sortname = g_strdup (details->sortname);
      if (id != NULL)
        *id = g_strdup (details->id);
  }

  if (name != NULL)
    *name = artist_name->str;

  g_string_free (artist_name, FALSE);
}

static GList*
get_release_labels (Mb5Release *release)
{
  Mb5LabelInfoList list;
  int i;
  char buffer[512]; /* for the GET() macro */
  GList *label_list = NULL;
  GList *it = NULL;

  list = mb5_release_get_labelinfolist (release);
  if (list == NULL)
    return NULL;

  for (i = 0; i < mb5_labelinfo_list_size (list); i++) {
    Mb5LabelInfo info;
    Mb5Label label;
    LabelDetails *label_data;

    info = mb5_labelinfo_list_item (list, i);
    if (info == NULL)
      continue;

    label = mb5_labelinfo_get_label (info);
    if (label == NULL)
      continue;

    mb5_label_get_name (label, buffer, sizeof (buffer));
    for (it = label_list; it != NULL; it = it->next) {
      if (strcmp (((LabelDetails*) it->data)->name, buffer) == 0)
        goto skip;
    }
    label_data = g_new0 (LabelDetails, 1);
    label_data->name = g_strdup (buffer);
    GET (label_data->sortname, mb5_label_get_sortname, label);
    GET (label_data->catalog_number, mb5_labelinfo_get_catalognumber, info);
    label_list = g_list_prepend (label_list, label_data);
  skip:
    ;
  }
  label_list = g_list_reverse (label_list);
  return label_list;
}

static void
fill_album_composer (AlbumDetails *album)
{
  char *old_composer;
  GList *l;
  TrackDetails *track_one; /* The first track on the album */
  gboolean one_composer = TRUE; /* TRUE if all tracks have the same composer */

  if (album->composer != NULL)
    return;

  if (album->tracks == NULL)
    return;

  l = album->tracks;
  track_one = (TrackDetails*)l->data;
  old_composer = track_one->composer;

  for (l = l->next; l != NULL; l = l->next) {
    char *new_composer;
    TrackDetails *track = (TrackDetails*)l->data;
    new_composer = track->composer;
    if (g_strcmp0 (old_composer, new_composer) != 0) {
      one_composer = FALSE;
      break;
    }
  }

  if (one_composer) {
    album->composer = g_strdup (track_one->composer);
    album->composer_sortname = g_strdup (track_one->composer_sortname);
  } else {
    album->composer = g_strdup ("Various Composers");
    album->composer_sortname = g_strdup ("Various Composers");
  }
}

typedef void (*RelationForeachFunc)(SjMetadataMusicbrainz5 *self, Mb5Relation relation, gpointer user_data);

static void relationlist_list_foreach_relation(SjMetadataMusicbrainz5 *self,
                                               Mb5RelationListList     relation_lists,
                                               const char             *targettype,
                                               const char             *relation_type,
                                               RelationForeachFunc     callback,
                                               gpointer                user_data)
{
  unsigned int j;

  if (relation_lists == NULL)
    return;

  for (j = 0; j < mb5_relationlist_list_size (relation_lists); j++) {
    Mb5RelationList relations;
    char type[512]; /* To hold relationlist target-type and relation type */
    unsigned int i;

    relations = mb5_relationlist_list_item (relation_lists, j);
    if (relations == NULL)
      return;

    mb5_relation_list_get_targettype (relations, type, sizeof (type));
    if (!g_str_equal (type, targettype))
      continue;

    for (i = 0; i < mb5_relation_list_size (relations); i++) {
      Mb5Relation relation;

      relation = mb5_relation_list_item (relations, i);
      if (relation == NULL)
        continue;

      mb5_relation_get_type (relation, type, sizeof (type));
      if (!g_str_equal (type, relation_type))
        continue;

      callback(self, relation, user_data);
    }
  }
}

static char*
get_sortname (GSList *artists)
{
  if (artists == NULL || artists->data == NULL || artists->next != NULL)
    return NULL;

  return g_strdup (((ArtistDetails*) artists->data)->sortname);
}

static char*
concatenate_composers (GSList *composers)
{
  GString *text;
  GSList *it;

  if (composers == NULL)
    return NULL;

  text = g_string_new (((ArtistDetails*) composers->data)->name);
  for (it = composers->next; it != NULL; it = it->next) {
    g_string_append_printf (text, ", %s", ((ArtistDetails*) it->data)->name);
  }

  return g_string_free (text, FALSE);
}

static void
build_composer_text (GSList  *composers,
                     GSList  *arrangers,
                     GSList  *orchestrators,
                     GSList  *transcribers,
                     char   **name,
                     char   **sortname)
{
  gsize i = 0;
  gsize j = 0;
  guint flags = 0; /* Which components do we have? */
  GSList *sort = NULL;
  gchar  *comp = NULL;
  gchar  *arr  = NULL;
  gchar  *arr_v[4];

  enum {
    HAVE_COMPOSERS     = 1 << 0,
    HAVE_ARRANGERS     = 1 << 1,
    HAVE_ORCHESTRATORS = 1 << 2,
    HAVE_TRANSCRIBERS  = 1 << 3
  };

  g_free (*sortname);
  g_free (*name);
  *sortname = NULL;
  *name = NULL;

  if (composers != NULL) {
    flags |= HAVE_COMPOSERS;
    comp = concatenate_composers (composers);
    sort = composers;
  }
  if (arrangers != NULL) {
    flags |= HAVE_ARRANGERS;
    arr_v[i++] = concatenate_composers (arrangers);
    sort = arrangers;
  }
  if (orchestrators != NULL) {
    flags |= HAVE_ORCHESTRATORS;
    arr_v[i++] = concatenate_composers (orchestrators);
    sort = orchestrators;
  }
  if (transcribers != NULL) {
    flags |= HAVE_TRANSCRIBERS;
    arr_v[i++] = concatenate_composers (transcribers);
    sort = transcribers;
  }

  arr_v[i] = NULL;
  if (i > 0)
    arr = g_strjoinv (", ", arr_v);

  if (flags & HAVE_COMPOSERS) {
    if (flags & HAVE_ARRANGERS) {
      /* Translators: This string is used to build the composer tag
         when a track has composers and arrangers, or a composer and a
         mixture of arrangers, orchestrators and transcribers */
      *name = g_strdup_printf (_("%s arr. %s"), comp, arr);
    } else if (flags & HAVE_ORCHESTRATORS) {
      /* Translators: This string is used to build the composer tag
         when a track has composers and orchestrators */
      *name = g_strdup_printf (_("%s orch. %s"), comp, arr);
    } else if (flags & HAVE_TRANSCRIBERS) {
      /* Translators: This string is used to build the composer tag
         when a track has composers and transcribers */
      *name = g_strdup_printf (_("%s trans. %s"), comp, arr);
    } else {
      /* Only composers */
      *name = g_strdup (comp);
    }
  } else {
    if (flags & HAVE_ARRANGERS) {
      /* Translators: This string is used to build the composer tag
         when a track has a mixture of arrangers, orchestrators and
         transcribers but no composers */
      *name = g_strdup_printf (_("arr. %s"), arr);
    } else if (flags & HAVE_ORCHESTRATORS) {
      /* Translators: This string is used to build the composer tag
         when a track has orchestrators but no composer */
      *name = g_strdup_printf (_("orch. %s"), arr);
    } else if (flags & HAVE_TRANSCRIBERS) {
      /* Translators: This string is used to build the composer tag
         when a track has a transcribers but no composer */
      *name = g_strdup_printf (_("trans. %s"), arr);
    }
  }

  if (flags)
    *sortname = get_sortname (sort);

  while (j < i)
    g_free (arr_v[j++]);
  g_free (arr);
  g_free (comp);
}

typedef struct {
  GSList *composers;
  GCancellable *cancellable;
  GError **error;
} ComposerCbContext;

static void composer_cb (SjMetadataMusicbrainz5 *self,
                         Mb5Relation             relation,
                         gpointer                user_data)
{
  Mb5Artist composer;
  ComposerCbContext *context = user_data;
  char buffer[512]; /* For the GET macro */
  ArtistDetails *details;

  composer = mb5_relation_get_artist (relation);
  if (composer == NULL)
    return;
  /* NB work-level-rels only returns artist name, sortname & id so
     we need to perform another query if we want the alias list
     therefore use query_artist rather than make_artist_details. */
  mb5_artist_get_id (composer, buffer, sizeof (buffer));
  details = query_artist (self, buffer, context->cancellable, context->error);
  if (details != NULL)
    context->composers = g_slist_append (context->composers, details);
}

typedef struct {
  TrackDetails *track;
  GCancellable *cancellable;
  GError       **error;
} WorkCbContext;

static void work_cb (SjMetadataMusicbrainz5 *self,
                     Mb5Relation             relation,
                     gpointer                user_data)
{
  WorkCbContext *context = user_data;
  ComposerCbContext composer_context;
  GSList *composers = NULL, *arrangers = NULL;
  GSList *orchestrators = NULL, *transcribers = NULL;
  Mb5RelationListList relation_lists;
  Mb5Work work;

  work = mb5_relation_get_work (relation);
  if (work == NULL)
    goto cleanup;

  relation_lists = mb5_work_get_relationlistlist (work);
  composer_context.cancellable = context->cancellable;
  composer_context.error = context->error;
  composer_context.composers = NULL;
  relationlist_list_foreach_relation (self, relation_lists,
                                      "artist", "composer",
                                      composer_cb, &composer_context);
  composers = composer_context.composers;
  if (*context->error != NULL)
    goto cleanup;
  composer_context.composers = NULL;
  relationlist_list_foreach_relation (self, relation_lists,
                                      "artist", "arranger",
                                      composer_cb, &composer_context);
  arrangers = composer_context.composers;
  if (*context->error != NULL)
    goto cleanup;
  composer_context.composers = NULL;
  relationlist_list_foreach_relation (self, relation_lists,
                                      "artist", "orchestrator",
                                      composer_cb, &composer_context);
  orchestrators = composer_context.composers;
  if (*context->error != NULL)
    goto cleanup;
  composer_context.composers = NULL;
  relationlist_list_foreach_relation (self, relation_lists,
                                      "artist", "instrument arranger",
                                      composer_cb, &composer_context);
  transcribers = composer_context.composers;
  if (*context->error != NULL)
    goto cleanup;

  build_composer_text (composers, arrangers, orchestrators, transcribers,
                       &context->track->composer, &context->track->composer_sortname);

 cleanup:
  g_slist_free (arrangers);
  g_slist_free (composers);
  g_slist_free (orchestrators);
  g_slist_free (transcribers);
}

static void
fill_recording_relations (SjMetadataMusicbrainz5  *self,
                          Mb5Recording             recording,
                          TrackDetails            *track,
                          GCancellable            *cancellable,
                          GError                 **error)
{
  Mb5RelationListList relation_lists;
  WorkCbContext context;

  context.track = track;
  context.cancellable = cancellable;
  context.error = error;
  relation_lists = mb5_recording_get_relationlistlist (recording);
  relationlist_list_foreach_relation(self, relation_lists,
                                     "work", "performance",
                                     work_cb, &context);
}

static void
parse_artist_credit (SjMetadataMusicbrainz5  *self,
                     Mb5ArtistCredit          credit,
                     char                   **name,
                     char                   **sortname,
                     char                   **id)
{
  if (credit) {
    GList *artists;
    artists = get_artist_list (self, credit);
    if (artists) {
      get_artist_info (artists, name, sortname, id);
      g_list_free_full (artists, artist_credit_destroy);
    }
  }
}

static void
fill_tracks_from_medium (SjMetadataMusicbrainz5  *self,
                         Mb5Medium                medium,
                         AlbumDetails            *album,
                         GCancellable            *cancellable,
                         GError                 **error)
{
  Mb5TrackList track_list;
  GList *tracks;
  unsigned int i;
  gboolean skip_data_tracks;
  char buffer[512]; /* for the GET() macro */

  track_list = mb5_medium_get_tracklist (medium);
  if (!track_list)
    return;

  album->number = mb5_track_list_size (track_list);

  tracks = NULL;
  skip_data_tracks = TRUE;

  for (i = 0; i < mb5_track_list_size (track_list); i++) {
    Mb5Track mbt;
    Mb5ArtistCredit credit;
    Mb5Recording recording;
    TrackDetails *track;
    gchar *title;
    gint track_offset;

    mbt = mb5_track_list_item (track_list, i);
    if (!mbt)
      continue;

    recording = mb5_track_get_recording (mbt);
    /*
     * title points to a string on the stack so set it to NULL to
     * prevent the GET macro from trying to free it
     */
    title = NULL;
    GET (title, mb5_track_get_title, mbt);
    if (title == NULL && recording != NULL)
      GET (title, mb5_recording_get_title, recording);

    /*
     * Skip data tracks at the start of the disc
     * https://musicbrainz.org/doc/Style/Unknown_and_untitled/Special_purpose_track_title
     */
    if (skip_data_tracks) {
      if (g_strcmp0 (title, "[data track]") == 0) {
        continue;
      } else {
        skip_data_tracks = FALSE;
        track_offset = mb5_track_get_position (mbt) - 1;
      }
    }

    track = g_new0 (TrackDetails, 1);
    track->title = g_strdup (title);
    track->album = album;

    track->number = mb5_track_get_position (mbt) - track_offset;

    /* Prefer metadata from Mb5Track over metadata from Mb5Recording
     * https://bugzilla.gnome.org/show_bug.cgi?id=690903#c8
     */
    track->duration = mb5_track_get_length (mbt) / 1000;
    credit = mb5_track_get_artistcredit (mbt);

    if (recording != NULL) {
      GET (track->track_id, mb5_recording_get_id, recording);
      fill_recording_relations (self, recording, track, cancellable, error);
      if (*error != NULL) {
        track_details_free (track);
        g_list_free_full (tracks, (GDestroyNotify) track_details_free);
      }

      if (track->duration == 0)
        track->duration = mb5_recording_get_length (recording) / 1000;
      if (credit == NULL)
          credit = mb5_recording_get_artistcredit (recording);
    }

      parse_artist_credit(self, credit,
                          &track->artist,
                          &track->artist_sortname,
                          &track->artist_id);
    if (track->artist == NULL)
      track->artist = g_strdup (album->artist);
    if (track->artist_sortname == NULL)
      track->artist_sortname = g_strdup (album->artist_sortname);
    if (track->artist_id == NULL)
      track->artist_id = g_strdup (album->artist_id);

    tracks = g_list_prepend (tracks, track);
  }
  album->tracks = g_list_reverse (tracks);
}

static void wikipedia_cb (SjMetadataMusicbrainz5 *self,
                          Mb5Relation             relation,
                          gpointer                user_data)
{
  AlbumDetails *album = (AlbumDetails *)user_data;
  char buffer[512]; /* for the GET() macro */
  char *wikipedia = NULL;

  GET (wikipedia, mb5_relation_get_target, relation);
  if (wikipedia != NULL) {
    g_free (album->wikipedia);
    album->wikipedia = wikipedia;
  }
}

static void discogs_cb (SjMetadataMusicbrainz5 *self,
                        Mb5Relation             relation,
                        gpointer                user_data)
{
  AlbumDetails *album = (AlbumDetails *)user_data;
  char buffer[512]; /* for the GET() macro */
  char *discogs = NULL;

  GET (discogs, mb5_relation_get_target, relation);
  if (discogs != NULL) {
    g_free (album->discogs);
    album->discogs = discogs;
  }
}

static void lyrics_cb (SjMetadataMusicbrainz5 *self,
                       Mb5Relation             relation,
                       gpointer                user_data)
{
  AlbumDetails *album = (AlbumDetails *)user_data;
  char buffer[512]; /* for the GET() macro */
  char *lyrics = NULL;

  GET (lyrics, mb5_relation_get_target, relation);
  if (lyrics != NULL) {
    g_free (album->lyrics_url);
    album->lyrics_url = lyrics;
  }
}

static void
fill_relations (SjMetadataMusicbrainz5 *self,
                Mb5RelationListList     relation_lists,
                AlbumDetails           *album)
{
  relationlist_list_foreach_relation (self, relation_lists,
                                      "url", "wikipedia",
                                      wikipedia_cb, album);
  relationlist_list_foreach_relation (self, relation_lists,
                                      "url", "discogs",
                                      discogs_cb, album);
  relationlist_list_foreach_relation (self, relation_lists,
                                      "url", "lyrics",
                                      lyrics_cb, album);
}

static AlbumDetails *
make_album_from_release (SjMetadataMusicbrainz5  *self,
                         Mb5ReleaseGroup          group,
                         Mb5Release               release,
                         Mb5Medium                medium,
                         GCancellable            *cancellable,
                         GError                 **error)
{
  AlbumDetails *album;
  Mb5ArtistCredit credit;
  Mb5RelationListList relationlists;
  Mb5MediumList media;
  char *date = NULL;
  char buffer[512]; /* for the GET macro */

  g_assert (release);
  g_return_val_if_fail (medium != NULL, NULL);

  album = g_new0 (AlbumDetails, 1);

  media = mb5_release_get_mediumlist (release);
  if (media)
    album->disc_count = mb5_medium_list_size (media);

  GET (album->album_id, mb5_release_get_id, release);
  GET (album->title, mb5_medium_get_title, medium);
  if (album->title == NULL)
    GET (album->title, mb5_release_get_title, release);

  credit = mb5_release_get_artistcredit (release);

  parse_artist_credit (self, credit,
                       &album->artist,
                       &album->artist_sortname,
                       &album->artist_id);

  GET (date, mb5_release_get_date, release);
  if (date) {
    album->release_date = gst_date_time_new_from_iso8601_string (date);
    g_free (date);
  }

  GET (album->asin, mb5_release_get_asin, release);
  mb5_release_get_country (release, buffer, sizeof(buffer));
  album->country = sj_metadata_helper_lookup_country_code (buffer);
  if (group) {
    GET (album->type, mb5_releasegroup_get_primarytype, group);
    if (album->type != NULL && (g_str_has_suffix (album->type, "Spokenword")
                                || g_str_has_suffix (album->type, "Interview")
                                || g_str_has_suffix (album->type, "Audiobook"))) {
      album->is_spoken_word = TRUE;
    }
    relationlists = mb5_releasegroup_get_relationlistlist (group);
    fill_relations (self, relationlists, album);
  }

  album->disc_number = mb5_medium_get_position (medium);

  GET(album->barcode, mb5_release_get_barcode, release);
  fill_tracks_from_medium (self, medium, album, cancellable, error);
  if (*error != NULL)
    return NULL;

  fill_album_composer (album);
  relationlists = mb5_release_get_relationlistlist (release);
  fill_relations (self, relationlists, album);
  album->labels = get_release_labels (release);

  sj_mb5_album_details_dump (album);
  return album;
}

/*
 * Virtual methods
 */
static GList *
mb5_list_albums (SjMetadata    *metadata,
                 char         **url,
                 GCancellable  *cancellable,
                 GError       **error)
{
  SjMetadataMusicbrainz5 *self;
  GList *albums = NULL;
  DiscDetails *disc;
  char buffer[1024];
  int i;

  g_return_val_if_fail (SJ_IS_METADATA_MUSICBRAINZ5 (metadata), NULL);

  self = SJ_METADATA_MUSICBRAINZ5 (metadata);

  disc = get_disc_details (self, cancellable, error);
  /* if *error != NULL then disc == NULL */
  if (disc == NULL)
    return NULL;

  for (i = 0; i < disc->count; i++) {
    AlbumDetails *album;
    Mb5Release full_release = NULL;
    Mb5Metadata release_md = NULL;
    char *includes = "aliases artists artist-credits labels recordings \
release-groups url-rels discids recording-level-rels work-level-rels work-rels \
artist-rels";

    /*
     * In order to get metadata for artist aliases & work / composer
     * relationships we need to perform a custom query
     */
    release_md = query_musicbrainz (self, "release", disc->release_ids[i], includes, cancellable, error);
    if (*error != NULL)
      goto free_releases;

    if (release_md && mb5_metadata_get_release (release_md))
      full_release = mb5_metadata_get_release (release_md);
    if (full_release) {
      Mb5MediumList media;
      Mb5Metadata metadata = NULL;
      Mb5ReleaseGroup group;
      int j;

      group = mb5_release_get_releasegroup (full_release);
      if (group) {
        /*
         * The release-group information we can extract from the
         * lookup_release query doesn't have the url relations for the
         * release-group, so run a separate query to get these urls
         */
        char *releasegroupid = NULL;
        char *includes = "artists url-rels";

        GET (releasegroupid, mb5_releasegroup_get_id, group);
        metadata = query_musicbrainz (self, "release-group", releasegroupid, includes, cancellable, error);
        g_free (releasegroupid);
        if (*error != NULL) {
          mb5_metadata_delete (release_md);
          goto free_releases;
        }

        if (metadata && mb5_metadata_get_releasegroup (metadata))
          group = mb5_metadata_get_releasegroup (metadata);

        media = mb5_release_media_matching_discid (full_release, disc->id);
        for (j = 0; j < mb5_medium_list_size (media); j++) {
          Mb5Medium medium;

          medium = mb5_medium_list_item (media, j);
          if (medium) {
            album = make_album_from_release (self, group, full_release, medium, cancellable, error);
            if (*error != NULL) {
              mb5_metadata_delete (metadata);
              mb5_medium_list_delete (media);
              mb5_metadata_delete (release_md);
              goto free_releases;
            }

            album->metadata_source = SOURCE_MUSICBRAINZ;
            albums = g_list_append (albums, album);
          }
        }
        mb5_metadata_delete (metadata);
        mb5_medium_list_delete (media);
      }
      mb5_metadata_delete (release_md);
    }
  }

  if (url != NULL)
    *url = g_strdup (disc->url);

  disc_details_free (disc);
  return albums;


 free_releases:
  g_list_free_full (albums, (GDestroyNotify) album_details_free);
  disc_details_free (disc);
  return NULL;
}

static void
setup_http_proxy (SjMetadataMusicbrainz5Private *priv)
{
  if (priv->proxy_mode == G_DESKTOP_PROXY_MODE_NONE ||
      priv->proxy_mode == G_DESKTOP_PROXY_MODE_AUTO ||
      priv->proxy_host == NULL || priv->proxy_port == 0) {
    /* NB passing a NULL string to mb5_query_set_proxyhost,
       mb5_query_set_proxyusername or mb5_query_set_proxypassword
       causes a crash on FreeBSD
       https://bugzilla.gnome.org/show_bug.cgi?id=742019 */
    mb5_query_set_proxyhost (priv->mb, "");
    mb5_query_set_proxyport (priv->mb, 0);
    mb5_query_set_proxyusername (priv->mb, "");
    mb5_query_set_proxypassword (priv->mb, "");
    if (priv->proxy_mode == G_DESKTOP_PROXY_MODE_AUTO)
      g_warning ("Automatic proxy mode not supported yet, disabling proxy usage");
  } else if (priv->proxy_mode == G_DESKTOP_PROXY_MODE_MANUAL) {
    mb5_query_set_proxyhost (priv->mb, priv->proxy_host);
    mb5_query_set_proxyport (priv->mb, priv->proxy_port);
    if (priv->proxy_use_authentication &&
        priv->proxy_username != NULL && priv->proxy_password != NULL) {
      mb5_query_set_proxyusername (priv->mb, priv->proxy_username);
      mb5_query_set_proxypassword (priv->mb, priv->proxy_password);
    } else {
      mb5_query_set_proxyusername (priv->mb, NULL);
      mb5_query_set_proxypassword (priv->mb, NULL);
    }
  }
}

/*
 * GObject methods
 */

static void
metadata_interface_init (gpointer g_iface, gpointer iface_data)
{
  SjMetadataClass *klass = (SjMetadataClass*)g_iface;

  klass->list_albums = mb5_list_albums;
}

static void
sj_metadata_musicbrainz5_init (SjMetadataMusicbrainz5 *self)
{
  SjMetadataMusicbrainz5Private *priv;

  priv = GET_PRIVATE (self);
  priv->mb = mb5_query_new (SJ_MUSICBRAINZ_USER_AGENT, NULL, 0);

  priv->artist_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, artist_details_destroy);
}

static void
sj_metadata_musicbrainz5_get_property (GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec)
{
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (object);
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    g_value_set_string (value, priv->cdrom);
    break;
  case PROP_PROXY_USE_AUTHENTICATION:
    g_value_set_boolean (value, priv->proxy_use_authentication);
    break;
  case PROP_PROXY_HOST:
    g_value_set_string (value, priv->proxy_host);
    break;
  case PROP_PROXY_PORT:
    g_value_set_int (value, priv->proxy_port);
    break;
  case PROP_PROXY_USERNAME:
    g_value_set_string (value, priv->proxy_username);
    break;
  case PROP_PROXY_PASSWORD:
    g_value_set_string (value, priv->proxy_password);
    break;
  case PROP_PROXY_MODE:
    g_value_set_enum (value, priv->proxy_mode);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_musicbrainz5_set_property (GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec)
{
  SjMetadataMusicbrainz5Private *priv = GET_PRIVATE (object);
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    if (priv->cdrom)
      g_free (priv->cdrom);
    priv->cdrom = g_value_dup_string (value);
    break;
  case PROP_PROXY_USE_AUTHENTICATION:
    priv->proxy_use_authentication = g_value_get_boolean (value);
    setup_http_proxy (priv);
    break;
  case PROP_PROXY_HOST:
    g_free (priv->proxy_host);
    priv->proxy_host = g_value_dup_string (value);
    setup_http_proxy (priv);
    break;
  case PROP_PROXY_PORT:
    priv->proxy_port = g_value_get_int (value);
    setup_http_proxy (priv);
    break;
  case PROP_PROXY_USERNAME:
    g_free (priv->proxy_username);
    priv->proxy_username = g_value_dup_string (value);
    setup_http_proxy (priv);
    break;
  case PROP_PROXY_PASSWORD:
    g_free (priv->proxy_password);
    priv->proxy_password = g_value_dup_string (value);
    setup_http_proxy (priv);
    break;
  case PROP_PROXY_MODE:
    priv->proxy_mode = g_value_get_enum (value);
    setup_http_proxy (priv);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_musicbrainz5_finalize (GObject *object)
{
  SjMetadataMusicbrainz5Private *priv;

  priv = GET_PRIVATE (object);

  if (priv->mb != NULL) {
    mb5_query_delete (priv->mb);
    priv->mb = NULL;
  }
  g_free (priv->cdrom);
  g_free (priv->proxy_host);
  g_free (priv->proxy_username);
  g_free (priv->proxy_password);
  g_hash_table_unref (priv->artist_cache);

  G_OBJECT_CLASS (sj_metadata_musicbrainz5_parent_class)->finalize (object);
}

static void
sj_metadata_musicbrainz5_class_init (SjMetadataMusicbrainz5Class *class)
{
  const gchar * const * l;
  GObjectClass *object_class = (GObjectClass*)class;

  g_type_class_add_private (class, sizeof (SjMetadataMusicbrainz5Private));

  object_class->get_property = sj_metadata_musicbrainz5_get_property;
  object_class->set_property = sj_metadata_musicbrainz5_set_property;
  object_class->finalize = sj_metadata_musicbrainz5_finalize;

  g_object_class_override_property (object_class,
                                    PROP_DEVICE, "device");
  g_object_class_override_property (object_class,
                                    PROP_PROXY_USE_AUTHENTICATION, "proxy-use-authentication");
  g_object_class_override_property (object_class,
                                    PROP_PROXY_HOST, "proxy-host");
  g_object_class_override_property (object_class,
                                    PROP_PROXY_PORT, "proxy-port");
  g_object_class_override_property (object_class,
                                    PROP_PROXY_USERNAME, "proxy-username");
  g_object_class_override_property (object_class,
                                    PROP_PROXY_PASSWORD, "proxy-password");
  g_object_class_override_property (object_class,
                                    PROP_PROXY_MODE, "proxy-mode");

  /* Although GLib supports fallback locales we do not use them as
     MusicBrainz does not provide locale information for the canonical
     artist name, only it's aliases. This means that it is not
     possible to fallback to other locales as the canonical name may
     actually be in a higher priority locale than the alias that
     matches one of the fallback locales. */
  l = g_get_language_names ();
  if (strcmp (l[0], "C") == 0) { /* "C" locale should be "en" for MusicBrainz */
    memcpy (language, "en", 3);
  } else if (strlen (l[0]) > 1) {
    memcpy (language, l[0], 2);
    language[2] = '\0';
  }
}

/*
 * Public methods.
 */

GObject *
sj_metadata_musicbrainz5_new (void)
{
  return g_object_new (SJ_TYPE_METADATA_MUSICBRAINZ5, NULL);
}
