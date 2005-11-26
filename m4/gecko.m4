dnl Copyright (C) 2000-2004 Marco Pesenti Gritti
dnl Copyright (C) 2003, 2004, 2005 Christian Persch
dnl
dnl This program is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License as published by the
dnl Free Software Foundation; either version 2 of the License, or (at your
dnl option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License along
dnl with this program; if not, write to the Free Software Foundation, Inc.,
dnl 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA

dnl GECKO_INIT([VARIABLE])
dnl
dnl Checks for gecko, and aborts if it's not found
dnl
dnl Checks for -fshort-wchar compiler variable, and adds it to
dnl CXXFLAGS and AM_CXXFLAGS if found
dnl
dnl Checks whether RTTI is enabled, and adds -fno-rtti to 
dnl CXXFLAGS and AM_CXXFLAGS otherwise
dnl
dnl Checks whether the gecko build is a debug build, and adds
dnl debug flags to CXXFLAGS and AM_CXXFLAGS if it is.
dnl
dnl Expanded variables:
dnl VARIABLE: Which gecko was found (e.g. "xulrunnner", "seamonkey", ...)
dnl VARIABLE_FLAVOUR: The flavour of the gecko that was found
dnl VARIABLE_HOME:
dnl VARIABLE_PREFIX:
dnl VARIABLE_INCLUDE_ROOT:

AC_DEFUN([GECKO_INIT],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl

AC_MSG_CHECKING([which gecko to use])

AC_ARG_WITH([gecko],
	AS_HELP_STRING([--with-gecko@<:@=mozilla|firefox|seamonkey|xulrunner@:>@],
		       [Which gecko engine to use (default: autodetect)]))

dnl Backward compat
AC_ARG_WITH([mozilla],[],[with_gecko=$withval],[])

_GECKO=$with_gecko

dnl Autodetect gecko
_geckos="firefox mozilla-firefox seamonkey mozilla xulrunner"
if test -z "$_GECKO"; then
	for lizard in $_geckos; do
		if $PKG_CONFIG --exists $lizard-xpcom; then
			_GECKO=$lizard
			break;
		fi
	done
fi

if test "x$_GECKO" = "x"; then
	AC_MSG_ERROR([No gecko found])
elif ! ( echo "$_geckos" | egrep "(^| )$_GECKO(\$| )" > /dev/null); then
	AC_MSG_ERROR([Unknown gecko "$_GECKO" specified])
fi

AC_MSG_RESULT([$_GECKO])

case "$_GECKO" in
mozilla) _GECKO_FLAVOUR=mozilla ;;
seamonkey) _GECKO_FLAVOUR=mozilla ;;
*firefox) _GECKO_FLAVOUR=toolkit ;;
xulrunner) _GECKO_FLAVOUR=toolkit ;;
esac


_GECKO_INCLUDE_ROOT="`$PKG_CONFIG --variable=includedir $_GECKO-gtkmozembed`"
_GECKO_HOME="`$PKG_CONFIG --variable=libdir $_GECKO-gtkmozembed`"
_GECKO_PREFIX="`$PKG_CONFIG --variable=prefix $_GECKO-gtkmozembed`"

$1[]=$_GECKO
$1[]_FLAVOUR=$_GECKO_FLAVOUR
$1[]_INCLUDE_ROOT=$_GECKO_INCLUDE_ROOT
$1[]_HOME=$_GECKO_HOME
$1[]_PREFIX=$_GECKO_PREFIX

dnl **************************************************************
dnl This is really gcc-only
dnl Do this test using CXX only since some versions of gcc
dnl 2.95-2.97 have a signed wchar_t in c++ only and some versions
dnl only have short-wchar support for c++.
dnl **************************************************************

AC_LANG_PUSH([C++])

_SAVE_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$CXXFLAGS -fshort-wchar"

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
	CXXFLAGS="$CXXFLAGS -fshort-wchar"
	AM_CXXFLAGS="$AM_CXXFLAGS -fshort-wchar"
fi

dnl **************
dnl Check for RTTI
dnl **************

AC_MSG_CHECKING([whether to enable C++ RTTI])
AC_ARG_ENABLE([cpp-rtti],
	AS_HELP_STRING([--enable-cpp-rtti],[Enable C++ RTTI]),
	[],[enable_cpp_rtti=no])
AC_MSG_RESULT([$enable_cpp_rtti])

if test "$enable_cpp_rtti" = "no"; then
	CXXFLAGS="-fno-rtti $CXXFLAGS"
	AM_CXXFLAGS="-fno-rtti $AM_CXXFLAGS"
fi

dnl *************
dnl Various tests
dnl *************

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS -I$_GECKO_INCLUDE_ROOT"

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
AC_PREPROC_IFELSE(
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
	CXXFLAGS="-DDEBUG -D_DEBUG $CXXFLAGS"
	AM_CXXFLAGS="-DDEBUG -D_DEBUG $AM_CXXFLAGS"
fi

])

dnl ***************************************************************************
dnl ***************************************************************************
dnl ***************************************************************************

dnl GECKO_CHECK_CONTRACTID(IDENTIFIER, CONTRACTID, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl
AC_DEFUN([GECKO_CHECK_CONTRACTID],
[AC_REQUIRE([GECKO_INIT])dnl

AC_CACHE_CHECK([for the $2 XPCOM component],
[gecko_cv_xpcom_contractid_[]$1],
[
gecko_cv_xpcom_contractid_[]$1[]=no

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
_SAVE_CXXFLAGS="$CFLAGS"
_SAVE_LDFLAGS="$LDFLAGS"
CPPFLAGS="$CPPFLAGS -I$_GECKO_INCLUDE_ROOT $($PKG_CONFIG --cflags-only-I $_GECKO-xpcom)"
CXXFLAGS="$CXXFLAGS $($PKG_CONFIG --cflags-only-other $_GECKO-xpcom)"
LDFLAGS="$LDFLAGS $($PKG_CONFIG --libs $_GECKO-xpcom) -Wl,--rpath=$_GECKO_HOME"

AC_RUN_IFELSE([AC_LANG_PROGRAM([
#include <stdlib.h>
#include <stdio.h>
#include <nsXPCOM.h>
#include <nsCOMPtr.h>
#include <nsILocalFile.h>
#include <nsIServiceManager.h>
#include <nsIComponentRegistrar.h>
#include <nsString.h>
],[
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
rv = registar->IsContractIDRegistered ("$2", &isRegistered);
registar = nsnull; // release registar before shutdown
	
NS_ShutdownXPCOM (nsnull);
exit (isRegistered ? EXIT_SUCCESS : EXIT_FAILURE);
])
],
[gecko_cv_xpcom_contractid_[]$1[]=present],
[gecko_cv_xpcom_contractid_[]$1[]="not present"],
[gecko_cv_xpcom_contractid_[]$1[]="not present (cross-compiling)"])

CPPFLAGS="$_SAVE_CPPFLAGS"
CXXFLAGS="$_SAVE_CXXFLAGS"
LDFLAGS="$_SAVE_LDFLAGS"

AC_LANG_POP([C++])

])

if test "$gecko_cv_xpcom_contractid_[]$1" = "present"; then
	ifelse([$3],,[:],[$3])
else
	ifelse([$4],,[AC_MSG_FAILURE([dnl
Contract ID "$2" is not registered, but $PACKAGE_NAME depends on it.])],
	[$4])
fi

])
