/*
 * Copyright (C) 2004 Ross Burton <ross@burtonini.com>
 *
 * Sound Juicer - sj-genres.c
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

#include "sound-juicer.h"

#include <glib/gi18n.h>

#include "sj-genres.h"

/**
 * Return the name of a genre, from it's number.
 */
const char*
sj_genre_name (SjGenre genre)
{
  switch(genre) {
  case BLUES:
    return _("Blues");
  case CLASSIC_ROCK:
    return _("Classic Rock");
  case COUNTRY:
    return _("Country");
  case DANCE:
    return _("Dance");
  case DISCO:
    return _("Disco");
  case FUNK:
    return _("Funk");
  case GRUNGE:
    return _("Grunge");
  case HIP_HOP:
    return _("Hip-Hop");
  case JAZZ:
    return _("Jazz");
  case METAL:
    return _("Metal");
  case NEW_AGE:
    return _("New Age");
  case OLDIES:
    return _("Oldies");
  case OTHER:
    return _("Other");
  case POP:
    return _("Pop");
  case R_AND_B:
    return _("R&B");
  case RAP:
    return _("Rap");
  case REGGAE:
    return _("Reggae");
  case ROCK:
    return _("Rock");
  case TECHNO:
    return _("Techno");
  case INDUSTRIAL:
    return _("Industrial");
  case ALTERNATIVE:
    return _("Alternative");
  case SKA:
    return _("Ska");
  case DEATH_METAL:
    return _("Death Metal");
  case PRANKS:
    return _("Pranks");
  case SOUNDTRACK:
    return _("Soundtrack");
  case EURO_TECHNO:
    return _("Euro-Techno");
  case AMBIENT:
    return _("Ambient");
  case TRIP_HOP:
    return _("Trip-Hop");
  case VOCAL:
    return _("Vocal");
  case JAZZ_AND_FUNK:
    return _("Jazz+Funk");
  case FUSION:
    return _("Fusion");
  case TRANCE:
    return _("Trance");
  case CLASSICAL:
    return _("Classical");
  case INSTRUMENTAL:
    return _("Instrumental");
  case ACID:
    return _("Acid");
  case HOUSE:
    return _("House");
  case GAME:
    return _("Game");
  case SOUND_CLIP:
    return _("Sound Clip");
  case GOSPEL:
    return _("Gospel");
  case NOISE:
    return _("Noise");
  case ALTERNROCK:
    return _("AlternRock");
  case BASS:
    return _("Bass");
  case SOUL:
    return _("Soul");
  case PUNK:
    return _("Punk");
  case SPACE:
    return _("Space");
  case MEDITATIVE:
    return _("Meditative");
  case INSTRUMENTAL_POP:
    return _("Instrumental Pop");
  case INSTRUMENTAL_ROCK:
    return _("Instrumental Rock");
  case ETHNIC:
    return _("Ethnic");
  case GOTHIC:
    return _("Gothic");
  case DARKWAVE:
    return _("Darkwave");
  case TECHNO_INDUSTRIAL:
    return _("Techno-Industrial");
  case ELECTRONIC:
    return _("Electronic");
  case POP_FOLK:
    return _("Pop-Folk");
  case EURODANCE:
    return _("Eurodance");
  case DREAM:
    return _("Dream");
  case SOUTHERN_ROCK:
    return _("Southern Rock");
  case COMEDY:
    return _("Comedy");
  case CULT:
    return _("Cult");
  case GANGSTA:
    return _("Gangsta");
  case TOP_40:
    return _("Top 40");
  case CHRISTIAN_RAP:
    return _("Christian Rap");
  case POP_FUNK:
    return _("Pop/Funk");
  case JUNGLE:
    return _("Jungle");
  case NATIVE_AMERICAN:
    return _("Native American");
  case CABARET:
    return _("Cabaret");
  case NEW_WAVE:
    return _("New Wave");
  case PSYCHADELIC:
    return _("Psychadelic");
  case RAVE:
    return _("Rave");
  case SHOWTUNES:
    return _("Showtunes");
  case TRAILER:
    return _("Trailer");
  case LO_FI:
    return _("Lo-Fi");
  case TRIBAL:
    return _("Tribal");
  case ACID_PUNK:
    return _("Acid Punk");
  case ACID_JAZZ:
    return _("Acid Jazz");
  case POLKA:
    return _("Polka");
  case RETRO:
    return _("Retro");
  case MUSICAL:
    return _("Musical");
  case ROCK_AND_ROLL:
    return _("Rock & Roll");
  case HARD_ROCK:
    return _("Hard Rock");
  default:
    return _("[unknown genre]");
  }
}

