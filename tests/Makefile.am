noinst_PROGRAMS = mb-test

mb_test_SOURCES = \
	mb-test.c \
	$(top_srcdir)/libjuicer/sj-metadata.c \
	$(top_srcdir)/libjuicer/sj-metadata-musicbrainz.c \
	$(top_srcdir)/libjuicer/sj-metadata-marshal.c \
	$(top_srcdir)/libjuicer/sj-error.c

mb_test_CPPFLAGS = \
	-I$(top_srcdir)/libjuicer \
	-I$(top_srcdir)/src \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

mb_test_CFLAGS = \
	$(WARN_CFLAGS) \
	$(MUSICBRAINZ_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(MEDIA_PROFILES_CFLAGS) \
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(AM_CPPFLAGS)

mb_test_LDADD = \
 	$(BURN_LIBS) \
	$(UI_LIBS) \
	$(MUSICBRAINZ_LIBS)

mb_test_LDFLAGS = \
	-export-dynamic \
	$(AM_LDFLAGS)
