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
#include <libgnome/gnome-help.h>
#include "sj-extracting.h"
#include "cd-drive.h"
#include "bacon-cd-selection.h"

extern GladeXML *glade;
extern GtkWidget *main_window;

/* Store the toggle buttons pertaining to the format */
static GtkWidget *format_widgets[SJ_NUMBER_FORMATS];
static GtkWidget *prefs_dialog;
static GtkWidget *cd_option, *path_option, *file_option, *basepath_label, *check_strip, *check_eject;
static GtkWidget *path_example_label;
static GList *cdroms = NULL;

typedef struct {
  char* name;
  char* pattern;
} FilePattern;

static FilePattern path_patterns[] = {
  {N_("Album Artist, Album Title"), "%aa/%at"},
  {N_("Track Artist, Album Title"), "%ta/%at"},
  {N_("Album Title"), "%at"},
  {N_("Album Artist"), "%aa"},
  {N_("Album Artist - Album Title"), "%aa - %at"},
  {N_("[none]"), ""},
  {NULL, NULL}
};

static FilePattern file_patterns[] = {
  {N_("Number - Title"), "%tN - %tt"},
  {N_("Track Title"), "%tt"},
  {N_("Track Artist - Track Title"), "%ta - %tt"},
  {N_("Number. Track Artist - Track Title"), "%tN. %ta - %tt"},
  {N_("Number-Track Artist-Track Title (lowercase)"), "%tN-%tA-%tT"},
  {NULL, NULL}
};

const char* prefs_get_default_device ()
{
  static const char* default_device = NULL;
  if (default_device == NULL) {
    CDDrive *cd;
    cdroms = scan_for_cdroms (FALSE, FALSE);
    if (cdroms == NULL) return NULL;
    cd = cdroms->data;
    default_device = cd->device;
  }
  return default_device;
}

gboolean cd_drive_exists (const char *device)
{
  GList *l;
  if (device == NULL) return FALSE;
  for (l = cdroms; l != NULL; l = l->next) {
    if (strcmp (((CDDrive *) (l->data))->device, device) == 0)
      return TRUE;
  }
  return FALSE;
}

/**
 * Changed the CD-ROM device in the prefs dialog
 */
void prefs_cdrom_changed_cb (GtkWidget *widget, const char* device)
{
  gconf_client_set_string (gconf_client, GCONF_DEVICE, device, NULL);
}

/**
 * The Eject when Finished check was toggled.
 */
void prefs_eject_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
  gconf_client_set_bool (gconf_client, GCONF_EJECT,
                         gtk_toggle_button_get_active (togglebutton),
                         NULL);
}

/**
 * Show the gnome help browser
 */
void show_help ()
{
  GError *error = NULL;

  gnome_help_display ("sound-juicer", "preferences", &error);
  if (error) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (prefs_dialog),
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     _("Could not display help for Sound Juicer\n"
                                       "%s"),
                                     error->message);

    gtk_widget_show_all (dialog);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy(dialog);
    g_error_free (error);
  }
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
  if (GTK_WIDGET (togglebutton) == format_widgets[SJ_FORMAT_VORBIS]) {
    format = "vorbis";
  } else if (GTK_WIDGET(togglebutton) == format_widgets[SJ_FORMAT_MPEG]) {
    format = "mpeg";
  } else if (GTK_WIDGET(togglebutton) == format_widgets[SJ_FORMAT_FLAC]) {
    format = "flac";
  } else if (GTK_WIDGET(togglebutton) == format_widgets[SJ_FORMAT_WAVE]) {
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
  int format;
  int i;
  g_assert (strcmp (entry->key, GCONF_FORMAT) == 0);
  if (!entry->value) return;
  value = g_ascii_strup (gconf_value_get_string (entry->value), -1);
  /* TODO: this line is pretty convoluted */
  enumvalue = g_enum_get_value_by_name (g_type_class_peek (encoding_format_get_type()), value);
  /* Fallback on vorbis if it's not found */
  if (enumvalue == NULL) {
    /* g_warning (_("Unknown format %s"), value); */
    format = 0;
  } else {
    format = enumvalue->value;
  }
  g_free (value);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (format_widgets[format]), TRUE);

  for (i = 0; i < SJ_NUMBER_FORMATS; i++) {
    gboolean supported;

    supported = sj_extractor_supports_format (i);
    gtk_widget_set_sensitive (format_widgets[i], supported);
  }
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
    base_path = g_get_home_dir ();
  } else {
    base_path = gconf_value_get_string (entry->value);
  }
  /* TODO: sanity check the path somewhat */
  /* TODO: use an ellipsising label? */
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

static void eject_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, gpointer user_data)
{
  gboolean value = FALSE;
  g_assert (strcmp (entry->key, GCONF_EJECT) == 0);
  if (entry->value != NULL) {
    value = gconf_value_get_bool (entry->value);
  }
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_eject), value);
}

