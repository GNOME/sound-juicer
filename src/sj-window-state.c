/*
 * Copyright (C) 2014 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-window-state.c
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
 */

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "sj-window-state.h"

#define SJ_WINDOW_SECTION_NAME "window"
#define SJ_WINDOW_WIDTH_KEY "width"
#define SJ_WINDOW_HEIGHT_KEY "heigth"
#define SJ_WINDOW_STATE_KEY "state"

enum {
  SJ_WINDOW_STATE_NORMAL,
  SJ_WINDOW_STATE_MAXIMIZED,
  SJ_WINDOW_STATE_FULLSCREEN
};

typedef struct
{
  gint height;
  gint width;
  gint state;
} SjWindowState;

static gchar*
get_state_file_name (void)
{
  return g_build_filename (g_get_user_config_dir (),
			   "sound-juicer",
			   "saved-state",
			   NULL);
}

static GKeyFile*
open_state_file (void)
{
  GError *err = NULL;
  GKeyFile *file = g_key_file_new ();
  gchar *filename = get_state_file_name ();
  if (!g_key_file_load_from_file (file,
                                  filename,
                                  G_KEY_FILE_KEEP_COMMENTS,
                                  &err)) {
    g_warning ("Unable to open '%s' - %s", filename, err->message);
    g_error_free (err);
  }
  g_free (filename);

  return file;
}

static void
save_state_file (GKeyFile *file)
{
  GError *err = NULL;
  gchar *filename = get_state_file_name ();
  gchar *pathname = g_path_get_dirname (filename);
  gchar *contents = g_key_file_to_data (file, NULL, NULL);
  if (g_mkdir_with_parents (pathname, 0755) == 0) {
    if (!g_file_set_contents (filename, contents, -1, &err)) {
      g_warning ("Unable to save '%s' - %s", filename, err->message);
      g_error_free (err);
    }
  } else {
    g_warning ("Unable to save '%s' as the directory '%s' could not be created",
               filename, pathname);
  }
  g_free (contents);
  g_free (pathname);
  g_free (filename);
}

static SjWindowState*
get_window_state (GKeyFile *file)
{
  SjWindowState *state = g_slice_new0 (SjWindowState);
  state->width = g_key_file_get_integer (file,
                                         SJ_WINDOW_SECTION_NAME,
                                         SJ_WINDOW_WIDTH_KEY,
                                         NULL);
  state->height = g_key_file_get_integer (file,
                                          SJ_WINDOW_SECTION_NAME,
                                          SJ_WINDOW_HEIGHT_KEY,
                                          NULL);
  state->state = g_key_file_get_integer (file,
                                         SJ_WINDOW_SECTION_NAME,
                                         SJ_WINDOW_STATE_KEY,
                                         NULL);
  if (state->state > SJ_WINDOW_STATE_FULLSCREEN)
    state->state = 0;

  return state;
}

static void
set_window_state (GKeyFile      *file,
                  SjWindowState *state)
{
  g_key_file_set_integer (file,
                          SJ_WINDOW_SECTION_NAME,
                          SJ_WINDOW_WIDTH_KEY,
                          state->width);
  g_key_file_set_integer (file,
                          SJ_WINDOW_SECTION_NAME,
                          SJ_WINDOW_HEIGHT_KEY,
                          state->height);
  g_key_file_set_integer (file,
                          SJ_WINDOW_SECTION_NAME,
                          SJ_WINDOW_STATE_KEY,
                          state->state);
}

static SjWindowState*
load_window_state (void)
{
  SjWindowState *state;
  GKeyFile *file = open_state_file ();
  state = get_window_state (file);
  g_key_file_unref (file);
  return state;
}

static void
save_window_state (SjWindowState *state)
{
  GKeyFile *file = g_key_file_new ();
  set_window_state (file, state);
  save_state_file (file);
  g_key_file_unref (file);
}

static void
apply_window_state (GtkWindow     *window,
                    SjWindowState *state)
{
  if (state->width > 0 && state->height > 0)
    gtk_window_set_default_size (window, state->width, state->height);
  if (state->state == SJ_WINDOW_STATE_FULLSCREEN)
    gtk_window_fullscreen (window);
  else if (state->state == SJ_WINDOW_STATE_MAXIMIZED)
    gtk_window_maximize (window);
}

static gboolean
configure_event_cb (GtkWidget *widget,
                    GdkEvent  *event,
                    gpointer   user_data)
{
  SjWindowState *state = user_data;

  g_return_val_if_fail (event->type == GDK_CONFIGURE, FALSE);

  if (state->state == SJ_WINDOW_STATE_NORMAL) {
    GdkEventConfigure *e = (GdkEventConfigure*) event;
    state->width = e->width;
    state->height = e->height;
  }

  return FALSE;
}

static gboolean
window_state_event_cb (GtkWidget *widget,
                       GdkEvent  *event,
                       gpointer   user_data)
{
  SjWindowState *state = user_data;
  GdkEventWindowState *e;

  g_return_val_if_fail (event->type == GDK_WINDOW_STATE, FALSE);

  e = (GdkEventWindowState*) event;
  if ((e->new_window_state & GDK_WINDOW_STATE_FULLSCREEN) != 0)
    state->state = SJ_WINDOW_STATE_FULLSCREEN;
  else if ((e->new_window_state & GDK_WINDOW_STATE_MAXIMIZED) != 0)
    state->state = SJ_WINDOW_STATE_MAXIMIZED;
  else
    state->state = SJ_WINDOW_STATE_NORMAL;

  return FALSE;
}

static void
destroy_cb (GtkWidget *window, gpointer user_data)
{
  SjWindowState *state = user_data;
  save_window_state (state);
  g_signal_handlers_disconnect_by_data (window, state);
  g_slice_free (SjWindowState, state);
}

void
sj_main_window_init (GtkWindow* window)
{
  SjWindowState *state = load_window_state ();
  apply_window_state (window, state);
  g_signal_connect (window, "configure-event",
                    G_CALLBACK (configure_event_cb), state);
  g_signal_connect (window, "window-state-event",
                    G_CALLBACK (window_state_event_cb), state);
  g_signal_connect (window, "destroy",
                    G_CALLBACK (destroy_cb), state);
}
