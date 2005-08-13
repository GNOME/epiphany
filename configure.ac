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

AC_INIT([GNOME Web Browser],[1.6.5],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

GNOME_COMMON_INIT

AC_PREREQ([2.59])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

EPIPHANY_MAJOR=1.6
AC_SUBST([EPIPHANY_MAJOR])

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip check-news])

dnl AC_SUBST([ACLOCAL_AMFLAGS], ["-I $ac_macro_dir \${ACLOCAL_FLAGS}"])
AC_SUBST([ACLOCAL_AMFLAGS], ["\${ACLOCAL_FLAGS}"])

AM_MAINTAINER_MODE
if test "x$enable_maintainer_mode" = "xyes"; then
	AC_DEFINE([MAINTAINER_MODE],[1],[Define to enable 'maintainer-only' behaviour])
	enable_debug=yes
	MORE_WARN_FLAGS=
        DEPRECATION_FLAGS="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED -DGNOME_VFS_DISABLE_DEPRECATED -DBONOBO_UI_DISABLE_DEPRECATED -DBONOBO_DISABLE_DEPRECATED -DLIBGLADE_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED"
	MOZILLA_WARN_CXXFLAGS="-Wall -Wconversion -Wpointer-arith -Wcast-align -Woverloaded-virtual -Wsynth -Wno-ctor-dtor-privacy -Wno-non-virtual-dtor"
fi

AC_PROG_INTLTOOL([0.29])

GLIB_REQUIRED=2.6.0
PANGO_REQUIRED=1.8.0
GTK_REQUIRED=2.6.3
LIBXML_REQUIRED=2.6.12
LIBXSLT_REQUIRED=1.1.7
LIBGLADE_REQUIRED=2.3.1
LIBGNOMEVFS_REQUIRED=2.9.2
LIBGNOMEUI_REQUIRED=2.6.0
GNOME_DESKTOP_REQUIRED=2.9.91
LIBSTARTUP_NOTIFICATION_REQUIRED=0.5

AC_ENABLE_SHARED([yes])
AC_ENABLE_STATIC([no])

AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
dnl GNOME_CXX_WARNINGS

AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
if $PKG_CONFIG --atleast-version $LIBSTARTUP_NOTIFICATION_REQUIRED libstartup-notification-1.0; then
	echo "Building with libstartup-notification"
	AC_DEFINE([HAVE_STARTUP_NOTIFICATION],[1],[Define to enable startup notification support])
	STARTUP_NOTIFICATION_PACKAGE=libstartup-notification-1.0
else
	echo "***** WARNING: Building without libstartup-notification"
	STARTUP_NOTIFICATION_PACKAGE=
fi

PKG_CHECK_MODULES([EPIPHANY_DEPENDENCY], [\
		  glib-2.0 >= $GLIB_REQUIRED \
		  pango >= $PANGO_REQUIRED \
		  gtk+-2.0 >= $GTK_REQUIRED \
		  libxml-2.0 >= $LIBXML_REQUIRED \
		  libxslt >= $LIBXSLT_REQUIRED \
		  libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED \
		  libglade-2.0 >= $LIBGLADE_REQUIRED \
		  bonobo-activation-2.0 \
		  ORBit-2.0 \
		  gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED \
		  gnome-vfs-module-2.0 \
		  gconf-2.0 \
		  gnome-desktop-2.0 >= $GNOME_DESKTOP_REQUIRED \
		  $STARTUP_NOTIFICATION_PACKAGE \
		  ])
AC_SUBST([EPIPHANY_DEPENDENCY_CFLAGS])
AC_SUBST([EPIPHANY_DEPENDENCY_LIBS])

ORBIT_IDL="`$PKG_CONFIG --variable=orbit_idl ORBit-2.0`"
AC_SUBST([ORBIT_IDL])

LIBBONOBO_IDL="`$PKG_CONFIG --variable=idldir libbonobo-2.0`"
AC_SUBST([LIBBONOBO_IDL])

BONOBO_ACTIVATION_IDL="`$PKG_CONFIG --variable=idldir bonobo-activation-2.0`"
AC_SUBST([BONOBO_ACTIVATION_IDL])

dnl *****
dnl GConf
dnl *****

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

dnl Check for -Wdeclaration-after-statement (since gcc 3.4)

