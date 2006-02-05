# Copyright (C) 2000-2004 Marco Pesenti Gritti
# Copyright (C) 2003, 2004, 2005 Christian Persch
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA

# GECKO_INIT(VARIABLE,[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
#
# Checks for gecko, and aborts if it's not found
#
# Checks for -fshort-wchar compiler variable, and adds it to
# AM_CXXFLAGS if found
#
# Checks whether RTTI is enabled, and adds -fno-rtti to 
# AM_CXXFLAGS otherwise
#
# Checks whether the gecko build is a debug build, and adds
# debug flags to AM_CXXFLAGS if it is.
#
# Expanded variables:
# VARIABLE: Which gecko was found (e.g. "xulrunnner", "seamonkey", ...)
# VARIABLE_FLAVOUR: The flavour of the gecko that was found
# VARIABLE_HOME:
# VARIABLE_PREFIX:
# VARIABLE_INCLUDE_ROOT:
# VARIABLE_VERSION: The version of the gecko that was found
# VARIABLE_VERSION_MAJOR:
# VARIABLE_VERSION_MINOR:

AC_DEFUN([GECKO_INIT],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl

AC_MSG_CHECKING([which gecko to use])

AC_ARG_WITH([gecko],
	AS_HELP_STRING([--with-gecko@<:@=mozilla|firefox|seamonkey|xulrunner@:>@],
		       [Which gecko engine to use (autodetected by default)]))

# Backward compat
AC_ARG_WITH([mozilla],[],[with_gecko=$withval],[])

gecko_cv_gecko=$with_gecko

# Autodetect gecko
_geckos="firefox mozilla-firefox seamonkey mozilla xulrunner"
if test -z "$gecko_cv_gecko"; then
	for lizard in $_geckos; do
		if $PKG_CONFIG --exists $lizard-xpcom; then
			gecko_cv_gecko=$lizard
			break;
		fi
	done
fi

AC_MSG_RESULT([$gecko_cv_gecko])

if test "x$gecko_cv_gecko" = "x"; then
	ifelse([$3],,[AC_MSG_ERROR([No gecko found; you may need to adjust PKG_CONFIG_PATH or install a mozilla/firefox/xulrunner -devel package])],[$3])
	gecko_cv_have_gecko=no
elif ! ( echo "$_geckos" | egrep "(^| )$gecko_cv_gecko(\$| )" > /dev/null); then
	AC_MSG_ERROR([Unknown gecko "gecko_cv_gecko" specified])
else
	ifelse([$2],,[],[$2])
	gecko_cv_have_gecko=yes
fi

# ****************
# Define variables
# ****************

if test "$gecko_cv_have_gecko" = "yes"; then

gecko_cv_extra_pkg_dependencies=

case "$gecko_cv_gecko" in
mozilla) gecko_cv_gecko_flavour=mozilla gecko_cv_extra_pkg_dependencies="$gecko_cv_extra_pkg_dependencies ${gecko_cv_gecko}-gtkmozembed" ;;
seamonkey) gecko_cv_gecko_flavour=mozilla gecko_cv_extra_pkg_dependencies="$gecko_cv_extra_pkg_dependencies ${gecko_cv_gecko}-gtkmozembed" ;;
*firefox) gecko_cv_gecko_flavour=toolkit  gecko_cv_extra_pkg_dependencies="$gecko_cv_extra_pkg_dependencies ${gecko_cv_gecko}-gtkmozembed" ;;
XXxulrunner) gecko_cv_gecko_flavour=toolkit gecko_cv_extra_pkg_dependencies="$gecko_cv_extra_pkg_dependencies ${gecko_cv_gecko}-gtkmozembed" ;;
xulrunner) gecko_cv_gecko_flavour=toolkit ;;
esac

_GECKO_INCLUDE_ROOT="`$PKG_CONFIG --variable=includedir ${gecko_cv_gecko}-xpcom`"
_GECKO_HOME="`$PKG_CONFIG --variable=libdir ${gecko_cv_gecko}-xpcom`"
_GECKO_PREFIX="`$PKG_CONFIG --variable=prefix ${gecko_cv_gecko}-xpcom`"

fi # if gecko_cv_have_gecko

if test "$gecko_cv_gecko_flavour" = "toolkit"; then
	AC_DEFINE([HAVE_MOZILLA_TOOLKIT],[1],[Define if mozilla is of the toolkit flavour])
fi

AM_CONDITIONAL([HAVE_MOZILLA_TOOLKIT],[test "$gecko_cv_gecko_flavour" = "toolkit"])

$1[]=$gecko_cv_gecko
$1[]_FLAVOUR=$gecko_cv_gecko_flavour
$1[]_INCLUDE_ROOT=$_GECKO_INCLUDE_ROOT
$1[]_HOME=$_GECKO_HOME
$1[]_PREFIX=$_GECKO_PREFIX

