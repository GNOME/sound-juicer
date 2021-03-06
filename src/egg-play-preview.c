/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

/*
 * EggPlayPreview GTK+ Widget - egg-play-preview.c
 *
 * Copyright (C) 2008 Luca Cavalli <luca.cavalli@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Authors: Luca Cavalli <luca.cavalli@gmail.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gst/gst.h>

#include "egg-play-preview.h"
#include "sj-util.h"

enum {
	PROP_NONE,

	PROP_URI,
	PROP_TITLE,
	PROP_ARTIST,
	PROP_ALBUM,
	PROP_DURATION,
	PROP_POSITION
};

enum {
	PLAY_STARTED_SIGNAL,
	PAUSED_SIGNAL,
	STOPPED_SIGNAL,

	LAST_SIGNAL
};

struct _EggPlayPreviewPrivate {

	GtkWidget *title_label;
	GtkWidget *artist_album_label;

	GtkWidget *play_button;
	GtkWidget *play_button_image;

	GtkWidget *time_scale;
	GtkWidget *time_label;

	GstElement *playbin;
	GstQuery   *query;
	GstState state;

	gchar *title;
	gchar *artist;
	gchar *album;

	gint64 duration;
	gint64 position;

	gint timeout_id;

	gboolean seeking;
	gboolean is_seekable;

	gchar *uri;
};

static void egg_play_preview_finalize     (GObject             *object);
static void egg_play_preview_dispose      (GObject             *object);
static void	egg_play_preview_set_property (GObject             *object,
										   guint                prop_id,
										   const GValue        *value,
										   GParamSpec          *pspec);
static void	egg_play_preview_get_property (GObject             *object,
										   guint                prop_id,
										   GValue              *value,
										   GParamSpec          *pspec);

static gboolean _timeout_cb                  (EggPlayPreview *play_preview);
static void _ui_update_duration              (EggPlayPreview *play_preview);
static void _ui_update_tags                  (EggPlayPreview *play_preview);
static void _ui_set_sensitive                (EggPlayPreview *play_preview,
											  gboolean sensitive);
static gboolean _change_value_cb             (GtkRange *range,
											  GtkScrollType scroll,
											  gdouble value,
											  EggPlayPreview *play_preview);
static void _clicked_cb                      (GtkButton      *button,
											  EggPlayPreview *play_preview);
static void _duration_notify_cb              (EggPlayPreview *play_preview,
											  GParamSpec     *pspec,
											  gpointer        user_data);
static void _style_updated_cb                (GtkWidget *widget,
											  gpointer   user_data);
static void _setup_pipeline                  (EggPlayPreview *play_preview);
static void _clear_pipeline                  (EggPlayPreview *play_preview);
static gboolean _process_bus_messages        (GstBus         *bus,
											  GstMessage     *msg,
											  EggPlayPreview *play_preview);
static gboolean _query_seeking               (GstElement *element);
static gint64 _query_duration                (GstElement *element);
static void _seek                            (EggPlayPreview *play_preview,
											  GstElement *element,
											  gint64      position);
static gboolean _is_playing                  (GstState state);
static void _play                            (EggPlayPreview *play_preview);
static void _pause                           (EggPlayPreview *play_preview);
static void _stop                            (EggPlayPreview *play_preview);

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (EggPlayPreview, egg_play_preview, GTK_TYPE_BOX)

static void
egg_play_preview_class_init (EggPlayPreviewClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->finalize = egg_play_preview_finalize;
	gobject_class->dispose = egg_play_preview_dispose;
	gobject_class->set_property = egg_play_preview_set_property;
	gobject_class->get_property = egg_play_preview_get_property;

	signals[PLAY_STARTED_SIGNAL] =
		g_signal_new ("play-started",
					  G_TYPE_FROM_CLASS (klass),
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (EggPlayPreviewClass, play),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);

	signals[PAUSED_SIGNAL] =
		g_signal_new ("paused",
					  G_TYPE_FROM_CLASS (klass),
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (EggPlayPreviewClass, pause),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);

	signals[STOPPED_SIGNAL] =
		g_signal_new ("stopped",
					  G_TYPE_FROM_CLASS (klass),
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (EggPlayPreviewClass, stop),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__VOID,
					  G_TYPE_NONE, 0);

	g_object_class_install_property (gobject_class,
									 PROP_URI,
									 g_param_spec_string ("uri",
														  "URI",
														  "The URI of the audio file",
														  NULL,
														  G_PARAM_READWRITE |
														  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
														  G_PARAM_STATIC_BLURB));

	g_object_class_install_property (gobject_class,
									 PROP_TITLE,
									 g_param_spec_string ("title",
														  "Title",
														  "The title of the current stream.",
														  NULL,
														  G_PARAM_READABLE |
														  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
														  G_PARAM_STATIC_BLURB));

	g_object_class_install_property (gobject_class,
									 PROP_TITLE,
									 g_param_spec_string ("artist",
														  "Artist",
														  "The artist of the current stream.",
														  NULL,
														  G_PARAM_READABLE |
														  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
														  G_PARAM_STATIC_BLURB));

	g_object_class_install_property (gobject_class,
									 PROP_ALBUM,
									 g_param_spec_string ("album",
														  "Album",
														  "The album of the current stream.",
														  NULL,
														  G_PARAM_READABLE |
														  G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
														  G_PARAM_STATIC_BLURB));

	g_object_class_install_property (gobject_class,
									 PROP_POSITION,
									 g_param_spec_int64 ("position",
														 "Position",
														 "The position in the current stream in seconds.",
														 0, G_MAXINT, 0,
														 G_PARAM_READWRITE |
														 G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
														 G_PARAM_STATIC_BLURB));

	g_object_class_install_property (gobject_class,
									 PROP_DURATION,
									 g_param_spec_int64 ("duration",
														 "Duration",
														 "The duration of the current stream in seconds.",
														 0, G_MAXINT, 0,
														 G_PARAM_READABLE |
														 G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK |
														 G_PARAM_STATIC_BLURB));

	gst_init (NULL, NULL);
}

static void
egg_play_preview_init (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	PangoAttribute *bold;
	PangoAttrList *attrs;
	GtkGrid *grid;

	play_preview->priv = priv =
		egg_play_preview_get_instance_private (play_preview);

	_setup_pipeline (play_preview);

	priv->title = NULL;
	priv->album = NULL;

	priv->duration = 0;
	priv->position = 0;
	priv->timeout_id = 0;

	priv->is_seekable = FALSE;

	priv->uri = NULL;

	grid = (GtkGrid*) gtk_grid_new ();
	g_object_set (grid,
				  "column-spacing", 12,
				  "border-width", 12,
				  NULL);

	/* track info */
	priv->title_label = gtk_label_new (NULL);
	g_object_set (priv->title_label,
				  "justify", GTK_JUSTIFY_LEFT,
				  "valign", GTK_ALIGN_CENTER,
				  "xalign", 0.0,
				  NULL);
	bold = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	attrs = pango_attr_list_new ();
	bold->start_index = 0;
	bold->end_index = G_MAXINT;
	pango_attr_list_insert (attrs, bold);
	gtk_label_set_attributes (GTK_LABEL (priv->title_label), attrs);
	pango_attr_list_unref(attrs);
	gtk_grid_attach (grid, priv->title_label, 0, 0, 1, 1);

	priv->artist_album_label = gtk_label_new (NULL);
	g_object_set (priv->artist_album_label,
				  "justify", GTK_JUSTIFY_LEFT,
				  "valign", GTK_ALIGN_CENTER,
				  "xalign", 0.0,
				  NULL);
	gtk_grid_attach (grid, priv->artist_album_label, 0, 1, 1, 1);

	/* play button */
	priv->play_button = gtk_button_new_from_icon_name ("media-playback-start",
													   GTK_ICON_SIZE_BUTTON);
	priv->play_button_image = gtk_button_get_image (GTK_BUTTON (priv->play_button));
	g_object_set (priv->play_button,
				  "valign", GTK_ALIGN_CENTER,
				  NULL);
	gtk_grid_attach (grid, priv->play_button, 1, 0, 1, 2);

	/* time scale */
	priv->time_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 1.0);
	gtk_scale_set_draw_value (GTK_SCALE (priv->time_scale), FALSE);
	gtk_widget_set_size_request (priv->time_scale, EGG_PLAYER_PREVIEW_WIDTH, -1);
	g_object_set (priv->time_scale,
				  "valign", GTK_ALIGN_CENTER,
				  "hexpand", TRUE,
				  NULL);
	gtk_grid_attach (grid, priv->time_scale, 2, 0, 1, 2);
	priv->time_label = gtk_label_new ("0:00/0:00");
	g_object_set (priv->time_label,
				  "halign", GTK_ALIGN_END,
				  "justify", GTK_JUSTIFY_RIGHT,
				  "valign", GTK_ALIGN_CENTER,
				  "xalign", 1.0, /* This is needed to right justify the label even though it is deprecated */
				  NULL);
	gtk_grid_attach (grid, priv->time_label, 3, 0, 1, 2);

	g_signal_connect (G_OBJECT (priv->play_button), "clicked",
					  G_CALLBACK (_clicked_cb), play_preview);
	g_signal_connect (G_OBJECT (priv->time_scale), "change-value",
					  G_CALLBACK (_change_value_cb), play_preview);
	g_signal_connect (play_preview, "notify::duration",
					  G_CALLBACK (_duration_notify_cb), NULL);
	g_signal_connect (priv->time_label, "style-updated",
					  G_CALLBACK (_style_updated_cb), play_preview);

	gtk_box_pack_start (GTK_BOX (play_preview), GTK_WIDGET (grid), FALSE, FALSE, 0);

	_ui_set_sensitive (play_preview, FALSE);

	gtk_widget_show_all (GTK_WIDGET (play_preview));
}

