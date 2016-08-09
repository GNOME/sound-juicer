bin_PROGRAMS += sound-juicer

sound_juicer_SOURCES += \
	src/sj-album-chooser-dialog.c \
	src/sj-album-chooser-dialog.h \
	src/egg-play-preview.c \
	src/egg-play-preview.h \
	src/sj-about.c \
	src/sj-about.h \
	src/sj-cell-renderer-text.c \
	src/sj-cell-renderer-text.h \
	src/sj-extracting.c \
	src/sj-extracting.h \
	src/sj-genres.c \
	src/sj-genres.h \
	src/sj-main.c \
	src/sj-main.h \
	src/sj-play.c \
	src/sj-play.h \
	src/sj-prefs.c \
	src/sj-prefs.h \
	src/sj-tree-view.c \
	src/sj-tree-view.h \
	src/sj-window-state.c \
	src/sj-window-state.h \
	src/sound-juicer.h

nodist_sound_juicer_SOURCES += $(gresource_source)

gresource_source = \
       src/sj-resources.c

gresource_xml = $(srcdir)/data/sound-juicer.gresource.xml
gresource_deps = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/data --generate-dependencies $(gresource_xml))
$(gresource_source) : $(gresource_xml) $(gresource_deps)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --generate-source --target=$@ --sourcedir=$(srcdir)/data $<

CLEANFILES += \
     $(gresource_source)
