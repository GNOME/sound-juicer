AM_CFLAGS = -Wall -Werror -export-dynamic -W -Wno-unused-parameter

AM_CPPFLAGS = \
	-DGNOME_DISABLE_DEPRECATED=1 \
	-DGCONF_DISABLE_DEPRECATED=1 \
	-DGTK_DISABLE_DEPRECATED=1 \
	-DGDK_DISABLE_DEPRECATED=1 \
	-DG_DISABLE_DEPRECATED=1 \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DDATADIR=\""$(datadir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\" \
	@MUSICBRAINZ_CFLAGS@ @GSTREAMER_CFLAGS@ \
	@LIBGLADE_CFLAGS@  @GNOME_CFLAGS@

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
	--header > marshal.tmp \
	&& mv marshal.tmp $@ ) \
	|| ( rm -f marshal.tmp && exit 1 )

sj-metadata-marshal.c: sj-metadata-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=metadata_marshal $< \
	--body > marshal.tmp \
	&& echo "#include \"sj-metadata-marshal.h\"" > $@ \
	&& cat marshal.tmp >> $@ \
	&& rm -f marshal.tmp ) \
	|| ( rm -f marshal.tmp && exit 1 )

BUILT_SOURCES = $(MARSHALFILES)
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = sj-metadata-marshal.list
