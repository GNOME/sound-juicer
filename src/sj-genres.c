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
{BLUES, BLUES},
{CLASSIC_ROCK, ROCK},
{COUNTRY, COUNTRY},
{DANCE, DANCE},
{DISCO, DISCO},
{FUNK, FUNK},
{GRUNGE, GRUNGE},
{HIP_HOP, RAP},
{JAZZ, JAZZ},
{METAL, METAL},
{NEW_AGE, NEW_AGE},
{OLDIES, OLDIES},
{OTHER, OTHER},
{POP, POP},
{R_AND_B, R_AND_B},
{RAP, RAP},
{REGGAE, REGGAE},
{ROCK, ROCK},
{TECHNO, TECHNO},
{INDUSTRIAL, INDUSTRIAL},
{ALTERNATIVE, ALTERNATIVE},
{SKA, SKA},
{DEATH_METAL, DEATH_METAL},
{PRANKS, PRANKS},
{SOUNDTRACK, SOUNDTRACK},
{EURO_TECHNO, EURO_TECHNO},
{AMBIENT, AMBIENT},
{TRIP_HOP, TRIP_HOP},
{VOCAL, VOCAL},
{JAZZ+FUNK, JAZZ+FUNK},
{FUSION, FUSION},
{TRANCE, TRANCE},
{CLASSICAL, CLASSICAL},
{INSTRUMENTAL, INSTRUMENTAL},
{ACID, ACID},
{HOUSE, HOUSE},
{GAME, GAME},
{SOUND_CLIP, SOUND_CLIP},
{GOSPEL, GOSPEL},
{NOISE, NOISE},
{ALTERNROCK, ALTERNROCK},
{BASS, BASS},
{SOUL, SOUL},
{PUNK, PUNK},
{SPACE, SPACE},
{MEDITATIVE, MEDITATIVE},
{INSTRUMENTAL_POP, POP},
{INSTRUMENTAL_ROCK, ROCK},
{ETHNIC, ETHNIC},
{GOTHIC, GOTHIC},
{DARKWAVE, DARKWAVE},
{TECHNO_INDUSTRIAL, TECHNO_INDUSTRIAL},
{ELECTRONIC, ELECTRONIC},
{POP_FOLK, POP_FOLK},
{EURODANCE, EURODANCE},
{DREAM, DREAM},
{SOUTHERN_ROCK, ROCK},
{COMEDY, COMEDY},
{CULT, CULT},
{GANGSTA, RAP},
{TOP_40, TOP_40},
{CHRISTIAN_RAP, RAP},
{POP/FUNK, POP/FUNK},
{JUNGLE, JUNGLE},
{NATIVE_AMERICAN, NATIVE_AMERICAN},
{CABARET, CABARET},
{NEW_WAVE, NEW_WAVE},
{PSYCHADELIC, PSYCHADELIC},
{RAVE, RAVE},
{SHOWTUNES, SHOWTUNES},
{TRAILER, TRAILER},
{LO_FI, LO_FI},
{TRIBAL, TRIBAL},
{ACID_PUNK, ACID_PUNK},
{ACID_JAZZ, JAZZ},
{POLKA, POLKA},
{RETRO, RETRO},
{MUSICAL, MUSICAL},
{ROCK_AND_ROLL, ROCK},
{HARD_ROCK, ROCK},
{LAST_GENRE, LAST_GENRE}
};
