EXTRA_DIST = \
		Makefile.mingw \
		docklet-win32.c

plugindir = $(libdir)/gaim

docklet_la_LDFLAGS = -module -avoid-version $(GTK_LIBS)

if PLUGINS

plugin_LTLIBRARIES = docklet.la

docklet_la_SOURCES = \
	eggtrayicon.h \
	eggtrayicon.c \
	docklet.h \
	docklet.c \
	docklet-x11.c

endif

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-DVERSION=\"$(VERSION)\" \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS)
