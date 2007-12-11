privatedatadir = $(pkgdatadir)
privatedata_DATA = sound-juicer.glade sj-play.png sj-record.png

desktopdir = $(datadir)/applications
desktop_in_files = sound-juicer.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = sound-juicer.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

man1_MANS = sound-juicer.1

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

icons = \
	sound-juicer-16.png \
	sound-juicer-22.png \
	sound-juicer-24.png \
	sound-juicer-32.png \
	sound-juicer-48.png \
	sound-juicer.svg

install-icons:
	for icon in $(icons); do \
		SIZE=`echo $$icon | cut -d- -f3 | cut -d. -f1`; \
		EXTENSION=`echo $$icon | cut -d. -f2`; \
		if test -z "$$SIZE"; then \
			SIZE="scalable"; \
		else \
			SIZE="$${SIZE}x$${SIZE}"; \
		fi; \
		mkdir -p $(DESTDIR)$(datadir)/icons/hicolor/$$SIZE/apps; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/hicolor/$$SIZE/apps/sound-juicer.$$EXTENSION; \
	done
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating GTK icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

uninstall-icons:
	for icon in $(icons); do \
		SIZE=`echo $$icon | cut -d- -f3 | cut -d. -f1`; \
		EXTENSION=`echo $$icon | cut -d. -f2`; \
		if test -z "$$SIZE"; then \
			SIZE="scalable"; \
		else \
			SIZE="$${SIZE}x$${SIZE}"; \
		fi; \
		rm -f $(DESTDIR)$(datadir)/icons/hicolor/$$SIZE/apps/sound-juicer.$$EXTENSION; \
	done

install-data-local: install-icons
if GCONF_SCHEMAS_INSTALL
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p 2>&1 > /dev/null; \
		done \
	fi ||  (echo ;\
                echo "*****************************************************"; \
                echo "Installation of schemas failed, install them manually"; \
                echo "*****************************************************";)
	@true
endif

uninstall-local: uninstall-icons

CLEANFILES = \
	$(desktop_DATA) \
	$(schema_DATA)

EXTRA_DIST = \
	$(privatedata_DATA) \
	$(schema_in_files) \
	$(man1_MANS) \
	$(icons)
