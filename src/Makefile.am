# General compiler flags
AM_CFLAGS = -Wall -Werror -export-dynamic -W -Wno-unused-parameter

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
	$(BACONFILES) \
	$(MARSHALFILES)

sound_juicer_CPPFLAGS = \
	@DISABLE_DEPRECATED_CFLAGS@ \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DDATADIR=\""$(datadir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\" \
	-DPKGDATADIR=\""$(datadir)/sound-juicer"\" \
	@MUSICBRAINZ_CFLAGS@ @GSTREAMER_CFLAGS@ \
	@LIBGLADE_CFLAGS@  @GNOME_CFLAGS@

sound_juicer_LDFLAGS = @MUSICBRAINZ_LIBS@ @GSTREAMER_LIBS@ @LIBGLADE_LIBS@ @GNOME_LIBS@


#
# Update the libbacon files
#

BACONDIR=$(srcdir)/../../libbacon/src/
BACONFILES=cd-drive.c cd-drive.h bacon-cd-selection.c bacon-cd-selection.h dvd_plus_rw_utils.cpp transport.hxx

regenerate-built-sources:
	BACONFILES="$(BACONFILES)" BACONDIR="$(BACONDIR)" $(top_srcdir)/src/update-from-bacon.sh


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
