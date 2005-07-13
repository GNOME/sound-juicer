/* Volume Button / popup widget
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "sj-volume.h"

#define SCALE_SIZE 100
#define CLICK_TIMEOUT 250

enum {
  SIGNAL_VALUE_CHANGED,
  NUM_SIGNALS
};

static void	sj_volume_button_class_init	(SjVolumeButtonClass * klass);
static void	sj_volume_button_init	(SjVolumeButton * button);
static void	sj_volume_button_dispose	(GObject        * object);

static gboolean	sj_volume_button_scroll	(GtkWidget      * widget,
						 GdkEventScroll * event);
static gboolean	sj_volume_button_press	(GtkWidget      * widget,
						 GdkEventButton * event);
static gboolean cb_dock_press			(GtkWidget      * widget,
						 GdkEventButton * event,
						 gpointer         data);

static gboolean cb_button_press			(GtkWidget      * widget,
						 GdkEventButton * event,
						 gpointer         data);
static gboolean cb_button_release		(GtkWidget      * widget,
						 GdkEventButton * event,
						 gpointer         data);

/* see below for scale definitions */
static GtkWidget *sj_volume_scale_new	(SjVolumeButton * button,
						 float min, float max,
						 float step);

static GtkButtonClass *parent_class = NULL;
static guint signals[NUM_SIGNALS] = { 0 };

GType
sj_volume_button_get_type (void)
{
  static GType sj_volume_button_type = 0;

  if (!sj_volume_button_type) {
    static const GTypeInfo sj_volume_button_info = {
      sizeof (SjVolumeButtonClass),
      NULL,
      NULL,
      (GClassInitFunc) sj_volume_button_class_init,
      NULL,
      NULL,
      sizeof (SjVolumeButton),
      0,
      (GInstanceInitFunc) sj_volume_button_init,
      NULL
    };

    sj_volume_button_type =
	g_type_register_static (GTK_TYPE_BUTTON, 
				"SjVolumeButton",
				&sj_volume_button_info, 0);
  }

  return sj_volume_button_type;
}

static void
sj_volume_button_class_init (SjVolumeButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);

  parent_class = g_type_class_ref (GTK_TYPE_BUTTON);

  /* events */
  gobject_class->dispose = sj_volume_button_dispose;
  gtkwidget_class->button_press_event = sj_volume_button_press;
  gtkwidget_class->scroll_event = sj_volume_button_scroll;

  /* signals */
  signals[SIGNAL_VALUE_CHANGED] = g_signal_new ("value-changed",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET (SjVolumeButtonClass, value_changed),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
sj_volume_button_init (SjVolumeButton *button)
{
  button->timeout = FALSE;
  button->click_id = 0;
  button->dock = button->scale = NULL;
  button->theme = gtk_icon_theme_get_default ();
}

static void
sj_volume_button_dispose (GObject *object)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (object);

  if (button->dock) {
    gtk_widget_destroy (button->dock);
    button->dock = NULL;
  }

  if (button->theme) {
    g_object_unref (G_OBJECT (button->theme));
    button->theme = NULL;
  }

  if (button->click_id != 0) {
    g_source_remove (button->click_id);
    button->click_id = 0;
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

/*
 * public API.
 */

GtkWidget *
sj_volume_button_new (float min, float max,
			 float step)
{
  SjVolumeButton *button;
  GtkWidget *frame, *box;

  button = g_object_new (SJ_TYPE_VOLUME_BUTTON, NULL);
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);

  /* image */
  button->image = gtk_image_new ();
  gtk_container_add (GTK_CONTAINER (button), button->image);
  gtk_widget_show_all (button->image);

  /* window */
  button->dock = gtk_window_new (GTK_WINDOW_POPUP);
  g_signal_connect (button->dock, "button-press-event",
		    G_CALLBACK (cb_dock_press), button);
  gtk_window_set_decorated (GTK_WINDOW (button->dock), FALSE);

  /* frame */
  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_OUT);
  gtk_container_add (GTK_CONTAINER (button->dock), frame);
  box = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (frame), box);

  /* + */
  button->plus = gtk_button_new_with_label (_("+"));
  gtk_button_set_relief (GTK_BUTTON (button->plus), GTK_RELIEF_NONE);
  g_signal_connect (button->plus, "button-press-event",
		    G_CALLBACK (cb_button_press), button);
  g_signal_connect (button->plus, "button-release-event",
		    G_CALLBACK (cb_button_release), button);
  gtk_box_pack_start (GTK_BOX (box), button->plus, TRUE, FALSE, 0);

  /* scale */
  button->scale = sj_volume_scale_new (button, min, max, step);
  gtk_widget_set_size_request (button->scale, -1, SCALE_SIZE);
  gtk_scale_set_draw_value (GTK_SCALE (button->scale), FALSE);
  gtk_range_set_inverted (GTK_RANGE (button->scale), TRUE);
  gtk_box_pack_start (GTK_BOX (box), button->scale, TRUE, FALSE, 0);

  /* - */
  button->min = gtk_button_new_with_label (_("-"));
  gtk_button_set_relief (GTK_BUTTON (button->min), GTK_RELIEF_NONE);
  g_signal_connect (button->min, "button-press-event",
		   G_CALLBACK (cb_button_press), button);
  g_signal_connect (button->min, "button-release-event",
		    G_CALLBACK (cb_button_release), button);
  gtk_box_pack_start (GTK_BOX (box), button->min, TRUE, FALSE, 0);

  return GTK_WIDGET (button);
}

