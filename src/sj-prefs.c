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

extern GladeXML *glade;
extern GtkWidget *main_window;

static GtkWidget *format_vorbis, *format_mpeg, *format_flac, *format_wave;
static GtkWidget *cd_option, *path_option, *file_option, *basepath_label, *check_strip;

typedef struct {
  char* name;
  char* pattern;
} FilePattern;

static FilePattern path_patterns[] = {
  {N_("Album Artist, Album Title"), "%aa/%at"},
  {N_("Track Artist, Album Title"), "%ta/%at"},
  {N_("Album Title"), "%at"},
  {N_("Album Artist"), "%aa"},
  {NULL, NULL}
};

static FilePattern file_patterns[] = {
  {N_("Number - Title"), "%tn - %tt"},
  {N_("Track Title"), "%tt"},
  {N_("Track Artist - Track Title"), "%ta - %tt"},
  {NULL, NULL}
};

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

  filesel = gtk_file_selection_new(_("Select Output Location"));
  gtk_file_selection_set_filename (GTK_FILE_SELECTION (filesel), base_path);
  gtk_widget_show_all (filesel);
  /* A little hacky, but it works :) */
  gtk_widget_hide (gtk_widget_get_parent (GTK_FILE_SELECTION(filesel)->file_list));
  res = gtk_dialog_run (GTK_DIALOG (filesel));
  if (res == GTK_RESPONSE_OK) {
    const char *filename;
    char *path;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
    path = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL); /* TODO: GError */
    gconf_client_set_string (gconf_client, GCONF_BASEPATH, path, NULL);
    g_free (path);
  }
  gtk_widget_destroy (filesel);
}

void prefs_path_option_changed (GtkOptionMenu *optionmenu, gpointer user_data)
{
  int history;
  const char* pattern;
  history = gtk_option_menu_get_history (optionmenu);
  pattern = path_patterns[history].pattern;
  if (pattern) {
    gconf_client_set_string (gconf_client, GCONF_PATH_PATTERN, pattern, NULL);
  }
}

void prefs_file_option_changed (GtkOptionMenu *optionmenu, gpointer user_data)
{
  int history;
  const char* pattern;
  history = gtk_option_menu_get_history (optionmenu);
  pattern = file_patterns[history].pattern;
  if (pattern) {
    gconf_client_set_string (gconf_client, GCONF_FILE_PATTERN, pattern, NULL);
  }
}

/**
 * The Strip Special Characters check was toggled.
 */
void prefs_strip_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
  gconf_client_set_bool (gconf_client, GCONF_STRIP,
                         gtk_toggle_button_get_active (togglebutton),
                         NULL);
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
  char* value;
  GEnumValue* enumvalue;
  g_assert (strcmp (entry->key, GCONF_FORMAT) == 0);
  if (!entry->value) return;
  value = g_ascii_strup (gconf_value_get_string (entry->value), -1);
  /* TODO: this line is pretty convoluted */
  enumvalue = g_enum_get_value_by_name (g_type_class_peek (encoding_format_get_type()), value);
  if (enumvalue == NULL) {
    g_warning (_("Unknown format %s"), value);
    g_free (value);
    return;
  }
  switch (enumvalue->value) {
  case VORBIS:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (format_vorbis), TRUE);
    break;
  case MPEG:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (format_mpeg), TRUE);
    break;
  case FLAC:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (format_flac), TRUE);
    break;
  case WAVE:
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (format_wave), TRUE);
    break;
  }
  g_free (value);
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

static void strip_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  gboolean value = FALSE;
  g_assert (strcmp (entry->key, GCONF_STRIP) == 0);
  if (entry->value != NULL) {
    value = gconf_value_get_bool (entry->value);
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_strip), value);
}

static void path_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  char *value;
  int i = 0;
  g_assert (strcmp (entry->key, GCONF_PATH_PATTERN) == 0);
  if (entry->value == NULL) {
    value = g_strdup (path_patterns[0].pattern);
  } else {
    value = g_strdup (gconf_value_get_string (entry->value));
  }
  while (path_patterns[i].pattern && strcmp(path_patterns[i].pattern, value) != 0) {
    i++;
  }
  g_free (value);
  gtk_option_menu_set_history (GTK_OPTION_MENU (path_option), i);
}

static void file_pattern_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  char *value;
  int i = 0;
  g_assert (strcmp (entry->key, GCONF_FILE_PATTERN) == 0);
  if (entry->value == NULL) {
    value = g_strdup (file_patterns[0].pattern);
  } else {
    value = g_strdup (gconf_value_get_string (entry->value));
  }
  while (file_patterns[i].pattern && strcmp(file_patterns[i].pattern, value) != 0) {
    i++;
  }
  g_free (value);
  gtk_option_menu_set_history (GTK_OPTION_MENU (file_option), i);
}

/**
 * Given a FilePattern array, generate a menu of them items
 */
static GtkWidget *generate_pattern_menu (FilePattern *patterns)
{
  int i;
  GtkWidget *menu, *item;
  menu = gtk_menu_new ();
  
  for (i = 0; patterns[i].pattern; ++i) {
    item = gtk_menu_item_new_with_label (patterns[i].name);
    g_object_set_data (G_OBJECT (item), "pattern", patterns[i].pattern);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  }
  return menu;
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
    path_option = glade_xml_get_widget (glade, "path_option");
    file_option = glade_xml_get_widget (glade, "file_option");
    format_vorbis = glade_xml_get_widget (glade, "format_vorbis");
    format_mpeg = glade_xml_get_widget (glade, "format_mpeg");
    format_flac = glade_xml_get_widget (glade, "format_flac");
    format_wave = glade_xml_get_widget (glade, "format_wave");
    check_strip = glade_xml_get_widget (glade, "check_strip");

    gtk_option_menu_set_menu (GTK_OPTION_MENU (path_option), generate_pattern_menu (path_patterns));
    gtk_option_menu_set_menu (GTK_OPTION_MENU (file_option), generate_pattern_menu (file_patterns));
    
    /* Connect to GConf to update the UI */
    gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_BASEPATH, basepath_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FORMAT, format_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_STRIP, strip_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
  }
  /*
   * TODO: this is pretty sick. Need another way of doing this --
   * maybe seperate the gconf callback from the actual key setting
   * code?
   */
  basepath_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEPATH, NULL, TRUE, NULL), NULL);
  device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);
  format_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FORMAT, NULL, TRUE, NULL), NULL);
  strip_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_STRIP, NULL, TRUE, NULL), NULL);

  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);
}
