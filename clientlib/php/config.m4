dnl Autotools configuration for the Dataman PHP extension.

PHP_ARG_WITH([dataman],
  [for Dataman support],
  [AS_HELP_STRING([--with-dataman[=DIR]],
    [Build with Dataman client support (default DIR is /usr/local)])],
  [no])

if test "$PHP_DATAMAN" != "no"; then
  if test "$PHP_DATAMAN" = "yes"; then
    DATAMAN_DIR="/usr/local"
  else
    DATAMAN_DIR="$PHP_DATAMAN"
  fi

  if test ! -r "$DATAMAN_DIR/include/dataman/dataman.h"; then
    AC_MSG_ERROR([dataman.h not found in $DATAMAN_DIR/include/dataman])
  fi

  PHP_ADD_INCLUDE([$DATAMAN_DIR/include/dataman])
  PHP_ADD_LIBRARY_WITH_PATH(
    [dataman],
    [$DATAMAN_DIR/lib],
    [DATAMAN_SHARED_LIBADD])
  PHP_SUBST([DATAMAN_SHARED_LIBADD])

  AC_DEFINE([HAVE_DATAMAN], [1],
    [Whether Dataman support is enabled])

  PHP_NEW_EXTENSION(
    [dataman],
    [dataman.c],
    [$ext_shared],,
    [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1])
fi
