# Copyright © 2000-2004 Marco Pesenti Gritti
# Copyright © 2003, 2004, 2005, 2006, 2007 Christian Persch
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
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

m4_define([epiphany_version_major],[2])
m4_define([epiphany_version_minor],[22])
m4_define([epiphany_version_micro],[2])
m4_define([epiphany_version],[epiphany_version_major.epiphany_version_minor.epiphany_version_micro])

# This can be either "trunk" or "branches/gnome-x-y"
m4_define([epiphany_branch],["branches/gnome-2-22"])

AC_INIT([GNOME Web Browser],[epiphany_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

GNOME_COMMON_INIT

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

EPIPHANY_API_VERSION=2.22

# This should stay at the major version of the latest stable releases
EPIPHANY_UA_VERSION=2.22

AC_SUBST([EPIPHANY_MAJOR],[epiphany_version_major.epiphany_version_minor])
AC_SUBST([EPIPHANY_API_VERSION],[epiphany_version_major.epiphany_version_minor])
AC_SUBST([EPIPHANY_UA_VERSION])

# for EPHY_CHECK_VERSION
AC_SUBST([EPIPHANY_MAJOR_VERSION], [epiphany_version_major])
AC_SUBST([EPIPHANY_MINOR_VERSION], [epiphany_version_minor])
AC_SUBST([EPIPHANY_MICRO_VERSION], [epiphany_version_micro])

# Build ID
BUILDID="$(TZ=UTC0 date +'%Y%m%d')"
AC_SUBST([BUILDID])

AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2 no-dist-gzip tar-ustar])

AM_MAINTAINER_MODE

AC_ENABLE_SHARED([yes])
AC_ENABLE_STATIC([no])

AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_PROG_CXX

AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

# Check whether a C++ was found (AC_PROG_CXX sets $CXX to "g++" even when it
# doesn't exist)

AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([],[])],[],[AC_MSG_ERROR([No C++ compiler found])])
AC_LANG_POP([C++])

IT_PROG_INTLTOOL([0.35.0])

PKG_PROG_PKG_CONFIG

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_CXX_WARNINGS
GNOME_MAINTAINER_MODE_DEFINES

MORE_WARN_FLAGS=
DEPRECATION_FLAGS=
GECKO_WARN_CXXFLAGS="-Wno-ctor-dtor-privacy -Wno-non-virtual-dtor"

if test "$enable_maintainer_mode" = "yes"; then
	AC_DEFINE([MAINTAINER_MODE],[1],[Define to enable 'maintainer-only' behaviour])
	enable_debug=yes
	DEPRECATION_FLAGS="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED -DLIBGLADE_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"
	GECKO_WARN_CXXFLAGS="-Wall -Wconversion -Wpointer-arith -Wcast-align -Woverloaded-virtual -Wsynth $GECKO_WARN_CXXFLAGS"
fi

GLIB_REQUIRED=2.16.0
GTK_REQUIRED=2.12.0
LIBXML_REQUIRED=2.6.12
LIBXSLT_REQUIRED=1.1.7
LIBGLADE_REQUIRED=2.3.1
LIBGNOME_REQUIRED=2.14.0
LIBGNOMEUI_REQUIRED=2.14.0
GNOME_DESKTOP_REQUIRED=2.9.91
LIBSTARTUP_NOTIFICATION_REQUIRED=0.5
LIBNOTIFY_REQUIRED=0.4
DBUS_GLIB_REQUIRED=0.35

PKG_CHECK_EXISTS([libnotify >= $LIBNOTIFY_REQUIRED],[have_libnotify=yes],[have_libnotify=no])

LIBNOTIFY_PACKAGE=
if test "$have_libnotify" = "yes"; then
  LIBNOTIFY_PACKAGE="libnotify >= $LIBNOTIFY_REQUIRED"
  AC_DEFINE([HAVE_LIBNOTIFY],[1],[Define if libnotify is available])
fi

