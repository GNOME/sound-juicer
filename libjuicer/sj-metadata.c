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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib-object.h>
#include <stdlib.h>
#include <stdio.h>
#include "sj-metadata.h"
#include "sj-metadata-marshal.h"

enum {
  METADATA,
  LAST_SIGNAL
};

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
                                         g_param_spec_string ("proxy-host", "proxy-host", NULL, NULL,
                                                              G_PARAM_READABLE|G_PARAM_WRITABLE|
                                                              G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

    g_object_interface_install_property (g_iface,
                                         g_param_spec_int ("proxy-port", "proxy-port", NULL,
                                                           0, G_MAXINT, 0,
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

void
sj_metadata_set_proxy (SjMetadata *metadata, const char* proxy)
{
  g_object_set (metadata, "proxy-host", proxy, NULL);
}

void
sj_metadata_set_proxy_port (SjMetadata *metadata, const int proxy_port)
{
  g_object_set (metadata, "proxy-port", proxy_port, NULL);
}

GList *
sj_metadata_list_albums (SjMetadata *metadata, char **url, GError **error)
{
  return SJ_METADATA_GET_CLASS (metadata)->list_albums (metadata, url, error);
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

GDate *
sj_metadata_helper_scan_date (const char *date)
{
  int matched, year=1, month=1, day=1;
  matched = sscanf(date, "%u-%u-%u", &year, &month, &day);
  if (matched >= 1) {
    return g_date_new_dmy ((day == 0) ? 1 : day, (month == 0) ? 1 : month, year);
  }

  return NULL;
}

