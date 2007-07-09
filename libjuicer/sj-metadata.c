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
#include "sj-metadata.h"
#include "sj-metadata-marshal.h"

enum {
  METADATA,
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

static void
sj_metadata_base_init (gpointer g_iface)
{
  static gboolean initialized = FALSE;
  if (!initialized) {
    signals[METADATA] = g_signal_new ("metadata",
                                      G_TYPE_FROM_CLASS (g_iface),
                                      G_SIGNAL_RUN_LAST,
                                      G_STRUCT_OFFSET (SjMetadataClass, metadata),
                                      NULL, NULL,
                                      metadata_marshal_VOID__POINTER_POINTER,
                                      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

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

void
sj_metadata_list_albums (SjMetadata *metadata, GError **error)
{
  SJ_METADATA_GET_CLASS (metadata)->list_albums (metadata, error);
}

char *
sj_metadata_get_submit_url (SjMetadata *metadata)
{
  if (SJ_METADATA_GET_CLASS (metadata)->get_submit_url)
    return SJ_METADATA_GET_CLASS (metadata)->get_submit_url (metadata);
  else
    return NULL;
}
