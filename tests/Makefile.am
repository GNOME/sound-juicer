AM_CPPFLAGS = \
	-DGNOME_DISABLE_DEPRECATED=1 \
	-DGTK_DISABLE_DEPRECATED=1 \
	-DGDK_DISABLE_DEPRECATED=1 \
	-DG_DISABLE_DEPRECATED=1 \
	-I$(top_srcdir)/src \
	@MUSICBRAINZ_CFLAGS@ @GSTREAMER_CFLAGS@ \
	@UI_CFLAGS@

AM_CFLAGS = -g -Wall -export-dynamic -Wunused

noinst_PROGRAMS = mb-test

mb_test_SOURCES = \
	mb-test.c \
	$(top_srcdir)/src/sj-metadata.c \
	$(top_srcdir)/src/sj-metadata-musicbrainz.c \
	$(top_srcdir)/src/sj-metadata-marshal.c \
	$(top_srcdir)/src/sj-error.c \
	$(top_srcdir)/src/cd-drive.c \
	$(top_srcdir)/src/transport.hxx \
	$(top_srcdir)/src/dvd_plus_rw_utils.cpp

mb_test_LDADD = @UI_LIBS@ @MUSICBRAINZ_LIBS@