# **************************************************************
# This is really gcc-only
# Do this test using CXX only since some versions of gcc
# 2.95-2.97 have a signed wchar_t in c++ only and some versions
# only have short-wchar support for c++.
# **************************************************************

_GECKO_EXTRA_CPPFLAGS=
_GECKO_EXTRA_CFLAGS=
_GECKO_EXTRA_CXXFLAGS=
_GECKO_EXTRA_LDFLAGS=

if test "$gecko_cv_have_gecko" = "yes"; then

AC_LANG_PUSH([C++])

_SAVE_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$CXXFLAGS $_GECKO_EXTRA_CXXFLAGS -fshort-wchar"

AC_CACHE_CHECK([for compiler -fshort-wchar option],
	gecko_cv_have_usable_wchar_option,
	[AC_RUN_IFELSE([AC_LANG_SOURCE(
		[[#include <stddef.h>
		  int main () {
		    return (sizeof(wchar_t) != 2) || (wchar_t)-1 < (wchar_t) 0 ;
		  } ]])],
		[gecko_cv_have_usable_wchar_option="yes"],
		[gecko_cv_have_usable_wchar_option="no"],
		[gecko_cv_have_usable_wchar_option="maybe (cross-compiling)"])])

CXXFLAGS="$_SAVE_CXXFLAGS"

AC_LANG_POP([C++])

if test "$gecko_cv_have_usable_wchar_option" = "yes"; then
	_GECKO_EXTRA_CXXFLAGS="-fshort-wchar"
	AM_CXXFLAGS="$AM_CXXFLAGS -fshort-wchar"
fi

fi # if gecko_cv_have_gecko

# **************
# Check for RTTI
# **************

if test "$gecko_cv_have_gecko" = "yes"; then

AC_MSG_CHECKING([whether to enable C++ RTTI])
AC_ARG_ENABLE([cpp-rtti],
	AS_HELP_STRING([--enable-cpp-rtti],[Enable C++ RTTI]),
	[],[enable_cpp_rtti=no])
AC_MSG_RESULT([$enable_cpp_rtti])

if test "$enable_cpp_rtti" = "no"; then
	_GECKO_EXTRA_CXXFLAGS="-fno-rtti $_GECKO_EXTRA_CXXFLAGS"
	AM_CXXFLAGS="-fno-rtti $AM_CXXFLAGS"
fi

fi # if gecko_cv_have_gecko

# *************
# Various tests
# *************

if test "$gecko_cv_have_gecko" = "yes"; then

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $_GECKO_EXTRA_CPPFLAGS -I$_GECKO_INCLUDE_ROOT"

AC_MSG_CHECKING([[whether we have a gtk 2 gecko build]])
AC_RUN_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #include <string.h>
                  int main(void) {
		    return strcmp (MOZ_DEFAULT_TOOLKIT, "gtk2") != 0;
		  } ]]
	)],
	[result=yes],
	[AC_MSG_ERROR([[This program needs a gtk 2 gecko build]])],
        [result=maybe])
AC_MSG_RESULT([$result])

AC_MSG_CHECKING([[whether we have a gecko debug build]])
AC_COMPILE_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #if !defined(MOZ_REFLOW_PERF) || !defined(MOZ_REFLOW_PERF_DSP)
		  #error No
		  #endif]]
	)],
	[gecko_cv_have_debug=yes],
	[gecko_cv_have_debug=no])
AC_MSG_RESULT([$gecko_cv_have_debug])

CPPFLAGS="$_SAVE_CPPFLAGS"

AC_LANG_POP([C++])

if test "$gecko_cv_have_debug" = "yes"; then
	_GECKO_EXTRA_CXXFLAGS="$_GECKO_EXTRA_CXXFLAGS -DDEBUG -D_DEBUG"
	AM_CXXFLAGS="-DDEBUG -D_DEBUG $AM_CXXFLAGS"
fi

fi # if gecko_cv_have_gecko

# ***********************
# Check for gecko version
# ***********************

if test "$gecko_cv_have_gecko" = "yes"; then

AC_MSG_CHECKING([for gecko version])

# We cannot in grep in mozilla-config.h directly, since in some setups
# (mult-arch for instance) it includes another file with the real
# definitions.

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I$_GECKO_INCLUDE_ROOT"

