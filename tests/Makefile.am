noinst_PROGRAMS = mb-test

mb_test_SOURCES = \
	mb-test.c

mb_test_CPPFLAGS = \
	-I$(top_srcdir)/libjuicer \
	-I$(top_srcdir)/src \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(DISABLE_DEPRECATED) \
	$(AM_CPPFLAGS)

mb_test_CFLAGS = \
	$(WARN_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(BURN_CFLAGS) \
	$(UI_CFLAGS) \
	$(AM_CPPFLAGS)

mb_test_LDADD = \
	$(BURN_LIBS) \
	$(UI_LIBS) \
	$(top_builddir)/libjuicer/libjuicer.la

mb_test_LDFLAGS = \
	-export-dynamic \
	$(AM_LDFLAGS)

-include $(top_srcdir)/git.mk
