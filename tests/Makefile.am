noinst_PROGRAMS += tests/mb-test

tests_mb_test_SOURCES = \
	tests/mb-test.c

tests_mb_test_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(sound_juicer_CPPFLAGS)

tests_mb_test_CFLAGS = $(sound_juicer_CFLAGS)

tests_mb_test_LDADD = \
	$(sound_juicer_LDADD) \
	libjuicer/sound_juicer-sj-error.o \
	libjuicer/sound_juicer-sj-metadata.o \
	libjuicer/sound_juicer-sj-metadata-getter.o \
	libjuicer/sound_juicer-sj-metadata-gvfs.o \
	libjuicer/sound_juicer-sj-metadata-musicbrainz5.o \
	libjuicer/sound_juicer-sj-structures.o \
	libjuicer/sound_juicer-sj-util.o
