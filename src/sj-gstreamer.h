#ifndef SJ_GSTREAMER_H
#define SJ_GSTREAMER_H

#include <glib/gerror.h>
#include "sj-structures.h"

typedef enum {
  VORBIS,
  MPEG,
  FLAC,
  WAVE
} EncoderFormat;

typedef void (*progress_cb_t) (int seconds);
typedef void (*completion_cb_t) (void);

void sj_gstreamer_init (int argc, char **argv, GError **error);

void sj_gstreamer_shutdown (void);

void sj_gstreamer_set_cdrom (const char* device);

void sj_gstreamer_set_callbacks (progress_cb_t progress, completion_cb_t completion);

void sj_gstreamer_extract_track (const TrackDetails *track, const char* path, GError **error);

void sj_gstreamer_cancel_extract (void);

#endif
