#ifndef SJ_MUSICBRAINZ_H
#define SJ_MUSICBRAINZ_H

#include <glib/glist.h>
#include <glib/gerror.h>

void sj_musicbrainz_init (void);
void sj_musicbrainz_set_cdrom (const char* device);
GList* sj_musicbrainz_list_albums (GError **error);

#endif
