SUBDIRS = tests

INTLTOOL_BUILT = \
        intltool-extract \
        intltool-merge \
        intltool-update

EXTRA_DIST = \
	autogen.sh \
	$(INTLTOOL_BUILT:=.in) \
	NOTES

CLEANFILES = $(INTLTOOL_BUILT)
