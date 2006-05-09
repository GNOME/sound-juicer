# General compiler flags
AM_CFLAGS = -export-dynamic @WARN_CFLAGS@

# General CPP flags. When I can demand automake 1.8, this can become
# sound_juicer_CPPFLAGS.
AM_CPPFLAGS = \
	-I$(top_srcdir)/libjuicer \
	@DISABLE_DEPRECATED_CFLAGS@ \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"  \
	-DDATADIR=\""$(datadir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\" \
	-DPKGDATADIR=\""$(datadir)/sound-juicer"\" \
	@GSTREAMER_CFLAGS@ @MEDIA_PROFILES_CFLAGS@ @BURN_CFLAGS@ @UI_CFLAGS@

if HAVE_TAGLIB
AM_CPPFLAGS += @TAGLIB_CFLAGS@
endif

# We always build Sound Juicer
bin_PROGRAMS = sound-juicer

if HAVE_TAGLIB
ID3MUX_FILES = gsttaglib.cc gsttaglib.h
endif

sound_juicer_SOURCES = \
	dummy.cpp \
	sound-juicer.h \
	sj-main.c \
	sj-prefs.h \
	sj-prefs.c \
	sj-play.c \
	sj-play.h \
	sj-about.h \
	sj-about.c \
	sj-extracting.h \
	sj-extracting.c \
	gconf-bridge.h \
	gconf-bridge.c \
	bacon-volume.c \
	bacon-volume.h \
	$(BACON_FILES) \
	$(ID3MUX_FILES)

sound_juicer_LDADD = $(top_builddir)/libjuicer/libjuicer.la
sound_juicer_LDFLAGS = @MEDIA_PROFILES_LIBS@ @GSTREAMER_LIBS@ @BURN_LIBS@ @UI_LIBS@
if HAVE_TAGLIB
sound_juicer_LDFLAGS += @TAGLIB_LIBS@ -lgsttag-0.10
endif

BACON_DIR=$(srcdir)/../../libbacon/src/
BACON_FILES=bacon-message-connection.h bacon-message-connection.c

regenerate-built-sources:
	BACONFILES="$(BACON_FILES)" BACONDIR="$(BACON_DIR)" $(top_srcdir)/src/update-from-bacon.sh
