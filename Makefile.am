SUBDIRS = po data libjuicer src tests help

ACLOCAL_AMFLAGS = -I m4

INTLTOOL_BUILT = \
        intltool-extract \
        intltool-merge \
        intltool-update

EXTRA_DIST = \
	autogen.sh \
	sound-juicer.spec \
	gnome-doc-utils.make \
	$(INTLTOOL_BUILT:=.in)

DISTCLEANFILES = po/.intltool-merge-cache \
	gnome-doc-utils.make \
	$(INTLTOOL_BUILT)

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper


WEBROOT := $(HOME)/WebSites/Burtonini/htdocs

release: dev-release
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/body.txt

dev-release: distcheck
	svn cp -m "Tag $(VERSION)" `svnpath` `svnpath tags/$(VERSION)`
	cp -f $(PACKAGE)-$(VERSION).tar.bz2 $(WEBROOT)/computing
	scp $(PACKAGE)-$(VERSION).tar.bz2 master.gnome.org:
	ssh master.gnome.org install-module $(PACKAGE)-$(VERSION).tar.bz2
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/sj-$(VERSION).txt
