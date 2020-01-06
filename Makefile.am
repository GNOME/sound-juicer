bin_PROGRAMS =
noinst_PROGRAMS =
sound_juicer_SOURCES =
nodist_sound_juicer_SOURCES =
libjuicer_SOURCES =
CLEANFILES =
DISTCLEANFILES =
EXTRA_DIST =
MAINTAINERCLEANFILES =
DISTCHECK_CONFIGURE_FLAGS =

sound_juicer_CPPFLAGS = \
	-I$(top_srcdir)/libjuicer \
	-DLOCALEDIR=\""$(localedir)"\"  \
	-DTOPSRCDIR=\""$(top_srcdir)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DPIXMAPDIR=\""$(datadir)/pixmaps"\" \
	-DPKGDATADIR=\""$(datadir)/sound-juicer"\" \
	-DG_LOG_USE_STRUCTURED=1 \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

sound_juicer_CFLAGS = \
	$(GSTREAMER_CFLAGS) \
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS)

sound_juicer_LDADD = \
	@LIBINTL@ \
	$(GSTREAMER_LIBS) \
	$(BURN_LIBS) \
	$(UI_LIBS)

SUBDIRS = . po help

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST += \
	autogen.sh

sound_juicer_LDFLAGS = \
	$(AM_LDFLAGS)

include data/Makefile.am
include libjuicer/Makefile.am
include src/Makefile.am
include tests/Makefile.am

MAINTAINERCLEANFILES += \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	ABOUT-NLS \
	INSTALL

-include $(top_srcdir)/build-aux/git.mk
-include $(top_srcdir)/build-aux/release.mk
