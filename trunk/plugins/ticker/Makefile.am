EXTRA_DIST = \
		Makefile.mingw

plugindir = $(libdir)/gaim

ticker_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

plugin_LTLIBRARIES = ticker.la

ticker_la_SOURCES = \
	gtkticker.c \
	gtkticker.h \
	ticker.c

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
