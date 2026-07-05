#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(dataman, 3.3.8,  tomg_66@users.sf.net)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([clientlib/c/accept.c])
AM_CONFIG_HEADER([config.h])

AC_CONFIG_FILES([Makefile server/Makefile clientlib/c/Makefile clientlib/c++/Makefile])
AC_ARG_ENABLE(java, [  --enable-java   build java classlib],
			  [ if test $enable_java = 'yes' ; then
			  		ac_config_files="$ac_config_files clientlib/java/Makefile"
					sed -e 's/^SUBDIRS = clientlib\/c/SUBDIRS = clientlib\/java clientlib\/c/' Makefile.in > outfile
					mv -f outfile Makefile.in
				else
					sed -e 's/clientlib\/java //' Makefile.in > outfile
					mv -f outfile Makefile.in
				fi
			  ],
			  [ ac_config_files="$ac_config_files clientlib/java/Makefile"
				sed -e 's/^SUBDIRS = clientlib\/c/SUBDIRS = clientlib\/java clientlib\/c/' Makefile.in > outfile
				mv -f outfile Makefile.in
			  ]
)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h malloc.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h dirent.h fnmatch.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_HEADER_TIME
AC_DEFINE(DWINDOW, [], [Comment out this line  if you wish to use the Dataman text windowing system manually])

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([atexit bzero ftruncate gethostbyname memmove memset select socket strchr strdup strrchr strtoul scandir alphasort fnmatch])
#AC_CONFIG_FILES([Makefile server/Makefile clientlib/c/Makefile clientlib/c++/Makefile clientlib/java/Makefile])
AC_OUTPUT
