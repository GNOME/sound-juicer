INCLUDES = -I../src/ @LIBGLADE_CFLAGS@ @GSTREAMER_CFLAGS@ @MUSICBRAINZ_CFLAGS@ @GNOME_CFLAGS@
AM_CFLAGS = -g -Wall -export-dynamic -Wunused
AM_DEFS = -DGNOME_DISABLE_DEPRECATED=1 -DGTK_DISABLE_DEPRECATED=1 -DGDK_DISABLE_DEPRECATED=1 -DG_DISABLE_DEPRECATED=1

noinst_PROGRAMS = mb-test

mb_test_SOURCES = mb-test.c  ../src/sj-musicbrainz.c ../src/sj-error.c
mb_test_LDADD = @GLIB_LIBS@ @MUSICBRAINZ_LIBS@

enum.c: enum.h
	glib-mkenums \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"       \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	enum.h > enum.c
