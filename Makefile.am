SUBDIRS = po data src tests help

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
