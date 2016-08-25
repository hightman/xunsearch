#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# $Id: $
# xunsearch-0.0.x configure autoconf script
#

AC_PREREQ([2.68])
AC_INIT([xunsearch], [1.4.10], [http://www.xunsearch.com/bugs])

# for automake
AM_INIT_AUTOMAKE

# Copy wright
AC_COPYRIGHT([Copyright (C)2007-2013, hightman
This package maybe copied, distributed and modified under
the terms of my License; see COPYING for more detials.])

# sorce code tree
AC_CONFIG_SRCDIR([src/conn.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_SED
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CPP
AC_PROG_EGREP

# Check large file support?
AC_SYS_LARGEFILE

# Checks for union semnum
AC_CACHE_CHECK([union semun for semaphore...], ac_cv_union_semun,
	AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
	], [union semun arg;], [ac_cv_union_semun=yes], [ac_cv_union_semun=no])
)
if test "$ac_cv_union_semun" = "yes" ; then
	AC_DEFINE(HAVE_UNION_SEMUN, 1, [Define to 1 if you have union semun])
fi

# Check sun_len in sys/un.h
AC_MSG_CHECKING([for sun_len in sys/un.h])
AC_EGREP_HEADER([sun_len], [sys/un.h],
	[
		AC_MSG_RESULT([yes])
		AC_DEFINE([HAVE_SOCKADDR_UN_SUN_LEN], 1, [Define if sockaddr_un in sys/un.h contains a sun_len component])
	],
	AC_MSG_RESULT([no])
)

# Debug flag check
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [enable debug symbol, assert and other verbose log infos]),
	[ ], [enable_debug=no]
)
if test "$enable_debug" = "yes"; then
	tmp_cflags=`echo $CFLAGS | sed 's/-g//g'`
	tmp_cxxflags=`echo $CXXFLAGS | sed 's/-g//g'`
	if test "$tmp_cflags" = "$CFLAGS" ; then CFLAGS="$CFLAGS -g"; fi
	if test "$tmp_cxxflags" = "$CXXFLAGS" ; then CXXFLAGS="$CXXFLAGS -g"; fi
	CFLAGS=`echo $CFLAGS | sed 's/-O@<:@0-9@:>@/-O0/g'`
	CXXFLAGS=`echo $CXXFLAGS | sed 's/-O@<:@0-9@:>@/-O0/g'`
	AC_DEFINE(DEBUG, 1, [Define to 1 if you want to enable debug flags])
else
	tmp_cflags=`echo $CFLAGS | sed 's/-O//g'`
	tmp_cxxflags=`echo $CXXFLAGS | sed 's/-O//g'`
	if test "$tmp_cflags" = "$CFLAGS" ; then CFLAGS="$CFLAGS -O2"; fi
	if test "$tmp_cxxflags" = "$CXXFLAGS" ; then CXXFLAGS="$CXXFLAGS -O2"; fi
	AC_DEFINE(NDEBUG, 1, [Define to 1 if you want to turn off assert statements])
fi

# Memory Cache flag check
AC_ARG_ENABLE(memory-cache,
	AS_HELP_STRING([--enable-memory-cache], [enable memory cache for search result. NOTE: cache may cause some un-stable in some platforms.]),
	[ ], [enable_memory_cache=no]
)
if test "$enable_memory_cache" != "no" ; then
	AC_DEFINE(HAVE_MEMORY_CACHE, 1, [Define to 1 if you want to enable memory cache for search result])
fi
AM_CONDITIONAL([HAVE_MEMORY_CACHE], [test "$enable_memory_cache" != "no"])

# library search dirs
ac_extra_added=
ac_search_dirs="/usr/local/<name> /usr/local /opt/local /usr"
if test "x$prefix" != "xNONE" ; then
	ac_search_dirs="$prefix/<name> $prefix $ac_search_dirs"
fi

