EXTRA_DIST = \
		Makefile.mingw

pkgdir = $(libdir)/gaim

NOVELLSOURCES = \
		nmfield.h \
		nmfield.c \
		nmconn.h \
		nmconn.c \
		nmconference.h \
		nmconference.c \
		nmcontact.h \
		nmcontact.c \
		nmevent.h \
		nmevent.c \
		nmmessage.h \
		nmmessage.c \
		nmrequest.h \
		nmrequest.c \
		nmrtf.h \
		nmrtf.c \
		nmuser.h \
		nmuser.c \
		nmuserrecord.h \
		nmuserrecord.c \
		novell.c

AM_CFLAGS = $(st)

libnovell_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_NOVELL

st = -DGAIM_STATIC_PRPL
noinst_LIBRARIES = libnovell.a
pkg_LTLIBRARIES =

libnovell_a_SOURCES = $(NOVELLSOURCES)
libnovell_a_CFLAGS = $(AM_CFLAGS)

else

st =
pkg_LTLIBRARIES = libnovell.la
noinst_LIBRARIES =

libnovell_la_SOURCES = $(NOVELLSOURCES)

endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/src \
	$(DEBUG_CFLAGS) \
	$(GLIB_CFLAGS)
