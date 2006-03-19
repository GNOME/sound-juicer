# General compiler flags
AM_CFLAGS = -Wall -export-dynamic -W -Wno-unused-parameter

# General CPP flags. When I can demand automake 1.8, this can become
# sound_juicer_CPPFLAGS.
AM_CPPFLAGS = \
	@DISABLE_DEPRECATED_CFLAGS@ \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DDATADIR=\""$(datadir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\" \
	-DPKGDATADIR=\""$(datadir)/sound-juicer"\" \
	@MUSICBRAINZ_CFLAGS@ @GSTREAMER_CFLAGS@ \
	@MEDIA_PROFILES_CFLAGS@ @BURN_CFLAGS@ @UI_CFLAGS@

# We always build Sound Juicer
bin_PROGRAMS = sound-juicer

if HAVE_CDIO
CDIO_SOURCES = 	sj-metadata-cdtext.h sj-metadata-cdtext.c
endif

sound_juicer_SOURCES = \
	dummy.cpp \
	sound-juicer.h \
	sj-structures.h \
	sj-structures.c \
	sj-main.c \
	sj-prefs.h \
	sj-prefs.c \
	sj-error.h \
	sj-error.c \
	sj-extractor.h \
	sj-extractor.c \
	sj-metadata.h \
	sj-metadata.c \
	$(CDIO_SOURCES) \
	sj-metadata-musicbrainz.h \
	sj-metadata-musicbrainz.c \
	sj-play.c \
	sj-play.h \
	sj-util.h \
	sj-util.c \
	sj-about.h \
	sj-about.c \
	sj-extracting.h \
	sj-extracting.c \
	gconf-bridge.h \
	gconf-bridge.c \
	bacon-volume.c \
	bacon-volume.h \
	$(BACON_FILES) \
	$(MARSHALFILES)

sound_juicer_LDFLAGS = @MUSICBRAINZ_LIBS@ @MEDIA_PROFILES_LIBS@ @GSTREAMER_LIBS@ @BURN_LIBS@ @UI_LIBS@
if HAVE_CDIO
sound_juicer_LDFLAGS += @CDIO_LIBS@
endif

#
# Build the GValue marshals
#

MARSHALFILES = sj-metadata-marshal.h sj-metadata-marshal.c
GLIB_GENMARSHAL =`pkg-config --variable=glib_genmarshal glib-2.0`

sj-metadata-marshal.h: sj-metadata-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=metadata_marshal $< \
	--header > marshal-header.tmp \
	&& mv marshal-header.tmp $@ ) \
	|| ( rm -f marshal-header.tmp && exit 1 )

sj-metadata-marshal.c: sj-metadata-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=metadata_marshal $< \
	--body > marshal-source.tmp \
	&& echo "#include \"sj-metadata-marshal.h\"" > $@ \
	&& cat marshal-source.tmp >> $@ \
	&& rm -f marshal-source.tmp ) \
	|| ( rm -f marshal-source.tmp && exit 1 )

BUILT_SOURCES = $(MARSHALFILES)
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = sj-metadata-marshal.list

BACON_DIR=$(srcdir)/../../libbacon/src/
BACON_FILES=bacon-message-connection.h bacon-message-connection.c

regenerate-built-sources:
	BACONFILES="$(BACON_FILES)" BACONDIR="$(BACON_DIR)" $(top_srcdir)/src/update-from-bacon.sh
