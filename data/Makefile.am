privatedatadir = $(pkgdatadir)
dist_privatedata_DATA = sound-juicer.ui

desktopdir = $(datadir)/applications
desktop_in_files = sound-juicer.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

schemadir = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = sound-juicer.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
@INTLTOOL_SCHEMAS_RULE@

profilesdir = $(datadir)/sound-juicer
dist_profiles_DATA = rhythmbox.gep

man1_MANS = sound-juicer.1

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

icons = \
	sound-juicer-16.png \
	sound-juicer-22.png \
	sound-juicer-24.png \
	sound-juicer-32.png \
	sound-juicer-48.png \
	sound-juicer-256.png

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

update-icon-cache:
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
install-data-hook: update-icon-cache
if GCONF_SCHEMAS_INSTALL
	@if test -z "$(DESTDIR)" ; then \
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
uninstall-hook: update-icon-cache

CLEANFILES = \
	$(desktop_DATA) \
	$(schema_DATA)

EXTRA_DIST = \
	$(schema_in_files) \
	$(man1_MANS) \
	$(icons)

-include $(top_srcdir)/git.mk