if test "x$enable_maintainer_mode" = "xyes"; then

	AC_LANG_PUSH([C])
	FLAGS="-Wdeclaration-after-statement"
	_SAVE_AM_CFLAGS=$AM_CFLAGS
	_SAVE_CFLAGS=$CFLAGS
	AM_CFLAGS="$AM_CFLAGS $FLAGS"
	CFLAGS="$CFLAGS $AM_CFLAGS"

	AC_CACHE_CHECK([for compiler $FLAGS option],
		ac_cv_have_declaration_after_statement,
		[AC_COMPILE_IFELSE(
			[AC_LANG_SOURCE([[int main () { return 0; }]])],
			[ac_cv_have_declaration_after_statement="yes"],
			[ac_cv_have_declaration_after_statement="no"],
			[ac_cv_have_declaration_after_statement="maybe"])])
	
	if test "x$ac_cv_have_declaration_after_statement" = "xyes"; then
		MORE_WARN_FLAGS="$MORE_WARN_FLAGS $FLAGS"
	fi
	AM_CFLAGS="$_SAVE_AM_CFLAGS"
	CFLAGS="$_SAVE_CFLAGS"
	AC_LANG_POP([C])
fi

dnl *******
dnl Mozilla
dnl *******

AC_MSG_CHECKING([which mozilla to use])

MOZILLA=
AC_ARG_WITH([mozilla],
	AS_HELP_STRING([--with-mozilla@<:@=mozilla|firefox|thunderbird@:>@],
		       [Whether to use mozilla, firefox or thunderbird gtkmozembed (default: mozilla)]),
	[MOZILLA="$withval"])

dnl try to autodetect: if mozilla exists, use it, else try firefox, then thunderbird
if test "x$MOZILLA" = "x"; then
  if $PKG_CONFIG --exists mozilla-gtkmozembed; then
    MOZILLA=mozilla
  elif $PKG_CONFIG --exists firefox-gtkmozembed; then
    MOZILLA=firefox
  elif $PKG_CONFIG --exists thunderbird-gtkmozembed; then
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
mozilla) min_version=1.7.5 flavour=mozilla ;;
firefox) min_version=1.0 flavour=toolkit ;;
thunderbird) min_version=1.0 flavour=toolkit ;;
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

MOZILLA_PREFIX="`$PKG_CONFIG --variable=prefix $MOZILLA-gtkmozembed`"
AC_SUBST(MOZILLA_PREFIX)

AC_ARG_ENABLE([cpp-rtti],
	      AS_HELP_STRING([--enable-cpp-rtti],[Enable C++ RTTI]),[],
	      [enable_cpp_rtti=no])

if test "x$enable_cpp_rtti" = "xno"; then
  AM_CXXFLAGS="-fno-rtti $AM_CXXFLAGS"
fi

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

_SAVE_CPPFLAGS=$CPPFLAGS
_SAVE_CXXFLAGS=$CXXFLAGS
_SAVE_AM_CXXFLAGS=$AM_CXXFLAGS
AM_CXXFLAGS="$AM_CXXFLAGS -fshort-wchar"
CXXFLAGS="$CXXFLAGS $AM_CXXFLAGS"

AC_CACHE_CHECK([for compiler -fshort-wchar option],
	ac_cv_have_usable_wchar_option,
	[AC_RUN_IFELSE([AC_LANG_SOURCE(
		[[#include <stddef.h>
		  int main () {
		    return (sizeof(wchar_t) != 2) || (wchar_t)-1 < (wchar_t) 0 ;
		  } ]])],
		[ac_cv_have_usable_wchar_option="yes"],
		[ac_cv_have_usable_wchar_option="no"],
		[ac_cv_have_usable_wchar_option="maybe"])])

if test "$ac_cv_have_usable_wchar_option" != "yes"; then
    AM_CXXFLAGS=$_SAVE_AM_CXXFLAGS
fi

dnl **********************************
dnl now tests for mozilla API variance
dnl **********************************

dnl FIXME find a m4/autoconf guru who can distill this into a nice macro

CPPFLAGS="-I$MOZILLA_INCLUDE_ROOT -I$MOZILLA_INCLUDE_ROOT/pipnss `$PKG_CONFIG --cflags-only-I $MOZILLA-xpcom`"
CXXFLAGS="$_SAVE_CXXFLAGS $AM_CXXFLAGS `$PKG_CONFIG --cflags-only-other $MOZILLA-xpcom`"

AC_MSG_CHECKING([[whether we have a gtk 2 mozilla build]])

AC_RUN_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #include <string.h>
                  int main(void) {
		    return strcmp (MOZ_DEFAULT_TOOLKIT, "gtk2") != 0;
		  } ]]
	)],
	[result=yes],
	[AC_MSG_ERROR([[Epiphany needs a gtk 2 mozilla build]])],
        [result=maybe])

AC_MSG_RESULT([$result])

dnl Check whether we have a mozilla debug build

AC_MSG_CHECKING([[whether we have a mozilla debug build]])

