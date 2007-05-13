/*
 * sj-metadata-musicbrainz.c
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
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
#include <stdio.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <glib/gerror.h>
#include <glib/glist.h>
#include <glib/gstrfuncs.h>
#include <glib/gmessages.h>
#include <gconf/gconf-client.h>
#include <musicbrainz/queries.h>
#include <musicbrainz/mb_c.h>
#include <nautilus-burn.h>
#include <stdlib.h>
#include <unistd.h>

#include "sj-metadata-musicbrainz.h"
#include "sj-structures.h"
#include "sj-error.h"

#define GCONF_MUSICBRAINZ_SERVER "/apps/sound-juicer/musicbrainz_server"
#define GCONF_PROXY_USE_PROXY "/system/http_proxy/use_http_proxy"
#define GCONF_PROXY_HOST "/system/http_proxy/host"
#define GCONF_PROXY_PORT "/system/http_proxy/port"
#define GCONF_PROXY_USE_AUTHENTICATION "/system/http_proxy/use_authentication"
#define GCONF_PROXY_USERNAME "/system/http_proxy/authentication_user"
#define GCONF_PROXY_PASSWORD "/system/http_proxy/authentication_password"


struct SjMetadataMusicbrainzPrivate {
  GError *construct_error;
  musicbrainz_t mb;
  char *http_proxy;
  int http_proxy_port;
  char *cdrom;
  /* TODO: remove and use an async queue? */
  GList *albums;
  GError *error;
};

#define GET_PRIVATE(o)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SJ_TYPE_METADATA_MUSICBRAINZ, SjMetadataMusicbrainzPrivate))

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_USE_PROXY,
  PROP_PROXY_HOST,
  PROP_PROXY_PORT,
};

static void metadata_interface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (SjMetadataMusicbrainz,
                        sj_metadata_musicbrainz,
                        G_TYPE_OBJECT, 0, 
                        G_IMPLEMENT_INTERFACE (SJ_TYPE_METADATA,
                                               metadata_interface_init));


/*
 * Private methods
 */

static int
get_duration_from_sectors (int sectors)
{
#define BYTES_PER_SECTOR 2352
#define BYTES_PER_SECOND (44100 / 8) / 16 / 2
  
  return (sectors * BYTES_PER_SECTOR / BYTES_PER_SECOND);
}

static GList*
get_offline_track_listing(SjMetadata *metadata, GError **error)
{
  SjMetadataMusicbrainzPrivate *priv;
  GList* list = NULL;
  AlbumDetails *album;
  TrackDetails *track;
  int num_tracks, i;

  g_return_val_if_fail (metadata != NULL, NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

  if (!mb_Query (priv->mb, MBQ_GetCDTOC)) {
    char message[255];
    mb_GetQueryError (priv->mb, message, 255);
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_CD_LOOKUP_ERROR,
                 _("Cannot read CD: %s"), message);
    return NULL;
  }
  
  num_tracks = mb_GetResultInt (priv->mb, MBE_TOCGetLastTrack);

  album = g_new0 (AlbumDetails, 1);
  album->artist = g_strdup (_("Unknown Artist"));
  album->title = g_strdup (_("Unknown Title"));
  album->genre = NULL;
  for (i = 1; i <= num_tracks; i++) {
    track = g_new0 (TrackDetails, 1);
    track->album = album;
    track->number = i;
    track->title = g_strdup_printf (_("Track %d"), i);
    track->artist = g_strdup (album->artist);
    track->duration = get_duration_from_sectors (mb_GetResultInt1 (priv->mb, MBE_TOCGetTrackNumSectors, i+1));
    album->tracks = g_list_append (album->tracks, track);
    album->number++;
  }
  return g_list_append (list, album);
}

