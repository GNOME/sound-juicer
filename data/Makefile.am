desktopdir = $(datadir)/applications
desktop_in_files = data/sound-juicer.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

# Use a custom set of keywords to prevent the icon name from being translated
$(desktop_DATA): data/sound-juicer.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --keyword= --keyword=Name             \
		--keyword=GenericName --keyword=Comment --keyword=Keywords   \
		--template $< -d $(top_srcdir)/po -o $@

DISTCHECK_CONFIGURE_FLAGS += --enable-appstream-util
@APPSTREAM_XML_RULES@
appstream_XML = $(appstream_in_files:.xml.in=.xml)
appstream_in_files = data/appdata/sound-juicer.appdata.xml.in

$(appstream_XML): $(appstream_in_files)
	$(MKDIR_P) data/appdata
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@

gsettings_SCHEMAS = data/org.gnome.sound-juicer.gschema.xml

@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = data/sound-juicer.convert

profilesdir = $(datadir)/sound-juicer
dist_profiles_DATA = data/rhythmbox.gep

man1_MANS = data/sound-juicer.1

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

icons = \
	data/sound-juicer-16.png \
	data/sound-juicer-22.png \
	data/sound-juicer-24.png \
	data/sound-juicer-32.png \
	data/sound-juicer-48.png \
	data/sound-juicer-256.png

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

uninstall-local: uninstall-icons
uninstall-hook: update-icon-cache

CLEANFILES += \
	$(desktop_DATA) \
	$(appstream_XML)

DISTCLEANFILES += \
	$(desktop_DATA)

EXTRA_DIST += \
	$(gsettings_SCHEMAS) \
	data/sound-juicer.convert \
	$(man1_MANS) \
	$(icons) \
	$(appstream_in_files) \
	$(gresource_xml) \
	$(gresource_deps)

MAINTAINERCLEANFILES += $(gsettings_SCHEMAS:.xml=.valid)
