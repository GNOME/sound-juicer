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
#include "sj-structures.h"
#include "sj-metadata-getter.h"
#include "sj-metadata.h"
#ifdef HAVE_MUSICBRAINZ5
#include "sj-metadata-musicbrainz5.h"
#endif /* HAVE_MUSICBRAINZ5 */
#include "sj-metadata-gvfs.h"
#include "sj-error.h"

#define SJ_SETTINGS_PROXY_MODE "mode"
#define SJ_SETTINGS_PROXY_HOST "host"
#define SJ_SETTINGS_PROXY_PORT "port"
#define SJ_SETTINGS_PROXY_USE_AUTHENTICATION "use-authentication"
#define SJ_SETTINGS_PROXY_USERNAME "authentication-user"
#define SJ_SETTINGS_PROXY_PASSWORD "authentication-password"

struct SjMetadataGetterPrivate {
  char *cdrom;
};

typedef struct SjMetadataGetterPrivate SjMetadataGetterPrivate;

static void sj_metadata_getter_finalize (GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE(SjMetadataGetter, sj_metadata_getter, G_TYPE_OBJECT);

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
  SjMetadataGetterPrivate *priv = GETTER_PRIVATE (object);

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

  priv = GETTER_PRIVATE (mdg);

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
bind_http_proxy_settings (SjMetadata *metadata)
{
  GSettings *settings;

  settings = g_settings_new ("org.gnome.system.proxy.http");
  /* bind with G_SETTINGS_BIND_GET_NO_CHANGES to avoid occasional
     segfaults in g_object_set_property called with an invalid pointer
     which I think were caused by the update being scheduled before
     metadata was destroy but happening afterwards (g_settings_bind is
     not called from the main thread). metadata is a short lived
     object so there shouldn't be a problem in practice, as the setting
     are unlikely to change while it exists. If the settings change
     between ripping one CD and the next then as a new metadata object
     is created for the second query it will have the updated
     settings. */
  g_settings_bind (settings, SJ_SETTINGS_PROXY_HOST,
                   metadata, "proxy-host",
                   G_SETTINGS_BIND_GET_NO_CHANGES);

  g_settings_bind (settings, SJ_SETTINGS_PROXY_PORT,
                   metadata, "proxy-port",
                   G_SETTINGS_BIND_GET_NO_CHANGES);

  g_settings_bind (settings, SJ_SETTINGS_PROXY_USERNAME,
                   metadata, "proxy-username",
                   G_SETTINGS_BIND_GET_NO_CHANGES);

  g_settings_bind (settings, SJ_SETTINGS_PROXY_PASSWORD,
                   metadata, "proxy-password",
                   G_SETTINGS_BIND_GET_NO_CHANGES);

  g_settings_bind (settings, SJ_SETTINGS_PROXY_USE_AUTHENTICATION,
                   metadata, "proxy-use-authentication",
                   G_SETTINGS_BIND_GET_NO_CHANGES);

  g_object_unref (settings);

  settings = g_settings_new ("org.gnome.system.proxy");
  g_settings_bind (settings, SJ_SETTINGS_PROXY_MODE,
                   metadata, "proxy-mode",
                   G_SETTINGS_BIND_GET_NO_CHANGES);
  g_object_unref (settings);
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
#ifdef HAVE_MUSICBRAINZ5
    SJ_TYPE_METADATA_MUSICBRAINZ5,
#endif /* HAVE_MUSICBRAINZ5 */
    SJ_TYPE_METADATA_GVFS
  };

  for (i = 0; albums == NULL && i < G_N_ELEMENTS (types); i++) {
    SjMetadata *metadata;

    metadata = g_object_new (types[i],
                             "device", task_data,
                             NULL);
    bind_http_proxy_settings (metadata);
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

  priv = GETTER_PRIVATE (mdg);
  task = g_task_new (mdg, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (priv->cdrom), g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, list_albums_thread_cb);
  g_object_unref (task);
}
