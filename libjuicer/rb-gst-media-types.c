/*
 *  Copyright (C) 2010  Jonathan Matthew  <jonathan@d14n.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  The Rhythmbox authors hereby grant permission for non-GPL compatible
 *  GStreamer plugins to be used and distributed together with GStreamer
 *  and Rhythmbox. This permission is above and beyond the permissions granted
 *  by the GPL license by which Rhythmbox is covered. If you modify this code
 *  you may extend this exception to your version of the code, but you are not
 *  obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

#include "config.h"
#include "sj-util.h"

#include <memory.h>

#include <gst/pbutils/encoding-target.h>
#include <gst/pbutils/missing-plugins.h>

#include "rb-gst-media-types.h"

#define USER_ENCODING_TARGET_FILE_NAME "encoding-profiles"
#define SOURCE_ENCODING_TARGET_FILE TOPSRCDIR"/data/encoding-profiles"
#define INSTALLED_ENCODING_TARGET_FILE DATADIR"/sound-juicer/encoding-profiles"

static GstEncodingTarget *default_target = NULL;

char *
rb_gst_caps_to_media_type (const GstCaps *caps)
{
	GstStructure *s;
	const char *media_type;

	/* the aim here is to reduce the caps to a single mimetype-like
	 * string, describing the audio encoding (for audio files) or the
	 * file type (for everything else).  raw media types are ignored.
	 *
	 * there are only a couple of special cases.
	 */

	if (gst_caps_get_size (caps) == 0)
		return NULL;

	s = gst_caps_get_structure (caps, 0);
	media_type = gst_structure_get_name (s);
	if (media_type == NULL) {
		return NULL;
	} else if (g_str_has_prefix (media_type, "audio/x-raw-") ||
	    g_str_has_prefix (media_type, "video/x-raw-")) {
		/* ignore raw types */
		return NULL;
	} else if (g_str_equal (media_type, "audio/mpeg")) {
		/* need to distinguish between mpeg 1 layer 3 and
		 * mpeg 2 or 4 here.
		 */
		int mpegversion = 0;
		gst_structure_get_int (s, "mpegversion", &mpegversion);
		switch (mpegversion) {
		case 2:
		case 4:
			return g_strdup ("audio/x-aac");		/* hmm. */

		case 1:
		default:
			return g_strdup ("audio/mpeg");
		}
	} else {
		/* everything else is fine as-is */
		return g_strdup (media_type);
	}
}

GstCaps *
rb_gst_media_type_to_caps (const char *media_type)
{
	/* special cases: */
	if (strcmp (media_type, "audio/mpeg") == 0) {
		return gst_caps_from_string ("audio/mpeg, mpegversion=(int)1");
	} else if (strcmp (media_type, "audio/x-aac") == 0) {
		return gst_caps_from_string ("audio/mpeg, mpegversion=(int){ 2, 4 }");
	} else {
		/* otherwise, the media type is enough */
		return gst_caps_from_string (media_type);
	}
}

const char *
rb_gst_media_type_to_extension (const char *media_type)
{
	if (media_type == NULL) {
		return NULL;
	} else if (!strcmp (media_type, "audio/mpeg")) {
		return "mp3";
	} else if (!strcmp (media_type, "audio/x-vorbis") || !strcmp (media_type, "application/ogg")) {
		return "ogg";
	} else if (!strcmp (media_type, "audio/x-opus")) {
		return "opus";
	} else if (!strcmp (media_type, "audio/x-flac") || !strcmp (media_type, "audio/flac")) {
		return "flac";
	} else if (!strcmp (media_type, "audio/x-aac") || !strcmp (media_type, "audio/aac") || !strcmp (media_type, "audio/x-alac")) {
		return "m4a";
	} else if (!strcmp (media_type, "audio/x-wavpack")) {
		return "wv";
	} else {
		return NULL;
	}
}

