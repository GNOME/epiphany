# Copyright (C) 2000-2004 Marco Pesenti Gritti
# Copyright (C) 2003, 2004, 2005, 2006 Christian Persch
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

AC_INIT([GNOME Web Browser],[1.9.4.90],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

GNOME_COMMON_INIT

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

EPIPHANY_MAJOR=1.9
EPIPHANY_API_VERSION=1.10
AC_SUBST([EPIPHANY_MAJOR])
AC_SUBST([EPIPHANY_API_VERSION])

AM_INIT_AUTOMAKE([1.9 dist-bzip2 no-dist-gzip check-news])

AM_MAINTAINER_MODE

AC_ENABLE_SHARED([yes])
AC_ENABLE_STATIC([no])

AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

IT_PROG_INTLTOOL([0.34.1])

PKG_PROG_PKG_CONFIG

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_CXX_WARNINGS

MORE_WARN_FLAGS=
DEPRECATION_FLAGS=
MOZILLA_WARN_CXXFLAGS="-Wno-ctor-dtor-privacy -Wno-non-virtual-dtor"

if test "x$enable_maintainer_mode" = "xyes"; then
	AC_DEFINE([MAINTAINER_MODE],[1],[Define to enable 'maintainer-only' behaviour])
	enable_debug=yes
        DEPRECATION_FLAGS="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED -DGNOME_VFS_DISABLE_DEPRECATED -DLIBGLADE_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED"
	MOZILLA_WARN_CXXFLAGS="-Wall -Wconversion -Wpointer-arith -Wcast-align -Woverloaded-virtual -Wsynth $MOZILLA_WARN_CXXFLAGS"
fi

GLIB_REQUIRED=2.8.0
GTK_REQUIRED=2.8.3
LIBXML_REQUIRED=2.6.12
LIBXSLT_REQUIRED=1.1.7
LIBGLADE_REQUIRED=2.3.1
LIBGNOMEVFS_REQUIRED=2.9.2
LIBGNOMEUI_REQUIRED=2.6.0
GNOME_DESKTOP_REQUIRED=2.9.91
LIBSTARTUP_NOTIFICATION_REQUIRED=0.5
LIBGNOMEPRINT_REQUIRED=2.4.0
LIBGNOMEPRINTUI_REQUIRED=2.4.0
DBUS_GLIB_REQUIRED=0.35

PKG_CHECK_MODULES([DEPENDENCIES], [\
		  glib-2.0 >= $GLIB_REQUIRED \
		  gtk+-2.0 >= $GTK_REQUIRED \
		  libxml-2.0 >= $LIBXML_REQUIRED \
		  libxslt >= $LIBXSLT_REQUIRED \
		  libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED \
		  libglade-2.0 >= $LIBGLADE_REQUIRED \
		  gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED \
		  gnome-vfs-module-2.0 \
		  gconf-2.0 \
		  gnome-desktop-2.0 >= $GNOME_DESKTOP_REQUIRED \
		  libstartup-notification-1.0 >= $LIBSTARTUP_NOTIFICATION_REQUIRED \
		  libgnomeprint-2.2 >= $LIBGNOMEPRINT_REQUIRED \
		  libgnomeprintui-2.2 >= $LIBGNOMEPRINTUI_REQUIRED \
		  ])
AC_SUBST([DEPENDENCIES_CFLAGS])
AC_SUBST([DEPENDENCIES_LIBS])

# ****
# DBUS
# ****

PKG_CHECK_MODULES([DBUS],[dbus-glib-1 >= $DBUS_GLIB_REQUIRED])
AC_SUBST([DBUS_CFLAGS])
AC_SUBST([DBUS_LIBS])

AC_PATH_PROG([DBUS_BINDING_TOOL],[dbus-binding-tool],[no])

if test "$DBUS_BINDING_TOOL" = "no"; then
	AC_MSG_ERROR([dbus-binding-tool not found])
fi

# *****
# GConf
# *****

AM_GCONF_SOURCE_2

AC_PATH_PROG([GCONFTOOL], [gconftool-2], [no])

if test "$GCONFTOOL" = "no"; then
	AC_MSG_ERROR([gconftool-2 not found])
fi

# ******************
# Portability checks
# ******************

AC_CHECK_FUNCS([mkdtemp])

# for backtrace()
AC_CHECK_HEADERS([execinfo.h])

# Check for -Wdeclaration-after-statement (since gcc 3.4)

if test "x$enable_maintainer_mode" = "xyes"; then

	AC_LANG_PUSH([C])
	_FLAGS="-Wdeclaration-after-statement"
	_SAVE_CFLAGS=$CFLAGS
	CFLAGS="$CFLAGS $_FLAGS"

	AC_CACHE_CHECK([for compiler $FLAGS option],
		ac_cv_have_declaration_after_statement,
		[AC_COMPILE_IFELSE(
			[AC_LANG_SOURCE([[int main () { return 0; }]])],
			[ac_cv_have_declaration_after_statement="yes"],
			[ac_cv_have_declaration_after_statement="no"],
			[ac_cv_have_declaration_after_statement="maybe"])])
	
	if test "x$ac_cv_have_declaration_after_statement" = "xyes"; then
		MORE_WARN_FLAGS="$MORE_WARN_FLAGS $_FLAGS"
	fi
	CFLAGS="$_SAVE_CFLAGS"
	AC_LANG_POP([C])
fi

# *******
# Mozilla
# *******

GECKO_INIT([MOZILLA])

AC_SUBST([MOZILLA])
AC_SUBST([MOZILLA_FLAVOUR])
AC_SUBST([MOZILLA_INCLUDE_ROOT])
AC_SUBST([MOZILLA_HOME])
AC_SUBST([MOZILLA_PREFIX])

case "$MOZILLA" in
mozilla) min_version=1.7.9 ;;
seamonkey) min_version=1.0 ;;
*firefox) min_version=1.0.5 ;;
*thunderbird) min_version=1.0.5 ;;
xulrunner) min_version=1.8 ;;
esac

