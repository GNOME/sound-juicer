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
	@MEDIA_PROFILES_CFLAGS@ @BURN_CFLAGS@ @UI_CFLAGS@ @HAL_CFLAGS@


# We always build Sound Juicer
bin_PROGRAMS = sound-juicer

sound_juicer_SOURCES = \
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
	sj-genres.c \
	sj-genres.h \
	sj-metadata.h \
	sj-metadata.c \
	sj-metadata-musicbrainz.h \
	sj-metadata-musicbrainz.c \
	sj-util.h \
	sj-util.c \
	sj-about.h \
	sj-about.c \
	sj-extracting.h \
	sj-extracting.c \
	$(MARSHALFILES)

sound_juicer_LDFLAGS = @MUSICBRAINZ_LIBS@ @MEDIA_PROFILES_LIBS@ @GSTREAMER_LIBS@ @BURN_LIBS@ @UI_LIBS@ @HAL_LIBS@

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
