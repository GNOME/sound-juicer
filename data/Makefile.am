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

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-icons:
# This sucks
	for size in 16x16 22x22 24x24 32x32 scalable; do mkdir --parents $(DESTDIR)/$(datadir)/icons/hicolor/$$size/apps; done
	install -m 644 $(srcdir)/sound-juicer-16.png $(DESTDIR)/$(datadir)/icons/hicolor/16x16/apps/sound-juicer.png
	install -m 644 $(srcdir)/sound-juicer-22.png $(DESTDIR)/$(datadir)/icons/hicolor/22x22/apps/sound-juicer.png
	install -m 644 $(srcdir)/sound-juicer-24.png $(DESTDIR)/$(datadir)/icons/hicolor/24x24/apps/sound-juicer.png
	install -m 644 $(srcdir)/sound-juicer-32.png $(DESTDIR)/$(datadir)/icons/hicolor/32x32/apps/sound-juicer.png
	install -m 644 $(srcdir)/sound-juicer.svg $(DESTDIR)/$(datadir)/icons/hicolor/scalable/apps/sound-juicer.svg

	@-if test -z "$(DESTDIR)"; then \
		echo "Updating GTK icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

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
	$(schema_in_files) \
	sound-juicer-16.png sound-juicer-22.png sound-juicer-24.png sound-juicer.svg