static gboolean
fire_signal_idle (SjMetadataMusicbrainz *m)
{
  g_return_val_if_fail (SJ_IS_METADATA_MUSICBRAINZ (m), FALSE);
  g_signal_emit_by_name (G_OBJECT (m), "metadata", m->priv->albums, m->priv->error);
  return FALSE;
}

/**
 * Virtual methods
 */

static GError*
mb_get_new_error (SjMetadata *metadata)
{
  GError *error = NULL;
  if (metadata == NULL || SJ_METADATA_MUSICBRAINZ (metadata)->priv == NULL) {
    g_set_error (&error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("MusicBrainz metadata object is not valid. This is bad, check your console for errors."));
    return error;
  }
  return SJ_METADATA_MUSICBRAINZ (metadata)->priv->construct_error;
}

/* Data imported from FreeDB is horrendeous for compilations,
 * Try to split the 'Various' artist */
static void
artist_and_title_from_title (TrackDetails *track, gpointer data)
{
  char *slash, **split;

  if (g_ascii_strncasecmp (track->album->album_id, "freedb:", 7) != 0 && track->album->artist_id[0] != '\0' && track->artist_id[0] != '\0') {
    track->title = g_strdup (data);
    return;
  }

  slash = strstr (data, " / ");
  if (slash == NULL) {
    track->title = g_strdup (data);
    return;
  }
  split = g_strsplit (data, " / ", 2);
  track->artist = g_strdup (split[0]);
  track->title = g_strdup (split[1]);
  g_strfreev (split);
}

#if WITH_CACHE
/**
 * Write the RDF in the MusicBrainz object to the file specified.
 */
static void
cache_rdf (musicbrainz_t mb, const char *filename)
{
  GError *error = NULL;
  int len;
  char *path, *rdf;

  g_assert (mb != NULL);
  g_assert (filename != NULL);

  /* Create the folder for the file */
  path = g_path_get_dirname (filename);
  g_mkdir_with_parents (path, 0755); /* Handle errors in set_contents() */
  g_free (path);
  
  /* How much data is there to save? */
  len = mb_GetResultRDFLen (mb);
  rdf = g_malloc0 (len);

  /* Get the RDF and save it */
  mb_GetResultRDF (mb, rdf, len);
  if (!g_file_set_contents (filename, rdf, len, &error)) {
    g_warning ("Cannot write cache file %s: %s", filename, error->message);
    g_error_free (error);
  }

  g_free (rdf);
}

/**
 * Load into the MusicBrainz object the RDF from the specified cache file if it
 * exists and is valid then return TRUE, otherwise return FALSE.
 */
