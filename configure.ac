AC_INIT([Sound Juicer],[3.12.0],[http://bugzilla.gnome.org/enter_bug.cgi?product=sound-juicer],[sound-juicer])

AC_PREREQ(2.52)

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz check-news])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([libjuicer/sj-extractor.c])
AC_CONFIG_HEADERS([config.h])

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE

# Do the gettext/i18n stuff
GETTEXT_PACKAGE=sound-juicer
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [The name of the gettext package.])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT


# Standard C and C++ compiler mojo
AC_ISC_POSIX
AM_PROG_CC_C_O
AC_HEADER_STDC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
IT_PROG_INTLTOOL([0.50.0])

GNOME_COMPILE_WARNINGS([maximum])
GNOME_DEBUG_CHECK
GNOME_MAINTAINER_MODE_DEFINES

GTK_REQUIRED=3.4.0
GLIB_GSETTINGS

AC_CHECK_FUNC(socket,,[AC_CHECK_LIB(socket,socket)])

# Use the GNOME documentation framework
YELP_HELP_INIT

# Find GLib and GObject
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.32 gthread-2.0 gobject-2.0 gio-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

# Find libbrasero-media
PKG_CHECK_MODULES(BURN, libbrasero-media3)
AC_SUBST(BURN_CFLAGS)
AC_SUBST(BURN_LIBS)

# Set a flag if we have _can_eject, added in 2.21.5.
# oldlibs=$LIBS
LIBS="$LIBS $BURN_LIBS"
# AC_CHECK_FUNCS(nautilus_burn_drive_can_eject)
# LIBS="$oldlibs"

# Find the UI libraries
PKG_CHECK_MODULES(UI, gtk+-3.0 >= $GTK_REQUIRED gio-2.0 gmodule-export-2.0 libcanberra-gtk3 gsettings-desktop-schemas)
AC_SUBST(UI_CFLAGS)
AC_SUBST(UI_LIBS)

PKG_CHECK_MODULES(GSTREAMER, gstreamer-1.0 gstreamer-plugins-base-1.0 gstreamer-pbutils-1.0)
AC_SUBST(GSTREAMER_CFLAGS)
AC_SUBST(GSTREAMER_LIBS)

# Find the GStreamer elements we use, and warn if they are not present.
GST_ELEMENT_CHECK(vorbisenc,1.0,,AC_MSG_WARN([The 'vorbisenc' element was not found. This will cause encoding to Ogg Vorbis to fail.]))
GST_ELEMENT_CHECK(flacenc,1.0,,AC_MSG_WARN([The 'flacenc' element was not found. This will cause encoding to FLAC to fail.]))
GST_ELEMENT_CHECK(wavenc,1.0,,AC_MSG_WARN([The 'wavenc' element was not found. This will cause encoding to Wave to fail.]))
GST_ELEMENT_CHECK(giosink,1.0,,AC_MSG_WARN([The 'giosink' element was not found. This will cause Sound Juicer to fail at runtime.]))

# MusicBrainz5
PKG_CHECK_MODULES(MUSICBRAINZ5, libmusicbrainz5 >= 5.0.1 libdiscid, [have_mb5=yes], [have_mb5=no])
AC_SUBST(MUSICBRAINZ5_CFLAGS)
AC_SUBST(MUSICBRAINZ5_LIBS)
if test "$have_mb5" = "yes" ; then
        AC_DEFINE([HAVE_MUSICBRAINZ5], 1, [Whether libmusicbrainz5 is available])
else
        AC_MSG_ERROR([libmusicbrainz5 needs to be available for sound-juicer to build])
fi

# ISO-CODES
PKG_CHECK_MODULES(ISO_CODES, [iso-codes])
if $PKG_CONFIG --variable=domains iso-codes | grep 3166 >/dev/null ; then
  AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
else
  AC_MSG_ERROR([iso-codes database does not support iso3166 country codes])
fi

# Find gio for the metadata extractor
PKG_CHECK_MODULES(GIO, gio-2.0)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

# Optionally enable G* deprecations
AC_MSG_CHECKING([whether to enable deprecation warnings])
AC_ARG_ENABLE([deprecations],
        [AS_HELP_STRING([--enable-deprecations],
        [enable deprecation warnings [default=no]])],
        [],[enable_deprecations=no])
AC_MSG_RESULT([$enable_deprecations])

if test "$enable_deprecations" = "no"; then
	DISABLE_DEPRECATED=""
	AC_SUBST(DISABLE_DEPRECATED)
fi

AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

# Finally, generate the files
AC_CONFIG_FILES([
sound-juicer.spec
Makefile
data/Makefile
data/sound-juicer.desktop.in
help/Makefile
libjuicer/Makefile
po/Makefile.in
src/Makefile
tests/Makefile
])

AC_OUTPUT
