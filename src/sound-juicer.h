/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sound-juicer.h
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

#ifndef SOUND_JUICER_H
#define SOUND_JUICER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <glade/glade-xml.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkliststore.h>
#include "sj-extractor.h"

/**
 * A GConf client
 */
extern GConfClient *gconf_client;

/**
 * The main window
 */
extern GtkWidget *main_window;

/**
 * The Glade UI file
 */
extern GladeXML *glade;

/**
 * The extractor GObject being used
 */
extern SjExtractor *extractor;

/**
 * Set if currently extracting
 */
extern gboolean extracting;

/**
 * The columns in the list view
 */
typedef enum {
  COLUMN_EXTRACT,
  COLUMN_NUMBER,
  COLUMN_TITLE,
  COLUMN_ARTIST,
  COLUMN_DURATION,
  COLUMN_DETAILS,
  COLUMN_TOTAL
} ViewColumn;

/**
 * The GtkTreeModel which all of the tracks are stored in
 */
extern GtkListStore *track_store;

/**
 * The device we are extracting from.
 */
extern const char *device;

/**
 * The root path to write files too
 */
extern const char *base_path;

/**
 * The pattern to expand when naming folders
 */
extern const char *path_pattern;

/**
 * The pattern to expand when naming files
 */
extern const char *file_pattern;

/**
 * If file names should be shell-friendly (i.e no [ /&*?\] characters
 */
extern gboolean strip_chars;

/**
 * If the CD-ROM should be ejected when it has been extracted.
 */
extern gboolean eject_finished;

/**
 * If we are in auto-start mode
 */
extern gboolean autostart;

/**
 * GConf key names
 */
#define GCONF_ROOT "/apps/sound-juicer"
#define GCONF_DEVICE GCONF_ROOT "/device"
#define GCONF_EJECT GCONF_ROOT "/eject"
#define GCONF_BASEPATH GCONF_ROOT "/base_path"
#define GCONF_FILE_PATTERN GCONF_ROOT "/file_pattern"
#define GCONF_PATH_PATTERN GCONF_ROOT "/path_pattern"
#define GCONF_FORMAT GCONF_ROOT "/format"
#define GCONF_PARANOIA GCONF_ROOT "/paranoia"
#define GCONF_STRIP GCONF_ROOT "/strip-special"
#define GCONF_MUSICBRAINZ_SERVER GCONF_ROOT "/musicbrainz_server"

#define GCONF_PROXY_ROOT "/system/http_proxy"
#define GCONF_HTTP_PROXY_ENABLE GCONF_PROXY_ROOT "/use_http_proxy"
#define GCONF_HTTP_PROXY GCONF_PROXY_ROOT "/host"
#define GCONF_HTTP_PROXY_PORT GCONF_PROXY_ROOT "/port"

#endif /* SOUND_JUICER_H */