float
sj_volume_button_get_value (SjVolumeButton * button)
{
  g_return_val_if_fail (button != NULL, 0);

  return gtk_range_get_value (GTK_RANGE (button->scale));
}

void
sj_volume_button_set_value (SjVolumeButton * button,
			       float value)
{
  g_return_if_fail (button != NULL);

  gtk_range_set_value (GTK_RANGE (button->scale), value);
}

/*
 * button callbacks.
 */

static gboolean
sj_volume_button_scroll (GtkWidget      * widget,
			    GdkEventScroll * event)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (widget);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  float d;

  if (event->type != GDK_SCROLL)
    return FALSE;

  d = sj_volume_button_get_value (button);
  if (event->direction == GDK_SCROLL_UP) {
    d += .1;
    if (d > adj->upper)
      d = adj->upper;
  } else {
    d -= .1;
    if (d < adj->lower)
      d = adj->lower;
  }
  sj_volume_button_set_value (button, d);

  return TRUE;
}

static gboolean
sj_volume_button_press (GtkWidget      * widget,
			   GdkEventButton * event)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (widget);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  gfloat v;
  gint x, y, m, dx, dy, sx, sy, ystartoff;
  GdkEventButton *e;

  /* position roughly */
  gdk_window_get_origin (widget->window, &x, &y);
  gtk_window_move (GTK_WINDOW (button->dock), x, y - 50);
  gtk_widget_show_all (button->dock);
  gdk_window_get_origin (button->dock->window, &dx, &dy);
  dy += button->dock->allocation.y;
  gdk_window_get_origin (button->scale->window, &sx, &sy);
  sy += button->scale->allocation.y;
  ystartoff = sy - dy;
  button->timeout = TRUE;

  /* position (needs widget to be shown already) */
  v = sj_volume_button_get_value (button);
  x += widget->allocation.x;
  x += (widget->allocation.width - button->dock->allocation.width) / 2;
  y += widget->allocation.y;
  y -= ystartoff;
  m = button->scale->allocation.height - widget->allocation.height;
  y -= m;
  y += m * v / adj->upper;
  gtk_window_move (GTK_WINDOW (button->dock), x, y);
  gdk_window_get_origin (button->scale->window, &sx, &sy);

  GTK_WIDGET_CLASS (parent_class)->button_press_event (widget, event);

  /* grab focus */
  gtk_widget_grab_focus (button->dock);
  gtk_grab_add (button->dock);
  gdk_pointer_grab (button->dock->window, TRUE,
      GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
      GDK_POINTER_MOTION_MASK, NULL, NULL, GDK_CURRENT_TIME);
  gdk_keyboard_grab (button->dock->window, TRUE, GDK_CURRENT_TIME);

  /* forward event to the slider */
  e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
  e->window = button->scale->window;
  e->x = button->scale->allocation.width / 2;
  /* FIXME: those magic numbers are numbers in the GtkScaleClass */
  e->y = ((10 + (80 - (v * button->scale->allocation.height) * 0.8)) /
      SCALE_SIZE) * button->scale->allocation.height;
  gtk_widget_event (button->scale, (GdkEvent *) e);
  e->window = event->window;
  gdk_event_free ((GdkEvent *) e);

  button->pop_time = event->time;

  return TRUE;
}