static void pattern_label_update (void)
{
  char *file_pattern, *path_pattern;
  char *file_value, *path_value, *example;
  gboolean strip;
  static AlbumDetails sample_album = {
    N_("Album Title"),
    N_("Album Artist"),
    0,
    NULL
  };
  static TrackDetails sample_track = {
    &sample_album,
    9,
    N_("Track Title"),
    N_("Track Artist"),
    60
  };
  /* TODO: sucky. Replace with get-gconf-key-with-default mojo */
  file_pattern = gconf_client_get_string (gconf_client, GCONF_FILE_PATTERN, NULL);
  if (file_pattern == NULL) {
    file_pattern = g_strdup(file_patterns[0].pattern);
  }
  path_pattern = gconf_client_get_string (gconf_client, GCONF_PATH_PATTERN, NULL);
  if (path_pattern == NULL) {
    path_pattern = g_strdup(path_patterns[0].pattern);
  }
  strip = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_strip));

  file_value = filepath_parse_pattern (file_pattern, &sample_track);
  path_value = filepath_parse_pattern (path_pattern, &sample_track);

  example = g_build_filename (G_DIR_SEPARATOR_S, path_value, file_value, NULL);
  g_free (file_value);
  g_free (file_pattern);
  g_free (path_value);
  g_free (path_pattern);

  gtk_label_set_text (GTK_LABEL (path_example_label), example+1);
  g_free (example);
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
  pattern_label_update ();
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
  pattern_label_update ();
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
    item = gtk_menu_item_new_with_label (_(patterns[i].name));
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
  int rc;

  if (prefs_dialog == NULL) {
    prefs_dialog = glade_xml_get_widget (glade, "prefs_dialog");
    g_assert (prefs_dialog != NULL);
    gtk_window_set_transient_for (GTK_WINDOW (prefs_dialog), GTK_WINDOW (main_window));


    cd_option = glade_xml_get_widget (glade, "cd_option");
    basepath_label = glade_xml_get_widget (glade, "path_label");
    path_option = glade_xml_get_widget (glade, "path_option");
    file_option = glade_xml_get_widget (glade, "file_option");
    format_widgets[SJ_FORMAT_VORBIS] = glade_xml_get_widget (glade, "format_vorbis");
    format_widgets[SJ_FORMAT_MPEG] = glade_xml_get_widget (glade, "format_mpeg");
    format_widgets[SJ_FORMAT_FLAC] = glade_xml_get_widget (glade, "format_flac");
    format_widgets[SJ_FORMAT_WAVE] = glade_xml_get_widget (glade, "format_wave");
    check_strip = glade_xml_get_widget (glade, "check_strip");
    check_eject = glade_xml_get_widget (glade, "check_eject");
    path_example_label = glade_xml_get_widget (glade, "path_example_label");

    gtk_option_menu_set_menu (GTK_OPTION_MENU (path_option), generate_pattern_menu (path_patterns));
    g_signal_connect (path_option, "changed", G_CALLBACK (prefs_path_option_changed), NULL);
    gtk_option_menu_set_menu (GTK_OPTION_MENU (file_option), generate_pattern_menu (file_patterns));
    g_signal_connect (file_option, "changed", G_CALLBACK (prefs_file_option_changed), NULL);

    /* Connect to GConf to update the UI */
    gconf_client_notify_add (gconf_client, GCONF_DEVICE, device_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_BASEPATH, basepath_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FORMAT, format_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_STRIP, strip_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_EJECT, eject_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_PATH_PATTERN, path_pattern_changed_cb, NULL, NULL, NULL);
    gconf_client_notify_add (gconf_client, GCONF_FILE_PATTERN, file_pattern_changed_cb, NULL, NULL, NULL);
  }
  basepath_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_BASEPATH, NULL, TRUE, NULL), NULL);
  device_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_DEVICE, NULL, TRUE, NULL), NULL);
  format_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FORMAT, NULL, TRUE, NULL), NULL);
  strip_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_STRIP, NULL, TRUE, NULL), NULL);
  eject_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_EJECT, NULL, TRUE, NULL), NULL);
  file_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_FILE_PATTERN, NULL, TRUE, NULL), NULL);
  path_pattern_changed_cb (gconf_client, -1, gconf_client_get_entry (gconf_client, GCONF_PATH_PATTERN, NULL, TRUE, NULL), NULL);

  gtk_widget_show_all (prefs_dialog);

  while (1) {
      rc = gtk_dialog_run (GTK_DIALOG (prefs_dialog));

      if (rc == GTK_RESPONSE_HELP)
	  show_help();
      else if (rc == GTK_RESPONSE_CLOSE || rc == GTK_RESPONSE_DELETE_EVENT)
	  break;
      }
      
  gtk_widget_hide (prefs_dialog);
}
