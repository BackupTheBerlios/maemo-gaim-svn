EXTRA_DIST = \
		COPYRIGHT \
		Doxyfile.in \
		gaim.pc.in \
		gaim.spec.in \
		gaim.apspec.in \
		gaim.desktop \
		gaim.sh.in \
		gaim.service.in \
		gaim2blt.pl \
		gaim-installer.nsi \
		HACKING \
		PROGRAMMING_NOTES \
		licq2gaim.pl \
		setup-gettext \
		ChangeLog.win32 \
		config.h.mingw \
		Makefile.mingw \
		README.mingw \
		VERSION \
		VERSION.in \
		plugins/win32/transparency/Makefile.mingw \
		plugins/win32/transparency/win2ktrans.c \
		plugins/win32/winprefs/gtkappbar.c \
		plugins/win32/winprefs/gtkappbar.h \
		plugins/win32/winprefs/Makefile.mingw \
		plugins/win32/winprefs/winprefs.c \
		po/Makefile.mingw

gaimincludedir=$(includedir)/gaim
gaiminclude_HEADERS = config.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gaim.pc

dist-hook: gaim.spec
	cp gaim.spec $(distdir)

distcheck-hook: plugins/perl/common/Gaim.pm
#	cp plugins/perl/common/Gaim.pm $(distdir)/plugins/perl/common

appsdir = $(datadir)/applications/hildon
apps_DATA = gaim.desktop

srvcdir = $(datadir)/dbus-1/services
srvc_DATA = gaim.service

bin_SCRIPTS = gaim.sh

SUBDIRS = doc intl pixmaps plugins po sounds src

docs: Doxyfile
if HAVE_DOXYGEN
	@echo "Running doxygen..."
	@doxygen
else
	@echo "doxygen was not found during configure.  Aborting."
	@echo;
endif

gaim.sh: gaim.sh.in
	@sed -e 's!\@bindir\@!$(bindir)!g' < $< > $@

gaim.service: gaim.service.in
	@sed -e 's!\@bindir\@!$(bindir)!g' < $< > $@
