/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-musicbrainz.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Ross Burton <ross@burtonini.com>
 */

#ifndef SJ_MUSICBRAINZ_H
#define SJ_MUSICBRAINZ_H

#include <glib/glist.h>
#include <glib/gerror.h>

void sj_musicbrainz_init (void);
void sj_musicbrainz_set_cdrom (const char* device);
void sj_musicbrainz_set_proxy (const char* proxy);
void sj_musicbrainz_set_proxy_port (int proxy_port);
GList* sj_musicbrainz_list_albums (GError **error);

#endif