PKG_CHECK_MODULES([GECKO],[$MOZILLA-gtkmozembed >= $min_version $MOZILLA-xpcom >= $min_version])
AC_SUBST([GECKO_CFLAGS])
AC_SUBST([GECKO_LIBS])

# **********************************
# now tests for mozilla API variance
# **********************************

# Changed from PRUnichar* to nsAString& in 1.8a1 and on aviary branch

AC_MSG_CHECKING([whether nsIFilePicker methods expect nsAString&])

GECKO_COMPILE_IFELSE([widget],
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <nsIFilePicker.h>]],
		[[nsIFilePicker *p;
		  p->SetDefaultString (nsEmbedString());]]
	)],
	[AC_DEFINE([MOZ_NSIFILEPICKER_NSASTRING_],[1],[Define if nsIFilePicker uses nsAString&]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

# Changed in 1.8a1 and again in 1.8a3 to its present form (variant 2);
# variant 1 is on 1.7 branch. Aviary branch also has variant 2.

AC_MSG_CHECKING([for nsIContentPolicy API])

GECKO_COMPILE_IFELSE([content],
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <nsIURI.h>
		  #include <nsIContentPolicy.h>]],
		[[nsIContentPolicy *p;
		  nsISupports *arg4;
		  p->ShouldLoad(0,nsnull,nsnull,arg4,nsEmbedCString(),nsnull,nsnull);]]
	)],
	[variant=2],
	[variant=1])

AC_MSG_RESULT([variant $variant])

AC_DEFINE_UNQUOTED([MOZ_NSICONTENTPOLICY_VARIANT], [$variant], [Define the nsIContentPolicy API variant])

# Firefox doesn't have this API (?)

AC_MSG_CHECKING([for nsIXULChromeRegistry API])

