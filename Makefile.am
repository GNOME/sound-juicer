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

tag:
# TODO: verify that the current path is trunk otherwise this fails
	svn cp -m "Sound Juicer $(VERSION)" `svnpath` `svnpath tags/$(VERSION)`

upload:
	cp -f $(PACKAGE)-$(VERSION).tar.bz2 $(WEBROOT)/computing
	scp $(PACKAGE)-$(VERSION).tar.bz2 master.gnome.org:
	ssh master.gnome.org install-module $(PACKAGE)-$(VERSION).tar.bz2

release: distcheck tag upload
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/sj-$(VERSION).txt
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/body.txt

dev-release: distcheck tag upload
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/sj-$(VERSION).txt