static gboolean
get_cached_rdf (musicbrainz_t mb, const char *cachepath)
{
  gboolean ret = FALSE;
  GError *error = NULL;
  char *rdf = NULL;

  g_assert (mb != NULL);
  g_assert (cachepath != NULL);

  if (!g_file_test (cachepath, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
    goto done;
  
  /* Cache file exists, open it */
  if (!g_file_get_contents (cachepath, &rdf, NULL, &error)) {
    g_warning ("Cannot open cache file %s: %s", cachepath, error->message);
    g_error_free (error);
    goto done;
  }
  
  /* Set the RDF */
  if (mb_SetResultRDF (mb, rdf))
    ret = TRUE;
  
  done:
    g_free (rdf);
    return ret;
}
#else
static gboolean
get_cached_rdf (musicbrainz_t mb, const char *cachepath)
{
  return FALSE;
}
static void
cache_rdf (musicbrainz_t mb, const char *filename) {
}
#endif

/**
 * Fill the MusicBrainz object with RDF.  Basically get the CD Index and check
 * the local cache, if that fails then lookup the data online.
 */
static void
get_rdf (SjMetadata *metadata)
{
  SjMetadataMusicbrainzPrivate *priv;
  char data[256];
  char *cdindex = NULL, *cachepath = NULL;

  g_assert (metadata != NULL);

  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

#if WITH_CACHE
  /* Get the Table of Contents */
  if (!mb_Query (priv->mb, MBQ_GetCDTOC)) {
    mb_GetQueryError (priv->mb, data, sizeof (data));
    g_print (_("This CD could not be queried: %s\n"), data);
    return;
  }

  /* Extract the CD Index */
  if (!mb_GetResultData(priv->mb, MBE_TOCGetCDIndexId, data, sizeof (data))) {
    mb_GetQueryError (priv->mb, data, sizeof (data));
    g_print (_("This CD could not be queried: %s\n"), data);
    return;
  }
  cdindex = g_strdup (data);

  cachepath = g_build_filename (g_get_home_dir (), ".gnome2", "sound-juicer", "cache", cdindex, NULL);
#endif
  
  if (!get_cached_rdf (priv->mb, cachepath)) {
    /* Don't re-use the CD Index as that doesn't send enough data to the server.
       By doing this we also pass track lengths, which can be proxied to FreeDB
       if required. */
    if (!mb_Query (priv->mb, MBQ_GetCDInfo)) {
      mb_GetQueryError (priv->mb, data, sizeof (data));
      g_print (_("This CD could not be queried: %s\n"), data);
      goto done;
    }
    cache_rdf (priv->mb, cachepath);
  }
  
 done:
  g_free (cachepath);
  g_free (cdindex);
}

/*
 * Magic character set encoding to try and repair brain-dead FreeDB encoding,
 * converting it to the current locale's encoding (which is likely to be the
 * intended encoding).
 */
static void
convert_encoding(char **str)
{
  char *iso8859;
  char *converted;

  if (str == NULL || *str == NULL)
    return;

  iso8859 = g_convert (*str, -1, "ISO-8859-1", "UTF-8", NULL, NULL, NULL);

  if (iso8859) {
    converted = g_locale_to_utf8 (iso8859, -1, NULL, NULL, NULL);

    if (converted) {
      g_free (*str);
      *str = converted;
    }
  }

  g_free (iso8859);
}

static gpointer
lookup_cd (SjMetadata *metadata)
{
  /** The size of the buffer used in MusicBrainz lookups */
  SjMetadataMusicbrainzPrivate *priv;
  GList *albums = NULL;
  GList *al, *tl;
  char data[256];
  int num_albums, i, j;
  NautilusBurnMediaType type;
  NautilusBurnDriveMonitor *monitor;
  NautilusBurnDrive *drive;

  /* TODO: fire error signal */
  g_return_val_if_fail (metadata != NULL, NULL);
  g_return_val_if_fail (SJ_IS_METADATA_MUSICBRAINZ (metadata), NULL);
  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;
  g_return_val_if_fail (priv->cdrom != NULL, NULL);
  priv->error = NULL; /* TODO: hack */

  if (! nautilus_burn_initialized ()) {
    nautilus_burn_init ();
  }
  monitor = nautilus_burn_get_drive_monitor ();
  drive = nautilus_burn_drive_monitor_get_drive_for_device (monitor, priv->cdrom);
  if (drive == NULL) {
    return NULL;
  }
  type = nautilus_burn_drive_get_media_type (drive);
  nautilus_burn_drive_unref (drive);

  if (type == NAUTILUS_BURN_MEDIA_TYPE_ERROR) {
    char *msg;
    SjError err;

    if (access (priv->cdrom, W_OK) == 0) {
      msg = g_strdup_printf (_("Device '%s' does not contain any media"), priv->cdrom);
      err = SJ_ERROR_CD_NO_MEDIA;
    } else {
      msg = g_strdup_printf (_("Device '%s' could not be opened. Check the access permissions on the device."), priv->cdrom);
      err = SJ_ERROR_CD_PERMISSION_ERROR;
    }
    priv->error = g_error_new (SJ_ERROR, err, _("Cannot read CD: %s"), msg);
    g_free (msg);

    priv->albums = NULL;
    g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
    return NULL;
  }

  get_rdf (metadata);

  num_albums = mb_GetResultInt(priv->mb, MBE_GetNumAlbums);
  if (num_albums < 1) {
    priv->albums = get_offline_track_listing (metadata, &(priv->error));
    g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
    return priv->albums;
  }

  for (i = 1; i <= num_albums; i++) {
    int num_tracks;
    AlbumDetails *album;
    gboolean from_freedb = FALSE;

    mb_Select1(priv->mb, MBS_SelectAlbum, i);
    album = g_new0 (AlbumDetails, 1);

    if (mb_GetResultData(priv->mb, MBE_AlbumGetAlbumId, data, sizeof (data))) {
      from_freedb = strstr(data, "freedb:") == data;

      mb_GetIDFromURL (priv->mb, data, data, sizeof (data));
      album->album_id = g_strdup (data);
    }

    if (mb_GetResultData (priv->mb, MBE_AlbumGetAlbumArtistId, data, sizeof (data))) {
      mb_GetIDFromURL (priv->mb, data, data, sizeof (data));
      album->artist_id = g_strdup (data);
      if (g_ascii_strncasecmp (MBI_VARIOUS_ARTIST_ID, data, 64) == 0) {
        album->artist = g_strdup (_("Various"));
      } else {
        if (data && mb_GetResultData1(priv->mb, MBE_AlbumGetArtistName, data, sizeof (data), 1)) {
          album->artist = g_strdup (data);
        } else {
          album->artist = g_strdup (_("Unknown Artist"));
        }
        if (data && mb_GetResultData1(priv->mb, MBE_AlbumGetArtistSortName, data, sizeof (data), 1)) {
          album->artist_sortname = g_strdup (data);
        }
      }
    }

    if (mb_GetResultData(priv->mb, MBE_AlbumGetAlbumName, data, sizeof (data))) {
      album->title = g_strdup (data);
    } else {
      album->title = g_strdup (_("Unknown Title"));
    }

    {
      int num_releases;
      num_releases = mb_GetResultInt (priv->mb, MBE_AlbumGetNumReleaseDates);
      if (num_releases > 0) {
        mb_Select1(priv->mb, MBS_SelectReleaseDate, 1);
        if (mb_GetResultData(priv->mb, MBE_ReleaseGetDate, data, sizeof (data))) {
          int matched, year=1, month=1, day=1;
          matched = sscanf(data, "%u-%u-%u", &year, &month, &day);
          if (matched >= 1) {
            album->release_date = g_date_new_dmy ((day == 0) ? 1 : day, (month == 0) ? 1 : month, year);
          }
        }
        mb_Select(priv->mb, MBS_Back);
      }
    }

    num_tracks = mb_GetResultInt(priv->mb, MBE_AlbumGetNumTracks);
    if (num_tracks < 1) {
      g_free (album->artist);
      g_free (album->artist_sortname);
      g_free (album->title);
      g_free (album);
      g_warning (_("Incomplete metadata for this CD"));
      priv->albums = get_offline_track_listing (metadata, &(priv->error));
      g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
      return priv->albums;
    }

    for (j = 1; j <= num_tracks; j++) {
      TrackDetails *track;
      track = g_new0 (TrackDetails, 1);

      track->album = album;

      track->number = j; /* replace with number lookup? */

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetTrackId, data, sizeof (data), j)) {
        mb_GetIDFromURL (priv->mb, data, data, sizeof (data));
        track->track_id = g_strdup (data);
      }

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetArtistId, data, sizeof (data), j)) {
        mb_GetIDFromURL (priv->mb, data, data, sizeof (data));
        track->artist_id = g_strdup (data);
      }

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetTrackName, data, sizeof (data), j)) {
        if (track->artist_id != NULL) {
          artist_and_title_from_title (track, data);
        } else {
          track->title = g_strdup (data);
        }
      }

      if (track->artist == NULL && mb_GetResultData1(priv->mb, MBE_AlbumGetArtistName, data, sizeof (data), j)) {
        track->artist = g_strdup (data);
      }

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetArtistSortName, data, sizeof (data), j)) {
        track->artist_sortname = g_strdup (data);
      }

      if (mb_GetResultData1(priv->mb, MBE_AlbumGetTrackDuration, data, sizeof (data), j)) {
        track->duration = atoi (data) / 1000;
      }

      if (from_freedb) {
        convert_encoding(&track->title);
        convert_encoding(&track->artist);
        convert_encoding(&track->artist_sortname);
      }
      
      album->tracks = g_list_append (album->tracks, track);
      album->number++;
    }

    if (from_freedb) {
      convert_encoding(&album->title);
      convert_encoding(&album->artist);
      convert_encoding(&album->artist_sortname);
    }

    albums = g_list_append (albums, album);

    mb_Select (priv->mb, MBS_Rewind);
  }

  /* For each album, we need to insert the duration data if necessary
   * We need to query this here because otherwise we would flush the
   * data queried from the server */
  /* TODO: scan for 0 duration before doing the query to avoid another lookup if
     we don't need to do it */
  mb_Query (priv->mb, MBQ_GetCDTOC);
  for (al = albums; al; al = al->next) {
    AlbumDetails *album = al->data;
    
    j = 1;
    for (tl = album->tracks; tl; tl = tl->next) {
      TrackDetails *track = tl->data;
      int sectors;
      
      if (track->duration == 0) {
        sectors = mb_GetResultInt1 (priv->mb, MBE_TOCGetTrackNumSectors, j+1);
        track->duration = get_duration_from_sectors (sectors);
      }
      j++;
    }
  }

  priv->albums = albums;
  g_idle_add ((GSourceFunc)fire_signal_idle, metadata);
  return albums;
}

