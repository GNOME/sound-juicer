/* 
 * Copyright (C) 2004 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-genres.h
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

#ifndef SJ_GENRES_H
#define SJ_GENRES_H

/**
 * List of genre names and numbers. Annoyingly genres are counted from 0 thus
 * the LAST trick doesn't work.  Always -1 from LAST_GENRE!
 */
typedef enum {
  BLUES = 0, /* 0 */
  CLASSIC_ROCK, /* 1 */
  COUNTRY, /* 2 */
  DANCE, /* 3 */
  DISCO, /* 4 */
  FUNK, /* 5 */
  GRUNGE, /* 6 */
  HIP_HOP, /* 7 */
  JAZZ, /* 8 */
  METAL, /* 9 */
  NEW_AGE, /* 10 */
  OLDIES, /* 11 */
  OTHER, /* 12 */
  POP, /* 13 */
  R_AND_B, /* 14 */
  RAP, /* 15 */
  REGGAE, /* 16 */
  ROCK, /* 17 */
  TECHNO, /* 18 */
  INDUSTRIAL, /* 19 */
  ALTERNATIVE, /* 20 */
  SKA, /* 21 */
  DEATH_METAL, /* 22 */
  PRANKS, /* 23 */
  SOUNDTRACK, /* 24 */
  EURO_TECHNO, /* 25 */
  AMBIENT, /* 26 */
  TRIP_HOP, /* 27 */
  VOCAL, /* 28 */
  JAZZ_AND_FUNK, /* 29 */
  FUSION, /* 30 */
  TRANCE, /* 31 */
  CLASSICAL, /* 32 */
  INSTRUMENTAL, /* 33 */
  ACID, /* 34 */
  HOUSE, /* 35 */
  GAME, /* 36 */
  SOUND_CLIP, /* 37 */
  GOSPEL, /* 38 */
  NOISE, /* 39 */
  ALTERNROCK, /* 40 */
  BASS, /* 41 */
  SOUL, /* 42 */
  PUNK, /* 43 */
  SPACE, /* 44 */
  MEDITATIVE, /* 45 */
  INSTRUMENTAL_POP, /* 46 */
  INSTRUMENTAL_ROCK, /* 47 */
  ETHNIC, /* 48 */
  GOTHIC, /* 49 */
  DARKWAVE, /* 50 */
  TECHNO_INDUSTRIAL, /* 51 */
  ELECTRONIC, /* 52 */
  POP_FOLK, /* 53 */
  EURODANCE, /* 54 */
  DREAM, /* 55 */
  SOUTHERN_ROCK, /* 56 */
  COMEDY, /* 57 */
  CULT, /* 58 */
  GANGSTA, /* 59 */
  TOP_40, /* 60 */
  CHRISTIAN_RAP, /* 61 */
  POP_FUNK, /* 62 */
  JUNGLE, /* 63 */
  NATIVE_AMERICAN, /* 64 */
  CABARET, /* 65 */
  NEW_WAVE, /* 66 */
  PSYCHADELIC, /* 67 */
  RAVE, /* 68 */
  SHOWTUNES, /* 69 */
  TRAILER, /* 70 */
  LO_FI, /* 71 */
  TRIBAL, /* 72 */
  ACID_PUNK, /* 73 */
  ACID_JAZZ, /* 74 */
  POLKA, /* 75 */
  RETRO, /* 76 */
  MUSICAL, /* 77 */
  ROCK_AND_ROLL, /* 78 */
  HARD_ROCK, /* 79 */
  LAST_GENRE
} SjGenre;

const char* sj_genre_name (SjGenre genre);

typedef struct {
  SjGenre genre;
  SjGenre canonical;
} GenreMap;

/* TODO: a little crappy. have an accessor? */
const GenreMap genremap[81];

#endif /* SJ_GENRES_H */
