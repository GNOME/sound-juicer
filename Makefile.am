SUBDIRS = po data libjuicer src tests help

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	autogen.sh \
	sound-juicer.spec \
	gnome-doc-utils.make

DISTCLEANFILES = po/.intltool-merge-cache \
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper


WEBROOT := $(HOME)/WebSites/Burtonini/htdocs

tag:
# TODO: verify that the current path is trunk otherwise this fails
	svn cp -m "Sound Juicer $(VERSION)" `svnpath` `svnpath tags/$(VERSION)`

upload:
	scp $(PACKAGE)-$(VERSION).tar.bz2 master.gnome.org:
	ssh master.gnome.org install-module $(PACKAGE)-$(VERSION).tar.bz2
	mv -f $(PACKAGE)-$(VERSION).tar.bz2 $(WEBROOT)/computing

dev-release: distcheck tag upload
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/sj-$(VERSION).txt

release: dev-release
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/body.txt