static void
mb_list_albums (SjMetadata *metadata, GError **error)
{
  GThread *thread;

  g_return_if_fail (SJ_IS_METADATA_MUSICBRAINZ (metadata));

  thread = g_thread_create ((GThreadFunc)lookup_cd, metadata, TRUE, error);
  if (thread == NULL) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 _("Could not create CD lookup thread"));
    return;
  }
}

static char *
mb_get_submit_url (SjMetadata *metadata)
{
  SjMetadataMusicbrainzPrivate *priv;
  char url[1025];

  g_return_val_if_fail (metadata != NULL, NULL);

  priv = SJ_METADATA_MUSICBRAINZ (metadata)->priv;

  if (mb_GetWebSubmitURL(priv->mb, url, 1024)) {
    return g_strdup(url);
  } else {
    return NULL;
  }
}


/*
 * GObject methods
 */

static void
metadata_interface_init (gpointer g_iface, gpointer iface_data)
{
  SjMetadataClass *klass = (SjMetadataClass*)g_iface;
  klass->get_new_error = mb_get_new_error;
  klass->list_albums = mb_list_albums;
  klass->get_submit_url = mb_get_submit_url;
}

static void
sj_metadata_musicbrainz_init (SjMetadataMusicbrainz *self)
{
  GConfClient *gconf_client;
  char *server_name = NULL;

  self->priv = GET_PRIVATE (self);
  self->priv->construct_error = NULL;
  self->priv->mb = mb_New ();
  /* TODO: something. :/ */
  if (!self->priv->mb) {
    g_set_error (&self->priv->construct_error,
                 SJ_ERROR, SJ_ERROR_CD_LOOKUP_ERROR,
                 _("Cannot create MusicBrainz client"));
    return;
  }
  mb_UseUTF8 (self->priv->mb, TRUE);

  gconf_client = gconf_client_get_default ();
  server_name = gconf_client_get_string (gconf_client, GCONF_MUSICBRAINZ_SERVER, NULL);
  if (server_name) {
    g_strstrip (server_name);
  }
  if (server_name && strcmp (server_name, "") != 0) {
    mb_SetServer (self->priv->mb, server_name, 80);
    g_free (server_name);
  }
  
  /* Set the HTTP proxy */
  if (gconf_client_get_bool (gconf_client, GCONF_PROXY_USE_PROXY, NULL)) {
    char *proxy_host = gconf_client_get_string (gconf_client, GCONF_PROXY_HOST, NULL);
    mb_SetProxy (self->priv->mb, proxy_host,
                 gconf_client_get_int (gconf_client, GCONF_PROXY_PORT, NULL));
    g_free (proxy_host);
    if (gconf_client_get_bool (gconf_client, GCONF_PROXY_USE_AUTHENTICATION, NULL)) {
#if HAVE_MB_SETPROXYCREDS
      char *username = gconf_client_get_string (gconf_client, GCONF_PROXY_USERNAME, NULL);
      char *password = gconf_client_get_string (gconf_client, GCONF_PROXY_PASSWORD, NULL);
      mb_SetProxyCreds (self->priv->mb, username, password);
      g_free (username);
      g_free (password);
#else
      g_warning ("mb_SetProxyCreds() not found, no proxy authorisation possible.");
#endif
    }
  }

  g_object_unref (gconf_client);

  if (g_getenv("MUSICBRAINZ_DEBUG")) {
    mb_SetDebug (self->priv->mb, TRUE);
  }
}