PKG_CHECK_MODULES([DEPENDENCIES], [
		  glib-2.0 >= $GLIB_REQUIRED
		  gmodule-2.0
		  gio-unix-2.0 >= $GLIB_REQUIRED
		  gtk+-2.0 >= $GTK_REQUIRED
		  gtk+-unix-print-2.0 >= $GTK_REQUIRED
		  libxml-2.0 >= $LIBXML_REQUIRED
		  libxslt >= $LIBXSLT_REQUIRED
		  libgnome-2.0 >= $LIBGNOME_REQUIRED
		  libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED
		  libglade-2.0 >= $LIBGLADE_REQUIRED \
		  gconf-2.0
		  gnome-desktop-2.0 >= $GNOME_DESKTOP_REQUIRED
		  libstartup-notification-1.0 >= $LIBSTARTUP_NOTIFICATION_REQUIRED
		  $LIBNOTIFY_PACKAGE
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

if test "$enable_maintainer_mode" = "yes"; then

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
	
	if test "$ac_cv_have_declaration_after_statement" = "yes"; then
		MORE_WARN_FLAGS="$MORE_WARN_FLAGS $_FLAGS"
	fi
	CFLAGS="$_SAVE_CFLAGS"
	AC_LANG_POP([C])
fi

# ************
# Engine check
# ************

AC_MSG_CHECKING(for which engine to use)

AC_ARG_WITH([engine],
	AS_HELP_STRING([--with-engine@<:@=mozilla|webkit|xulrunner@:>@],
		[Which engine to build against @<:@mozilla@:>@]),
	[],[with_engine="mozilla"])

AC_MSG_RESULT($with_engine)
AC_SUBST([with_engine])

# *******
# Mozilla
# *******

if test "$with_engine" = "mozilla"; then

GECKO_INIT([GECKO])

AC_DEFINE([WITH_GECKO_ENGINE],[1],[Define if you wish to enable mozilla engine])

AC_SUBST([GECKO])
AC_SUBST([GECKO_FLAVOUR])
AC_SUBST([GECKO_INCLUDE_ROOT])
AC_SUBST([GECKO_HOME])
AC_SUBST([GECKO_PREFIX])
AC_SUBST([GECKO_EXTRA_LIBS])
AC_SUBST([GECKO_GLUE_LIBS])

if test "$gecko_cv_gecko_version_int" -lt "1008000"; then
	AC_MSG_ERROR([Gecko version $gecko_cv_gecko_version is not supported!])
fi

if test "$gecko_cv_gecko_flavour" != "toolkit"; then
	AC_MSG_ERROR(["$gecko_cv_gecko_flavour" flavoured geckos aren't tasty enough!])
fi

case "$gecko_cv_gecko" in
xulrunner) min_version=1.8 ;;
libxul*) min_version=1.9 ;;
*firefox) min_version=1.5 ;;
*) AC_MSG_ERROR([Unsupported gecko "$gecko_cv_gecko"]) ;;
esac

# Added $gecko_cv_gecko-js for debian xulrunner brokenness
if test $gecko_cv_gecko != "libxul-embedding" -a $gecko_cv_gecko != "libxul"; then
	# debian xulrunner lacks -xpcom -> -js depends
	if pkg-config --exists ${gecko_cv_gecko}-js; then
		gecko_cv_extra_pkg_dependencies="$gecko_cv_extra_pkg_dependencies ${gecko_cv_gecko}-js"
	fi
	GECKO_CFLAGS=`pkg-config --cflags ${gecko_cv_gecko}-xpcom '>=' $min_version ${gecko_cv_gecko}-js $gecko_cv_extra_pkg_dependencies`
	GECKO_LIBS=`pkg-config --libs ${gecko_cv_gecko}-xpcom '>=' $min_version ${gecko_cv_gecko}-js $gecko_cv_extra_pkg_dependencies`

#	PKG_CHECK_MODULES([GECKO],[ ${gecko_cv_gecko}-xpcom >= $min_version ${gecko_cv_gecko}-js $gecko_cv_extra_pkg_dependencies])
else
	GECKO_CFLAGS="$GECKO_CFLAGS"
	GECKO_LIBS="$GECKO_LIBS `pkg-config --libs ${gecko_cv_gecko}`"
fi

AC_MSG_CHECKING(nspr in gecko)
if test "$GECKO_NSPR" != yes; then
	AC_MSG_RESULT(no)
	AC_MSG_CHECKING(nspr in system)
	gecko_nspr=
	if $PKG_CONFIG --exists mozilla-nspr; then
		gecko_nspr=mozilla-nspr
	elif $PKG_CONFIG --exists ${gecko_cv_gecko}-nspr; then
		gecko_nspr=${gecko_cv_gecko}-nspr
	fi
	if $PKG_CONFIG --exists nspr; then
		gecko_nspr=nspr
	fi
	if test -z "$gecko_nspr"; then
		AC_MSG_ERROR([no nspr available])
	fi

	GECKO_CFLAGS="$GECKO_CFLAGS `$PKG_CONFIG --cflags ${gecko_nspr}`"
	GECKO_LIBS="$GECKO_LIBS `$PKG_CONFIG --libs ${gecko_nspr}`"
	_GECKO_CFLAGS="$GECKO_CFLAGS"
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(yes)
fi

