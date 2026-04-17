#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([libsie], [1.1.6], [http://www.somat.com/])
AC_PREREQ(2.59)

LIBSIE_MAJOR_VERSION=1
LIBSIE_MINOR_VERSION=1
LIBSIE_MICRO_VERSION=6
LIBSIE_VERSION=$LIBSIE_MAJOR_VERSION.$LIBSIE_MINOR_VERSION.$LIBSIE_MICRO_VERSION

# Making releases:
#
#    Here are a set of rules to help you update your library version
# information:
# 
#   1. Start with version information of `0:0:0' for each libtool library.
# 
#   2. Update the version information only immediately before a public
#      release of your software.  More frequent updates are unnecessary,
#      and only guarantee that the current interface number gets larger
#      faster.
# 
#   3. If the library source code has changed at all since the last
#      update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
# 
#   4. If any interfaces have been added, removed, or changed since the
#      last update, increment CURRENT, and set REVISION to 0.
# 
#   5. If any interfaces have been added since the last public release,
#      then increment AGE.
# 
#   6. If any interfaces have been removed since the last public release,
#      then set AGE to 0.

LT_CURRENT=8
LT_REVISION=4
LT_AGE=8

AC_SUBST(LIBSIE_MAJOR_VERSION)
AC_SUBST(LIBSIE_MINOR_VERSION)
AC_SUBST(LIBSIE_MICRO_VERSION)
AC_SUBST(LIBSIE_VERSION)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AC_CANONICAL_HOST

AM_INIT_AUTOMAKE

libcheck_name="check"
libcheck_tarball="check-cvs-20051222-hacked.tar.gz"


# Checks for programs.
#AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_ENABLE_SHARED
AM_PROG_LIBTOOL
AM_SANITY_CHECK

AC_CHECK_SIZEOF(double)

if test "$ac_cv_sizeof_double" = "8"; then
    sie_float64_type="double"
else
    AC_ERROR([Cannot determine float64 type])
fi

AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long)
AC_CHECK_SIZEOF(long long)

if test "$ac_cv_sizeof_int" = "4"; then
    sie_uint32_type="unsigned int"
elif test "$ac_cv_sizeof_long" = "4"; then
    sie_uint32_type="unsigned long"
elif test "$ac_cv_sizeof_long_long" = "4"; then
    sie_uint32_type="unsigned long long"
else
    AC_ERROR([Cannot determine 32-bit types])
fi

AC_SUBST(sie_float64_type)
AC_SUBST(sie_uint32_type)

AC_CONFIG_FILES([include/sie.h])

# Support large files
# AC_SYS_LARGEFILE

AC_C_BIGENDIAN([
  AC_DEFINE(SIE_BIG_ENDIAN, 1, [ Define if big-endian platform. ])
], [
  AC_DEFINE(SIE_LITTLE_ENDIAN, 1, [ Define if little-endian platform. ])
], [
  AC_ERROR(Could not determine host endianness.)
])

AC_ARG_ENABLE(wall,
  [AS_HELP_STRING([--disable-wall], [disable -Wall])],
  [test x"$enable_wall" = xyes && CFLAGS_WALL="-Wall"], [CFLAGS_WALL="-Wall"])
AC_ARG_ENABLE(werror,
  [AS_HELP_STRING([--enable-werror], [enable -Werror])],
  [test x"$enable_werror" = xyes && CFLAGS_WERROR="-Werror"], [])

AC_ARG_WITH(docs,
  [AS_HELP_STRING([--with-docs],
     [whether to try and build documentation])],
  with_docs=yes,
  with_docs=no)

AM_CONDITIONAL(WANT_DOCS, test x"$with_docs" != xno)

# Checks for libraries.

AC_ARG_WITH(apr,
  [AS_HELP_STRING([--with-apr<=DIR>],
     [where to find the Apache Portable Runtime library])],
  [],
  with_apr=yes)

if test x"$with_apr" = xno; then
  AC_DEFINE(SIE_LIBAPR_EMULATION, 1, [ Use eDAQ-only APR emulation. ])
  # KLUDGE definitely wrong, but the Classic toolchain sucks!
  CFLAGS="-D_GNU_SOURCE $CFLAGS"
else
  AC_PATH_PROG(APR_CONFIG, apr-1-config)
  if test -z "$APR_CONFIG"; then
    AC_MSG_ERROR(apr-1-config binary not found in path)
  fi
  APR_CPPFLAGS=`$APR_CONFIG --cppflags`
  APR_CFLAGS=`$APR_CONFIG --cflags`
  APR_INCLUDES=`$APR_CONFIG --includes`
  APR_LINK_LD=`$APR_CONFIG --link-ld`
  APR_LIBS=`$APR_CONFIG --libs`
fi

AC_SUBST(APR_CPPFLAGS)
AC_SUBST(APR_CFLAGS)
AC_SUBST(APR_INCLUDES)
AC_SUBST(APR_LINK_LD)
AC_SUBST(APR_LIBS)

AM_CONDITIONAL(WANT_LIBAPR, test x"$with_apr" != xno)

AC_ARG_WITH(check,
  [AS_HELP_STRING([--with-check<=DIR>],
     [where to find the libcheck library])],
  [if test -d "$with_check"; then
     LDFLAGS="$LDFLAGS -L$with_check/lib"
     CPPFLAGS="$CPPFLAGS -I$with_check/include"
   fi],
  with_check=no)

if test x"$with_check" != xno; then
  AC_DEFINE(HAVE_LIBCHECK, 1, [ Define if you have the libcheck library. ])
fi

AM_CONDITIONAL(WANT_LIBCHECK, test x"$with_check" != xno)

if test x"$with_check" = xyes; then
  # Configure local libcheck
  AC_MSG_WARN([CONFIGURING $libcheck_name])
  if ! test -d build ; then
      # KLUDGE errors!
      mkdir -p build
  fi
  if ! test -d build/$libcheck_name ; then
      # KLUDGE gunzip / tar check!
      # KLUDGE errors!
      ( cd build && gunzip -c ../tarballs/$libcheck_tarball | tar xf - )
  fi
  if ! test -f build/$libcheck_name/config.status ; then
      #( cd build/$libcheck_name && ./configure && make )
      ( cd build/$libcheck_name && ./configure )
  fi
  CPPFLAGS="-Ibuild/$libcheck_name/src $CPPFLAGS"
  AC_MSG_WARN([DONE CONFIGURING $libcheck_name])
fi


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([memmove memset strchr strdup strtol])

# $abs_top_builddir ???
# KLUDGE this largefile crap just feels wrong, figure out what's up
CPPFLAGS="-D_LARGEFILE64_SOURCE -Iinclude $CPPFLAGS"
CFLAGS="$CFLAGS -fno-strict-aliasing"
CFLAGS="$CFLAGS $APR_CFLAGS $APR_INCLUDES $CFLAGS_WALL $CFLAGS_WERROR"

AC_SUBST(libcheck_name)

AC_SUBST(CFLAGS)
AC_SUBST(DEFS)

#AC_CONFIG_HEADERS([config.h])

# KLUDGE need AC_CONFIG_HEADERS and version.h.in
#AC_CONFIG_FILES([Makefile version.h])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
