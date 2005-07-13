/* Sj Volume Button / popup widget
 * (c) copyright 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
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
 *
 * The Sj project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Sj. This
 * permission are above and beyond the permissions granted by the GPL license
 * Sj is covered by.
 */

#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <gtk/gtkbutton.h>
#include <gtk/gtkicontheme.h>

G_BEGIN_DECLS

#define SJ_TYPE_VOLUME_BUTTON \
  (sj_volume_button_get_type ())
#define SJ_VOLUME_BUTTON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SJ_TYPE_VOLUME_BUTTON, \
			       SjVolumeButton))

typedef struct _SjVolumeButton {
  GtkButton parent;

  /* popup */
  GtkWidget *dock, *scale, *image, *plus, *min;
  gint click_id;
  gfloat direction;
  gboolean timeout;
  guint32 pop_time;
  GtkIconTheme *theme;
} SjVolumeButton;

typedef struct _SjVolumeButtonClass {
  GtkButtonClass parent_class;

  /* signals */
  void	(* value_changed)	(SjVolumeButton * button);

  gpointer __bla[4];
} SjVolumeButtonClass;

GType		sj_volume_button_get_type	(void);

GtkWidget *	sj_volume_button_new		(float min, float max,
						 float step);
float		sj_volume_button_get_value	(SjVolumeButton * button);
void		sj_volume_button_set_value	(SjVolumeButton * button,
						 float value);

G_END_DECLS

#endif /* __BUTTON_H__ */
