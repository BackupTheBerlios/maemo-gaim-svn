pkgdir = $(libdir)/gaim

ZEPHYRSOURCES = \
	ZAsyncLocate.c \
	ZCkAuth.c \
	ZCkIfNot.c \
	ZClosePort.c \
	ZCmpUID.c \
	ZCmpUIDP.c \
	ZFlsLocs.c \
	ZFlsSubs.c \
	ZFmtAuth.c \
	ZFmtList.c \
	ZFmtNotice.c \
	ZFmtRaw.c \
	ZFmtRawLst.c \
	ZFmtSmRLst.c \
	ZFmtSmRaw.c \
	ZFreeNot.c \
	ZGetLocs.c \
	ZGetSender.c \
	ZGetSubs.c \
	ZGetWGPort.c \
	ZIfNotice.c \
	ZInit.c \
	ZLocations.c \
	ZMakeAscii.c \
	ZMkAuth.c \
	ZNewLocU.c \
	ZOpenPort.c \
	ZParseNot.c \
	ZPeekIfNot.c \
	ZPeekNot.c \
	ZPeekPkt.c \
	ZPending.c \
	ZReadAscii.c \
	ZRecvNot.c \
	ZRecvPkt.c \
	ZRetSubs.c \
	ZSendList.c \
	ZSendNot.c \
	ZSendPkt.c \
	ZSendRLst.c \
	ZSendRaw.c \
	ZSetDest.c \
	ZSetFD.c \
	ZSetSrv.c \
	ZSubs.c \
	ZVariables.c \
	ZWait4Not.c \
	ZhmStat.c \
	Zinternal.c \
	com_err.h \
	error_message.c \
	error_table.h \
	et_name.c \
	init_et.c \
	internal.h \
	mit-copyright.h \
	mit-sipb-copyright.h \
	sysdep.h \
	zephyr.h \
	zephyr_err.c \
	zephyr_err.h \
	\
	zephyr.c

ZEPHYRSOURCESEXT = zephyr.c

AM_CFLAGS = $(st)

ZEPHYRLIBS = $(KRB4_LDFLAGS) $(KRB4_LIBS)

ZEPHYRLIBSEXT = $(ZEPHYR_LDFLAGS) $(ZEPHYR_LIBS)

libzephyr_la_LDFLAGS = -module -avoid-version $(GLIB_LIBS)

if STATIC_ZEPHYR

st = -DGAIM_STATIC_PRPL -Dlint
noinst_LIBRARIES = libzephyr.a
libzephyr_a_SOURCES = $(ZEPHYRSOURCES)
libzephyr_a_CFLAGS  = $(AM_CFLAGS)
libzephyr_a_LIBADD  = $(ZEPHYRLIBS)

else

st = -Dlint
pkg_LTLIBRARIES = libzephyr.la

if EXTERNAL_LIBZEPHYR
libzephyr_la_SOURCES = $(ZEPHYRSOURCESEXT)
libzephyr_la_LIBADD = $(ZEPHYRLIBSEXT)
else
libzephyr_la_SOURCES = $(ZEPHYRSOURCES)
libzephyr_la_LIBADD  = $(ZEPHYRLIBS)
endif


endif

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/protocols \
	-DCONFDIR=\"$(confdir)\" \
	$(GLIB_CFLAGS) \
	$(KRB4_CFLAGS) \
	$(DEBUG_CFLAGS)
