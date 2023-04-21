#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([libimobiledevice-glue], [m4_esyscmd(./git-version-gen $RELEASE_VERSION)], [https://github.com/libimobiledevice/libimobiledevice-glue/issues], [], [https://libimobiledevice.org])
AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip check-news])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl libtool versioning
# +1 : 0 : +1  == adds new functions to the interface
# +1 : 0 : 0   == changes or removes functions (changes include both
#                 changes to the signature and the semantic)
#  ? :+1 : ?   == just internal changes
# CURRENT : REVISION : AGE
LIBIMOBILEDEVICE_GLUE_SO_VERSION=0:0:0

# Check if we have a version defined
if test -z $PACKAGE_VERSION; then
  AC_MSG_ERROR([PACKAGE_VERSION is not defined. Make sure to configure a source tree checked out from git or that .tarball-version is present.])
fi

dnl Minimum package versions
LIBPLIST_VERSION=2.3.0

AC_SUBST(LIBIMOBILEDEVICE_GLUE_SO_VERSION)
AC_SUBST(LIBPLIST_VERSION)

# Checks for programs.
AC_PROG_CC
#AC_PROG_CXX
AM_PROG_CC_C_O
LT_INIT

# Checks for libraries.
PKG_CHECK_MODULES(libplist, libplist-2.0 >= $LIBPLIST_VERSION)

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([asprintf strcasecmp strdup strerror strndup stpcpy vasprintf getifaddrs])

AC_CHECK_HEADER(endian.h, [ac_cv_have_endian_h="yes"], [ac_cv_have_endian_h="no"])
if test "x$ac_cv_have_endian_h" = "xno"; then
  AC_DEFINE(__LITTLE_ENDIAN,1234,[little endian])
  AC_DEFINE(__BIG_ENDIAN,4321,[big endian])
  AC_C_BIGENDIAN([ac_cv_c_bigendian="yes"], [ac_cv_c_bigendian="no"], [], [])
  if test "x$ac_cv_c_bigendian" = "xyes"; then
    AC_DEFINE(__BYTE_ORDER,4321,[big endian byte order])
  else
    AC_DEFINE(__BYTE_ORDER,1234,[little endian byte order])
  fi
fi

# Check for operating system
AC_MSG_CHECKING([for platform-specific build settings])
case ${host_os} in
  *mingw32*|*cygwin*)
    AC_MSG_RESULT([${host_os}])
    win32=true
    AC_DEFINE(WINVER, 0x0501, [minimum Windows version])
    ;;
  darwin*)
    AC_MSG_RESULT([${host_os}])
    AX_PTHREAD([], [AC_MSG_ERROR([pthread is required to build $PACKAGE])])
    AC_CHECK_FUNCS([pthread_once pthread_cancel])
    ;;
  *)
    AC_MSG_RESULT([${host_os}])
    AX_PTHREAD([], [AC_MSG_ERROR([pthread is required to build $PACKAGE_NAME])])
    AC_CHECK_FUNC(pthread_cancel, [AC_DEFINE(HAVE_PTHREAD_CANCEL)], [
      AC_CHECK_LIB(pthread, [pthread_cancel],[AC_DEFINE(HAVE_PTHREAD_CANCEL)])
    ])
    AC_CHECK_FUNC(pthread_once, [AC_DEFINE(HAVE_PTHREAD_ONCE)], [
      AC_CHECK_LIB(pthread, [pthread_once], [], [AC_MSG_ERROR([pthread with pthread_once required to build $PACKAGE_NAME])])
    ])
    ;;
esac
AM_CONDITIONAL(WIN32, test x$win32 = xtrue)

# Check if the C compiler supports __attribute__((constructor))
AC_CACHE_CHECK([wether the C compiler supports constructor/destructor attributes],
  ac_cv_attribute_constructor, [
  ac_cv_attribute_constructor=no
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
    [[
      static void __attribute__((constructor)) test_constructor(void) {
      }
      static void __attribute__((destructor)) test_destructor(void) {
      }
    ]], [])],
    [ac_cv_attribute_constructor=yes]
  )]
)
if test "$ac_cv_attribute_constructor" = "yes"; then
  AC_DEFINE(HAVE_ATTRIBUTE_CONSTRUCTOR, 1, [Define if the C compiler supports constructor/destructor attributes])
fi

AC_CHECK_MEMBER(struct dirent.d_type, AC_DEFINE(HAVE_DIRENT_D_TYPE, 1, [define if struct dirent has member d_type]),, [#include <dirent.h>])

AS_COMPILER_FLAGS(GLOBAL_CFLAGS, "-Wall -Wextra -Wmissing-declarations -Wredundant-decls -Wshadow -Wpointer-arith -Wwrite-strings -Wswitch-default -Wno-unused-parameter -fsigned-char -fvisibility=hidden")
AC_SUBST(GLOBAL_CFLAGS)

case "$GLOBAL_CFLAGS" in
  *-fvisibility=hidden*)
    AC_DEFINE([HAVE_FVISIBILITY], [1], [Define if compiled with -fvisibility=hidden])
esac

# check for large file support
AC_SYS_LARGEFILE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_CONFIG_FILES([
Makefile
src/Makefile
src/libimobiledevice-glue-1.0.pc
include/Makefile
])
AC_OUTPUT

echo "
Configuration for $PACKAGE $VERSION:
-------------------------------------------

  Install prefix: .........: $prefix

  Now type 'make' to build $PACKAGE $VERSION,
  and then 'make install' for installation.
"
