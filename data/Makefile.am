privatedatadir = $(datadir)/sound-juicer/
privatedata_DATA = sound-juicer.glade sj-play.png sj-record.png

desktopdir = $(datadir)/applications
desktop_in_files = sound-juicer.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = sound-juicer.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

install-icons:
	for size in 16x16 22x22 24x24 scalable; do mkdir --parents $(DESTDIR)/$(datadir)/icons/hicolor/$$size/apps; done
	install sound-juicer-16.png $(DESTDIR)/$(datadir)/icons/hicolor/16x16/apps/sound-juicer.png
	install sound-juicer-22.png $(DESTDIR)/$(datadir)/icons/hicolor/22x22/apps/sound-juicer.png
	install sound-juicer-24.png $(DESTDIR)/$(datadir)/icons/hicolor/24x24/apps/sound-juicer.png
	install sound-juicer.svg $(DESTDIR)/$(datadir)/icons/hicolor/scalable/apps/sound-juicer.svg

install-data-local: install-icons
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	  $(GCONFTOOL) --makefile-install-rule $(schema_DATA) || \
                (echo ;\
                echo "*****************************************************"; \
                echo "Installation of schemas failed, install them manually"; \
                echo "*****************************************************";)
	@true


CLEANFILES = \
	$(desktop_DATA) \
	$(schema_DATA)

EXTRA_DIST = \
	$(privatedata_DATA) \
	$(desktop_in_files) \
	$(schema_in_files)
