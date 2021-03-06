/*
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-play.h
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifndef SJ_PLAY_H
#define SJ_PLAY_H

#include <gtk/gtk.h>
#include "sj-structures.h"

void sj_play_init (void);

void stop_playback	(void);

G_MODULE_EXPORT void on_tracklist_row_activate (GtkTreeView *treeview, GtkTreePath *path,
                                GtkTreeViewColumn *col, gpointer user_data);

void stop_ui_hack (void);

void toggle_play (void);

void play_next_track (void);

void play_previous_track (void);

G_MODULE_EXPORT void on_tracklist_row_selected (GtkTreeView *treeview,
		                gpointer user_data);

G_MODULE_EXPORT void on_volume_changed (GtkWidget* volb, gdouble value, gpointer data);

G_MODULE_EXPORT gboolean on_seek_press (GtkWidget * scale,
			GdkEvent * event,
			gpointer user_data);

G_MODULE_EXPORT void on_seek_moved (GtkWidget * scale, gpointer user_data);

G_MODULE_EXPORT gboolean on_seek_release (GtkWidget * scale,
			  GdkEvent * event,
			  gpointer user_data);

#endif /* SJ_PLAY_H_H */