AC_PREPROC_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #if !defined(MOZ_REFLOW_PERF) || !defined(MOZ_REFLOW_PERF_DSP)
		  #error No
		  #endif]]
	)],
	[AM_CXXFLAGS="-DDEBUG -D_DEBUG $AM_CXXFLAGS" have_mozilla_debug=yes],
	[have_mozilla_debug=no])

AC_MSG_RESULT([$have_mozilla_debug])

dnl Determine gecko version
dnl We don't use the version from the .pc file, since that's the app version
dnl and not the Gecko version

AC_MSG_CHECKING([[for gecko version]])

AC_PREPROC_IFELSE(
	[AC_LANG_SOURCE(
		[[#include <mozilla-config.h>
		  #define _DECL_VERSION(major,minor) ((major << 8) + minor)
		  #if (_DECL_VERSION(MOZILLA_VERSION_MAJOR,MOZILLA_VERSION_MINOR) < _DECL_VERSION(1,8))
		  #error No
		  #endif]]
	)],
	[have_gecko_1_8=yes gecko_version="1.8"],
	[have_gecko_1_8=no gecko_version="1.7"])

AC_MSG_RESULT([$gecko_version])

if test "x$have_gecko_1_8" = "xyes"; then
	AC_DEFINE([HAVE_GECKO_1_8],[1],[Define if we have gecko 1.8])
fi
AM_CONDITIONAL([HAVE_GECKO_1_8],[test "x$have_gecko_1_8" = "xyes"])

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

dnl Firefox doesn't have this API (?)

AC_MSG_CHECKING([for nsIXULChromeRegistry API])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <content/nsIChromeRegistry.h>]],
		[[nsIXULChromeRegistry *p;
		  p->SelectSkin (nsEmbedCString(), PR_FALSE);]]
	)],
	[AC_DEFINE([MOZ_NSIXULCHROMEREGISTRY_SELECTSKIN],[1],[Define if nsIXULChromeRegistry::SelectSkin exists]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl changed from nsIXULChromeRegistry in 1.8a4

AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/chrome/nsIChromeRegistrySea.h],
	[AC_DEFINE([HAVE_CHROME_NSICHROMEREGISTRYSEA_H], [1], [Define if nsIChromeRegistrySea.h exists])])

dnl added after 1.7.3, on aviary and in 1.8a5 on trunk

AC_MSG_CHECKING([for nsIDOMNSEvent::GetIsTrusted])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <dom/nsIDOMNSEvent.h>]],
		[[nsIDOMNSEvent *p;
		  p->GetIsTrusted (nsnull);]]
	)],
	[AC_DEFINE([MOZ_NSIDOMNSEVENT_GETISTRUSTED],[1],[Define if nsIDOMNSEvent::GetIsTrusted exists]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

dnl changed location in 1.8a6

AC_MSG_CHECKING([for mozilla security compoment])

AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[#include <nsIX509Cert.h>]],
		[[nsIX509Cert *c;
		  c->GetIssuer (nsnull);]]
	)],
	[AC_DEFINE([HAVE_MOZILLA_PSM], [1],[Define if you have the mozilla psm headers installed]) have_psm=yes],
	[have_psm=no])

AC_MSG_RESULT([$have_psm])

AM_CONDITIONAL([HAVE_MOZILLA_PSM],[test "x$have_psm" = "xyes"])

dnl restore flags
CPPFLAGS=$_SAVE_CPPFLAGS
CXXFLAGS=$_SAVE_CXXFLAGS

AC_LANG_POP([C++])

dnl Broken everywhere

AC_DEFINE([GTKMOZEMBED_BROKEN_FOCUS],[1],[Define if GtkMozEmbed has broken focus handling])

dnl Needed since 1.8b2
dnl Define this down here so it doesn't affect the API checks above

AC_DEFINE([MOZILLA_INTERNAL_API],[1],[Define for access to internal mozilla API])

dnl Not a simple file check due to the header being in the SDK since 1.8a6
dnl https://bugzilla.mozilla.org/show_bug.cgi?id=271068

if test "x$flavour" = "xmozilla"; then
	AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/nsIPassword.h],
		[AC_DEFINE([HAVE_NSIPASSWORD_H],[1],
		[Define if mozilla nsIPassword header is available])],
		[AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/wallet/nsIPassword.h],
			[AC_DEFINE([HAVE_NSIPASSWORD_H],[1],
			[Define if mozilla nsIPassword header is available])])])
elif test "x$flavour" = "xtoolkit"; then
	AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/passwordmgr/nsIPassword.h],
		[AC_DEFINE([HAVE_NSIPASSWORD_H],[1],
		[Define if mozilla nsIPassword header is available])])
fi