AC_SUBST([GECKO_CFLAGS])
AC_SUBST([GECKO_LIBS])

# *****************
# Weasel UA version
# *****************

# FIXMEchpe: find a way to always automatically use the latest weasel minor version!

if test "$gecko_cv_gecko_version_int" -ge "1009000"; then
  WEASEL_UA_VERSION="3.0"
elif test "$gecko_cv_gecko_version_int" -ge "1008001"; then
  WEASEL_UA_VERSION="2.0.0.4"
else
  WEASEL_UA_VERSION="1.5.0.12"
fi

AC_SUBST([WEASEL_UA_VERSION])

# **********************************
# now tests for mozilla API variance
# **********************************

# First build a test program to see whether compiling and running XPCOM
# programs works at all, so we don't get false test failures below

GECKO_XPCOM_PROGRAM_CHECK

# Check for PSM header(s) to infer PSM presence

GECKO_CHECK_HEADERS([pipnss],[nsIX509Cert.h],[have_psm=yes],[have_psm=no])

# Make sure we don't accidentally build without PSM support

AC_MSG_CHECKING([whether to include PSM support])
AC_ARG_ENABLE([psm],
	AS_HELP_STRING([--disable-psm],[Disable PSM support (default: enabled)]),
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

# Check whether to enable our filepicker component

AC_MSG_CHECKING([whether to enable native gtk+ filepicker])
AC_ARG_ENABLE([filepicker],
	AS_HELP_STRING([--enable-filepicker],[Whether to enable the gtk+ native filepicker; not needed when the mozilla build itself has native gtk+ filepicker enabled (default:enabled)]),
	[],[enable_filepicker=yes])
AC_MSG_RESULT([$enable_filepicker])

if test "$enable_filepicker" = "yes"; then
	AC_DEFINE([ENABLE_FILEPICKER],[1],[Define to enable the native filepicker])
fi

# Check for nsINonBlockingAlertService
# Added in 1.8.1 and 1.9

GECKO_CHECK_HEADERS([windowwatcher],[nsINonBlockingAlertService.h])

# Check for nsIGlobalHistory3
# Added in 1.8.1 and 1.9

GECKO_CHECK_HEADERS([docshell],[nsIGlobalHistory3.h])

# Check for nsIIOService2.h
# Added in 1.9

GECKO_CHECK_HEADERS([necko],[nsIIOService2.h])

# Check for nsIMutableArray.h
# Split out into its own header in 1.9

GECKO_CHECK_HEADERS([xpcom],[nsIMutableArray.h])

# Check for spell check extension

GECKO_CHECK_HEADERS([spellchecker],[mozISpellCheckingEngine.h],
	[have_gecko_spell_checker=yes],[have_gecko_spell_checker=no])

# Check for some contractIDs that we need but are provided by extensions
# which may or may not have been built.

#REQUIRED_CONTRACTS="@mozilla.org/permissionmanager;1 @mozilla.org/PopupWindowManager;1 @mozilla.org/cookie/permission;1 @mozilla.org/permissions/contentblocker;1"
#REQUIRED_EXTENSIONS="cookie,permissions"
#
#GECKO_CHECK_CONTRACTIDS([$REQUIRED_CONTRACTS],
#	[],[AC_MSG_ERROR([$gecko_cv_gecko needs to be compiled with at least --enable-extensions=default,$REQUIRED_EXTENSIONS])])

fi # with_engine = mozilla

GECKO_DEFINES
AM_CONDITIONAL([WITH_GECKO_ENGINE], [test "$with_engine" = "mozilla"])
AM_CONDITIONAL([HAVE_XULRUNNER],[test "$gecko_cv_gecko" = "xulrunner"])
AM_CONDITIONAL([HAVE_MOZILLA_PSM],[test "$enable_psm" = "yes" -a "$have_psm" = "yes"])
AM_CONDITIONAL([ENABLE_FILEPICKER],[test "$enable_filepicker" = "yes"])

# *********
# Xulrunner
# *********

# This is the mozilla backend for xulrunner 1.9

if test "$with_engine" = "xulrunner"; then

LIBXUL_INIT

AC_DEFINE([WITH_XULRUNNER_ENGINE],[1],[Define if building the Xulrunner backend])

WEASEL_UA_VERSION=3.0
AC_SUBST([WEASEL_UA_VERSION])


fi

LIBXUL_DEFINES

AM_CONDITIONAL([WITH_XULRUNNER_ENGINE],[test "$with_engine" = "xulrunner"])

# ******
# WebKit
# ******

if test "$with_engine" = "webkit" ; then
   AC_DEFINE([WITH_WEBKIT_ENGINE],[1],[Define if you wish to enable webkit engine])
   PKG_CHECK_MODULES([WEBKIT], [webkit-1.0])
   AC_SUBST([WEBKIT_CFLAGS])
   AC_SUBST([WEBKIT_LIBS])
fi # with_engine = webkit

AM_CONDITIONAL([WITH_WEBKIT_ENGINE], test "$with_engine" = "webkit")

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

if test "$have_python" != "no"; then
	AM_PATH_PYTHON([2.3],[],[no])

	if test "x$PYTHON" = "x:"; then
		have_python=no
	fi
fi

if test "$have_python" != "no"; then
	AM_CHECK_PYTHON_HEADERS([],[have_python=no])
fi

if test "$have_python" != "no"; then
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

	# FIXME: do we really need this test?
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

if test "$have_python" != "no"; then
	PYGTK_REQUIRED=2.7.1
	GNOME_PYTHON_REQUIRED=2.6.0

	PKG_CHECK_MODULES([PYGTK], [pygtk-2.0 >= $PYGTK_REQUIRED gnome-python-2.0 >= $GNOME_PYTHON_REQUIRED],
		[],[have_python=no])

	AC_SUBST([PYGTK_CFLAGS])
	AC_SUBST([PYGTK_LIBS])
fi

if test "$have_python" != "no"; then
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

	# Check for -fno-strict-aliasing
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

if test "$have_python" != "yes"; then
	if test "$enable_python" = "yes"; then
		AC_MSG_ERROR([Python not found])
	elif test "$enable_python" = "autodetect"; then
		enable_python=no
		AC_MSG_WARN([Python not found, disabling python support])
	fi
elif test "$enable_python" != "no"; then
	enable_python=yes
	AC_DEFINE([ENABLE_PYTHON],[1],[Define to compile with python support])
	EPIPHANY_FEATURES="$EPIPHANY_FEATURES python"
fi

AM_CONDITIONAL([ENABLE_PYTHON],[test "$enable_python" = "yes"])

AC_SUBST([EPIPHANY_FEATURES])

# *******
# Plugins
# *******

# Gecko 1.9 can handle .desktop files from file: URLs itself, but we
# also support windows .lnk files, so always enable this plugin

if test "$with_engine" = "mozilla" ; then

AC_MSG_CHECKING([whether to build the deskop file plugin])
AC_ARG_ENABLE([desktop-file-plugin],
	AS_HELP_STRING([--disable-desktop-file-plugin],[Disable the desktop file plugin (default: enabled)]),
	[],[enable_desktop_file_plugin=yes])
AC_MSG_RESULT([$enable_desktop_file_plugin])

if test "$enable_desktop_file_plugin" = "yes"; then
	if test "${gecko_cv_gecko}" = "libxul-embedding" -o "${gecko_cv_gecko}" = "libxul"; then
		PKG_CHECK_MODULES([DESKTOP_FILE_PLUGIN_DEP],[${gecko_cv_gecko} mozilla-plugin gtk+-2.0 >= 2.6.0])
	else
		PKG_CHECK_MODULES([DESKTOP_FILE_PLUGIN_DEP],[${gecko_cv_gecko}-xpcom ${gecko_cv_gecko}-plugin gtk+-2.0 >= 2.6.0])
	fi
	AC_SUBST([DESKTOP_FILE_PLUGIN_DEP_CFLAGS])
	AC_SUBST([DESKTOP_FILE_PLUGIN_DEP_LIBS])
fi

if test "$enable_desktop_file_plugin" = "yes"; then
	AC_DEFINE([HAVE_PRIVATE_PLUGINS],[1],[Define if any private plugins are enabled])
fi

else # with_engine = mozilla
        enable_desktop_file_plugin=no
fi   # with_engine != mozilla

AM_CONDITIONAL([ENABLE_DESKTOP_FILE_PLUGIN],[test "$enable_desktop_file_plugin" = "yes"])

# *******************
# Additional features
# *******************

# Zeroconf bookmarks sites

AC_MSG_CHECKING([whether to enable zeroconf bookmarks support])
AC_ARG_ENABLE([zeroconf],
	AS_HELP_STRING([--disable-zeroconf],[Whether to enable zeroconf bookmarks support (default: autodetect)]),
	[], [enable_zeroconf=auto])
AC_MSG_RESULT([$enable_zeroconf])

if test "$enable_zeroconf" != "no"; then
	AVAHI_REQUIRED=0.6.22

	PKG_CHECK_MODULES(AVAHI, [avahi-gobject >= $AVAHI_REQUIRED],[have_avahi=yes],[have_avahi=no])

        if test "$enable_zeroconf" = "yes" -a "$have_avahi" = "no"; then
                AC_MSG_ERROR([zeroconf support was requested but avahi not found])
        elif test "$have_avahi" = "yes"; then
                enable_zeroconf=yes
        else
                enable_zeroconf=no
        fi

	AC_SUBST(AVAHI_CFLAGS)
	AC_SUBST(AVAHI_LIBS)
fi

if test "$enable_zeroconf" = "yes"; then
	AC_DEFINE([ENABLE_ZEROCONF],[1],[Define to enable zeroconf bookmarks support])
fi

AM_CONDITIONAL([ENABLE_ZEROCONF],[test "$enable_zeroconf" = "yes"])

# NetworkManager

AC_MSG_CHECKING([whether to enable NetworkManager support])
AC_ARG_ENABLE([network-manager],
	AS_HELP_STRING([--enable-network-manager],[Whether to enable automatic network status with NetworkManager (default: disabled)]),
	[],[enable_network_manager=no])
AC_MSG_RESULT([$enable_network_manager])

if test "$enable_network_manager" != "no"; then
	AC_CHECK_HEADERS([NetworkManager/NetworkManager.h],
		[have_network_manager=yes],[have_network_manager=no])

	if test "$enable_network_manager" = "yes" -a "$have_network_manager" = "no"; then
		AC_MSG_ERROR([NetworkManager support requested but NetworkManager headers not found])
	elif test "$have_network_manager" = "no"; then
		AC_MSG_WARN([NetworkManager headers not found; disabling NetworkManager support])
	fi
fi

if test "$enable_network_manager" = "yes"; then
	AC_DEFINE([ENABLE_NETWORK_MANAGER],[1],[Define for NetworkManager support])
fi

AM_CONDITIONAL([ENABLE_NETWORK_MANAGER],[test "$enable_network_manager" = "yes"])

# Enchant spell checking
# Does not appear to work on gecko 1.8.0, so only offer for >= 1.8.1

if test "$with_engine" = "mozilla" ; then

if test "$gecko_cv_gecko_version_int" -ge "1008001"; then

# Default to 'disabled' until it's fully functional
AC_MSG_CHECKING([whether spell checking support is requested])
AC_ARG_ENABLE([spell-checker],
	AS_HELP_STRING([--enable-spell-checker],[Whether to enable spell checking using enchant]),
	[],[enable_spell_checker=no])
AC_MSG_RESULT([$enable_spell_checker])
else # gecko >= 1.8.1
        enable_spell_checker=no
fi   # gecko < 1.8.1

if test "$enable_spell_checker" = "yes" -a "$have_gecko_spell_checker" != "yes"; then
	AC_MSG_ERROR([Spell check support requested but $gecko_cv_gecko was not compiled with 'spellchecker' extension enabled.])
fi

if test "$enable_spell_checker" = "yes" -a "$have_gecko_spell_checker" = "yes"; then
	# FIXME check which version we really need
	ENCHANT_REQUIRED=1.0

	PKG_CHECK_MODULES([SPELLCHECKER],[enchant >= $ENCHANT_REQUIRED])
	AC_SUBST([SPELLCHECKER_CFLAGS])
	AC_SUBST([SPELLCHECKER_LIBS])

	AC_DEFINE([ENABLE_SPELLCHECKER],[1],[Define to enable the spell checker])
fi

else # with_engine = mozilla
        enable_spell_checker=no
fi   # with_engine != mozilla


AM_CONDITIONAL([ENABLE_SPELLCHECKER],[test "$enable_spell_checker" = "yes" -a "$have_gecko_spell_checker" = "yes"])

# ************
# Misc defines
# ************

# uninstalled share dir to search data
AC_DEFINE_UNQUOTED([SHARE_UNINSTALLED_DIR], ["`pwd`/data"], [path to source data dir])

# *******************************
# Add warning flags
# *******************************

AM_CPPFLAGS="$AM_CPPFLAGS $DEPRECATION_FLAGS"
AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS $MORE_WARN_FLAGS"
AM_CXXFLAGS="$AM_CXXFLAGS $WARN_CXXFLAGS $GECKO_WARN_CXXFLAGS"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

# *******************************
# Internationalization
# ******************************* 

ISO_CODES_REQUIRED=0.35

PKG_CHECK_EXISTS([iso-codes >= $ISO_CODES_REQUIRED],
	[have_iso_codes=yes],[have_iso_codes=no])

if test "$have_iso_codes" = "yes"; then
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

if test "$have_iso_codes" = "yes"; then
	AC_DEFINE_UNQUOTED([ISO_CODES_PREFIX],["`$PKG_CONFIG --variable=prefix iso-codes`"],[ISO codes prefix])
	AC_DEFINE([HAVE_ISO_CODES],[1],[Define if you have the iso-codes package])
else
	AC_MSG_ERROR([iso-codes is required])
fi

GETTEXT_PACKAGE=epiphany
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

# uninstalled share dir to search data
AC_DEFINE_UNQUOTED([SHARE_UNINSTALLED_DIR], ["`pwd`/data"], [path to source data dir])

# ****************
# Distributor name
# ****************

AC_ARG_WITH([distributor-name],
  AS_HELP_STRING([--with-distributor-name=name],[Set the distributor name]),
  [LSB_DISTRIBUTOR="$withval"])

if test -z "$LSB_DISTRIBUTOR" = ""; then
	AC_CHECK_PROGS([LSB_RELEASE], [lsb_release],)
	if test -n "$LSB_RELEASE"; then
		# Fallback on lsb_release if available
		LSB_DISTRIBUTOR=$($LSB_RELEASE -i -s)
	else
		# Fallback on the product name
		LSB_DISTRIBUTOR="Epiphany"
	fi
fi

AC_DEFINE_UNQUOTED([LSB_DISTRIBUTOR],["$LSB_DISTRIBUTOR"],[Distributor name])

# *****************
# API Documentation
# *****************

GNOME_DOC_INIT
GTK_DOC_CHECK([1.0])

# ****************************
# SVN for ChangeLog generation
# ****************************

AC_SUBST([SVN_ROOT],[http://svn.gnome.org/svn])
AC_SUBST([SVN_MODULE],[epiphany])
AC_SUBST([SVN_BRANCH],[epiphany_branch])

# ************
# Output files
# ************

AC_CONFIG_FILES([
Makefile
data/Makefile
data/epiphany-${EPIPHANY_API_VERSION}.pc:data/epiphany.pc.in
data/epiphany.desktop.in
data/bme.desktop.in
data/chrome/Makefile
data/chrome/brand.dtd
data/chrome/brand.properties
data/glade/Makefile
data/icons/Makefile
data/art/Makefile
data/ui/Makefile
data/weasel-ua-pref.js
doc/Makefile
doc/reference/Makefile
lib/Makefile
lib/egg/Makefile
lib/widgets/Makefile
embed/Makefile
embed/mozilla/Makefile
embed/webkit/Makefile
src/Makefile
src/bookmarks/Makefile
src/ephy-version.h
help/Makefile
plugins/Makefile
plugins/desktop-file/Makefile
po/Makefile.in
],
[],
[EPIPHANY_API_VERSION=$EPIPHANY_API_VERSION])

AC_OUTPUT

# *************************************
# *************************************

echo "
Epiphany was configured with the following options:
        
	Source code location       : $srcdir
	Compiler                   : $CC
	Prefix                     : $prefix
	Extra debugging support    : $enable_debug

 	Zeroconf bookmarks support : $enable_zeroconf
 	NetworkManager support     : $enable_network_manager
 	Python support             : $enable_python
 	Desktop file plugin        : $enable_desktop_file_plugin
 	Spellchecker               : $enable_spell_checker (experimental)
 
        Engine                     : $with_engine
 "

if test "$with_engine" = "mozilla"; then
echo "        Gecko backend              : $gecko_cv_gecko version $gecko_cv_gecko_version
	PSM support                : $enable_psm
"
fi