static void
sj_metadata_musicbrainz_get_property (GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  SjMetadataMusicbrainzPrivate *priv = SJ_METADATA_MUSICBRAINZ (object)->priv;
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
sj_metadata_musicbrainz_set_property (GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  SjMetadataMusicbrainzPrivate *priv = SJ_METADATA_MUSICBRAINZ (object)->priv;
  g_assert (priv);

  switch (property_id) {
  case PROP_DEVICE:
    if (priv->cdrom)
      g_free (priv->cdrom);
    priv->cdrom = g_value_dup_string (value);
    if (priv->cdrom)
      mb_SetDevice (priv->mb, priv->cdrom);
    break;
  case PROP_PROXY_HOST:
    if (priv->http_proxy) {
      g_free (priv->http_proxy);
    }
    priv->http_proxy = g_value_dup_string (value);
    /* TODO: check this unsets the proxy if NULL, or should we pass "" ? */
    mb_SetProxy (priv->mb, priv->http_proxy, priv->http_proxy_port);
    break;
  case PROP_PROXY_PORT:
    priv->http_proxy_port = g_value_get_int (value);
    mb_SetProxy (priv->mb, priv->http_proxy, priv->http_proxy_port);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
sj_metadata_musicbrainz_finalize (GObject *object)
{
  SjMetadataMusicbrainzPrivate *priv;
  
  priv = SJ_METADATA_MUSICBRAINZ (object)->priv;

  g_free (priv->http_proxy);
  g_free (priv->cdrom);
  mb_Delete (priv->mb);

  G_OBJECT_CLASS (sj_metadata_musicbrainz_parent_class)->finalize (object);
}

static void
sj_metadata_musicbrainz_class_init (SjMetadataMusicbrainzClass *class)
{
  GObjectClass *object_class = (GObjectClass*)class;

  g_type_class_add_private (class, sizeof (SjMetadataMusicbrainzPrivate));

  object_class->get_property = sj_metadata_musicbrainz_get_property;
  object_class->set_property = sj_metadata_musicbrainz_set_property;
  object_class->finalize = sj_metadata_musicbrainz_finalize;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
  g_object_class_override_property (object_class, PROP_PROXY_HOST, "proxy-host");
  g_object_class_override_property (object_class, PROP_PROXY_PORT, "proxy-port");
}


/*
 * Public methods.
 */

GObject *
sj_metadata_musicbrainz_new (void)
{
  return g_object_new (sj_metadata_musicbrainz_get_type (), NULL);
}

