SUBDIRS = po data libjuicer src tests help

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	autogen.sh \
	sound-juicer.spec

MAINTAINERCLEANFILES = \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/build-aux/compile \
	$(srcdir)/build-aux/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/build-aux/config.sub \
	$(srcdir)/build-aux/depcomp \
	$(srcdir)/build-aux/install-sh \
	$(srcdir)/build-aux/ltmain.sh \
	$(srcdir)/build-aux/missing \
	$(srcdir)/build-aux/mkinstalldirs

DISTCLEANFILES = po/.intltool-merge-cache

-include $(top_srcdir)/build-aux/git.mk
-include $(top_srcdir)/build-aux/release.mk
