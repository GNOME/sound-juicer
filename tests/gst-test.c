#include <glib.h>
#include "sj-gstreamer.h"
#include "sj-structures.h"

int main(int argc, char** argv)
{
  GError *error = NULL;

  TrackDetails track;
  track.artist = "Some Artist";
  track.title = "Some Title";
  track.number = 7;

  sj_gstreamer_init (argc, argv, &error);
  if (error) {
    g_print ("Cannot init SJ_GST: %s\n", error->message);
    return 1;
  }
  sj_gstreamer_set_cdrom ("/dev/cdroms/cdrom1");
  sj_gstreamer_extract_track (&track, "test.ogg", NULL);
  return 0;
}
