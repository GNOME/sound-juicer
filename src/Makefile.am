AM_CFLAGS = -Wall -export-dynamic

DEFS += \
	-DGNOME_DISABLE_DEPRECATED=1 \
	-DGTK_DISABLE_DEPRECATED=1 \
	-DGDK_DISABLE_DEPRECATED=1 \
	-DG_DISABLE_DEPRECATED=1 \
	-DDATADIR=\""$(datadir)/sound-juicer"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\"

INCLUDES = @LIBGLADE_CFLAGS@ @GSTREAMER_CFLAGS@ @MUSICBRAINZ_CFLAGS@ @GNOME_CFLAGS@

bin_PROGRAMS = sound-juicer

sound_juicer_SOURCES = \
	sj-main.c \
	sj-prefs.h \
	sj-prefs.c \
	sj-error.h \
	sj-error.c \
	sj-extractor.h \
	sj-extractor.c \
	sj-musicbrainz.h \
	sj-musicbrainz.c \
	sj-util.h \
	sj-util.c \
	sj-about.h \
	sj-about.c \
	sj-extracting.h \
	sj-extracting.c \
	bacon-cd-selection.h \
	bacon-cd-selection.c \
	cd-drive.h \
	cd-drive.c

sound_juicer_LDADD = @MUSICBRAINZ_LIBS@ @GSTREAMER_LIBS@ @LIBGLADE_LIBS@ @GNOME_LIBS@

#
# Update the libbacon files
#

BACONDIR=$(srcdir)/../../libbacon/src/
BACONFILES=cd-drive.c cd-drive.h bacon-cd-selection.c bacon-cd-selection.h

regenerate-built-sources:
	BACONFILES="$(BACONFILES)" BACONDIR="$(BACONDIR)" $(srcdir)/src/update-from-bacon.sh