# Libevent package check
AC_ARG_WITH(libevent,
	AS_HELP_STRING([--with-libevent@<:@=DIR@:>@],
		[specify the install directory of libevent, The version should be 2.0.0 or greater.]),
	[ ], [with_libevent=yes]
)
if test "$with_libevent" != "no" ; then
	AC_MSG_CHECKING([for event2/event.h of libevent])
	LIBEVENT_DIR=""
	if test "$with_libevent" = "yes" ; then
        search_dirs=`echo $ac_search_dirs | sed 's#<name>#libevent#g'`
		for tmpdir in $search_dirs; do
			if test -f $tmpdir/include/event2/event.h ; then
				LIBEVENT_DIR=$tmpdir
				break
			fi
		done
	elif test -f "$with_libevent/include/event2/event.h" ; then
		LIBEVENT_DIR=$with_libevent
	fi
	if test "$LIBEVENT_DIR" = "" ; then
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([event2/event.h NOT FOUND. please check your libevent install directory. >= 2.0.0])
	else
		AC_MSG_RESULT([yes: $LIBEVENT_DIR])
	fi

	# Check libevent version ( >= 2.0.0)
	AC_MSG_CHECKING([for correct libevent version])
	AC_EGREP_CPP(yes, [
#include "$LIBEVENT_DIR/include/event2/event-config.h"
#if _EVENT_NUMERIC_VERSION >= 0x02000000
yes
#endif
	], [
		AC_MSG_RESULT(ok)
	], [
		AC_MSG_ERROR([libevent 2.0.0 or greater required.])
	])

	# Check libevent library
	AC_CHECK_LIB(event_core, event_base_new,
		[
            tmp_extra_added=`echo $ac_extra_added | sed "s#$LIBEVENT_DIR##"`
			if test "$ac_extra_added" = "$tmp_extra_added" ; then
              ac_extra_added="$ac_extra_added $LIBEVENT_DIR"
			  ac_extra_added=`echo $ac_extra_added | sed "s#^\s*##"`
			  LDFLAGS="$LDFLAGS -L$LIBEVENT_DIR/lib -Wl,-rpath,$LIBEVENT_DIR/lib"
			  CFLAGS="$CFLAGS -I$LIBEVENT_DIR/include"
			  CXXFLAGS="$CXXFLAGS -I$LIBEVENT_DIR/include"
            fi
		], [
			AC_MSG_ERROR([event_base_new() NOT found in libevent, please check it first.])
		], [
			-L$LIBEVENT_DIR/lib
		]
	)
