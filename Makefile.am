SUBDIRS = data src tests

INTLTOOL_BUILT = \
        intltool-extract \
        intltool-merge \
        intltool-update

EXTRA_DIST = \
	autogen.sh \
	$(INTLTOOL_BUILT:=.in)

CLEANFILES = $(INTLTOOL_BUILT)
