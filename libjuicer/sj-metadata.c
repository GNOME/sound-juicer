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
#include <glib-object.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <stdio.h>
#include <gdesktop-enums.h>

#include <unistd.h>
#include <udisks/udisks.h>

#include "sj-metadata.h"
#include "sj-error.h"
#include "sj-structures.h"

enum {
  METADATA,
  LAST_SIGNAL
};

static GType
g_desktop_proxy_mode_get_type (void)
{
  static GType etype = 0;
  if (etype == 0) {
    static const GEnumValue values[] = {
      { G_DESKTOP_PROXY_MODE_NONE, "G_DESKTOP_PROXY_MODE_NONE", "none" },
      { G_DESKTOP_PROXY_MODE_MANUAL, "G_DESKTOP_PROXY_MODE_MANUAL", "manual" },
      { G_DESKTOP_PROXY_MODE_AUTO, "G_DESKTOP_PROXY_MODE_AUTO", "auto" },
      { 0, NULL, NULL }
    };
    etype = g_enum_register_static ("GDesktopProxyMode", values);
  }
  return etype;
}

static void
sj_metadata_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;
  if (!initialized) {
    /* TODO: make these constructors */
    /* TODO: add nice nick and blurb strings */
    g_object_interface_install_property (g_iface,
                                         g_param_spec_string ("device", "device", NULL, NULL,
                                                              G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                              G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

    g_object_interface_install_property (g_iface,
                                         g_param_spec_boolean ("proxy-use-authentication", "proxy-use-authentication",
                                                               "Whether the http proxy requires authentication", FALSE,
                                                               G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                               G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
    g_object_interface_install_property (g_iface,
                                         g_param_spec_string ("proxy-host", "proxy-host", NULL, NULL,
                                                              G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                              G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

    g_object_interface_install_property (g_iface,
                                         g_param_spec_int ("proxy-port", "proxy-port", NULL,
                                                           0, G_MAXINT, 0,
                                                           G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                           G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

    g_object_interface_install_property (g_iface,
                                         g_param_spec_string ("proxy-username", "proxy-username", NULL, NULL,
                                                              G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                              G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

    g_object_interface_install_property (g_iface,
                                         g_param_spec_string ("proxy-password", "proxy-password", NULL, NULL,
                                                              G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                              G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
    g_object_interface_install_property (g_iface,
                                         g_param_spec_enum ("proxy-mode", "proxy-mode", NULL,
                                                            g_desktop_proxy_mode_get_type(), G_DESKTOP_PROXY_MODE_NONE,
                                                            G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                            G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

    initialized = TRUE;
  }
}

GType
sj_metadata_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (SjMetadataClass), /* class_size */
      sj_metadata_base_init,   /* base_init */
      NULL,           /* base_finalize */
      NULL,
      NULL,           /* class_finalize */
      NULL,           /* class_data */
      0,
      0,              /* n_preallocs */
      NULL,
      NULL
    };

    type = g_type_register_static (G_TYPE_INTERFACE, "SjMetadata", &info, 0);
    g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
  }
  return type;
}

void
sj_metadata_set_cdrom (SjMetadata *metadata, const char* device)
{
  g_object_set (metadata, "device", device, NULL);
}

GList *
sj_metadata_list_albums (SjMetadata *metadata, char **url, GCancellable *cancellable, GError **error)
{
  GError *err = NULL;
  GCancellable *cancel;
  GList *albums = NULL;

  if (cancellable != NULL) {
    if (g_cancellable_set_error_if_cancelled (cancellable, &err))
      goto out;
    cancel = cancellable;
  } else {
    cancel = g_cancellable_new ();
  }
  albums = SJ_METADATA_GET_CLASS (metadata)->list_albums (metadata, url, cancel, &err);
  if (cancellable == NULL) {
    g_object_unref (cancel);
  } else if (err == NULL &&
             g_cancellable_set_error_if_cancelled (cancellable, &err)) {
    g_list_free_full (albums, (GDestroyNotify) album_details_free);
    albums = NULL;
  }
 out:
  if (err != NULL)
    g_propagate_error (error, err);
  return albums;
}

char *
sj_metadata_helper_scan_disc_number (const char *album_title, int *disc_number)
{
  GRegex *disc_regex;
  GMatchInfo *info;
  char *new_title;
  int num;

  disc_regex = g_regex_new (".+( \\(disc (\\d+).*)", 0, 0, NULL);
  new_title = NULL;
  *disc_number = 0;

  if (g_regex_match (disc_regex, album_title, 0, &info)) {
    int pos = 0;
    char *s;

    g_match_info_fetch_pos (info, 1, &pos, NULL);
    if (pos) {
      new_title = g_strndup (album_title, pos);
    }

    s = g_match_info_fetch (info, 2);
    num = atoi (s);
    *disc_number = num;
    g_free (s);
  }

  g_match_info_free (info);
  g_regex_unref (disc_regex);

  return new_title;
}

/**
 * sj_metadata_helper_check_media:
 * @cdrom: UNIX device path of the CD-ROM to check.
 * @error: (nullable): Return location for setting error details.
 *
 * Checks whether the device @cdrom has media available or not.
 *
 * Returns: %TRUE if @cdrom is ready. If %FALSE, @error is set with additional
 * detail.
 */
gboolean
sj_metadata_helper_check_media (const char *cdrom, GError **error)
{
  GError *err = NULL;
  g_autoptr (UDisksClient) client = NULL;
  g_autoptr (UDisksObject) object = NULL;
  g_autoptr (UDisksBlock) block = NULL;
  g_autoptr (UDisksDrive) drive = NULL;
  g_autofree gchar *basename = NULL;
  g_autofree gchar *object_path = NULL;

  g_assert_null (err);
  client = udisks_client_new_sync (NULL, &err);
  if (err != NULL) {
    g_propagate_error (error, err);
    return FALSE;
  }
  basename = g_path_get_basename (cdrom);
  object_path = g_strjoin ("/", "/org/freedesktop/UDisks2/block_devices", basename, NULL);
  object = udisks_client_get_object (client, object_path);
  if (object == NULL) {
    g_set_error (error, SJ_ERROR, SJ_ERROR_INTERNAL_ERROR, _("Cannot get device ‘%s’"), cdrom);
    return FALSE;
  }
  block = udisks_object_get_block (object);
  g_assert_nonnull (block);
  drive = udisks_client_get_drive_for_block (client, block);
  g_assert_nonnull (drive);

  if (!udisks_drive_get_media_available (drive)) {
    char *msg;
    SjError err;

    if (access (cdrom, W_OK) == 0) {
      msg = g_strdup_printf (_("Device ‘%s’ does not contain any media"), cdrom);
      err = SJ_ERROR_CD_NO_MEDIA;
    } else {
      msg = g_strdup_printf (_("Device ‘%s’ could not be opened. Check the access permissions on the device."), cdrom);
      err = SJ_ERROR_CD_PERMISSION_ERROR;
    }
    g_set_error (error, SJ_ERROR, err, _("Cannot read CD: %s"), msg);
    g_free (msg);

    return FALSE;
  }

  return TRUE;
}

/* ISO-3166 helpers, these functions translate between a country code
 * returned by MusicBrainz and the country name. Adapted from the
 * totem language name lookup functions before it switched to using
 * the GStreamer language helpers
 */
static GHashTable *country_table;

void
sj_metadata_helper_cleanup (void)
{
  if (country_table == NULL)
    return;

  g_hash_table_destroy (country_table);
  country_table = NULL;
}

static void
country_table_parse_start_tag (GMarkupParseContext *ctx,
                               const gchar         *element_name,
                               const gchar        **attr_names,
                               const gchar        **attr_values,
                               gpointer             data,
                               GError             **error)
{
  const char *ccode, *country_name;

  if (!g_str_equal (element_name, "iso_3166_entry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  ccode = NULL;
  country_name = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "alpha_2_code"))
        {
          /* skip if empty */
          if (**attr_values)
            {
              g_return_if_fail (strlen (*attr_values) == 2);
              ccode = *attr_values;
            }
        } else if (g_str_equal (*attr_names, "name")) {
        country_name = *attr_values;
      }

      ++attr_names;
      ++attr_values;
    }

  if (country_name == NULL)
    return;

  if (ccode != NULL)
    {
      g_hash_table_insert (country_table,
                           g_strdup (ccode),
                           g_strdup (country_name));
    }
}

#define ISO_CODES_DATADIR ISO_CODES_PREFIX"/share/xml/iso-codes"
#define ISO_CODES_LOCALESDIR ISO_CODES_PREFIX"/share/locale"

static void
country_table_init (void)
{
  GError *err = NULL;
  char *buf;
  gsize buf_len;

  country_table = g_hash_table_new_full
    (g_str_hash, g_str_equal, g_free, g_free);

  bindtextdomain ("iso_3166", ISO_CODES_LOCALESDIR);
  bind_textdomain_codeset ("iso_3166", "UTF-8");

  if (g_file_get_contents (ISO_CODES_DATADIR "/iso_3166.xml",
                           &buf, &buf_len, &err))
    {
      GMarkupParseContext *ctx;
      GMarkupParser parser =
        { country_table_parse_start_tag, NULL, NULL, NULL, NULL };

      ctx = g_markup_parse_context_new (&parser, 0, NULL, NULL);

      if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
        {
          g_warning ("Failed to parse '%s': %s\n",
                     ISO_CODES_DATADIR"/iso_3166.xml",
                     err->message);
          g_error_free (err);
        }

      g_markup_parse_context_free (ctx);
      g_free (buf);
    } else {
    g_warning ("Failed to load '%s': %s\n",
               ISO_CODES_DATADIR"/iso_3166.xml", err->message);
    g_error_free (err);
  }
}

char *
sj_metadata_helper_lookup_country_code (const char *code)
{
  const char *country_name;
  gsize len, i;
  /* Musicbrainz uses some additional codes on top of ISO-3166 so
     treat those as a special case if we don't get a match from the
     iso-codes data */
  static const struct {
    const char *code;
    const char *name;
  } mb_countries[]  = {
    {"XC", N_("Czechoslovakia")},
    {"XG", N_("East Germany")},
    {"XE", N_("Europe")},
    {"CS", N_("Serbia and Montenegro")},
    {"SU", N_("Soviet Union")},
    {"XW", N_("Worldwide")},
    {"YU", N_("Yugoslavia")}
  };

  g_return_val_if_fail (code != NULL, NULL);

  len = strlen (code);
  if (len != 2)
    return NULL;
  if (country_table == NULL)
    country_table_init ();

  country_name = (const gchar*) g_hash_table_lookup (country_table, code);

  if (country_name)
    return g_strdup (dgettext ("iso_3166", country_name));

  for (i = 0; i < G_N_ELEMENTS (mb_countries); i++) {
    if (strcmp (code, mb_countries[i].code) == 0)
      return g_strdup (gettext (mb_countries[i].name));
  }

  /* Musicbrainz uses XU for unknown so don't print an error, just
     treat it as if a country wasn't provided */
  if (strcmp (code, "XU") != 0)
    g_warning ("Unknown country code: %s", code);
  return NULL;
}
