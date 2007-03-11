# Copyright © 2000-2004 Marco Pesenti Gritti
# Copyright © 2003, 2004, 2005, 2006 Christian Persch
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

AC_INIT([GNOME Web Browser],[2.18.0.90],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

GNOME_COMMON_INIT

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

EPIPHANY_MAJOR=2.18
EPIPHANY_API_VERSION=2.18

# This should stay at the major version of the latest stable releases
EPIPHANY_UA_VERSION=2.18

AC_SUBST([EPIPHANY_MAJOR])
AC_SUBST([EPIPHANY_API_VERSION])
AC_SUBST([EPIPHANY_UA_VERSION])

AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2 no-dist-gzip])

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

IT_PROG_INTLTOOL([0.35.0])

PKG_PROG_PKG_CONFIG

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_CXX_WARNINGS
GNOME_MAINTAINER_MODE_DEFINES

MORE_WARN_FLAGS=
DEPRECATION_FLAGS=
MOZILLA_WARN_CXXFLAGS="-Wno-ctor-dtor-privacy -Wno-non-virtual-dtor"

if test "x$enable_maintainer_mode" = "xyes"; then
	AC_DEFINE([MAINTAINER_MODE],[1],[Define to enable 'maintainer-only' behaviour])
	enable_debug=yes
# -DGTK_DISABLE_DEPRECATED 
        DEPRECATION_FLAGS="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED -DGNOME_VFS_DISABLE_DEPRECATED -DLIBGLADE_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED"
	MOZILLA_WARN_CXXFLAGS="-Wall -Wconversion -Wpointer-arith -Wcast-align -Woverloaded-virtual -Wsynth $MOZILLA_WARN_CXXFLAGS"
fi

GLIB_REQUIRED=2.12.0
GTK_REQUIRED=2.10.0
LIBXML_REQUIRED=2.6.12
LIBXSLT_REQUIRED=1.1.7
LIBGLADE_REQUIRED=2.3.1
LIBGNOMEVFS_REQUIRED=2.9.2
LIBGNOME_REQUIRED=2.14.0
LIBGNOMEUI_REQUIRED=2.14.0
GNOME_DESKTOP_REQUIRED=2.9.91
LIBSTARTUP_NOTIFICATION_REQUIRED=0.5
DBUS_GLIB_REQUIRED=0.35