/*
 * +/- button callbacks.
 */

static gboolean
cb_button_timeout (gpointer data)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (data);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  float val;
  gboolean res = TRUE;

  if (button->click_id == 0)
    return FALSE;

  val = sj_volume_button_get_value (button);
  val += button->direction;
  if (val <= adj->lower) {
    res = FALSE;
    val = adj->lower;
  } else if (val > adj->upper) {
    res = FALSE;
    val = adj->upper;
  }
  sj_volume_button_set_value (button, val);

  if (!res) {
    g_source_remove (button->click_id);
    button->click_id = 0;
  }

  return res;
}

static gboolean
cb_button_press (GtkWidget      * widget,
		 GdkEventButton * event,
		 gpointer         data)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (data);
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));

  if (button->click_id != 0)
    g_source_remove (button->click_id);
  button->direction = (widget == button->plus) ?
      fabs (adj->page_increment) : - fabs (adj->page_increment);
  button->click_id = g_timeout_add (CLICK_TIMEOUT,
				    (GSourceFunc) cb_button_timeout, button);
  cb_button_timeout (button);

  return TRUE;
}

static gboolean
cb_button_release (GtkWidget      * widget,
		   GdkEventButton * event,
		   gpointer         data)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (data);

  if (button->click_id != 0) {
    g_source_remove (button->click_id);
    button->click_id = 0;
  }

  return TRUE;
}

/*
 * Scale callbacks.
 */

static void
sj_volume_release_grab (SjVolumeButton *button,
			   GdkEventButton * event)
{
  GdkEventButton *e;

  /* ungrab focus */
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gtk_grab_remove (button->dock);

  /* hide again */
  gtk_widget_hide (button->dock);
  button->timeout = FALSE;

  e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
  e->window = GTK_WIDGET (button)->window;
  e->type = GDK_BUTTON_RELEASE;
  gtk_widget_event (GTK_WIDGET (button), (GdkEvent *) e);
  e->window = event->window;
  gdk_event_free ((GdkEvent *) e);
}

static gboolean
cb_dock_press (GtkWidget      * widget,
	       GdkEventButton * event,
	       gpointer         data)
{
  SjVolumeButton *button = SJ_VOLUME_BUTTON (data);

  if (event->type == GDK_BUTTON_PRESS) {
    sj_volume_release_grab (button, event);
    return TRUE;
  }

  return FALSE;
}

/*
 * Scale stuff.
 */

#define SJ_TYPE_VOLUME_SCALE \
  (sj_volume_scale_get_type ())
#define SJ_VOLUME_SCALE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SJ_TYPE_VOLUME_SCALE, \
			       SjVolumeScale))

typedef struct _SjVolumeScale {
  GtkVScale parent;
  SjVolumeButton *button;
} SjVolumeScale;

static GType	sj_volume_scale_get_type	 (void);

static void	sj_volume_scale_class_init    (GtkVScaleClass * klass);

static gboolean	sj_volume_scale_press	 (GtkWidget      * widget,
						  GdkEventButton * event);
static gboolean sj_volume_scale_release	 (GtkWidget      * widget,
						  GdkEventButton * event);
static void	sj_volume_scale_value_changed (GtkRange      * range);

static GtkVScaleClass *scale_parent_class = NULL;

