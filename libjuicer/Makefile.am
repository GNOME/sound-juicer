libjuicer_SOURCES += \
	libjuicer/rb-gst-media-types.c \
	libjuicer/rb-gst-media-types.h \
	libjuicer/sj-error.c \
	libjuicer/sj-error.h \
	libjuicer/sj-extractor.c \
	libjuicer/sj-extractor.h \
	libjuicer/sj-metadata.c \
	libjuicer/sj-metadata.h \
	libjuicer/sj-metadata-getter.c \
	libjuicer/sj-metadata-getter.h \
	libjuicer/sj-metadata-gvfs.c \
	libjuicer/sj-metadata-gvfs.h \
	libjuicer/sj-metadata-musicbrainz5.c \
	libjuicer/sj-metadata-musicbrainz5.h \
	libjuicer/sj-structures.c \
	libjuicer/sj-structures.h \
	libjuicer/sj-util.c \
	libjuicer/sj-util.h

sound_juicer_SOURCES += $(libjuicer_SOURCES)
sound_juicer_CFLAGS += $(CDIO_CFLAGS)
sound_juicer_LDADD += $(CDIO_LIBS)
sound_juicer_LDADD += $(MUSICBRAINZ5_LIBS)
sound_juicer_CFLAGS += $(MUSICBRAINZ5_CFLAGS)
