bin_PROGRAMS = sound-juicer

sound_juicer_SOURCES = \
	sound-juicer.h \
	sj-main.h \
	sj-main.c \
	sj-prefs.h \
	sj-prefs.c \
	sj-play.c \
	sj-play.h \
	sj-about.h \
	sj-about.c \
	sj-extracting.h \
	sj-extracting.c \
	sj-inhibit.h \
	sj-inhibit.c \
	sj-genres.h \
	sj-genres.c \
	gconf-bridge.h \
	gconf-bridge.c \
	egg-play-preview.c \
	egg-play-preview.h \
	$(BACON_FILES)

sound_juicer_CPPFLAGS = \
	-I$(top_srcdir)/libjuicer \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"  \
	-DDATADIR=\""$(datadir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\" \
	-DPKGDATADIR=\""$(datadir)/sound-juicer"\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

sound_juicer_CFLAGS = \
	$(GSTREAMER_CFLAGS) \
	$(MEDIA_PROFILES_CFLAGS) \
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

sound_juicer_CFLAGS = \
	$(GSTREAMER_CFLAGS) \
	$(MEDIA_PROFILES_CFLAGS) \
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

sound_juicer_LDADD = \
	$(top_builddir)/libjuicer/libjuicer.la \
	$(MEDIA_PROFILES_LIBS) \
	$(GSTREAMER_LIBS) \
	$(BURN_LIBS) \
	$(UI_LIBS)

sound_juicer_LDFLAGS = \
	$(AM_LDFLAGS)

BACON_DIR=$(srcdir)/../../libbacon/src/
BACON_FILES=bacon-message-connection.h bacon-message-connection.c

regenerate-built-sources:
	$(AM_V_GEN) BACONFILES="$(BACON_FILES)" BACONDIR="$(BACON_DIR)" $(top_srcdir)/src/update-from-bacon.sh
