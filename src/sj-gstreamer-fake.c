#include <glib/gerror.h>
#include <glib/gmessages.h>
#include <glib/gtimer.h>
#include <glib/gmain.h>
#include "sj-gstreamer.h"

void sj_gstreamer_init (int argc, char **argv, GError **error) 
{
  g_print ("gstreamer-fake: init()\n");
}

void sj_gstreamer_shutdown (void)
{
  g_print ("gstreamer-fake: shutdown()\n");
}

void sj_gstreamer_set_cdrom (const char* device)
{
  g_print ("gstreamer-fake: set_cdrom(\"%s\")\n", device == NULL ? "NULL" : device);
}

static int counter;

static gboolean idle_callback(gpointer data)
{
  g_print(".");
  g_usleep(100);
  counter++;
  if (counter > 100) {
    /* fire finish */
  } else if (counter % 10 == 0) {
    /* fire progress event */
  }
  return TRUE;
}

void sj_gstreamer_extract_track (const TrackDetails *track, const char* path, GError **error)
{
  g_print ("gstreamer-fake: extract_track()\n");
  g_idle_add (idle_callback, NULL);
}