static void
egg_play_preview_finalize (GObject *object)
{
	gst_deinit ();

	G_OBJECT_CLASS (egg_play_preview_parent_class)->finalize (object);
}

static void
egg_play_preview_dispose (GObject *object)
{
	EggPlayPreview *play_preview;
	EggPlayPreviewPrivate *priv;

	play_preview = EGG_PLAY_PREVIEW (object);
	priv = egg_play_preview_get_instance_private (play_preview);

	_clear_pipeline (play_preview);

	g_clear_pointer (&priv->title, g_free);
	g_clear_pointer (&priv->artist, g_free);
	g_clear_pointer (&priv->album, g_free);
	g_clear_pointer (&priv->uri, g_free);

	if (priv->timeout_id != 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}
}

static void
egg_play_preview_set_property (GObject       *object,
							   guint          prop_id,
							   const GValue  *value,
							   GParamSpec    *pspec)
{
	EggPlayPreview *play_preview;

	play_preview = EGG_PLAY_PREVIEW (object);

	switch (prop_id) {
    case PROP_URI:
		egg_play_preview_set_uri (play_preview,
								  g_value_get_string (value));
		break;

	case PROP_POSITION:
		egg_play_preview_set_position (play_preview,
									   g_value_get_int (value));
		break;

    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

static void
egg_play_preview_get_property (GObject     *object,
							   guint        prop_id,
							   GValue      *value,
							   GParamSpec  *pspec)
{
	EggPlayPreview *play_preview;

	play_preview = EGG_PLAY_PREVIEW (object);

	switch (prop_id) {
    case PROP_URI:
		g_value_set_string (value,
							egg_play_preview_get_uri (play_preview));
		break;

    case PROP_TITLE:
		g_value_set_string (value,
							egg_play_preview_get_title (play_preview));
		break;

    case PROP_ARTIST:
		g_value_set_string (value,
							egg_play_preview_get_artist (play_preview));
		break;

    case PROP_ALBUM:
		g_value_set_string (value,
							egg_play_preview_get_album (play_preview));
		break;

	case PROP_POSITION:
		g_value_set_int (value,
                         egg_play_preview_get_position (play_preview));
		break;

	case PROP_DURATION:
		g_value_set_int (value,
                         egg_play_preview_get_duration (play_preview));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
calculate_widths (GtkWidget *label, int *widths)
{
	int i = 0;
	for (i = 0; i < 10; i++) {
		gchar *s;
		s = g_strdup_printf ("%d", i);
		gtk_label_set_text (GTK_LABEL (label), s);
		gtk_widget_get_preferred_width (label, NULL, &widths[i]);
		g_free (s);
	}
}

static int
get_widest_digit (int *widths, int max)
{
	int max_width = 0;
	int i_max = 0;
	int i;

	for (i = 0; i <= max; i++) {
		if (widths[i] > max_width) {
			max_width = widths[i];
			i_max = i;
		}
	}

	return i_max;
}

static gchar *
get_widest_time (int *widths, int duration)
{
	int minutes;
	int seconds;
	int m, s1, s2;
	int width = 1;

	minutes = duration / 60;
	seconds = duration % 60;

	s1 = get_widest_digit (widths, 9);
	if (minutes == 0)
		s2 = get_widest_digit (widths, seconds % 10);
	else
		s2 = get_widest_digit (widths, 5);
	if (minutes > 9) {
		m = get_widest_digit (widths, 9);
		m += get_widest_digit (widths, minutes % 10) * 10;
		width = 2;
	} else {
		m = get_widest_digit (widths, minutes);
	}

	return g_strdup_printf ("%0*d:%d%d/%d:%02d", width, m, s2, s1, minutes, seconds);
}

static void
set_time_label_width (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	int widths[10];
	int w;
	gchar *s;

	priv = egg_play_preview_get_instance_private (play_preview);
	g_object_set (priv->time_label, "width-request", -1, NULL);
	calculate_widths (priv->time_label, widths);
	s = get_widest_time (widths, priv->duration / GST_SECOND);
	gtk_label_set_text (GTK_LABEL (priv->time_label), s);
	gtk_widget_get_preferred_width (priv->time_label, NULL, &w);
	g_object_set (priv->time_label, "width-request", w, NULL);
	g_free (s);
	_ui_update_duration (play_preview);
}

static void
_duration_notify_cb (EggPlayPreview *play_preview,
					 GParamSpec     *pspec,
					 gpointer        user_data)
{
	set_time_label_width (play_preview);
}

static void
_style_updated_cb (GtkWidget *widget,
				   gpointer   user_data)
{
	set_time_label_width (user_data);
}

static gboolean
_timeout_cb (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	priv = egg_play_preview_get_instance_private (play_preview);

	if (priv->seeking)
		return TRUE;

	if (gst_element_query (priv->playbin, priv->query)) {
		gst_query_parse_position (priv->query, NULL, &priv->position);
	} else {
		return TRUE;
	}

	g_object_notify (G_OBJECT (play_preview), "position");

	gtk_range_set_value (GTK_RANGE (priv->time_scale), priv->position * (100.0 / priv->duration));
	_ui_update_duration (play_preview);

	return TRUE;
}

static void
_ui_update_duration (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	gchar *str;

	priv = egg_play_preview_get_instance_private (play_preview);

	str = g_strdup_printf ("%u:%02u/%u:%02u",
						   (int) (priv->position / 60 / GST_SECOND), (int) (priv->position / GST_SECOND) % 60,
						   (int) (priv->duration / 60 / GST_SECOND), (int) (priv->duration / GST_SECOND) % 60);

	gtk_label_set_text (GTK_LABEL (priv->time_label), str);
	g_free (str);
}

static void
_ui_update_tags (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	gchar *str;

	priv = egg_play_preview_get_instance_private (play_preview);

	str = g_strdup_printf ("%s", priv->title ? priv->title : _("Unknown Title"));
	gtk_label_set_text (GTK_LABEL (priv->title_label), str);
	g_free (str);

	str = g_strdup_printf ("%s - %s",
						   priv->artist ? priv->artist : _("Unknown Artist"),
						   priv->album ? priv->album : _("Unknown Album"));
	gtk_label_set_text (GTK_LABEL (priv->artist_album_label), str);
	g_free (str);
}

static void
_ui_set_sensitive (EggPlayPreview *play_preview, gboolean sensitive)
{
	EggPlayPreviewPrivate *priv;

	priv = egg_play_preview_get_instance_private (play_preview);

	gtk_widget_set_sensitive (priv->play_button, sensitive);
	gtk_widget_set_sensitive (priv->time_scale, sensitive && priv->is_seekable);
}

/*
 * Note that handling ‘change-value’ is preferable to handling
 * ‘value-changed’ as ‘change-value’ is only emitted in response to
 * scroll events so it avoids a feedback loop which would exist
 * between a ‘value-changed’ handler and timeout_cb() which calls
 * gtk_range_set_value().
 */
static gboolean
_change_value_cb (GtkRange *range, GtkScrollType scroll, gdouble value, EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	GtkAdjustment *adjustment;
	double lower, upper;

	priv = egg_play_preview_get_instance_private (play_preview);
	adjustment = gtk_range_get_adjustment (range);
	lower = gtk_adjustment_get_lower (adjustment);
	upper = gtk_adjustment_get_upper (adjustment);
	/* Clamp value to be within the adjustment range. */
	if (value < lower)
		value = lower;

	if (value > upper)
		value = upper;

	if (priv->is_seekable && value != gtk_adjustment_get_value (adjustment)) {
		priv->position = (gint64) (value / 100.0 * priv->duration);
		_seek (play_preview, priv->playbin, priv->position);
		_ui_update_duration (play_preview);
	}

	return FALSE;
}

static void
_clicked_cb (GtkButton *button, EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	priv = egg_play_preview_get_instance_private (play_preview);

	if (priv->playbin == NULL)
		return;

	if (_is_playing (GST_STATE (priv->playbin))) {		
		_pause (play_preview);
		g_signal_emit (play_preview, signals[PAUSED_SIGNAL], 0);
	} else {
		_play (play_preview);
		g_signal_emit (play_preview, signals[PLAY_STARTED_SIGNAL], 0);
	}
}

static void
_setup_pipeline (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	GstBus *bus = NULL;
        guint flags;

	priv = egg_play_preview_get_instance_private (play_preview);

	priv->state = GST_STATE_NULL;

	priv->playbin = gst_element_factory_make ("playbin", "playbin");
	if (!priv->playbin)
		return;

        /* Disable video output */
        g_object_get (G_OBJECT (priv->playbin),
                      "flags", &flags,
                      NULL);
        flags &= ~0x00000001;
	g_object_set (G_OBJECT (priv->playbin),
                      "flags", flags,
                      NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (priv->playbin));
	gst_bus_add_watch (bus, (GstBusFunc) _process_bus_messages, play_preview);
	gst_object_unref (bus);

	gst_element_set_state (priv->playbin, GST_STATE_NULL);
	priv->query = gst_query_new_position (GST_FORMAT_TIME);
}

static void
_clear_pipeline (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	GstBus *bus;

	priv = egg_play_preview_get_instance_private (play_preview);

	if (priv->playbin) {
		bus = gst_pipeline_get_bus (GST_PIPELINE (priv->playbin));
		gst_bus_set_flushing (bus, TRUE);
		gst_object_unref (bus);

		gst_element_set_state (priv->playbin, GST_STATE_NULL);
                gst_object_unref (GST_OBJECT (priv->playbin));
		priv->playbin = NULL;
		gst_query_unref (priv->query);
		priv->query = NULL;
	}
}

static gboolean
_process_bus_messages (GstBus *bus, GstMessage *msg, EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;
	GstTagList *tag_list = NULL;
	gchar *title = NULL, *artist = NULL, *album = NULL;
	gint64 duration;
	GstState state;
	GstStateChangeReturn result;

	priv = egg_play_preview_get_instance_private (play_preview);

        SJ_BEGIN_IGNORE_SWITCH_ENUM
	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_DURATION_CHANGED:
		if (!gst_element_query_duration (priv->playbin, GST_FORMAT_TIME, &duration))
			break;

		if (priv->duration == duration)
			break;

		priv->duration = duration;
		g_object_notify (G_OBJECT (play_preview), "duration");
		break;

	case GST_MESSAGE_EOS:
		_stop (play_preview);
		break;

	case GST_MESSAGE_TAG:
		gst_message_parse_tag (msg, &tag_list);
		gst_tag_list_get_string (tag_list, GST_TAG_TITLE, &title);
		gst_tag_list_get_string (tag_list, GST_TAG_ARTIST, &artist);
		gst_tag_list_get_string (tag_list, GST_TAG_ALBUM, &album);
		gst_tag_list_unref (tag_list);

		if (g_strcmp0 (title, priv->title) != 0) {
			g_free (priv->title);
			priv->title = title;
			g_object_notify (G_OBJECT (play_preview), "title");
		} else {
			g_free (title);
		}
		if (g_strcmp0 (artist, priv->artist) != 0) {
			g_free (priv->artist);
			priv->artist = artist;
			g_object_notify (G_OBJECT (play_preview), "artist");
		} else {
			g_free (artist);
		}

		if (g_strcmp0 (album, priv->album) != 0) {
			g_free (priv->album);
			priv->album = album;
			g_object_notify (G_OBJECT (play_preview), "album");
		} else {
			g_free (album);
		}

		_ui_update_tags (play_preview);
		break;

	case GST_MESSAGE_ASYNC_DONE:
		if (GST_OBJECT (priv->playbin) == GST_MESSAGE_SRC (msg))
			priv->seeking = FALSE;
		break;

	case GST_MESSAGE_STATE_CHANGED:
		result = gst_element_get_state (GST_ELEMENT (priv->playbin), &state, NULL, 500);

		if (result != GST_STATE_CHANGE_SUCCESS)
			break;

		if (priv->state == state || state < GST_STATE_PAUSED)
			break;

		if (state == GST_STATE_PLAYING) {
			g_signal_emit (G_OBJECT (play_preview), signals[PLAY_STARTED_SIGNAL], 0);
		} else if (state == GST_STATE_PAUSED) {
			g_signal_emit (G_OBJECT (play_preview), signals[PAUSED_SIGNAL], 0);
		} else {
			g_signal_emit (G_OBJECT (play_preview), signals[STOPPED_SIGNAL], 0);
		}

		priv->state = state;
		break;

	default:
		break;
	}
        SJ_END_IGNORE_SWITCH_ENUM

	return TRUE;
}

static gboolean
_query_seeking (GstElement *element)
{
	GstStateChangeReturn result;
	GstState state;
	GstState pending;
	GstQuery *query;
	gboolean seekable;

	seekable = _query_duration (element) > 0;

	result = gst_element_get_state (element, &state, &pending, GST_CLOCK_TIME_NONE);

	if (result == GST_STATE_CHANGE_FAILURE)
		return FALSE;

	if (pending)
		state = pending;

	result = gst_element_set_state (element, GST_STATE_PAUSED);

	if (result == GST_STATE_CHANGE_ASYNC)
		gst_element_get_state (element, NULL, NULL, GST_CLOCK_TIME_NONE);

	query = gst_query_new_seeking (GST_FORMAT_TIME);

	if (gst_element_query (element, query))
		gst_query_parse_seeking (query, NULL, &seekable, NULL, NULL);

	gst_query_unref (query);

	gst_element_set_state (element, state);

	return seekable;
}

static gint64
_query_duration (GstElement *element)
{
	GstStateChangeReturn result;
	GstState state;
	GstState pending;
	gint64 duration;

	duration = 0;

	result = gst_element_get_state (element, &state, &pending, GST_CLOCK_TIME_NONE);

	if (result == GST_STATE_CHANGE_FAILURE)
		return 0;

	if (pending)
		state = pending;

	result = gst_element_set_state (element, GST_STATE_PAUSED);

	if (result == GST_STATE_CHANGE_ASYNC)
		gst_element_get_state (element, NULL, NULL, GST_CLOCK_TIME_NONE);

	gst_element_query_duration (element, GST_FORMAT_TIME, &duration);

	gst_element_set_state (element, state);

	return duration;
}

static void
_seek (EggPlayPreview *play_preview, GstElement *element, gint64 position)
{
	EggPlayPreviewPrivate *priv;

	priv = egg_play_preview_get_instance_private (play_preview);
	if (gst_element_seek_simple (priv->playbin, GST_FORMAT_TIME,
								 GST_SEEK_FLAG_FLUSH, position))
		priv->seeking = TRUE;
}

static gboolean
_is_playing (GstState state)
{
	return (state == GST_STATE_PLAYING);
}

static void
_play (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	priv = egg_play_preview_get_instance_private (play_preview);

	gst_element_set_state (priv->playbin, GST_STATE_PLAYING);

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->play_button_image),
	                              "media-playback-pause",
	                              GTK_ICON_SIZE_BUTTON);
}

static void
_pause (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	priv = egg_play_preview_get_instance_private (play_preview);

	gst_element_set_state (priv->playbin, GST_STATE_PAUSED);

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->play_button_image),
								  "media-playback-start",
	                              GTK_ICON_SIZE_BUTTON);
}

