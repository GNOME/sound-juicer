/* 
 * Sound Juicer - sj-main.h
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
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifndef SJ_MAIN_H
#define SJ_MAIN_H

#include <gtk/gtkwidget.h>
#include "sj-structures.h"

void sj_stock_init (void);

void on_quit_activate (GtkMenuItem *item, gpointer user_data);
void on_destroy_activate (GtkMenuItem *item, gpointer user_data);
void on_eject_activate (GtkMenuItem *item, gpointer user_data);
void on_select_all_activate (GtkMenuItem *item, gpointer user_data);
void on_deselect_all_activate (GtkMenuItem *item, gpointer user_data);
void on_destroy_signal (GtkMenuItem *item, gpointer user_data);

AlbumDetails* multiple_album_dialog (GList* albums);

void baseuri_changed_cb (GConfClient *client, guint cnxn_id, 
			 GConfEntry *entry, gpointer user_data);
void path_pattern_changed_cb (GConfClient *client, guint cnxn_id, 
			      GConfEntry *entry, gpointer user_data);
void file_pattern_changed_cb (GConfClient *client, guint cnxn_id, 
			      GConfEntry *entry, gpointer user_data);
void paranoia_changed_cb (GConfClient *client, guint cnxn_id, 
			  GConfEntry *entry, gpointer user_data);
void strip_changed_cb (GConfClient *client, guint cnxn_id, 
		       GConfEntry *entry, gpointer user_data);
void eject_changed_cb (GConfClient *client, guint cnxn_id, 
		       GConfEntry *entry, gpointer user_data);
void audio_volume_changed_cb (GConfClient *client, guint cnxn_id, 
			      GConfEntry *entry, gpointer user_data);

const char* prefs_get_default_device (void);

void device_changed_cb (GConfClient *client, guint cnxn_id, 
			GConfEntry *entry, gpointer user_data);
void profile_changed_cb (GConfClient *client, guint cnxn_id, 
			 GConfEntry *entry, gpointer user_data);

void http_proxy_setup (GConfClient *client);
void http_proxy_enable_changed_cb (GConfClient *client, guint cnxn_id, 
				   GConfEntry *entry, gpointer user_data);
void http_proxy_changed_cb (GConfClient *client, guint cnxn_id, 
			    GConfEntry *entry, gpointer user_data);
void http_proxy_port_changed_cb (GConfClient *client, guint cnxn_id, 
				 GConfEntry *entry, gpointer user_data);

void on_reread_activate (GtkWidget *button, gpointer user_data);
void on_submit_activate (GtkWidget *menuitem, gpointer user_data);
void on_genre_edit_changed(GtkEditable *widget, gpointer user_data);
void on_contents_activate(GtkWidget *button, gpointer user_data);

GtkWidget* sj_make_volume_button (void);

#endif /* SJ_MAIN_H */
