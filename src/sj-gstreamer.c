#include <glib/gerror.h>
#include <glib/gtypes.h>
#include <gst/gst.h>
#include "sj-structures.h"
#include "sj-gstreamer.h"
#include "sj-error.h"

static GstElement *pipeline;
static GstElement *cdparanoia, *vorbisenc, *filesink;
static GstFormat track_format;
GstPad *source_pad;

#define THREADED 0
#define MAINLOOP 1

typedef void (*progress_cb_t) (int seconds);
typedef void (*completion_cb_t) (void);

progress_cb_t progress_cb;
completion_cb_t completion_cb;

static void eos_cb (GstElement *gstelement, gpointer user_data)
{
  gst_element_set_state (pipeline, GST_STATE_READY);
  completion_cb();
}

void sj_gstreamer_init (int argc, char **argv, GError **error)
{
  gst_init (&argc, &argv);

#if THREADED
  pipeline = gst_thread_new ("pipeline");
#else
  pipeline = gst_pipeline_new ("pipeline");
#endif

  /* Read from CD */
  cdparanoia = gst_element_factory_make ("cdparanoia", "cdparanoia");
  if (cdparanoia == NULL) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 "Could not create cdparanoia element");
    return;
  }
  g_object_set (G_OBJECT (cdparanoia), "paranoia_mode", 0, NULL);
  /* Get the track format for seeking later */
  track_format = gst_format_get_by_nick ("track");
  g_assert (track_format != 0); /* TODO: GError */
  /* Get the source pad for seeking */
  source_pad = gst_element_get_pad (cdparanoia, "src");
  g_assert (source_pad); /* TODO: GError */

  /* Encode to Ogg Vorbis */
  vorbisenc = gst_element_factory_make ("vorbisenc", "vorbisenc");
  if (vorbisenc == NULL) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 "Could not create Vorbis encoding element");
    return;
  }
  /* Connect to the eos so we know when its finished */
  g_signal_connect (vorbisenc, "eos", eos_cb, NULL);

  /* Write to disk */
  filesink = gst_element_factory_make ("filesink", "filesink");
  if (filesink == NULL) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 "Could not create file sink element");
    return;
  }

  /* Add the elements to the pipeline */
  gst_bin_add (GST_BIN (pipeline), cdparanoia);
  gst_bin_add (GST_BIN (pipeline), vorbisenc);
  gst_bin_add (GST_BIN (pipeline), filesink);

  /* Link it all together */
  gst_element_link_many (cdparanoia, vorbisenc, filesink, NULL);
}

void sj_gstreamer_shutdown (void)
{
  g_return_if_fail (pipeline != NULL);
  gst_element_set_state (pipeline, GST_STATE_NULL);
}

void sj_gstreamer_set_cdrom (const char* device)
{
  g_assert (cdparanoia != NULL);
  g_return_if_fail (device != NULL);

  g_object_set (G_OBJECT (cdparanoia), "location", device, NULL);
}

static gint seconds;

static gboolean tick_timeout_cb(gpointer user_data)
{
  gint64 nanos;
  gint secs;
  static GstFormat format = GST_FORMAT_TIME;

  if (gst_element_get_state (pipeline) != GST_STATE_PLAYING) {
    return FALSE;
  }

  if (!gst_pad_query (source_pad, GST_QUERY_POSITION, &format, &nanos)) {
    g_print ("pad_query failed!\n");
    return TRUE;
  }

  secs = nanos / GST_SECOND;
  if (secs != seconds) {
    seconds = secs;
    progress_cb(seconds);
  }
  return TRUE;
}

void sj_gstreamer_set_callbacks (progress_cb_t progress, completion_cb_t completion)
{
  progress_cb = progress;
  completion_cb = completion;
}

void sj_gstreamer_extract_track (const TrackDetails *track, const char* path, GError **error)
{
  GstEvent *event;
  GstCaps *caps;
  char *tracknumber;

  g_return_if_fail (pipeline != NULL);
  g_return_if_fail (path != NULL);
  g_return_if_fail (track != NULL);

  /* Set the output filename */
  gst_element_set_state (filesink, GST_STATE_NULL);
  g_object_set (G_OBJECT (filesink), "location", path, NULL);

  /* Set the Ogg metadata */
  /* Requires gst 0.6.1 to work correctly */
  tracknumber = g_strdup_printf("%d", track->number);
  caps = GST_CAPS_NEW ("vorbisenc_metadata", 
                       "application/x-gst-metadata",
                       "title", GST_PROPS_STRING (track->title),
                       "artist", GST_PROPS_STRING (track->artist),
                       "tracknumber", GST_PROPS_STRING (tracknumber),
                       "album", GST_PROPS_STRING (track->album->title),
                       "comment", GST_PROPS_STRING("Ripped with Sound Juicer")
                       );
  g_object_set (G_OBJECT (vorbisenc), "metadata", caps, NULL);
  g_free (tracknumber);

  /* Let's get ready to rumble! */
  gst_element_set_state (pipeline, GST_STATE_PAUSED);

  /* Seek to the right track */
  event = gst_event_new_segment_seek (track_format | GST_SEEK_METHOD_SET | GST_SEEK_FLAG_FLUSH,
			              track->number-1, track->number);
  if (!gst_pad_send_event (source_pad, event)) {
    g_set_error (error,
                 SJ_ERROR, SJ_ERROR_INTERNAL_ERROR,
                 "Could not seek to track");
    return;
  }
  
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
#if MAINLOOP
  g_idle_add ((GSourceFunc)gst_bin_iterate, pipeline);
  g_timeout_add (200, (GSourceFunc)tick_timeout_cb, NULL);
#else
  while (gst_bin_iterate (GST_BIN (pipeline))) {
    g_print(".");
  }
#endif
}
