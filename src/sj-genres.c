/*
 * Copyright (C) 2007 Jonh Wendell <wendell@bani.com.br>
 *
 * Sound Juicer - sj-genres.c
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
 * Authors: Jonh Wendell <wendell@bani.com.br>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "sj-genres.h"

static const char* const known_genres[] = {
  N_("Ambient"),
  N_("Blues"),
  N_("Classical"),
  N_("Country"),
  N_("Dance"),
  N_("Electronica"),
  N_("Folk"),
  N_("Funk"),
  N_("Jazz"),
  N_("Latin"),
  N_("Pop"),
  N_("Rap"),
  N_("Reggae"),
  N_("Rock"),
  N_("Soul"),
  N_("Spoken Word")
};

static gboolean is_known_genre (const char *str) {
  guint i;

  for (i = 0; i < G_N_ELEMENTS(known_genres); i++) {
    if (strcasecmp (str, known_genres[i]) == 0)
      return TRUE;
  }
  return FALSE;
}

static gboolean in_array (const char *str, const char** array) {
  const char **list = array;
  gboolean found = FALSE;

  while (*list != NULL)
    if (strcasecmp (str, *list++) == 0)
      return TRUE;

  return found;
}

static char* genre_filename (void) {
  return g_build_filename (g_get_user_config_dir (),
			   "sound-juicer",
			   "genres",
			   NULL);
}

static char** saved_genres (void) {
  char *filename, *file_contents = NULL;
  char **genres_from_file = NULL;
  gboolean success;
  int len;

  filename = genre_filename ();
  success = g_file_get_contents (filename,
				 &file_contents,
				 NULL,
				 NULL);

  g_free (filename);

  if (success) {
    genres_from_file = g_strsplit (file_contents, "\n", 0);
    len = g_strv_length (genres_from_file);
    if (len >= 1) {
      if (strlen (genres_from_file[len-1]) == 0) {
        g_free (genres_from_file[len-1]);
        genres_from_file[len-1] = NULL;
      }
    }

    g_free (file_contents);
  }

  return genres_from_file;
}

static GtkTreeModel* create_genre_list (void) {
  GtkListStore *store;
  guint i;
  char **genres;

  store = gtk_list_store_new (1, G_TYPE_STRING);

  for (i = 0; i < G_N_ELEMENTS(known_genres); i++) {
    gtk_list_store_insert_with_values (store, NULL, -1,
                                       0, _(known_genres[i]),
                                       -1);
  }

  genres = saved_genres ();
  if (genres) {
    for (i = 0; genres[i] != NULL; i++) {
      gtk_list_store_insert_with_values (store, NULL, -1,
                                         0, genres[i],
                                         -1);
    }

    g_strfreev (genres);
  }

  return GTK_TREE_MODEL (store);
}

void setup_genre_entry (GtkWidget *entry) {
  GtkEntryCompletion *completion;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  completion = gtk_entry_get_completion (GTK_ENTRY (entry));
  if (completion != NULL)
    g_object_unref (completion);

  completion = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (completion, create_genre_list ());
  gtk_entry_completion_set_text_column (completion, 0);
  gtk_entry_completion_set_inline_completion (completion, TRUE);
  gtk_entry_set_completion (GTK_ENTRY (entry), completion);
}

void save_genre (GtkWidget *entry) {
  const char *genre;
  char **genres;
  int len;
  char *content, *filename, *path;
  GError *error = NULL;

  g_return_if_fail (GTK_IS_ENTRY (entry));

  genre = gtk_entry_get_text (GTK_ENTRY (entry));

  if (is_known_genre (genre))
    return;

  len = 0;
  genres = saved_genres ();
  if (genres) {
    if (in_array ((const char *)genre, (const char **) genres)) {
      g_strfreev (genres);
      return;
    }
    len = g_strv_length (genres);
  }

  genres = realloc (genres, (len + 2) * sizeof (char *));
  genres[len] = g_strjoin (NULL, genre, "\n", NULL);
  genres[len+1] = NULL;

  content = g_strjoinv ("\n", genres);
  filename = genre_filename ();

  path = g_path_get_dirname (filename);
  g_mkdir_with_parents (path, 0755);
  g_free (path);

  g_file_set_contents (filename,
		       content,
		       -1,
		       &error);

  g_free (filename);
  g_free (content);
  g_strfreev (genres);

  if (error) {
    g_warning (_("Error while saving custom genre: %s"), error->message);
    g_error_free (error);
  }

  setup_genre_entry (entry);
}
