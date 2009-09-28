/* 
 * Copyright (C) 2007 Carl-Anton Ingmarsson <ca.ingmarsson@gmail.com>
 * Copyright (C) 2006-2007 Richard Hughes <richard@hughsie.com>
 *
 * Sound Juicer - sj-inhibit.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Carl-Anton Ingmarsson <ca.ingmarsson@gmail.com>
 */

#include <dbus/dbus-glib.h>
 
#include "sj-inhibit.h"
 
/* PowerManagent defines */
#define	PM_DBUS_SERVICE    "org.gnome.SessionManager"
#define	PM_DBUS_INHIBIT_PATH   "/org/gnome/SessionManager"
#define	PM_DBUS_INHIBIT_INTERFACE    "org.gnome.SessionManager"

/** cookie is returned as an unsigned integer */
guint
sj_inhibit (const gchar * appname, const gchar * reason, guint xid)
{
  gboolean res;
  guint cookie;
  GError *error = NULL;
  DBusGProxy *proxy = NULL;
  DBusGConnection *session_connection = NULL;

  /* get the DBUS session connection */
  session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (error != NULL) {
    g_warning ("DBUS cannot connect : %s", error->message);
    g_error_free (error);
    return 0;
  }

  /* get the proxy for PowerManagement */
  proxy = dbus_g_proxy_new_for_name (session_connection,
				      PM_DBUS_SERVICE,
				      PM_DBUS_INHIBIT_PATH,
				      PM_DBUS_INHIBIT_INTERFACE);
  if (proxy == NULL) {
    g_warning ("Could not get DBUS proxy: %s", PM_DBUS_SERVICE);
    return 0;
  }

  res = dbus_g_proxy_call (proxy,
			    "Inhibit", &error,
			    G_TYPE_STRING, appname, /* app-id */
			    G_TYPE_UINT, xid,
			    G_TYPE_STRING, reason,
			    G_TYPE_UINT, 4+8, /* flags, inhibit being marked idle and allowing suspend */
			    G_TYPE_INVALID,
			    G_TYPE_UINT, &cookie,
                            G_TYPE_INVALID);

  /* check the return value */
  if (!res) {
    cookie = 0;
    g_warning ("Inhibit method failed");
  }

  /* check the error value */
  if (error != NULL) {
    g_warning ("Inhibit problem : %s", error->message);
    g_error_free (error);
    cookie = 0;
  }

  g_object_unref (G_OBJECT (proxy));
  return cookie;
}

void
sj_uninhibit (guint cookie)
{
  gboolean res;
  GError *error = NULL;
  DBusGProxy *proxy = NULL;
  DBusGConnection *session_connection = NULL;

  /* check if the cookie is valid */
  if (cookie <= 0) {
    g_warning ("Invalid cookie");
    return;
  }

  /* get the DBUS session connection */
  session_connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
  if (error) {
    g_warning ("DBUS cannot connect : %s", error->message);
    g_error_free (error);
    return;
  }

  /* get the proxy for PowerManagement */
  proxy = dbus_g_proxy_new_for_name (session_connection,
				      PM_DBUS_SERVICE,
				      PM_DBUS_INHIBIT_PATH,
				      PM_DBUS_INHIBIT_INTERFACE);
  if (proxy == NULL) {
    g_warning ("Could not get DBUS proxy: %s", PM_DBUS_SERVICE);
    return;
  }

  res = dbus_g_proxy_call (proxy,
			    "Uninhibit",
			    &error,
			    G_TYPE_UINT, cookie,
			    G_TYPE_INVALID,
                            G_TYPE_INVALID);

  /* check the return value */
  if (!res) {
    g_warning ("Uninhibit method failed");
  }

  /* check the error value */
  if (error != NULL) {
    g_warning ("Inhibit problem : %s", error->message);
    g_error_free (error);
  }
  g_object_unref (G_OBJECT (proxy));
}

