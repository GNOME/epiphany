dnl Copyright (C) 2000-2004 Marco Pesenti Gritti
dnl Copyright (C) 2003, 2004 Christian Persch
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

AC_INIT([GNOME Web browser], [1.5.0],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

GNOME_COMMON_INIT

AC_PREREQ([2.57])

AM_CONFIG_HEADER([config.h])
AC_CONFIG_SRCDIR([configure.ac])

EPIPHANY_MAJOR=1.5
AC_SUBST([EPIPHANY_MAJOR])

AM_INIT_AUTOMAKE([dist-bzip2 check-news])

AM_MAINTAINER_MODE

AC_PROG_INTLTOOL([0.29])

GLIB_REQUIRED=2.5.3
PANGO_REQUIRED=1.5.1
GTK_REQUIRED=2.5.3
LIBXML_REQUIRED=2.6.6
LIBGNOMEVFS_REQUIRED=2.3.1
LIBGLADE_REQUIRED=2.3.1
LIBGNOMEUI_REQUIRED=2.6.0

AC_SUBST([GLIB_REQUIRED])
AC_SUBST([PANGO_REQUIRED])
AC_SUBST([GTK_REQUIRED])
AC_SUBST([LIBXML_REQUIRED])
AC_SUBST([LIBGNOMEVFS_REQUIRED])
AC_SUBST([LIBGLADE_REQUIRED])
AC_SUBST([LIBGNOMEUI_REQUIRED])

AC_ENABLE_SHARED([yes])
AC_ENABLE_STATIC([no])

AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INTLTOOL
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

AC_SUBST([AM_CXXFLAGS])

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([error])

PKG_CHECK_MODULES([EPIPHANY_DEPENDENCY], \
		  glib-2.0 >= $GLIB_REQUIRED \
		  pango >= $PANGO_REQUIRED \
		  gtk+-2.0 >= $GTK_REQUIRED \
		  libxml-2.0 >= $LIBXML_REQUIRED \
		  libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED \
		  libglade-2.0 >= $LIBGLADE_REQUIRED \
		  bonobo-activation-2.0 \
		  ORBit-2.0 \
		  libglade-2.0 >= $LIBGLADE_REQUIRED \
		  gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED \
		  gnome-vfs-module-2.0 \
		  gconf-2.0 \
		 )
AC_SUBST([EPIPHANY_DEPENDENCY_CFLAGS])
AC_SUBST([EPIPHANY_DEPENDENCY_LIBS])

ORBIT_IDL="`$PKG_CONFIG --variable=orbit_idl ORBit-2.0`"
AC_SUBST([ORBIT_IDL])

LIBBONOBO_IDL="`$PKG_CONFIG --variable=idldir libbonobo-2.0`"
AC_SUBST([LIBBONOBO_IDL])

BONOBO_ACTIVATION_IDL="`$PKG_CONFIG --variable=idldir bonobo-activation-2.0`"
AC_SUBST([BONOBO_ACTIVATION_IDL])

dnl whether to build with DEBUG defined
AC_ARG_WITH([mozilla-debug],
	    AC_HELP_STRING([--mozilla-debug],[Whether mozilla is a debug build]),
	    [AM_CXXFLAGS="-DDEBUG $AM_CXXFLAGS"])

AC_ARG_ENABLE([cpp-rtti],
	      AC_HELP_STRING([--enable-cpp-rtti],[Enable C++ RTTI]),[],
	      [enable_cpp_rtti=no])

if test "x$enable_cpp_rtti" = "xno"; then
  AM_CXXFLAGS="-fno-rtti $AM_CXXFLAGS"
fi

dnl Specify the gconf configuration source, 
dnl default to xml::$(sysconfdir)/gconf/gconf.xml.defaults

AC_PATH_PROG([GCONFTOOL], [gconftool-2], [no])

if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

AM_GCONF_SOURCE_2

dnl ******************
dnl Portability checks
dnl ******************

AC_CHECK_FUNCS([mkdtemp])

dnl ******************
dnl Mozilla name check
dnl ******************

AC_MSG_CHECKING([which mozilla to use])

MOZILLA=
AC_ARG_WITH([mozilla],
	AC_HELP_STRING([--with-mozilla@<:@=mozilla|firefox|thunderbird@:>@],
		       [Whether to use mozilla, firefox or thunderbird gtkmozembed (default: mozilla)]),
	[MOZILLA="$withval"])

dnl try to autodetect: if mozilla exists, use it, else try firefox, then thunderbird
if test "x$MOZILLA" = "x"; then
  if pkg-config --exists mozilla-gtkmozembed; then
    MOZILLA=mozilla
  elif pkg-config --exists firefox-gtkmozembed; then
    MOZILLA=firefox
  elif pkg-config --exists thunderbird-gtkmozembed; then
    MOZILLA=thunderbird
  else
    AC_MSG_ERROR([no mozilla installation found])
  fi
elif test "x$with_mozilla" != "xmozilla" -a "x$with_mozilla" != "xfirefox" -a "x$with_mozilla" != "xthunderbird"; then
  AC_MSG_ERROR([unknown mozilla name ($MOZILLA)])
fi

AC_SUBST(MOZILLA)

AC_MSG_RESULT([$MOZILLA])

case "$MOZILLA" in
mozilla) min_version=1.7 flavour=mozilla ;;
firefox) min_version=0.10 flavour=toolkit ;;
thunderbird) min_version=0.8 flavour=toolkit ;;
esac