static GType
sj_volume_scale_get_type (void)
{
  static GType sj_volume_scale_type = 0;

  if (!sj_volume_scale_type) {
    static const GTypeInfo sj_volume_scale_info = {
      sizeof (GtkVScaleClass),
      NULL,
      NULL,
      (GClassInitFunc) sj_volume_scale_class_init,
      NULL,
      NULL,
      sizeof (SjVolumeScale),
      0,
      NULL,
      NULL
    };

    sj_volume_scale_type =
        g_type_register_static (GTK_TYPE_VSCALE,
				"SjVolumeScale",
				&sj_volume_scale_info, 0);
  }

  return sj_volume_scale_type;
}

static void
sj_volume_scale_class_init (GtkVScaleClass * klass)
{
  GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS (klass);
  GtkRangeClass *gtkrange_class = GTK_RANGE_CLASS (klass);

  scale_parent_class = g_type_class_ref (GTK_TYPE_VSCALE);

  gtkwidget_class->button_press_event = sj_volume_scale_press;
  gtkwidget_class->button_release_event = sj_volume_scale_release;
  gtkrange_class->value_changed = sj_volume_scale_value_changed;
}

static GtkWidget *
sj_volume_scale_new (SjVolumeButton * button,
			float min, float max,
			float step)
{
  SjVolumeScale *scale = g_object_new (SJ_TYPE_VOLUME_SCALE, NULL);
  GtkObject *adj;

  adj = gtk_adjustment_new (min, min, max, step, 5 * step, 0);
  gtk_range_set_adjustment (GTK_RANGE (scale), GTK_ADJUSTMENT (adj));
  scale->button = button;

  return GTK_WIDGET (scale);
}

static gboolean
sj_volume_scale_press (GtkWidget      * widget,
			  GdkEventButton * event)
{
  SjVolumeScale *scale = SJ_VOLUME_SCALE (widget);
  SjVolumeButton *button = scale->button;

  /* the scale will grab input; if we have input grabbed, all goes
   * horribly wrong, so let's not do that. */
  gtk_grab_remove (button->dock);

  return GTK_WIDGET_CLASS (scale_parent_class)->button_press_event (widget, event);
}

static gboolean
sj_volume_scale_release (GtkWidget      * widget,
			    GdkEventButton * event)
{
  SjVolumeScale *scale = SJ_VOLUME_SCALE (widget);
  SjVolumeButton *button = scale->button;
  gboolean res;

  if (button->timeout) {
    /* if we did a quick click, leave the window open; else, hide it */
    if (event->time > button->pop_time + CLICK_TIMEOUT) {
      sj_volume_release_grab (button, event);
      GTK_WIDGET_CLASS (scale_parent_class)->button_release_event (widget, event);
      return TRUE;
    }
    button->timeout = FALSE;
  }

  res = GTK_WIDGET_CLASS (scale_parent_class)->button_release_event (widget, event);

  /* the scale will release input; right after that, we *have to* grab
   * it back so we can catch out-of-scale clicks and hide the popup,
   * so I basically want a g_signal_connect_after_always(), but I can't
   * find that, so we do this complex 'first-call-parent-then-do-actual-
   * action' thingy... */
  gtk_grab_add (button->dock);

  return res;
}

static void
sj_volume_scale_value_changed (GtkRange * range)
{
  SjVolumeScale *scale = SJ_VOLUME_SCALE (range);
  SjVolumeButton *button = scale->button;
  GtkAdjustment *adj = gtk_range_get_adjustment (GTK_RANGE (button->scale));
  int step = adj->upper / 4;
  const char *s;
  float val = gtk_range_get_value (range);
  GdkPixbuf *buf;

  if (val >= 0 && val < step)
    s = "stock_volume-0";
  else if (val >= step && val < step * 2)
    s = "stock_volume-min";
  else if (val >= step * 2 && val < step * 3)
    s = "stock_volume-med";
  else
    s = "stock_volume-max";

  /* update label */
  buf = gtk_icon_theme_load_icon (button->theme, s, 16, 0, NULL);
  gtk_image_set_from_pixbuf (GTK_IMAGE (button->image), buf);

  /* signal */
  g_signal_emit (button, signals[SIGNAL_VALUE_CHANGED], 0);
}
