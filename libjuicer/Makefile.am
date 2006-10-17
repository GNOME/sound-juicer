noinst_LTLIBRARIES = libjuicer.la

libjuicer_la_SOURCES = \
	dummy.cpp \
	$(MARSHALFILES) \
	sj-structures.h \
	sj-structures.c \
	sj-error.h sj-error.c \
	sj-extractor.h \
	sj-extractor.c \
	sj-metadata.h \
	sj-metadata.c \
	sj-metadata-musicbrainz.h \
	sj-metadata-musicbrainz.c \
	sj-util.h sj-util.c

libjuicer_la_CPPFLAGS = \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(AM_CPPFLAGS)

libjuicer_la_CFLAGS = \
	$(WARN_CFLAGS)	\
	$(MUSICBRAINZ_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(MEDIA_PROFILES_CFLAGS) \
	$(BURN_CFLAGS) \
 	$(UI_CFLAGS) \
	$(AM_CFLAGS)

libjuicer_la_LIBADD = \
	$(MUSICBRAINZ_LIBS) \
	$(MEDIA_PROFILES_LIBS) \
	$(GSTREAMER_LIBS) \
	$(BURN_LIBS) \
	$(UI_LIBS)

libjuicer_la_LDFLAGS = \
	-export-dynamic \
	$(AM_LDFLAGS)

if HAVE_CDIO
libjuicer_la_SOURCES += \
	sj-metadata-cdtext.h \
	sj-metadata-cdtext.c

libjuicer_la_CFLAGS += \
	$(CDIO_CFLAGS)

libjuicer_la_LDFLAGS += \
	$(CDIO_LIBS)
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