MOZILLA_FLAVOUR=$flavour
AC_SUBST(MOZILLA_FLAVOUR)

PKG_CHECK_MODULES(MOZILLA_COMPONENT, $MOZILLA-gtkmozembed >= $min_version $MOZILLA-xpcom >= $min_version)
AC_SUBST(MOZILLA_COMPONENT_CFLAGS)
AC_SUBST(MOZILLA_COMPONENT_LIBS)

MOZILLA_INCLUDE_ROOT="`$PKG_CONFIG --variable=includedir $MOZILLA-gtkmozembed`"
AC_SUBST(MOZILLA_INCLUDE_ROOT)

MOZILLA_HOME="`$PKG_CONFIG --variable=libdir $MOZILLA-gtkmozembed`"
AC_SUBST(MOZILLA_HOME)

dnl *************************************************************************
dnl This is from Mozilla's configure.in. They set almost all the config stuff
dnl they need in mozilla-config.h Except for this compiler flag, which can't
dnl go in mozilla-config.h So we check the flag too and now we can include
dnl mozilla-config.h without breaking epiphany.
dnl This is really gcc-only
dnl Do this test using CXX only since some versions of gcc
dnl 2.95-2.97 have a signed wchar_t in c++ only and some versions
dnl only have short-wchar support for c++.
AC_LANG_PUSH([C++])
_SAVE_CXXFLAGS=$CXXFLAGS
_SAVE_AM_CXXFLAGS=$AM_CXXFLAGS
AM_CXXFLAGS="$AM_CXXFLAGS -fshort-wchar"
CXXFLAGS="$CXXFLAGS $AM_CXXFLAGS"

AC_CACHE_CHECK([for compiler -fshort-wchar option],
    ac_cv_have_usable_wchar_option,
    [AC_TRY_RUN([#include <stddef.h>
                 int main () {
                   return (sizeof(wchar_t) != 2) ||
                          (wchar_t)-1 < (wchar_t) 0 ; } ],
                ac_cv_have_usable_wchar_option="yes",
                ac_cv_have_usable_wchar_option="no",
                ac_cv_have_usable_wchar_option="maybe")])

if test "$ac_cv_have_usable_wchar_option" != "yes"; then
    AM_CXXFLAGS=$_SAVE_AM_CXXFLAGS
fi

dnl **********************************
dnl now tests for mozilla API variance
dnl **********************************

dnl FIXME find a m4/autoconf guru who can distill this into a nice macro

CXXFLAGS="$_SAVE_CXXFLAGS $AM_CXXFLAGS -I$MOZILLA_INCLUDE_ROOT `pkg-config --cflags $MOZILLA-xpcom`"

dnl Changed from PRUnichar* to nsAString& in 1.8a1 and on aviary branch

AC_MSG_CHECKING([whether nsIFilePicker methods expect nsAString&])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <widget/nsIFilePicker.h>]],
		[[nsIFilePicker *p;
		  p->SetDefaultString (nsEmbedString());]]
	)],
	[AC_DEFINE([MOZ_NSIFILEPICKER_NSASTRING_],[1],[Define if nsIFilePicker uses nsAString&]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl Changed from char* to nsAString& in 1.8a1

AC_MSG_CHECKING([whether nsIMIMEInfo methods expect nsAString&])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <mimetype/nsIMIMEInfo.h>]],
		[[nsIMIMEInfo *p;
		  p->SetDescription (nsEmbedString());]]
	)],
	[AC_DEFINE([MOZ_NSIMIMEINFO_NSACSTRING_],[1],[Define if nsIMIMEInfo uses nsAString&]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl Changed in 1.8a1 and again in 1.8a3 to its present form (variant 2);
dnl variant 1 is on 1.7 branch. Aviary branch also has variant 2.

AC_MSG_CHECKING([for nsIContentPolicy API])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <nsIURI.h>
		  #include <content/nsIContentPolicy.h>]],
		[[nsIContentPolicy *p;
		  nsISupports *arg4;
		  p->ShouldLoad(0,nsnull,nsnull,arg4,nsEmbedCString(),nsnull,nsnull);]]
	)],
	[variant=2],
	[variant=1])

AC_DEFINE_UNQUOTED([MOZ_NSICONTENTPOLICY_VARIANT], [$variant], [Define the nsIContentPolicy API variant])

AC_MSG_RESULT([variant $variant])

dnl Changed from char* to nsIURI* in 1.7.4 on 1.7 branch,
dnl in 1.8a3 on trunk and on aviary branch