GECKO_COMPILE_IFELSE([content],
	[AC_LANG_PROGRAM(
		[[#include <nsEmbedString.h>
		  #include <nsIChromeRegistry.h>]],
		[[nsIXULChromeRegistry *p;
		  p->SelectSkin (nsEmbedCString(), PR_FALSE);]]
	)],
	[AC_DEFINE([MOZ_NSIXULCHROMEREGISTRY_SELECTSKIN],[1],[Define if nsIXULChromeRegistry::SelectSkin exists]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

# changed from nsIXULChromeRegistry in 1.8a4

AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/chrome/nsIChromeRegistrySea.h],
	[AC_DEFINE([HAVE_CHROME_NSICHROMEREGISTRYSEA_H], [1], [Define if nsIChromeRegistrySea.h exists])])

# changed location in 1.8a6

AC_MSG_CHECKING([for mozilla security compoment])

GECKO_COMPILE_IFELSE([pipnss],
	[AC_LANG_PROGRAM(
		[[#include <nsIX509Cert.h>]],
		[[nsIX509Cert *c;
		  c->GetIssuer (nsnull);]]
	)],
	[have_psm=yes],
	[have_psm=no])

AC_MSG_RESULT([$have_psm])

# This is only present on toolkit

AC_MSG_CHECKING([for toolkit's nsITypeAheadFind])

GECKO_COMPILE_IFELSE([fastfind],
	[AC_LANG_PROGRAM(
		[[#include <nsITypeAheadFind.h>]],
		[[nsITypeAheadFind *p;
		  p->SetDocShell(nsnull);]]
	)],
	[AC_DEFINE([HAVE_TYPEAHEADFIND],[1],[Define if we have tookit's typeaheadfind]) result=yes],
	[result=no])

AC_MSG_RESULT([$result])

# Needed since 1.8b2
# Define this down here so it doesn't affect the API checks above

AC_DEFINE([MOZILLA_INTERNAL_API],[1],[Define for access to internal mozilla API])

# Make sure we don't accidentally build without PSM support

AC_MSG_CHECKING([whether to include PSM support])
AC_ARG_ENABLE([psm],
	AS_HELP_STRING([psm],[Disable PSM support (default: enabled)]),
	[],[enable_psm=yes])
AC_MSG_RESULT([$enable_psm])

if test "$enable_psm" = "yes" -a "$have_psm" = "no"; then
	AC_MSG_ERROR([PSM headers not found; use --disable-psm to disable building with PSM support])
elif test "$have_psm" = "no"; then
	AC_MSG_WARN([PSM headers not found; PSM support disabled])
fi

if test "$enable_psm" = "yes" -a "$have_psm" = "yes"; then
	AC_DEFINE([HAVE_MOZILLA_PSM],[1],[Define if you have the mozilla NSS headers installed]) 
fi

AM_CONDITIONAL([HAVE_MOZILLA_PSM],[test "$enable_psm" = "yes" -a "$have_psm" = "yes"])

# Not a simple file check due to the header being in the SDK since 1.8a6
# https://bugzilla.mozilla.org/show_bug.cgi?id=271068

if test "$MOZILLA_FLAVOUR" = "mozilla"; then
	AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/nsIPassword.h],
		[AC_DEFINE([HAVE_NSIPASSWORD_H],[1],
		[Define if mozilla nsIPassword header is available])],
		[AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/wallet/nsIPassword.h],
			[AC_DEFINE([HAVE_NSIPASSWORD_H],[1],
			[Define if mozilla nsIPassword header is available]) have_password_h=yes])],
			[have_password_h=no])
elif test "$MOZILLA_FLAVOUR" = "toolkit"; then
	AC_CHECK_FILE([$MOZILLA_INCLUDE_ROOT/passwordmgr/nsIPassword.h],
		[AC_DEFINE([HAVE_NSIPASSWORD_H],[1],
			[Define if mozilla nsIPassword header is available]) have_password_h=yes],
		[have_password_h=no])
fi

if test "x$have_password_h" = "xno"; then
	AC_MSG_WARN([nsIPassword.h not found; personal data manger will have no passwords support!])
fi

# Check whether to enable our filepicker component

AC_MSG_CHECKING([whether to enable native gtk+ filepicker])
AC_ARG_ENABLE([filepicker],
	AS_HELP_STRING([--enable-filepicker],[Whether to enable the gtk+ native filepicker; not needed when the mozilla build itself has native gtk+ filepicker enabled (default:enabled)]),
	[],
	[if test "$MOZILLA_VERSION_MAJOR" = "1" -a "$MOZILLA_VERSION_MINOR" -ge "8"; then
		enable_filepicker=no
 	 else
		enable_filepicker=yes
 	 fi])
AC_MSG_RESULT([$enable_filepicker])

AM_CONDITIONAL([ENABLE_FILEPICKER],[test "x$enable_filepicker" = "xyes"])
if test "x$enable_filepicker" = "xyes"; then
	AC_DEFINE([ENABLE_FILEPICKER],[1],[Define to enable the native filepicker])
fi

# Check for some contractIDs that we need but are provided by extensions
# which may or may not have been built.

GECKO_CHECK_CONTRACTID([@mozilla.org/permissionmanager;1],
	[],[AC_MSG_ERROR([Gecko needs to be compiled with at least --enable-extensions=default,cookie,permissions])])
GECKO_CHECK_CONTRACTID([@mozilla.org/PopupWindowManager;1],
	[],[AC_MSG_ERROR([Gecko needs to be compiled with at least --enable-extensions=default,cookie,permissions])])
GECKO_CHECK_CONTRACTID([@mozilla.org/cookie/permission;1],
	[],[AC_MSG_ERROR([Gecko needs to be compiled with at least --enable-extensions=default,cookie,permissions])])
	
# This one ought to exist too but somehow doesn't on breezy at least...!?
# GECKO_CHECK_CONTRACTID([@mozilla.org/permissions/contentblocker;1],
#	[],[AC_MSG_ERROR([Gecko needs to be compiled with at least --enable-extensions=default,cookie,permissions])])

# ***************
# Multimedia keys
# ***************

AC_CHECK_HEADERS([X11/XF86keysym.h])

# ******
# Python
# ******

EPIPHANY_FEATURES=

AC_MSG_CHECKING([whether Python support is requested])

AC_ARG_ENABLE([python],
	AS_HELP_STRING([--enable-python],[Enable python support]),
	[enable_python=$enableval have_python=$enableval],
	[enable_python=autodetect have_python=yes])

AC_MSG_RESULT([$enable_python])

if test "x$have_python" != "xno"; then
	AM_PATH_PYTHON([2.3],[],[no])

	if test "x$PYTHON" = "x:"; then
		have_python=no
	fi
fi

if test "x$have_python" != "xno"; then
	AM_CHECK_PYTHON_HEADERS([],[have_python=no])
fi

if test "x$have_python" != "xno"; then
	PY_PREFIX=`$PYTHON -c 'import sys ; print sys.prefix'`
	PY_EXEC_PREFIX=`$PYTHON -c 'import sys ; print sys.exec_prefix'`
	PYTHON_LIBS="-lpython$PYTHON_VERSION"
	PYTHON_LIB_LOC="-L$PY_EXEC_PREFIX/lib/python$PYTHON_VERSION/config"
	PYTHON_CFLAGS="-I$PY_PREFIX/include/python$PYTHON_VERSION"
	PYTHON_MAKEFILE="$PY_EXEC_PREFIX/lib/python$PYTHON_VERSION/config/Makefile"
	PYTHON_LOCALMODLIBS=`sed -n -e 's/^LOCALMODLIBS=\(.*\)/\1/p' $PYTHON_MAKEFILE`
	PYTHON_BASEMODLIBS=`sed -n -e 's/^BASEMODLIBS=\(.*\)/\1/p' $PYTHON_MAKEFILE`
	PYTHON_OTHER_LIBS=`sed -n -e 's/^LIBS=\(.*\)/\1/p' $PYTHON_MAKEFILE`
	PYTHON_EXTRA_LIBS="$PYTHON_LOCALMODLIBS $PYTHON_BASEMODLIBS $PYTHON_OTHER_LIBS"
	AC_SUBST([PYTHON_LIBS])
	AC_SUBST([PYTHON_LIB_LOC])
	AC_SUBST([PYTHON_CFLAGS])
	AC_SUBST([PYTHON_EXTRA_LIBS])

	dnl FIXME: do we really need this test?
	AC_MSG_CHECKING([whether we can build a shared library depending on libpython])
	rm -rf testpython
	mkdir testpython
	cd testpython
	cat > testpython.c <<EOF
#include <Python.h>
int testpython (void)
{
Py_Exit (0);
}
EOF

	if /bin/sh ../libtool --mode=compile ${CC} $PYTHON_CFLAGS -c testpython.c >/dev/null 2>&1 && \
		/bin/sh ../libtool --mode=link ${CC} -o testpython.la -rpath `pwd` -module -avoid-version $PYTHON_LIB_LOC testpython.lo $PYTHON_LIBS $PYTHON_EXTRA_LIBS >/dev/null 2>&1 && \
		grep 'dlname.*testpython' testpython.la >/dev/null 2>&1; then
		result=yes
	else
		result=no
		have_python=no
	fi
	cd ..
	rm -rf testpython
	AC_MSG_RESULT([$result])
fi

if test "x$have_python" != "xno"; then
	PYGTK_REQUIRED=2.7.1
	GNOME_PYTHON_REQUIRED=2.6.0

	PKG_CHECK_MODULES([PYGTK], [pygtk-2.0 >= $PYGTK_REQUIRED gnome-python-2.0 >= $GNOME_PYTHON_REQUIRED],
		[],[have_python=no])

	AC_SUBST([PYGTK_CFLAGS])
	AC_SUBST([PYGTK_LIBS])
fi

if test "x$have_python" != "xno"; then
	AC_MSG_CHECKING([for pygtk defs])
	PYGTK_DEFSDIR=`$PKG_CONFIG --variable=defsdir pygtk-2.0`
	AC_MSG_RESULT([$PYGTK_DEFSDIR])

	AC_MSG_CHECKING([for pygtk codegen])
	PYGTK_CODEGEN="$PYTHON `$PKG_CONFIG --variable=codegendir pygtk-2.0`/codegen.py"
	AC_MSG_RESULT([$PYGTK_CODEGEN])

	AC_MSG_CHECKING([for pygtk h2def])
	PYGTK_H2DEF="$PYTHON `$PKG_CONFIG --variable=codegendir pygtk-2.0`/h2def.py"
	AC_MSG_RESULT([$PYGTK_H2DEF])
	
	AC_SUBST([PYGTK_DEFSDIR])
	AC_SUBST([PYGTK_CODEGEN])
	AC_SUBST([PYGTK_H2DEF])

	dnl Check for -fno-strict-aliasing
	FLAGS="-fno-strict-aliasing"
	save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $FLAGS"
	AC_MSG_CHECKING([whether [$]CC understands $FLAGS])
	AC_TRY_COMPILE([], [], [compiler_has_option=yes], [compiler_has_option=no])
	CFLAGS="$save_CFLAGS"
	AC_MSG_RESULT($compiler_has_option)
	if test $compiler_has_option = yes; then
		NO_STRICT_ALIASING_CFLAGS="$FLAGS"
	fi
	AC_SUBST([NO_STRICT_ALIASING_CFLAGS])
fi

if test "x$have_python" != "xyes"; then
	if test "x$enable_python" = "xyes"; then
		AC_MSG_ERROR([Python not found])
	elif test "x$enable_python" = "xautodetect"; then
		enable_python=no
		AC_MSG_WARN([Python not found, disabling python support])
	fi
elif test "x$enable_python" != "xno"; then
	enable_python=yes
	AC_DEFINE([ENABLE_PYTHON],[1],[Define to compile with python support])
	EPIPHANY_FEATURES="$EPIPHANY_FEATURES python"
fi

AM_CONDITIONAL([ENABLE_PYTHON],[test "x$enable_python" = "xyes"])

AC_SUBST([EPIPHANY_FEATURES])

# *******
# Plugins
# *******

# Gecko 1.9 can handle .desktop files from file: URLs itself, so disable
# this plugin at least until we handle non-file: URLs

if test "$MOZILLA_VERSION_MAJOR" = "1" -a "$MOZILLA_VERSION_MINOR" -le "8"; then

AC_MSG_CHECKING([whether to build the deskop file plugin])
AC_ARG_ENABLE([desktop-file-plugin],
	AS_HELP_STRING([--disable-desktop-file-plugin],[Disable the desktop file plugin (default: enabled)]),
	[],[enable_desktop_file_plugin=yes])
AC_MSG_RESULT([$enable_desktop_file_plugin])

fi

AM_CONDITIONAL([ENABLE_DESKTOP_FILE_PLUGIN],[test "x$enable_desktop_file_plugin" = "xyes"])

if test "x$enable_desktop_file_plugin" = "xyes"; then
	PKG_CHECK_MODULES([DESKTOP_FILE_PLUGIN_DEP],[$MOZILLA-xpcom $MOZILLA-plugin gtk+-2.0 >= 2.6.0])
	AC_SUBST([DESKTOP_FILE_PLUGIN_DEP_CFLAGS])
	AC_SUBST([DESKTOP_FILE_PLUGIN_DEP_LIBS])
fi

if test "x$enable_desktop_file_plugin" = "xyes"; then
	AC_DEFINE([HAVE_PRIVATE_PLUGINS],[1],[Define if any private plugins are enabled])
fi

# *******************
# Additional features
# *******************

# Zeroconf bookmarks sites

AC_MSG_CHECKING([whether to enable zeroconf bookmarks support])
AC_ARG_ENABLE([zeroconf],
	AS_HELP_STRING([--enable-zeroconf],[Whether to enable zeroconf bookmarks support (default:disabled)]),
	[], [enable_zeroconf=no])
AC_MSG_RESULT([$enable_zeroconf])

if test "x$enable_zeroconf" = "xyes"; then
	AC_DEFINE([ENABLE_ZEROCONF],[1],[Define to enable zeroconf bookmarks support])
fi

AM_CONDITIONAL([ENABLE_ZEROCONF],[test "x$enable_zeroconf" = "xyes"])

# Certificates manager

if test "$enable_psm" = "yes"; then

AC_MSG_CHECKING([whether to build the certificates manager])
AC_ARG_ENABLE([certificate-manager],
	AS_HELP_STRING([--enable-certificate-manager],[Whether to enable the certificate manager (experimental; default:disabled)]),
	[],[enable_certificate_manager=no])
AC_MSG_RESULT([$enable_certificate_manager])

fi

if test "$enable_certificate_manager" = "yes"; then
	AC_DEFINE([ENABLE_CERTIFICATE_MANAGER],[1],[Define to enable the certificate manager build])
fi

AM_CONDITIONAL([ENABLE_CERTIFICATE_MANAGER],[test "$enable_certificate_manager" = "yes"])

# *******************************
# Add warning flags
# *******************************

AM_CPPFLAGS="$AM_CPPFLAGS $DEPRECATION_FLAGS"
AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS $MORE_WARN_FLAGS"
AM_CXXFLAGS="$AM_CXXFLAGS $WARN_CXXFLAGS $MOZILLA_WARN_CXXFLAGS"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

# *******************************
# Internationalization
# ******************************* 

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
else
	AC_MSG_ERROR([iso-codes is required])
fi

# Translators: new languages must be added to the po/LINGUAS file
ALL_LINGUAS="`cat "$srcdir/po/LINGUAS" | grep -v '^#'`"

AC_SUBST([CONFIG_STATUS_DEPENDENCIES],['$(top_srcdir)/po/LINGUAS'])

GETTEXT_PACKAGE=epiphany
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR([GNOMELOCALEDIR])

# uninstalled share dir to search data
AC_DEFINE_UNQUOTED([SHARE_UNINSTALLED_DIR], ["`pwd`/data"], [path to source data dir])

# *****************
# API Documentation
# *****************

GNOME_DOC_INIT
GTK_DOC_CHECK([1.0])

# ************
# Output files
# ************

AC_CONFIG_FILES([
Makefile
data/Makefile
data/epiphany-${EPIPHANY_API_VERSION}.pc:data/epiphany.pc.in
data/epiphany.desktop.in
data/bme.desktop.in
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
plugins/Makefile
plugins/desktop-file/Makefile
po/Makefile.in
],
[],
[EPIPHANY_API_VERSION=$EPIPHANY_API_VERSION])

AC_OUTPUT