/**tho
 * The map of all genres => entry in short genre list.
 */
GenreMap genremap[] = {
{ACID, OTHER},
{ACID_JAZZ, JAZZ},
{ACID_PUNK, ROCK},
{ALTERNATIVE, ALTERNATIVE},
{ALTERNROCK, ROCK},
{AMBIENT, AMBIENT},
{BASS, DANCE},
{BLUES, BLUES},
{CABARET, OTHER},
{CHRISTIAN_RAP, RAP},
{CLASSICAL, CLASSICAL},
{CLASSIC_ROCK, ROCK},
{COMEDY, OTHER},
{COUNTRY, COUNTRY},
{CULT, OTHER},
{DANCE, DANCE},
{DARKWAVE, OTHER},
{DEATH_METAL, ROCK},
{DISCO, DANCE},
{DREAM, AMBIENT},
{ELECTRONIC, ELECTRONIC},
{ETHNIC, ETHNIC},
{EURODANCE, DANCE},
{EURO_TECHNO, DANCE},
{FUNK, R_AND_B},
{FUSION, OTHER},
{GAME, OTHER},
{GANGSTA, RAP},
{GOSPEL, R_AND_B},
{GOTHIC, ROCK},
{GRUNGE, ROCK},
{HARD_ROCK, ROCK},
{HIP_HOP, RAP},
{HOUSE, DANCE},
{INDUSTRIAL, DANCE},
{INSTRUMENTAL, INSTRUMENTAL},
{INSTRUMENTAL_POP, POP},
{INSTRUMENTAL_ROCK, ROCK},
{JAZZ_AND_FUNK, JAZZ},
{JAZZ, JAZZ},
{JUNGLE, DANCE},
{LO_FI, DANCE},
{MEDITATIVE, MEDITATIVE},
{METAL, ROCK},
{MUSICAL, OTHER},
{NATIVE_AMERICAN, ETHNIC},
{NEW_AGE, AMBIENT},
{NEW_WAVE, AMBIENT},
{NOISE, NOISE},
{OLDIES, OTHER},
{OTHER, OTHER},
{POLKA, OTHER},
{POP, POP},
{POP/FUNK, FUNK},
{POP_FOLK, POP},
{PRANKS, OTHER},
{PSYCHADELIC, DANCE},
{PUNK, ROCK},
{RAP, RAP},
{RAVE, DANCE},
{REGGAE, REGGAE},
{RETRO, OTHER},
{ROCK, ROCK},
{ROCK_AND_ROLL, ROCK},
{R_AND_B, R_AND_B},
{SHOWTUNES, OTHER},
{SKA, REGGAE},
{SOUL, R_AND_B},
{SOUNDTRACK, OTHER},
{SOUND_CLIP, OTHER},
{SOUTHERN_ROCK, ROCK},
{SPACE, OTHER},
{TECHNO, DANCE},
{TECHNO_INDUSTRIAL, DANCE},
{TOP_40, POP},
{TRAILER, OTHER},
{TRANCE, DANCE},
{TRIBAL, ETHNIC},
{TRIP_HOP, DANCE},
{VOCAL, OTHER},
{LAST_GENRE, LAST_GENRE}
};
