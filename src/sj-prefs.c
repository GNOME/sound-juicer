/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-about.c
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

#include "sound-juicer.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade-xml.h>
#include <gconf/gconf-client.h>
#include "cd-drive.h"
#include "bacon-cd-selection.h"

#define GCONF_ROOT "/apps/sound-juicer"
#define GCONF_DEVICE GCONF_ROOT "/device"
#define GCONF_BASEPATH GCONF_ROOT "/base_path"
#define GCONF_FORMAT GCONF_ROOT "/format"

extern GladeXML *glade;
extern GtkWidget *main_window;

static GConfClient *gconf_client;
static GtkWidget *format_vorbis, *format_mpeg, *format_flac, *format_wave;
static GtkWidget *cd_option, *basepath_label;

const char* prefs_get_default_device ()
{
  static const char* default_device = NULL;
  if (default_device == NULL) {
    CDDrive *cd;
    GList *cdroms = NULL;
    cdroms = scan_for_cdroms (FALSE, FALSE);
    if (cdroms == NULL) return NULL;
    cd = cdroms->data;
    default_device = g_strdup (cd->device);
    while (cdroms != NULL) {
      CDDrive *cdrom = cdroms->data;
      cd_drive_free (cdrom);
      cdroms = g_list_remove (cdroms, cdrom);
      g_free (cdrom);    
    }
  }
  return default_device;
}

/**
 * Changed the CD-ROM device in the prefs dialog
 */
void prefs_cdrom_changed_cb (GtkWidget *widget, const char* device)
{
  gconf_client_set_string (gconf_client, GCONF_DEVICE, device, NULL);
}

/**
 * Clicked on Browse in the Prefs dialog
 */
void prefs_browse_clicked (GtkButton *button, gpointer user_data)
{
  GtkWidget *filesel;
  int res;

  filesel = gtk_file_selection_new(_("Output Location"));
  gtk_widget_show_all (filesel);
  /* A little hacky, but it works :) */
  gtk_widget_hide (gtk_widget_get_parent (GTK_FILE_SELECTION(filesel)->file_list));
  res = gtk_dialog_run (GTK_DIALOG (filesel));
  if (res == GTK_RESPONSE_OK) {
    const char *filename;
    char *path;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
    path = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL); /* TODO: GError */
    gconf_client_set_string (gconf_client, GCONF_BASEPATH, path, NULL); /* TODO: GError */
    g_free (path);
  }
  gtk_widget_destroy (filesel);
}


/**
 * One of the format toggle buttons in the prefs dialog has been
 * toggled.
 */
void on_format_toggled (GtkToggleButton *togglebutton,
                               gpointer user_data)
{
  const char* format;
  if (!gtk_toggle_button_get_active (togglebutton)) {
    return;
  }
  if (GTK_WIDGET (togglebutton) == format_vorbis) {
    format = "vorbis";
  } else if (GTK_WIDGET(togglebutton) == format_mpeg) {
    format = "mpeg";
  } else if (GTK_WIDGET(togglebutton) == format_flac) {
    format = "flac";
  } else if (GTK_WIDGET(togglebutton) == format_wave) {
    format = "wave";
  } else {
    return;
  }
  gconf_client_set_string (gconf_client, GCONF_FORMAT, format, NULL); /* TODO: GError */
}

static void device_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_DEVICE) == 0);

  if (entry->value == NULL) {
    bacon_cd_selection_set_device (BACON_CD_SELECTION (cd_option),
                                   bacon_cd_selection_get_default_device (BACON_CD_SELECTION (cd_option)));
  } else {
    bacon_cd_selection_set_device (BACON_CD_SELECTION (cd_option),
                                   gconf_value_get_string (entry->value));
  }
}

static void format_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  g_assert (strcmp (entry->key, GCONF_FORMAT) == 0);
  g_message ("%s\n", __FUNCTION__);
}

static void basepath_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  /*
   * The conflict between separation of the prefs and the main window,
   * and the problem of duplication, is very clear here. This code is
   * identical to the code in the main key listener, but modifying the
   * prefs window from the main is ugly. Maybe put the validation code
   * into sj-utils?
   */
  const char* base_path;
  g_assert (strcmp (entry->key, GCONF_BASEPATH) == 0);
  if (entry->value == NULL) {
    base_path = g_strdup (g_get_home_dir ());
  } else {
    base_path = gconf_value_get_string (entry->value);
  }
  /* TODO: sanity check the path somewhat */
  /* TODO: use an elipsising label? */
  gtk_label_set_text (GTK_LABEL (basepath_label), base_path);
}

/**
 * Clicked on Preferences in the UI
 */
void on_edit_preferences_cb (GtkMenuItem *item, gpointer user_data)
{
  static GtkWidget *dialog;
  if (dialog == NULL) {
    dialog = glade_xml_get_widget (glade, "prefs_dialog");
    g_assert (dialog != NULL);
    gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (main_window));

    cd_option = glade_xml_get_widget (glade, "cd_option");
    basepath_label = glade_xml_get_widget (glade, "path_label");
    format_vorbis = glade_xml_get_widget (glade, "format_vorbis");
    format_mpeg = glade_xml_get_widget (glade, "format_mpeg");
    format_flac = glade_xml_get_widget (glade, "format_flac");
    format_wave = glade_xml_get_widget (glade, "format_wave");

    /* Connect to GConf to update the UI */
    gconf_client = gconf_client_get_default ();
    gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_BASEPATH, basepath_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FORMAT, format_changed_cb, NULL, NULL, NULL);
  }
  /*
   * TODO: this is pretty sick. Need another way of doing this --
   * maybe seperate the gconf callback from the actual key setting
   * code?
   */
  basepath_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEPATH, NULL, TRUE, NULL), NULL);
  device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);
  format_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FORMAT, NULL, TRUE, NULL), NULL);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);
}