static void
_stop (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	priv = egg_play_preview_get_instance_private (play_preview);

	gst_element_set_state (priv->playbin, GST_STATE_READY);

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->play_button_image),
								  "media-playback-start",
	                              GTK_ICON_SIZE_BUTTON);
}

GtkWidget *
egg_play_preview_new (void)
{
	EggPlayPreview *play_preview;

	play_preview = g_object_new (EGG_TYPE_PLAY_PREVIEW, NULL);

	return GTK_WIDGET (play_preview);
}

GtkWidget *
egg_play_preview_new_with_uri (const gchar *uri)
{
	EggPlayPreview *play_preview;

	play_preview = g_object_new (EGG_TYPE_PLAY_PREVIEW, NULL);
	egg_play_preview_set_uri (play_preview, uri);

	return GTK_WIDGET (play_preview);
}

void
egg_play_preview_set_uri (EggPlayPreview *play_preview, const gchar *uri)
{
	EggPlayPreviewPrivate *priv;

	g_return_if_fail (EGG_IS_PLAY_PREVIEW (play_preview));

	priv = egg_play_preview_get_instance_private (play_preview);

	if (priv->uri) {
		g_free (priv->uri);
		priv->uri = NULL;
		priv->duration = 0;
	}

	if (priv->timeout_id != 0) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	_stop (play_preview);
	priv->is_seekable = FALSE;

	if (gst_uri_is_valid (uri)) {
		priv->uri = g_strdup (uri);

		g_object_set (G_OBJECT (priv->playbin), "uri", uri, NULL);

		priv->duration = _query_duration (priv->playbin);
		priv->is_seekable = _query_seeking (priv->playbin);

		g_object_notify (G_OBJECT (play_preview), "duration");

		_pause (play_preview);

		_ui_set_sensitive (play_preview, TRUE);
		_ui_update_duration (play_preview);
		_ui_update_tags (play_preview);
		priv->timeout_id = g_timeout_add (250, (GSourceFunc) _timeout_cb, play_preview);
		g_source_set_name_by_id (priv->timeout_id, "[sound-juicer] _timeout_cb");
	}

	g_object_notify (G_OBJECT (play_preview), "uri");
}

