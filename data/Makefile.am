pixmapdir = $(datadir)/pixmaps
pixmap_DATA = sound-juicer.png

privatedatadir = $(datadir)/sound-juicer/
privatedata_DATA = sound-juicer.glade orange-slice.png

desktopdir = $(datadir)/applications
desktop_in_files = sound-juicer.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = sound-juicer.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	  gconftool-2 --makefile-install-rule $(schema_DATA)

CLEANFILES = \
	$(desktop_DATA) \
	$(schema_DATA)

EXTRA_DIST = \
	$(pixmap_DATA) \
	$(privatedata_DATA) \
	$(desktop_in_files) \
	$(schema_in_files)