else
	AC_MSG_ERROR([libevent is required, get it from: http://libevent.org/])
fi

# scws library check
AC_ARG_WITH(scws,
	AS_HELP_STRING([--with-scws@<:@=DIR@:>@],
		[specify the install directory of scws.]),
	[ ], [with_scws=yes]
)
if test "$with_scws" != "no" ; then
	AC_MSG_CHECKING([for scws.h of scws])
	SCWS_DIR=""
	if test "$with_scws" = "yes" ; then
        search_dirs=`echo $ac_search_dirs | sed 's#<name>#scws#g'`
		for tmpdir in $search_dirs; do
			if test -f $tmpdir/include/scws/scws.h ; then
				SCWS_DIR=$tmpdir
				break
			fi
		done
	elif test -f "$with_scws/include/scws/scws.h" ; then
		SCWS_DIR=$with_scws
	fi
	if test "$SCWS_DIR" = "" ; then
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([scws.h NOT FOUND. please check your scws install directory. >= 1.1.6])
	else
		AC_MSG_RESULT([yes: $SCWS_DIR])
	fi
	
	# Etc directory (dictionary & rules)
	if test "$SCWS_DIR" = "/usr"; then
		SCWS_ETCDIR=/etc
	else
		SCWS_ETCDIR=$SCWS_DIR/etc
	fi

	# Check scws library
	AC_CHECK_LIB(scws, scws_new,
		[
            tmp_extra_added=`echo $ac_extra_added | sed "s#$SCWS_DIR##"`
			if test "$ac_extra_added" = "$tmp_extra_added" ; then
              ac_extra_added="$ac_extra_added $SCWS_DIR"
			  ac_extra_added=`echo $ac_extra_added | sed "s#^\s*##"`
			  LDFLAGS="$LDFLAGS -L$SCWS_DIR/lib -Wl,-rpath,$SCWS_DIR/lib"
			  CFLAGS="$CFLAGS -I$SCWS_DIR/include"
			  CXXFLAGS="$CXXFLAGS -I$SCWS_DIR/include"
            fi
			AC_DEFINE_UNQUOTED(SCWS_ETCDIR, "$SCWS_ETCDIR", [Resource directory of scws])
		], [
			AC_MSG_ERROR([scws_new() NOT found in libscws, please check it first.])
		], [
			-L$SCWS_DIR/lib
		]
	)
else
	AC_MSG_ERROR([scws is required, get it from: http://www.xunsearch.com/scws/])
fi

# Xapian-core package check
AC_ARG_WITH(xapian,
	AS_HELP_STRING([--with-xapian@<:@=DIR@:>@],
		[specify the install directory of xapian-core(scws patched). The version should be 1.0.2 or greater.]),
	[ ], [with_xapian=yes]
)
if test "$with_xapian" != "no" ; then
	AC_MSG_CHECKING([for xapian.h of xapian-core])
	XAPIAN_DIR=""
	if test "$with_xapian" = "yes" ; then
        search_dirs=`echo $ac_search_dirs | sed 's#<name>#xapian#g'`       
		for tmpdir in $search_dirs; do
			if test -f $tmpdir/include/xapian.h ; then
				XAPIAN_DIR=$tmpdir
				break
			fi
		done
	elif test -f "$with_xapian/include/xapian.h" ; then
		XAPIAN_DIR=$with_xapian
	fi
	if test "$XAPIAN_DIR" = "" ; then
		AC_MSG_RESULT(no)
		AC_MSG_ERROR([xapian.h NOT FOUND. Please check your xapian-core install directory.])
	else
		AC_MSG_RESULT([yes: $XAPIAN_DIR])
	fi

	# Check libxapian version ( >= 1.0.2)
	AC_MSG_CHECKING([for correct xapian version])
	AC_EGREP_CPP(yes, [
#include "$XAPIAN_DIR/include/xapian/version.h"
#if XAPIAN_MAJOR_VERSION >= 1 && (XAPIAN_MINOR_VERSION > 0 || XAPIAN_REVERSION >= 2)
yes
#endif
	], [
		AC_MSG_RESULT(ok)
	], [
		AC_MSG_ERROR([xapian-core 1.0.2 or greater required.])
	])

	# Check xapian-core library
	AC_MSG_CHECKING([for library of xapian-core])
	AC_LANG(C++)
	saved_cxxflags=$CXXFLAGS
	CXXFLAGS="$CXXFLAGS -I$XAPIAN_DIR/include"
	saved_libs=$LIBS
	LIBS="$LIBS -lxapian -L$XAPIAN_DIR/lib"
	AC_LINK_IFELSE(
		[
			AC_LANG_PROGRAM([
#include <xapian.h>
			], [
Xapian::Document doc;
Xapian::TermGenerator indexer;
indexer.load_scws(NULL, 0, 0);
indexer.set_document(doc);
			])
		], [
			LIBS=$saved_libs
			CXXFLAGS=$saved_cxxflags
            tmp_extra_added=`echo $ac_extra_added | sed "s#$XAPIAN_DIR##"`
			if test "$ac_extra_added" = "$tmp_extra_added" ; then
              ac_extra_added="$ac_extra_added $XAPIAN_DIR"
			  ac_extra_added=`echo $ac_extra_added | sed "s#^\s*##"`
			  CXXFLAGS="$CXXFLAGS -I$XAPIAN_DIR/include"
			  LDFLAGS="$LDFLAGS -L$XAPIAN_DIR/lib -Wl,-rpath,$XAPIAN_DIR/lib"
            fi
			AC_DEFINE_UNQUOTED(XAPIAN_DIR, "$XAPIAN_DIR", [Install directory of xapian])
			AC_MSG_RESULT(ok)
		], [
			AC_MSG_RESULT(no)
			AC_MSG_ERROR([Invalid xapian library, maybe not do the patch with scws?.]) 
		]
	)
else
	AC_MSG_ERROR([Xapian-core(scws patched) is required, get it from: http://www.xunsearch.com/download/packages/])
fi

# Pthread check
AC_CHECK_LIB(pthread, 
	pthread_mutex_init,
	[ 
		LIBS="$LIBS -lpthread"
	], [
		AC_MSG_ERROR([You need to have lib-pthread around])
	]
)

# Has sdk dev files?
AM_CONDITIONAL([HAVE_SDK_PHP_DEV], [test -d sdk/php/dev])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netinet/in.h stdlib.h string.h strings.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Check sizeof off_t
AC_CHECK_SIZEOF([off_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_STRTOD
#AC_CHECK_FUNCS([alarm dup2 ftruncate getcwd inet_ntoa memchr memset mkdir munmap putenv realpath rmdir setproctitle socket strcasecmp strchr strdup strerror strncasecmp strrchr])
AC_CHECK_FUNCS([setproctitle])

# Define the prefix
if test "x$prefix" = "xNONE" ; then
  ac_install_prefix=$ac_default_prefix
else
  ac_install_prefix=$prefix
fi
AC_DEFINE_UNQUOTED(PREFIX, ["$ac_install_prefix"], [Install prefix of the software])

#php option
AC_ARG_ENABLE(php,
	AS_HELP_STRING( [--enable-php], [install php sdk (yes|no, default=yes)]),
	[
		case "${enableval}" in
			yes) enable_php=yes ;;
			no)  enable_php=no ;;
			*)   AC_MSG_ERROR(bad value ${enableval} for --enable-php) ;;
		esac
	], [
		enable_php=yes
	]
)
AM_CONDITIONAL([ENABLE_PHP], [test ${enable_php} = yes])

# Generate the make file
AC_CONFIG_FILES([Makefile src/Makefile sdk/Makefile])
AC_OUTPUT([src/xs-ctl.sh src/xs-optimize.sh])
