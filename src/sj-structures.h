#ifndef SJ_STRUCTURES_H
#define SJ_STRUCTURES_H

#include <glib/glist.h>

typedef struct _AlbumDetails AlbumDetails;
typedef struct _TrackDetails TrackDetails;


struct _TrackDetails {
  AlbumDetails *album;
  int number; /* track number */
  const char *title;
  const char *artist;
  int duration; /* seconds */
};

struct _AlbumDetails {
  const char* title;
  const char* artist;
  GList* tracks;
};

#endif
