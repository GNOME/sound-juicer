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

DISTCLEANFILES = po/.intltool-merge-cache \
		 $(INTLTOOL_BUILT)

distuninstallcheck_listfiles = \
	find -path ./var/scrollkeeper/\* -prune -or -type f -print