AC_MSG_CHECKING([whether to enable native gtk+ filepicker])
AC_ARG_ENABLE([filepicker],
	AS_HELP_STRING([--enable-filepicker],[Whether to enable the gtk+ native filepicker; not needed when the mozilla build itself has native gtk+ filepicker enabled (default:enabled)]),
	[filepicker=$enableval],
	[if test "x$have_gecko_1_8" = "xyes"; then filepicker=no; else filepicker=yes; fi])
AC_MSG_RESULT([$filepicker])

AM_CONDITIONAL([ENABLE_FILEPICKER],[test "x$filepicker" = "xyes"])
if test "x$filepicker" = "xyes"; then
	AC_DEFINE([ENABLE_FILEPICKER],[1],[Define to enable the native filepicker])
fi

dnl ***************
dnl Multimedia keys
dnl ***************

AC_CHECK_HEADERS([X11/XF86keysym.h])

dnl ****
dnl DBUS
dnl ****

AC_MSG_CHECKING([whether DBUS support is requested])

AC_ARG_ENABLE([dbus],
	AS_HELP_STRING([--enable-dbus],[Enable DBUS (default=no)]),
	[enable_dbus=$enableval],
	[enable_dbus=no])

AC_MSG_RESULT([$enable_dbus])

if test "x$enable_dbus" = "xyes" ; then

	AC_MSG_CHECKING([for new DBUS API])

	_SAVE_LDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS `$PKG_CONFIG --libs dbus-1`"
	AC_TRY_LINK_FUNC([dbus_message_iter_get_boolean],[have_new_dbus=no],[have_new_dbus=yes])
	LDFLAGS="$_SAVE_LDFLAGS"

	AC_MSG_RESULT([$have_new_dbus])

	if test "x$have_new_dbus" = "xyes"; then
		AC_DEFINE([HAVE_NEW_DBUS],[1],[Define if compiling with DBUS 0.3])

		DBUS_REQUIRED=0.34
		DBUS_GLIB_REQUIRED=0.34
	else
		DBUS_REQUIRED=0.22
		DBUS_GLIB_REQUIRED=0.22
	fi

	AC_DEFINE([ENABLE_DBUS],[1],[Define if DBUS support is enabled (default=disabled)])

	PKG_CHECK_MODULES([DBUS], [dbus-1 >= $DBUS_REQUIRED dbus-glib-1 >= $DBUS_GLIB_REQUIRED])
fi

AM_CONDITIONAL([ENABLE_DBUS], [test "x$enable_dbus" = "xyes"])

dnl *******************************
dnl Add warning flags
dnl *******************************

AM_CPPFLAGS="$AM_CPPFLAGS $DEPRECATION_FLAGS"
AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS $MORE_WARN_FLAGS"
AM_CXXFLAGS="$AM_CXXFLAGS $WARN_CXXFLAGS $MOZILLA_WARN_CXXFLAGS"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

dnl *******************************
dnl Internationalization
dnl ******************************* 

AC_MSG_CHECKING([for iso-codes package])
have_iso_codes=no
if $PKG_CONFIG --exists iso-codes --atleast-version=0.35; then
	have_iso_codes=yes
fi
AC_MSG_RESULT([$have_iso_codes])

if test "x$have_iso_codes" = "xyes"; then
	AC_MSG_CHECKING([whether iso-codes has iso-639 and iso-3166 domains])
	if $PKG_CONFIG --variable=domains iso-codes | grep -q 639 && \
	   $PKG_CONFIG --variable=domains iso-codes | grep -q 3166 ; then
		result=yes
	else
		result=no
		have_iso_codes=no
	fi
	AC_MSG_RESULT([$result])
fi

if test "x$have_iso_codes" = "xyes"; then
	AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
	AC_DEFINE([HAVE_ISO_CODES],[1],[Define if you have the iso-codes package])
fi

dnl Translators: new languages must be added to the po/LINGUAS file
ALL_LINGUAS="`cat "$srcdir/po/LINGUAS" | grep -v '^#'`"

AC_SUBST([CONFIG_STATUS_DEPENDENCIES],['$(top_srcdir)/po/LINGUAS'])

GETTEXT_PACKAGE=epiphany
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR([GNOMELOCALEDIR])

dnl uninstalled share dir to search data
AC_DEFINE_UNQUOTED([SHARE_UNINSTALLED_DIR], ["`pwd`/data"], [path to source data dir])

dnl *****************
dnl API Documentation
dnl *****************

GTK_DOC_CHECK([1.0])

dnl ************
dnl Output files
dnl ************

AC_CONFIG_FILES([
Makefile
data/Makefile
data/epiphany-1.6.pc
data/glade/Makefile
data/art/Makefile
data/ui/Makefile
doc/Makefile
doc/reference/Makefile
idl/Makefile
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
help/fi/Makefile
help/ja/Makefile
help/uk/Makefile
po/Makefile.in
])

AC_OUTPUT
