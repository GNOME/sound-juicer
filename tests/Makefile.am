AM_CPPFLAGS = \
	-DGNOME_DISABLE_DEPRECATED=1 \
	-DGTK_DISABLE_DEPRECATED=1 \
	-DGDK_DISABLE_DEPRECATED=1 \
	-DG_DISABLE_DEPRECATED=1 \
	-I$(top_srcdir)/src \
	@MUSICBRAINZ_CFLAGS@ @GSTREAMER_CFLAGS@ \
	@LIBGLADE_CFLAGS@ @GNOME_CFLAGS@

AM_CFLAGS = -g -Wall -export-dynamic -Wunused

noinst_PROGRAMS = mb-test

mb_test_SOURCES = mb-test.c $(top_srcdir)/src/sj-musicbrainz.c $(top_srcdir)/src/sj-error.c $(top_srcdir)/src/cd-drive.c $(top_srcdir)/src/transport.hxx $(top_srcdir)/src/dvd_plus_rw_utils.cpp
mb_test_LDADD = @GLIB_LIBS@ @MUSICBRAINZ_LIBS@

enum.c: enum.h
	glib-mkenums \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"       \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	enum.h > enum.c
