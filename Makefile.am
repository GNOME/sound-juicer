SUBDIRS = po data src tests help

INTLTOOL_BUILT = \
        intltool-extract \
        intltool-merge \
        intltool-update

EXTRA_DIST = \
	autogen.sh \
	sound-juicer.spec \
	xmldocs.make \
	omf.make \
	$(INTLTOOL_BUILT:=.in)

CLEANFILES = $(INTLTOOL_BUILT)

DISTCLEANFILES = po/.intltool-merge-cache

distuninstallcheck_listfiles = \
	find -path ./var/scrollkeeper/\* -prune -or -type f -print