PKG_CHECK_MODULES([DEPENDENCIES], [
		  glib-2.0 >= $GLIB_REQUIRED
		  gmodule-2.0
		  gtk+-2.0 >= $GTK_REQUIRED
		  gtk+-unix-print-2.0 >= $GTK_REQUIRED
		  libxml-2.0 >= $LIBXML_REQUIRED
		  libxslt >= $LIBXSLT_REQUIRED
		  libgnome-2.0 >= $LIBGNOME_REQUIRED
		  libgnomeui-2.0 >= $LIBGNOMEUI_REQUIRED
		  libglade-2.0 >= $LIBGLADE_REQUIRED \
		  gnome-vfs-2.0 >= $LIBGNOMEVFS_REQUIRED
		  gnome-vfs-module-2.0
		  gconf-2.0
		  gnome-desktop-2.0 >= $GNOME_DESKTOP_REQUIRED
		  libstartup-notification-1.0 >= $LIBSTARTUP_NOTIFICATION_REQUIRED
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
AC_SUBST([MOZILLA_EXTRA_LIBS])
AC_SUBST([MOZILLA_GLUE_LIBS])

if test "$gecko_cv_gecko_version_int" -lt "1008000"; then
	AC_MSG_ERROR([Gecko version $gecko_cv_gecko_version is not supported!])
fi

if test "$gecko_cv_gecko_flavour" != "toolkit"; then
	AC_MSG_ERROR(["$gecko_cv_gecko_flavour" flavoured geckos aren't tasty enough!])
fi

case "$gecko_cv_gecko" in
xulrunner) min_version=1.8 ;;
*firefox) min_version=1.5 ;;
*) AC_MSG_ERROR([Unsupported gecko "$gecko_cv_gecko"]) ;;
esac

# Added $gecko_cv_gecko-js for debian xulrunner brokenness
PKG_CHECK_MODULES([GECKO],[${gecko_cv_gecko}-xpcom >= $min_version ${gecko_cv_gecko}-js $gecko_cv_extra_pkg_dependencies])
AC_SUBST([GECKO_CFLAGS])
AC_SUBST([GECKO_LIBS])

# **********************************
# now tests for mozilla API variance
# **********************************

# First build a test program to see whether compiling and running XPCOM
# programs works at all, so we don't get false test failures below

GECKO_XPCOM_PROGRAM_CHECK

# Make sure we DON'T have the typeaheadfind extension

AC_MSG_CHECKING([whether typeaheadfind is broken])

GECKO_RUN_IFELSE([],
[GECKO_XPCOM_PROGRAM(
[[#include <nsIComponentRegistrar.h>]],
[[
status = 99;
nsCOMPtr<nsIComponentRegistrar> registrar;
rv = NS_GetComponentRegistrar (getter_AddRefs (registrar));
if (NS_FAILED (rv)) break;

status = 98;
nsCID *classID = nsnull;
rv = registrar->ContractIDToCID ("@mozilla.org/typeaheadfind;1", &classID);
if (NS_FAILED (rv) || !classID) break;

nsCID badClassID = { 0x46590685, 0xbc00, 0x4aac,
		     { 0xab, 0xed, 0x2c, 0x10, 0xa5, 0xb9, 0x45, 0xa4 } };

status = classID->Equals (badClassID) ? 97 : EXIT_SUCCESS;
]]
)],
[have_broken_typeaheadfind=no],
[have_broken_typeaheadfind=yes],
[have_broken_typeaheadfind=maybe])

AC_MSG_RESULT([$have_broken_typeaheadfind])

if test "$have_broken_typeaheadfind" = "yes"; then
	AC_MSG_ERROR([You must not compile $gecko_cv_gecko with the "typeaheadfind" extension enabled!])
fi

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

AM_CONDITIONAL([HAVE_MOZILLA_PSM],[test "$enable_psm" = "yes" -a "$have_psm" = "yes"])

# Check whether to enable our filepicker component

AC_MSG_CHECKING([whether to enable native gtk+ filepicker])
AC_ARG_ENABLE([filepicker],
	AS_HELP_STRING([--enable-filepicker],[Whether to enable the gtk+ native filepicker; not needed when the mozilla build itself has native gtk+ filepicker enabled (default:enabled)]),
	[],[enable_filepicker=no])
AC_MSG_RESULT([$enable_filepicker])

AM_CONDITIONAL([ENABLE_FILEPICKER],[test "$enable_filepicker" = "yes"])
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

REQUIRED_CONTRACTS="@mozilla.org/permissionmanager;1 @mozilla.org/PopupWindowManager;1 @mozilla.org/cookie/permission;1 @mozilla.org/permissions/contentblocker;1"
REQUIRED_EXTENSIONS="cookie,permissions"

GECKO_CHECK_CONTRACTIDS([$REQUIRED_CONTRACTS],
	[],[AC_MSG_ERROR([$gecko_cv_gecko needs to be compiled with at least --enable-extensions=default,$REQUIRED_EXTENSIONS])])

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

# Gecko 1.9 can handle .desktop files from file: URLs itself, but we
# also support windows .lnk files, so always enable this plugin

AC_MSG_CHECKING([whether to build the deskop file plugin])
AC_ARG_ENABLE([desktop-file-plugin],
	AS_HELP_STRING([--disable-desktop-file-plugin],[Disable the desktop file plugin (default: enabled)]),
	[],[enable_desktop_file_plugin=yes])
AC_MSG_RESULT([$enable_desktop_file_plugin])

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
	AS_HELP_STRING([--disable-zeroconf],[Whether to enable zeroconf bookmarks support (default: enabled)]),
	[], [enable_zeroconf=yes])
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

if test "$gecko_cv_gecko_version_int" -ge "1008001"; then

# Default to 'disabled' until it's fully functional
AC_MSG_CHECKING([whether spell checking support is requested])
AC_ARG_ENABLE([spell-checker],
	AS_HELP_STRING([--enable-spell-checker],[Whether to enable spell checking using enchant]),
	[],[enable_spell_checker=no])
AC_MSG_RESULT([$enable_spell_checker])

else
	enable_spell_checker=no
fi # gecko >= 1.8.1

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
AM_CXXFLAGS="$AM_CXXFLAGS $WARN_CXXFLAGS $MOZILLA_WARN_CXXFLAGS"
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
data/chrome/Makefile
data/chrome/brand.dtd
data/chrome/brand.properties
data/glade/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/actions/Makefile
data/icons/16x16/places/Makefile
data/icons/16x16/status/Makefile
data/icons/22x22/Makefile
data/icons/22x22/actions/Makefile
data/icons/22x22/places/Makefile
data/icons/22x22/status/Makefile
data/icons/24x24/Makefile
data/icons/24x24/actions/Makefile
data/icons/24x24/places/Makefile
data/icons/24x24/status/Makefile
data/icons/32x32/Makefile
data/icons/32x32/actions/Makefile
data/icons/32x32/status/Makefile
data/icons/scalable/Makefile
data/icons/scalable/actions/Makefile
data/icons/scalable/status/Makefile
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

# *************************************
# *************************************

echo "
Epiphany was configured with the following options:
        
	Source code location       : $srcdir
	Compiler                   : $CC
	Prefix                     : $prefix
	Extra debugging support    : $enable_debug

	Gecko backend              : $gecko_cv_gecko version $gecko_cv_gecko_version
	PSM support                : $enable_psm
	Zeroconf bookmarks support : $enable_zeroconf
	NetworkManager support     : $enable_network_manager
	Certificate manager        : $enable_certificate_manager (experimental)
	Python support             : $enable_python
	Desktop file plugin        : $enable_desktop_file_plugin
	Spellchecker               : $enable_spell_checker (experimental)

"
