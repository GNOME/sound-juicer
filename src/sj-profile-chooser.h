/* audio-profile-choose.h: combo box to choose a specific profile */

/*
 * Copyright (C) 2003 Thomas Vander Stichele
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef SJ_AUDIO_PROFILE_CHOOSE_H
#define SJ_AUDIO_PROFILE_CHOOSE_H

#include <profiles/audio-profile.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <gtk/gtkcombobox.h>

G_BEGIN_DECLS

/* create a new Profile Choose Dialog */
GtkWidget*      sj_audio_profile_choose_new		(void);
GMAudioProfile* sj_audio_profile_choose_get_active	(GtkWidget  *choose);
gboolean        sj_audio_profile_choose_set_active	(GtkWidget  *choose,
							 const char *id);

G_END_DECLS

#endif /* SJ_AUDIO_PROFILE_CHOOSE_H */