void
egg_play_preview_set_position (EggPlayPreview *play_preview, gint64 position)
{
	EggPlayPreviewPrivate *priv;

	g_return_if_fail (EGG_IS_PLAY_PREVIEW (play_preview));

	priv = egg_play_preview_get_instance_private (play_preview);

	/* FIXME: write function content */
	if (priv->is_seekable) {
		_seek (play_preview, priv->playbin, MIN (position, priv->duration));

		g_object_notify (G_OBJECT (play_preview), "position");
	}
}

gchar *
egg_play_preview_get_uri (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	g_return_val_if_fail (EGG_IS_PLAY_PREVIEW (play_preview), NULL);

	priv = egg_play_preview_get_instance_private (play_preview);

	return priv->uri;
}

gchar *
egg_play_preview_get_title (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	g_return_val_if_fail (EGG_IS_PLAY_PREVIEW (play_preview), NULL);

	priv = egg_play_preview_get_instance_private (play_preview);

	return priv->title;
}

gchar *
egg_play_preview_get_artist (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	g_return_val_if_fail (EGG_IS_PLAY_PREVIEW (play_preview), NULL);

	priv = egg_play_preview_get_instance_private (play_preview);

	return priv->artist;
}

gchar *
egg_play_preview_get_album (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	g_return_val_if_fail (EGG_IS_PLAY_PREVIEW (play_preview), NULL);

	priv = egg_play_preview_get_instance_private (play_preview);

	return priv->album;
}

gint64
egg_play_preview_get_position (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	g_return_val_if_fail (EGG_IS_PLAY_PREVIEW (play_preview), 0);

	priv = egg_play_preview_get_instance_private (play_preview);

	return priv->position;
}

gint64
egg_play_preview_get_duration (EggPlayPreview *play_preview)
{
	EggPlayPreviewPrivate *priv;

	g_return_val_if_fail (EGG_IS_PLAY_PREVIEW (play_preview), -1);

	priv = egg_play_preview_get_instance_private (play_preview);

	return priv->duration;
}
