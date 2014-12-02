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

WEBROOT := $(HOME)/WebSites/Burtonini/htdocs

tag:
	git tag -f $(VERSION)

upload:
	scp $(PACKAGE)-$(VERSION).tar.bz2 master.gnome.org:
	ssh master.gnome.org install-module $(PACKAGE)-$(VERSION).tar.bz2
	mv -f $(PACKAGE)-$(VERSION).tar.bz2 $(WEBROOT)/computing

dev-release: distcheck tag upload
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/sj-$(VERSION).txt

release: dev-release
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/body.txt

-include $(top_srcdir)/build-aux/git.mk
