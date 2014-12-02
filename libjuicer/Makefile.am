noinst_LTLIBRARIES = libjuicer.la

libjuicer_la_SOURCES = \
	$(MARSHALFILES) \
	rb-gst-media-types.h \
	rb-gst-media-types.c \
	sj-structures.h \
	sj-structures.c \
	sj-error.h sj-error.c \
	sj-extractor.h \
	sj-extractor.c \
	sj-metadata.h \
	sj-metadata.c \
	sj-metadata-gvfs.h \
	sj-metadata-gvfs.c \
	sj-metadata-getter.c \
	sj-metadata-getter.h \
	sj-util.h sj-util.c

libjuicer_la_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\" \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(AM_CPPFLAGS)

libjuicer_la_CFLAGS = \
	$(WARN_CFLAGS)	\
	$(GSTREAMER_CFLAGS) \
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(CDIO_CFLAGS) \
	$(AM_CFLAGS)

libjuicer_la_LIBADD = \
	$(GSTREAMER_LIBS) \
	$(BURN_LIBS) \
	$(UI_LIBS) \
	$(CDIO_LIBS)

libjuicer_la_LDFLAGS = \
	-export-dynamic \
	$(AM_LDFLAGS)

libjuicer_la_SOURCES += \
	sj-metadata-musicbrainz5.h \
	sj-metadata-musicbrainz5.c
libjuicer_la_LIBADD += $(MUSICBRAINZ5_LIBS)
libjuicer_la_CFLAGS += $(MUSICBRAINZ5_CFLAGS)


-include $(top_srcdir)/build-aux/git.mk
