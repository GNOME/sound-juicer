#ifndef SJ_GSTREAMER_H
#define SJ_GSTREAMER_H

#include <glib/gerror.h>
#include "sj-structures.h"

void sj_gstreamer_init (int argc, char **argv, GError **error);
void sj_gstreamer_shutdown (void);

void sj_gstreamer_set_cdrom (const char* device);

void sj_gstreamer_extract_track (const TrackDetails *track, const char* path, GError **error);

#endif