const char *
rb_gst_mime_type_to_media_type (const char *mime_type)
{
	if (!strcmp (mime_type, "application/x-id3") || !strcmp (mime_type, "audio/mpeg")) {
		return "audio/mpeg";
	} else if (!strcmp (mime_type, "application/ogg") || !strcmp (mime_type, "audio/x-vorbis")) {
		return "audio/x-vorbis";
	} else if (!strcmp (mime_type, "audio/flac")) {
		return "audio/x-flac";
	} else if (!strcmp (mime_type, "audio/aac") || !strcmp (mime_type, "audio/mp4") || !strcmp (mime_type, "audio/m4a")) {
		return "audio/x-aac";
	}
	return mime_type;
}

const char *
rb_gst_media_type_to_mime_type (const char *media_type)
{
	if (!strcmp (media_type, "audio/x-vorbis")) {
		return "application/ogg";
	} else if (!strcmp (media_type, "audio/x-flac")) {
		return "audio/flac";
	} else if (!strcmp (media_type, "audio/x-aac")) {
		return "audio/mp4";	/* probably */
	} else {
		return media_type;
	}
}

gboolean
rb_gst_media_type_matches_profile (GstEncodingProfile *profile, const char *media_type)
{
	const GstCaps *pcaps;
	const GList *cl;
	GstCaps *caps;
	gboolean matches = FALSE;

	caps = rb_gst_media_type_to_caps (media_type);
	if (caps == NULL) {
		return FALSE;
	}

	pcaps = gst_encoding_profile_get_format (profile);
	if (gst_caps_can_intersect (caps, pcaps)) {
		matches = TRUE;
	}

	if (matches == FALSE && GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
		for (cl = gst_encoding_container_profile_get_profiles (GST_ENCODING_CONTAINER_PROFILE (profile)); cl != NULL; cl = cl->next) {
			GstEncodingProfile *cp = cl->data;
			pcaps = gst_encoding_profile_get_format (cp);
			if (gst_caps_can_intersect (caps, pcaps)) {
				matches = TRUE;
				break;
			}
		}
	}
	gst_caps_unref (caps);

	return matches;
}

char *
rb_gst_encoding_profile_get_media_type (GstEncodingProfile *profile)
{
	if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
		const GList *cl = gst_encoding_container_profile_get_profiles (GST_ENCODING_CONTAINER_PROFILE (profile));
		for (; cl != NULL; cl = cl->next) {
			GstEncodingProfile *p = cl->data;
			if (GST_IS_ENCODING_AUDIO_PROFILE (p)) {
				return rb_gst_caps_to_media_type (gst_encoding_profile_get_format (p));
			}

		}

		/* now what? */
		return NULL;
	} else {
		return rb_gst_caps_to_media_type (gst_encoding_profile_get_format (profile));
	}
}

static gchar *
get_encoding_target_name (void)
{
	return g_build_filename (g_get_user_data_dir (), PACKAGE, USER_ENCODING_TARGET_FILE_NAME, NULL);
}

GstEncodingTarget *
rb_gst_get_default_encoding_target (void)
{
	if (default_target == NULL) {
		char *target_file;
		GError *error = NULL;

		target_file = get_encoding_target_name ();
		if (!g_file_test (target_file, G_FILE_TEST_EXISTS)) {
			g_free (target_file);
			if (g_file_test (SOURCE_ENCODING_TARGET_FILE,
					 G_FILE_TEST_EXISTS) != FALSE) {
				target_file = SOURCE_ENCODING_TARGET_FILE;
			} else {
				target_file = INSTALLED_ENCODING_TARGET_FILE;
			}
		}

		default_target = gst_encoding_target_load_from_file (target_file, &error);
		if (default_target == NULL) {
			g_warning ("Unable to load encoding profiles from %s: %s", target_file, error ? error->message : "no error");
			g_clear_error (&error);
			return NULL;
		}
	}

	return default_target;
}

