SUBDIRS = po data src tests help

INTLTOOL_BUILT = \
        intltool-extract \
        intltool-merge \
        intltool-update

doap.rdf: doap.rdf.in Makefile.am
	sed -e s/\@VERSION\@/$(VERSION)/g \
	    -e s/\@MAJMIN\@/`echo $(VERSION) | sed "s|\([[:digit:]]\+\.[[:digit:]]\+\).*|\1|"`/g \
	    -e s/\@DATE\@/`date +%F`/g \
	< $< > $@

EXTRA_DIST = \
	autogen.sh \
	sound-juicer.spec \
	gnome-doc-utils.make \
	doap.rdf doap.rdf.in \
	$(INTLTOOL_BUILT:=.in)

CLEANFILES = doap.rdf

DISTCLEANFILES = po/.intltool-merge-cache \
	gnome-doc-utils.make \
	$(INTLTOOL_BUILT)

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper


WEBROOT := $(HOME)/WebSites/Burtonini/htdocs
release: dist
# TODO: tag
	cp -f doap.rdf $(WEBROOT)/blosxom/computers/sound-juicer
	cp -f $(PACKAGE)-$(VERSION).tar.gz $(WEBROOT)/computing
	scp $(PACKAGE)-$(VERSION).tar.gz master.gnome.org:
	ssh master.gnome.org install-module $(PACKAGE)-$(VERSION).tar.gz
	gcq $(WEBROOT)/blosxom/computers/sound-juicer/sj-$(VERSION).txt