AC_EGREP_CPP([\"1\.9],
	[#include <mozilla-config.h>
	 MOZILLA_VERSION],
	[gecko_cv_gecko_version_major=1 gecko_cv_gecko_version_minor=9],
	[AC_EGREP_CPP([\"1\.8],
		[#include <mozilla-config.h>
		 MOZILLA_VERSION],
		[gecko_cv_gecko_version_major=1 gecko_cv_gecko_version_minor=8],
		[gecko_cv_gecko_version_major=1 gecko_cv_gecko_version_minor=7])
])

CPPFLAGS="$_SAVE_CPPFLAGS"

AC_LANG_POP([C++])

gecko_cv_gecko_version="$gecko_cv_gecko_version_major.$gecko_cv_gecko_version_minor"

AC_MSG_RESULT([$gecko_cv_gecko_version])

if test "$gecko_cv_gecko_version_major" != "1" -o "$gecko_cv_gecko_version_minor" -lt "7" -o "$gecko_cv_gecko_version_minor" -gt "9"; then
	AC_MSG_ERROR([Gecko version $gecko_cv_gecko_version is not supported!])
fi

if test "$gecko_cv_gecko_version_major" = "1" -a "$gecko_cv_gecko_version_minor" -ge "7"; then
	AC_DEFINE([HAVE_GECKO_1_7],[1],[Define if we have gecko 1.7])
	gecko_cv_have_gecko_1_7=yes
fi
if test "$gecko_cv_gecko_version_major" = "1" -a "$gecko_cv_gecko_version_minor" -ge "8"; then
	AC_DEFINE([HAVE_GECKO_1_8],[1],[Define if we have gecko 1.8])
	gecko_cv_have_gecko_1_8=yes
fi
if test "$gecko_cv_gecko_version_major" = "1" -a "$gecko_cv_gecko_version_minor" -ge "9"; then
	AC_DEFINE([HAVE_GECKO_1_9],[1],[Define if we have gecko 1.9])
	gecko_cv_have_gecko_1_9=yes
fi

fi # if gecko_cv_have_gecko

AM_CONDITIONAL([HAVE_GECKO_1_7],[test "$gecko_cv_gecko_version_major" = "1" -a "$gecko_cv_gecko_version_minor" -ge "7"])
AM_CONDITIONAL([HAVE_GECKO_1_8],[test "$gecko_cv_gecko_version_major" = "1" -a "$gecko_cv_gecko_version_minor" -ge "8"])
AM_CONDITIONAL([HAVE_GECKO_1_9],[test "$gecko_cv_gecko_version_major" = "1" -a "$gecko_cv_gecko_version_minor" -ge "9"])

$1[]_VERSION=$gecko_cv_gecko_version
$1[]_VERSION_MAJOR=$gecko_cv_gecko_version_major
$1[]_VERSION_MINOR=$gecko_cv_gecko_version_minor

])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_DISPATCH([MACRO], [INCLUDEDIRS], ...)