GstEncodingProfile *
rb_gst_get_encoding_profile (const char *media_type)
{
	const GList *l;
	GstEncodingTarget *target;
	target = rb_gst_get_default_encoding_target ();

	for (l = gst_encoding_target_get_profiles (target); l != NULL; l = l->next) {
		GstEncodingProfile *profile = l->data;
		if (rb_gst_media_type_matches_profile (profile, media_type)) {
			gst_encoding_profile_ref (profile);
			return profile;
		}
	}

	return NULL;
}

gboolean
rb_gst_media_type_is_lossless (const char *media_type)
{
	int i;
	const char *lossless_types[] = {
		"audio/x-flac",
		"audio/x-alac",
		"audio/x-shorten",
		"audio/x-wavpack"	/* not completely sure */
	};

	for (i = 0; i < G_N_ELEMENTS (lossless_types); i++) {
		if (strcmp (media_type, lossless_types[i]) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

GstEncodingProfile *
rb_gst_get_audio_encoding_profile (GstEncodingProfile *profile)
{
	if (GST_IS_ENCODING_AUDIO_PROFILE (profile)) {
		return profile;
	} else if (GST_IS_ENCODING_CONTAINER_PROFILE (profile)) {
		const GList *l = gst_encoding_container_profile_get_profiles (GST_ENCODING_CONTAINER_PROFILE (profile));
		for (; l != NULL; l = l->next) {
			GstEncodingProfile *p = rb_gst_get_audio_encoding_profile (l->data);
			if (p != NULL) {
				return p;
			}
		}
	}

	g_warning ("no audio encoding profile in profile %s", gst_encoding_profile_get_name (profile));
	return NULL;
}

static GstElementFactory *
get_audio_encoder_factory (GstEncodingProfile *profile)
{
	GstEncodingProfile *p = rb_gst_get_audio_encoding_profile (profile);
	GstElementFactory *f;
	GList *l;
	GList *fl;

	if (p == NULL)
		return NULL;

	l = gst_element_factory_list_get_elements (GST_ELEMENT_FACTORY_TYPE_ENCODER, GST_RANK_MARGINAL);
	fl = gst_element_factory_list_filter (l, gst_encoding_profile_get_format (p), GST_PAD_SRC, FALSE);

	if (fl != NULL) {
		f = gst_object_ref (fl->data);
	} else {
		g_warning ("no encoder factory for profile %s", gst_encoding_profile_get_name (profile));
		f = NULL;
	}
	gst_plugin_feature_list_free (l);
	gst_plugin_feature_list_free (fl);
	return f;
}

GstElement *
rb_gst_encoding_profile_get_encoder (GstEncodingProfile *profile)
{
	GstElementFactory *factory;

	factory = get_audio_encoder_factory (profile);
	if (factory == NULL) {
		return NULL;
	}

	return gst_element_factory_create (factory, NULL);
}

void
rb_gst_encoding_profile_set_preset (GstEncodingProfile *profile, const char *preset)
{
	GstEncodingProfile *p;
	const gchar *pre;

	pre = sj_str_is_empty (preset) ? NULL : preset;
	p = rb_gst_get_audio_encoding_profile (profile);
	if (p != NULL) {
		gst_encoding_profile_set_preset (p, pre);
	}
}

const gchar*
rb_gst_encoding_profile_get_preset (GstEncodingProfile *profile)
{
	GstEncodingProfile *p;
	const gchar *preset = NULL;

	p = rb_gst_get_audio_encoding_profile (profile);
	if (p != NULL) {
		preset = gst_encoding_profile_get_preset (p);
	}
	return (preset == NULL) ? "" : preset;
}

void
rb_gst_encoding_profile_save_profiles (void)
{
	GstEncodingTarget *target;
	gchar *file_name, *directory, *display_name;
	gboolean retrying = FALSE;
	GError *error = NULL;

	target = rb_gst_get_default_encoding_target ();
	file_name = get_encoding_target_name ();
 retry_save:
	if (!gst_encoding_target_save_to_file (target, file_name, &error)) {
		if (!retrying &&
		    g_error_matches (error, g_file_error_quark (), G_FILE_ERROR_NOENT)) {
			g_error_free (error);
			error = NULL;
			retrying = TRUE;
			directory = g_path_get_dirname (file_name);
			if (g_mkdir_with_parents (directory, 0755) == 0) {
				g_free (directory);
				goto retry_save;
			} else {
				display_name = g_filename_display_name (directory);
				g_warning ("Error saving encoding target file, unable to create directory '%s'",
					   display_name);
				g_free (display_name);
				g_free (directory);
			}
		} else {
			display_name = g_filename_display_name (file_name);
			g_warning ("Error saving encoding target file '%s' - '%s'",
				   display_name,
				   error->message);
			g_free (display_name);
			g_error_free (error);
		}
	}
	g_free (file_name);
}

static void
clear_preset_info (gpointer data)
{
	SjPresetInfo *info = data;

	g_free (info->name);
	g_free (info->description);
}

GArray *
rb_gst_encoding_profile_get_presets (GstEncodingProfile *profile)
{
	GArray *array;
	GstElement *encoder;

	array = g_array_new (FALSE, FALSE, sizeof(SjPresetInfo));
	g_array_set_clear_func (array, clear_preset_info);
	encoder = rb_gst_encoding_profile_get_encoder (profile);
	if (encoder != NULL && GST_IS_PRESET (encoder)) {
		gchar **presets;
		int i;

		presets = gst_preset_get_preset_names (GST_PRESET (encoder));
		for (i = 0; presets != NULL && presets[i] != NULL; i++) {
			SjPresetInfo info;

			info.description = NULL;
			info.name = g_strdup (presets[i]);
			gst_preset_get_meta (GST_PRESET (encoder), presets[i], "comment", &info.description);
			g_array_append_val (array, info);
		}
		g_strfreev (presets);
	}
	if (encoder != NULL)
		g_object_unref (encoder);
	return array;
}

gboolean
rb_gst_check_missing_plugins (GstEncodingProfile *profile,
			      char ***details,
			      char ***descriptions)
{
	GstElement *encodebin;
	GstBus *bus;
	GstPad *pad;
	gboolean ret;

	ret = FALSE;

	encodebin = gst_element_factory_make ("encodebin", NULL);
	if (encodebin == NULL) {
		g_warning ("Unable to create encodebin");
		return TRUE;
	}

	bus = gst_bus_new ();
	gst_element_set_bus (encodebin, bus);
	gst_bus_set_flushing (bus, FALSE);		/* necessary? */

	g_object_set (encodebin, "profile", profile, NULL);
	pad = gst_element_get_static_pad (encodebin, "audio_0");
	if (pad == NULL) {
		GstMessage *message;
		GList *messages = NULL;
		GList *m;
		int i;

		message = gst_bus_pop (bus);
		while (message != NULL) {
			if (gst_is_missing_plugin_message (message)) {
				messages = g_list_append (messages, message);
			} else {
				gst_message_unref (message);
			}
			message = gst_bus_pop (bus);
		}

		if (messages != NULL) {
			if (details != NULL) {
				*details = g_new0(char *, g_list_length (messages)+1);
			}
			if (descriptions != NULL) {
				*descriptions = g_new0(char *, g_list_length (messages)+1);
			}
			i = 0;
			for (m = messages; m != NULL; m = m->next) {
				char *str;
				if (details != NULL) {
					str = gst_missing_plugin_message_get_installer_detail (m->data);
					(*details)[i] = str;
				}
				if (descriptions != NULL) {
					str = gst_missing_plugin_message_get_description (m->data);
					(*descriptions)[i] = str;
				}
				i++;
			}

			ret = TRUE;
			g_list_free_full (messages, (GDestroyNotify)gst_message_unref);
		}

	} else {
		gst_element_release_request_pad (encodebin, pad);
		gst_object_unref (pad);
	}

	gst_object_unref (encodebin);
	gst_object_unref (bus);
	return ret;
}
