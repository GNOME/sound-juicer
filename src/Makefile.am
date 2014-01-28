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
	sj-window-state.c \
	sj-window-state.h \
	egg-play-preview.c \
	egg-play-preview.h

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
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

sound_juicer_LDADD = \
	$(top_builddir)/libjuicer/libjuicer.la \
	$(GSTREAMER_LIBS) \
	$(BURN_LIBS) \
	$(UI_LIBS)

sound_juicer_LDFLAGS = \
	$(AM_LDFLAGS)

-include $(top_srcdir)/git.mk
