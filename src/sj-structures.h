/* 
 * Copyright (C) 2003 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-structures.h
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

#ifndef SJ_STRUCTURES_H
#define SJ_STRUCTURES_H

#include <glib/glist.h>

typedef struct _AlbumDetails AlbumDetails;
typedef struct _TrackDetails TrackDetails;


struct _TrackDetails {
  AlbumDetails *album;
  int number; /* track number */
  char *title;
  char *artist;
  int duration; /* seconds */
};

struct _AlbumDetails {
  char* title;
  char* artist;
  GList* tracks;
};

#endif
