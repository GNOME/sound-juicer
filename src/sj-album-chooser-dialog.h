/*
 * Copyright (C) 2016 Phillip Wood <phillip.wood@dunelm.org.uk>
 *
 * Sound Juicer - sj-album-chooser-dialog.h
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
 * Authors: Phillip Wood <phillip.wood@dunelm.org.uk>
 */

#ifndef SJ_ALBUM_CHOOSER_DIALOG_H
#define SJ_ALBUM_CHOOSER_DIALOG_H

#include <gtk/gtk.h>
#include "sj-structures.h"

G_BEGIN_DECLS

#define SJ_TYPE_ALBUM_CHOOSER_DIALOG (sj_album_chooser_dialog_get_type ())
#define SJ_ALBUM_CHOOSER_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), SJ_TYPE_ALBUM_CHOOSER_DIALOG, SjAlbumChooserDialog))
#define SJ_ALBUM_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), SJ_TYPE_ALBUM_CHOOSER_DIALOG, SjAlbumChooserDialogClass))
#define SJ_IS_ALBUM_CHOOSER_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SJ_TYPE_ALBUM_CHOOSER_DIALOG))
#define SJ_IS_ALBUM_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SJ_TYPE_ALBUM_CHOOSER_DIALOG))
#define SJ_ALBUM_CHOOSER_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), SJ_TYPE_ALBUM_CHOOSER_DIALOG, SjAlbumChooserDialogClass))

typedef struct _SjAlbumChooserDialog SjAlbumChooserDialog;
typedef struct _SjAlbumChooserDialogClass SjAlbumChooserDialogClass;

GType sj_album_chooser_dialog_get_type (void) G_GNUC_CONST;
GtkWidget* sj_album_chooser_dialog_new (GtkWindow *parent, GList *albums);
AlbumDetails* sj_album_chooser_dialog_get_selected_album (SjAlbumChooserDialog *dialog);

G_END_DECLS

#endif /* SJ_ALBUM_CHOOSER_DIALOG_H */