m4_define([GECKO_DISPATCH],
[

if test "$gecko_cv_have_gecko" != "yes"; then
	AC_MSG_FAILURE([Gecko not present; can't run this test!])
fi

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
_SAVE_CXXFLAGS="$CXXFLAGS"
_SAVE_LDFLAGS="$LDFLAGS"
CPPFLAGS="$CPPFLAGS $_GECKO_EXTRA_CPPFLAGS -I$_GECKO_INCLUDE_ROOT $($PKG_CONFIG --cflags-only-I ${gecko_cv_gecko}-xpcom)"
CXXFLAGS="$CXXFLAGS $_GECKO_EXTRA_CXXFLAGS $($PKG_CONFIG --cflags-only-other ${gecko_cv_gecko}-xpcom)"
LDFLAGS="$LDFLAGS $_GECKO_EXTRA_LDFLAGS $($PKG_CONFIG --libs ${gecko_cv_gecko}-xpcom) -Wl,--rpath=$_GECKO_HOME"

_GECKO_DISPATCH_INCLUDEDIRS="$2"

# Sigh Gentoo has a rubbish header layout
# http://bugs.gentoo.org/show_bug.cgi?id=100804
# Mind you, it's useful to be able to test against uninstalled mozilla builds...
_GECKO_DISPATCH_INCLUDEDIRS="$_GECKO_DISPATCH_INCLUDEDIRS dom necko pref"

# Now add them to CPPFLAGS
for i in $_GECKO_DISPATCH_INCLUDEDIRS; do
	CPPFLAGS="$CPPFLAGS -I$_GECKO_INCLUDE_ROOT/$i"
done

m4_indir([$1],m4_shiftn(2,$@))

CPPFLAGS="$_SAVE_CPPFLAGS"
CXXFLAGS="$_SAVE_CXXFLAGS"
LDFLAGS="$_SAVE_LDFLAGS"

AC_LANG_POP([C++])

])# _GECKO_DISPATCH

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_CHECK_HEADERS(INCLUDEDIRS, HEADERS, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [INCLUDES])

AC_DEFUN([GECKO_CHECK_HEADERS],[GECKO_DISPATCH([AC_CHECK_HEADERS],$@)])

# GECKO_COMPILE_IFELSE(INCLUDEDIRS, PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])

AC_DEFUN([GECKO_COMPILE_IFELSE],[GECKO_DISPATCH([AC_COMPILE_IFELSE],$@)])

# GECKO_RUN_IFELSE(INCLUDEDIRS, PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])

AC_DEFUN([GECKO_RUN_IFELSE],[GECKO_DISPATCH([AC_RUN_IFELSE],$@)])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_CHECK_CONTRACTID(CONTRACTID, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Checks wheter CONTRACTID is a registered contract ID

AC_DEFUN([GECKO_CHECK_CONTRACTID],
[AC_REQUIRE([GECKO_INIT])dnl

AS_VAR_PUSHDEF([gecko_cv_have_CID],[gecko_cv_have_$1])

AC_CACHE_CHECK([for the $1 XPCOM component],
[gecko_cv_have_CID],
[
AS_VAR_SET(gecko_cv_have_CID,[no])

GECKO_RUN_IFELSE([],
[AC_LANG_PROGRAM([[
#include <mozilla-config.h>
#include <stdlib.h>
#include <stdio.h>
#include <nsXPCOM.h>
#include <nsCOMPtr.h>
#include <nsILocalFile.h>
#include <nsIServiceManager.h>
#include <nsIComponentRegistrar.h>
#include <nsString.h>
]],[[
// redirect unwanted mozilla debug output
freopen ("/dev/null", "w", stdout);
freopen ("/dev/null", "w", stderr);

nsresult rv;
nsCOMPtr<nsILocalFile> directory;
rv = NS_NewNativeLocalFile (NS_LITERAL_CSTRING("$_GECKO_HOME"), PR_FALSE, getter_AddRefs (directory));
if (NS_FAILED (rv) || !directory) {
	exit (EXIT_FAILURE);
}

nsCOMPtr<nsIServiceManager> sm;
rv = NS_InitXPCOM2 (getter_AddRefs (sm), directory, nsnull);
if (NS_FAILED (rv)) {
	exit (EXIT_FAILURE);
}

nsCOMPtr<nsIComponentRegistrar> registar (do_QueryInterface (sm, &rv));
sm = nsnull; // release service manager
if (NS_FAILED (rv)) {
	NS_ShutdownXPCOM (nsnull);
	exit (EXIT_FAILURE);
}

PRBool isRegistered = PR_FALSE;
rv = registar->IsContractIDRegistered ("$1", &isRegistered);
registar = nsnull; // release registar before shutdown
	
NS_ShutdownXPCOM (nsnull);
exit (isRegistered ? EXIT_SUCCESS : EXIT_FAILURE);
]])
],
[AS_VAR_SET(gecko_cv_have_CID,[yes])],
[AS_VAR_SET(gecko_cv_have_CID,[no])],
[AS_VAR_SET(gecko_cv_have_CID,[maybe])])

])

if test AS_VAR_GET(gecko_cv_have_CID) = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_FAILURE([dnl
Contract ID "$1" is not registered, but $PACKAGE_NAME depends on it.])],
	[$3])
fi

AS_VAR_POPDEF([gecko_cv_have_CID])

])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# GECKO_XPIDL([ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
#
# Checks for xpidl program and include directory
#
# Variables set:
# XPIDL:        the xpidl program
# XPIDL_IDLDIR: the xpidl include directory

AC_DEFUN([GECKO_XPIDL],
[AC_REQUIRE([GECKO_INIT])dnl

_GECKO_LIBDIR="`$PKG_CONFIG --variable=libdir ${gecko_cv_gecko}-xpcom`"

AC_PATH_PROG([XPIDL],[xpidl],[no],[$_GECKO_LIBDIR:$PATH])

XPIDL_IDLDIR="`$PKG_CONFIG --variable=idldir ${gecko_cv_gecko}-xpcom`"

# Older gecko's don't have this variable, see
# https://bugzilla.mozilla.org/show_bug.cgi?id=240473

if test -z "$XPIDL_IDLDIR" -o ! -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	XPIDL_IDLDIR="`echo $_GECKO_LIBDIR | sed -e s!lib!share/idl!`"
fi

# Some distributions (Gentoo) have it in unusual places

if test -z "$XPIDL_IDLDIR" -o ! -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	XPIDL_IDLDIR="$_GECKO_INCLUDE_ROOT/idl"
fi

if test "$XPIDL" != "no" -a -n "$XPIDL_IDLDIR" -a -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	ifelse([$1],,[:],[$1])
else
	ifelse([$2],,[AC_MSG_FAILURE([XPIDL program or include directory not found])],[$2])
fi

])
