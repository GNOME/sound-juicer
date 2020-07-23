/*
 * sj-metadata.c
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gsettings-desktop-schemas/gdesktop-enums.h>
#include "sj-structures.h"
#include "sj-metadata-getter.h"
#include "sj-metadata.h"
#include "sj-metadata-musicbrainz5.h"
#include "sj-metadata-gvfs.h"
#include "sj-error.h"

#define SJ_SETTINGS_PROXY_MODE "mode"
#define SJ_SETTINGS_PROXY_HOST "host"
#define SJ_SETTINGS_PROXY_PORT "port"
#define SJ_SETTINGS_PROXY_USE_AUTHENTICATION "use-authentication"
#define SJ_SETTINGS_PROXY_USERNAME "authentication-user"
#define SJ_SETTINGS_PROXY_PASSWORD "authentication-password"

/* Note that this wouldn't work if we had non-musicbrainz online
 * metadata getters, or used a different server */
#define DEFAULT_MUSICBRAINZ_SERVER "musicbrainz.org"

struct SjMetadataGetterPrivate {
  char *cdrom;
};

typedef struct SjMetadataGetterPrivate SjMetadataGetterPrivate;

static void sj_metadata_getter_finalize (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE (SjMetadataGetter, sj_metadata_getter, G_TYPE_OBJECT);

#define GETTER_PRIVATE(o)                                            \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), SJ_TYPE_METADATA_GETTER, SjMetadataGetterPrivate))

static void
sj_metadata_getter_class_init (SjMetadataGetterClass *klass)
{
  GObjectClass *object_class;
  object_class = (GObjectClass *)klass;
  object_class->finalize = sj_metadata_getter_finalize;
}

static void
sj_metadata_getter_finalize (GObject *object)
{
  SjMetadataGetter *self = (SjMetadataGetter *)object;
  SjMetadataGetterPrivate *priv;
  priv = sj_metadata_getter_get_instance_private (self);
  g_free (priv->cdrom);
  G_OBJECT_CLASS (sj_metadata_getter_parent_class)->finalize (object);
}

static void
sj_metadata_getter_init (SjMetadataGetter *mdg)
{
}

SjMetadataGetter *
sj_metadata_getter_new (void)
{
  return SJ_METADATA_GETTER (g_object_new (SJ_TYPE_METADATA_GETTER, NULL));
}

void
sj_metadata_getter_set_cdrom (SjMetadataGetter *mdg, const char* device)
{
  SjMetadataGetterPrivate *priv;
  priv = sj_metadata_getter_get_instance_private (mdg);
  g_free (priv->cdrom);

#ifdef __sun
  if (g_str_has_prefix (device, "/dev/dsk/")) {
    priv->cdrom = g_strdup_printf ("/dev/rdsk/%s", device + strlen ("/dev/dsk/"));
    return;
  }
#endif
  priv->cdrom = g_strdup (device);
}

static void
set_http_proxy (SjMetadata *metadata,
                const char *proxy_url)
{
  GstUri *uri;
  const char *host;
  guint port;
  const char *userinfo;
  g_autofree char *user = NULL;
  g_autofree char *password = NULL;
  g_auto(GStrv) user_strv = NULL;

  uri = gst_uri_from_string (proxy_url);
  if (!uri) {
    GST_DEBUG ("Failed to parse URI '%s'", proxy_url);
    g_object_set (metadata,
                  "proxy-mode", G_DESKTOP_PROXY_MODE_NONE,
                  NULL);
    return;
  }

  host = gst_uri_get_host (uri);
  port = gst_uri_get_port (uri);

  g_object_set (metadata,
                "proxy-host", host,
                "proxy-port", port,
                NULL);

  /* https doesn't handle authentication yet */
  if (gst_uri_has_protocol (proxy_url, "https")) {
    g_object_set (metadata,
                  "proxy-use-authentication", FALSE,
                  NULL);
    goto finish;
  }

  userinfo = gst_uri_get_userinfo (uri);
  if (userinfo == NULL) {
    g_object_set (metadata,
                  "proxy-use-authentication", FALSE,
                  NULL);
    goto finish;
  }

  user_strv = g_strsplit (userinfo, ":", 2);
  user = g_uri_unescape_string (user_strv[0], NULL);
  password = g_uri_unescape_string (user_strv[1], NULL);

  g_object_set (metadata,
                "proxy-username", user,
                "proxy-password", password,
                "proxy-use-authentication", TRUE,
                NULL);

finish:
  gst_uri_unref (uri);
}

static void
set_http_proxy_settings (SjMetadata *metadata)
{
  GError *error = NULL;
  char **uris;

  uris = g_proxy_resolver_lookup (g_proxy_resolver_get_default (),
                                  DEFAULT_MUSICBRAINZ_SERVER,
                                  NULL,
                                  &error);
  if (!uris) {
    if (error != NULL) {
      g_debug ("Failed to look up proxy for '%s': %s",
               DEFAULT_MUSICBRAINZ_SERVER,
               error->message);
      g_clear_error (&error);
    }
    g_object_set (metadata,
                  "proxy-mode", G_DESKTOP_PROXY_MODE_NONE,
                  NULL);
    return;
  }

  if (g_str_equal (uris[0], "direct://")) {
    g_strfreev (uris);
    g_object_set (metadata,
                  "proxy-mode", G_DESKTOP_PROXY_MODE_NONE,
                  NULL);
    return;
  }

  g_object_set (metadata,
                "proxy-mode", G_DESKTOP_PROXY_MODE_MANUAL,
                NULL);
  set_http_proxy (metadata, uris[0]);
  g_strfreev (uris);
}

GList *
sj_metadata_getter_list_albums_finish (SjMetadataGetter  *getter,
                                       GAsyncResult      *result,
                                       gchar            **url,
                                       GError           **error)
{
  GList *albums;
  GTask *task;

  g_return_val_if_fail (g_task_is_valid (result, getter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  task = (GTask*) result;
  albums = g_task_propagate_pointer (task, error);
  if (albums != NULL)
    *url = g_strdup (g_task_get_task_data (task));

  return albums;
}

static void
free_album_list (gpointer albums)
{
  g_list_free_full (albums, (GDestroyNotify) album_details_free);
}

static void
list_albums_thread_cb (GTask        *task,
                       gpointer      source,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  guint i;
  GError *error = NULL;
  GList *albums = NULL;
  gchar *url = NULL;
  GType types[] = {
    SJ_TYPE_METADATA_MUSICBRAINZ5,
    SJ_TYPE_METADATA_GVFS
  };

  for (i = 0; albums == NULL && i < G_N_ELEMENTS (types); i++) {
    SjMetadata *metadata;

    metadata = g_object_new (types[i],
                             "device", task_data,
                             NULL);
    set_http_proxy_settings (metadata);
    if (url == NULL)
      albums = sj_metadata_list_albums (metadata, &url, cancellable, &error);
    else
      albums = sj_metadata_list_albums (metadata, NULL, cancellable, &error);

    g_object_unref (metadata);

    if (error != NULL) {
      free_album_list (albums);
      g_free (url);
      g_task_return_error (task, error);
      return;
    }
  }

  g_task_set_task_data (task, url, g_free);
  g_task_return_pointer (task, albums, free_album_list);
}

void
sj_metadata_getter_list_albums_async (SjMetadataGetter    *mdg,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GTask *task;
  SjMetadataGetterPrivate *priv;

  priv = sj_metadata_getter_get_instance_private (mdg);
  task = g_task_new (mdg, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (priv->cdrom), g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, list_albums_thread_cb);
  g_object_unref (task);
}
