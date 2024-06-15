AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)

AM_CFLAGS = $(GLOBAL_CFLAGS) $(PTHREAD_CFLAGS) $(libplist_CFLAGS)

AM_LDFLAGS = $(PTHREAD_LIBS) $(libplist_LIBS)

lib_LTLIBRARIES = libimobiledevice-glue-1.0.la
libimobiledevice_glue_1_0_la_LDFLAGS = $(AM_LDFLAGS) -version-info $(LIBIMOBILEDEVICE_GLUE_SO_VERSION) -no-undefined
libimobiledevice_glue_1_0_la_LIBADD =
libimobiledevice_glue_1_0_la_SOURCES =	\
	glue.c	\
	socket.c	\
	thread.c	\
	utils.c		\
	nskeyedarchive.c \
	collection.c	\
	termcolors.c	\
	cbuf.c          \
	opack.c         \
	tlv.c           \
	sha1.c          \
	sha256.c        \
	sha512.c        \
	fixedint.h      \
	common.h

if WIN32
libimobiledevice_glue_1_0_la_LDFLAGS += -avoid-version -static-libgcc
libimobiledevice_glue_1_0_la_LIBADD += -lws2_32 -lIphlpapi
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libimobiledevice-glue-1.0.pc