AC_MSG_CHECKING([whether nsIBrowserHistory methods expect nsIURI*])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsIURI.h>
		  #include <nsIGlobalHistory.h>
		  #include <docshell/nsIGlobalHistory2.h>
		  #include <history/nsIBrowserHistory.h>]],
		[[nsIBrowserHistory* p;
		  nsIURI *arg1;
		  p->RemovePage(arg1);]]
	)],
	[AC_DEFINE([MOZ_NSIGLOBALHISTORY_NSIURIP],[1],[dummy]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl This is only present in new-toolkit apps

AC_MSG_CHECKING([for nsIBrowserHistory::AddPageWithDetails])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsIURI.h>
		  #include <nsIGlobalHistory.h>
		  #include <docshell/nsIGlobalHistory2.h>
		  #include <history/nsIBrowserHistory.h>]],
		[[nsIBrowserHistory *p;
		  p->AddPageWithDetails(nsnull,nsnull,0);]]
	)],
	[AC_DEFINE([MOZ_NSIBROWSERHISTORY_ADDPAGEWITHDETAILS],[1],[dummy]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl changed in 1.8a4

AC_MSG_CHECKING([for whether nsIGlobalHistory2::AddURI takes a referrer])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsIURI.h>
		  #include <nsIGlobalHistory.h>
		  #include <docshell/nsIGlobalHistory2.h>
		  #include <history/nsIBrowserHistory.h>]],
		[[nsIBrowserHistory *p;
		  p->AddURI(nsnull,PR_FALSE,PR_FALSE,nsnull);]]
	)],
	[AC_DEFINE([MOZ_NSIBROWSERHISTORY_ADDURI_WITH_REFERRER],[1],[dummy]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl Firefox doesn't have this API (?)

AC_MSG_CHECKING([for nsIXULChromeRegistry API])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <content/nsIChromeRegistry.h>]],
		[[nsIXULChromeRegistry *p;
		  p->SelectSkin (nsEmbedCString(), PR_FALSE);]]
	)],
	[AC_DEFINE([MOZ_NSIXULCHROMEREGISTRY_SELECTSKIN],[1],[dummy]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl changed from nsIXULChromeRegistry in 1.8a4

AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/chrome/nsIChromeRegistrySea.h],
	[AC_DEFINE([HAVE_CHROME_NSICHROMEREGISTRYSEA_H], [1], [Define if nsIChromeRegistrySea.h exists])])

dnl check for broken reload in GtkMozEmbed
dnl FIXME: I have NO IDEA how to do this. For now, just always enable our workaround
dnl
dnl This is fixed since 1.7.4 on 1.7 branch, and since 1.8a3 on trunk

AC_MSG_CHECKING([for broken reload])

AC_DEFINE([GTKMOZEMBED_BROKEN_RELOAD],[1],[Define if GtkMozEmbed has broken reload])

AC_MSG_RESULT([couldn't autodetect, assuming yes])

dnl restore flags
CXXFLAGS=$_SAVE_CXXFLAGS
AC_LANG_POP(C++)

AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/pipnss/nsIX509Cert.h],
	[AC_DEFINE(HAVE_MOZILLA_PSM, 1,
	[Define if you have the mozilla psm headers installed])])

AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/nsIPassword.h],
	[AC_DEFINE(HAVE_NSIPASSWORD_H, 1,
	[Define if mozilla nsIPassword header is available])])

MOZILLA_WARN_CXXFLAGS="-Wall -Wconversion -Wpointer-arith -Wcast-align -Woverloaded-virtual -Wsynth -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor"
AC_SUBST([MOZILLA_WARN_CXXFLAGS])

dnl *******************************
dnl Internationalization
dnl ******************************* 
dnl Add the languages which your application supports here.

ALL_LINGUAS="am ar az be bg bn bs ca cs cy da de el en_CA en_GB es et eu fa fi fr ga gu he hi hr hu id is it ja ko li lt mi mk ml mn ms nb ne nl nn no pa pl pt pt_BR ro ru sk sl sq sr sr@Latn sv ta th tk tr uk vi wa zh_CN zh_TW"
GETTEXT_PACKAGE=epiphany
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Gettext package])
AM_GLIB_GNU_GETTEXT

dnl uninstalled share dir to search data
AC_DEFINE_UNQUOTED([SHARE_UNINSTALLED_DIR], ["`pwd`/data"], [path to source data dir])

GTK_DOC_CHECK([1.0])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/epiphany-1.6.pc
data/glade/Makefile
data/art/Makefile
data/ui/Makefile
doc/Makefile
doc/reference/Makefile
lib/Makefile
lib/egg/Makefile
lib/widgets/Makefile
embed/Makefile
embed/mozilla/Makefile
src/Makefile
src/bookmarks/Makefile
help/Makefile
help/C/Makefile
help/es/Makefile
help/eu/Makefile
help/ja/Makefile
po/Makefile.in
])

AC_OUTPUT
