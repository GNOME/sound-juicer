INCLUDES = -I../src/ @LIBGLADE_CFLAGS@ @GSTREAMER_CFLAGS@ @MUSICBRAINZ_CFLAGS@ @GNOME_CFLAGS@
AM_CFLAGS = -g -Wall
DEFS = -DGNOME_DISABLE_DEPRECATED=1

noinst_PROGRAMS = data-test glade-test gst-test mb-test

data_test_SOURCES = data-test.c
data_test_LDADD = @GLIB_LIBS@

gst_test_SOURCES = gst-test.c ../src/sj-gstreamer.c ../src/sj-error.c
gst_test_LDADD = @GLIB_LIBS@ @GSTREAMER_LIBS@

mb_test_SOURCES = mb-test.c  ../src/sj-musicbrainz.c
mb_test_LDADD = @GLIB_LIBS@ @MUSICBRAINZ_LIBS@

glade_test_SOURCES = glade-test.c ../src/sj-error.c ../src/sj-gstreamer.c ../src/sj-musicbrainz.c ../src/bacon-cd-selection.c ../src/cd-drive.c
glade_test_LDADD = @MUSICBRAINZ_LIBS@ @GSTREAMER_LIBS@ @LIBGLADE_LIBS@ @GNOME_LIBS@
