INCLUDES = -I../src/ @LIBGLADE_CFLAGS@ @GSTREAMER_CFLAGS@ @MUSICBRAINZ_CFLAGS@ @GNOME_CFLAGS@
AM_CFLAGS = -g -Wall -export-dynamic -Wunused
DEFS = -DGNOME_DISABLE_DEPRECATED=1

noinst_PROGRAMS = data-test glade-test gst-test mb-test

mb_test_SOURCES = mb-test.c  ../src/sj-musicbrainz.c
mb_test_LDADD = @GLIB_LIBS@ @MUSICBRAINZ_LIBS@

glade_test_SOURCES = glade-test.c prefs-test.c ../src/sj-error.c ../src/sj-extractor.c ../src/sj-musicbrainz.c ../src/bacon-cd-selection.c ../src/cd-drive.c ../src/sj-util.c
glade_test_LDADD = @MUSICBRAINZ_LIBS@ @GSTREAMER_LIBS@ @LIBGLADE_LIBS@ @GNOME_LIBS@


enum.c: enum.h
	glib-mkenums \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"       \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	enum.h > enum.c
