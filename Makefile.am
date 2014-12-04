SUBDIRS = po data libjuicer src tests help

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

EXTRA_DIST = \
	autogen.sh \
	sound-juicer.spec

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL)

DISTCLEANFILES = po/.intltool-merge-cache

-include $(top_srcdir)/build-aux/git.mk
-include $(top_srcdir)/build-aux/release.mk
