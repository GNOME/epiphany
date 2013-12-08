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

m4_define([epiphany_version_major],[3])
m4_define([epiphany_version_minor],[11])
m4_define([epiphany_version_micro],[1])
m4_define([epiphany_version],[epiphany_version_major.epiphany_version_minor.epiphany_version_micro])

AC_INIT([GNOME Web Browser],[epiphany_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

AC_SUBST([EPIPHANY_MAJOR],[epiphany_version_major.epiphany_version_minor])

# for EPHY_CHECK_VERSION
AC_SUBST([EPIPHANY_MAJOR_VERSION], [epiphany_version_major])
AC_SUBST([EPIPHANY_MINOR_VERSION], [epiphany_version_minor])
AC_SUBST([EPIPHANY_MICRO_VERSION], [epiphany_version_micro])

AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip tar-ustar])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_MAINTAINER_MODE([enable])

# Initialize libtool
LT_PREREQ(2.2)
LT_INIT([dlopen disable-static])

AC_PROG_CC

AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

IT_PROG_INTLTOOL([0.50.0])

GLIB_GSETTINGS

PKG_PROG_PKG_CONFIG

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

GNOME_CODE_COVERAGE

MORE_WARN_FLAGS="-Wdeclaration-after-statement -Wno-deprecated-declarations"

GLIB_REQUIRED=2.35.6
GTK_REQUIRED=3.11.2
LIBXML_REQUIRED=2.6.12
LIBXSLT_REQUIRED=1.1.7
WEBKIT_GTK_REQUIRED=2.2.2
LIBSOUP_REQUIRED=2.42.1
GNOME_DESKTOP_REQUIRED=2.91.2
LIBSECRET_REQUIRED=0.14
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=0.0.1
LIBNOTIFY_REQUIRED=0.5.1
GCR_REQUIRED=3.5.5
AVAHI_REQUIRED=0.6.22

WEBKIT_GTK_PC_NAME=webkit2gtk-3.0

# Tests

AC_MSG_CHECKING([whether to build tests])
AC_ARG_ENABLE([tests],
	AS_HELP_STRING([--enable-tests],[Wheter to build tests (default: yes)]),
	[], [enable_tests=yes])
AC_MSG_RESULT([$enable_tests])

AM_CONDITIONAL([ENABLE_TESTS],[test "$enable_tests" = "yes"])

PKG_CHECK_MODULES([DEPENDENCIES], [
		  glib-2.0 >= $GLIB_REQUIRED
		  gmodule-2.0
		  gthread-2.0
		  gio-unix-2.0 >= $GLIB_REQUIRED
		  gtk+-3.0 >= $GTK_REQUIRED
		  gtk+-unix-print-3.0 >= $GTK_REQUIRED
		  x11
		  libwnck-3.0
		  libxml-2.0 >= $LIBXML_REQUIRED
		  libxslt >= $LIBXSLT_REQUIRED
		  $WEBKIT_GTK_PC_NAME >= $WEBKIT_GTK_REQUIRED
		  libsoup-2.4 >= $LIBSOUP_REQUIRED
		  libsecret-1 >= $LIBSECRET_REQUIRED
		  gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED
		  gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED
		  libnotify >= $LIBNOTIFY_REQUIRED
		  sqlite3
		  gcr-3 >= $GCR_REQUIRED
		  avahi-gobject >= $AVAHI_REQUIRED
		  avahi-client >= $AVAHI_REQUIRED
		  ])

PKG_CHECK_MODULES(WEB_EXTENSION, [
                  $WEBKIT_GTK_PC_NAME >= $WEBKIT_GTK_REQUIRED
                  libsecret-1 >= $LIBSECRET_REQUIRED
                  ])
AC_SUBST(WEB_EXTENSION_CFLAGS)
AC_SUBST(WEB_EXTENSION_LIBS)

# ******************
# Portability checks
# ******************

AC_CHECK_FUNCS([localtime_r memchr memmove memset mkdir mkdtemp realpath sqrt strchr strcspn strdup strerror strrchr strtol strtoul strstr])

# for backtrace()
AC_CHECK_HEADERS([execinfo.h fcntl.h])
AC_C_INLINE
AC_FUNC_MKTIME
AC_FUNC_STRTOD
AC_TYPE_SIZE_T

# ***************
# Multimedia keys
# ***************

AC_CHECK_HEADERS([X11/XF86keysym.h])

# ***
# NSS
# ***

AC_MSG_CHECKING([whether NSS support is requested])
AC_ARG_ENABLE([nss],
        [AS_HELP_STRING([--enable-nss], [Enable NSS support (default: enabled)])],
        [], [enable_nss=yes])
AC_MSG_RESULT([$enable_nss])

if test "$enable_nss" = "yes"; then
   PKG_CHECK_MODULES([NSS], [nss])

   AC_DEFINE([ENABLE_NSS], [1], [Define to compile with NSS support])
fi

AM_CONDITIONAL([ENABLE_NSS],[test "$enable_nss" = "yes"])

# *******************************
# Add warning flags
# *******************************

AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS $MORE_WARN_FLAGS"
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([AM_CFLAGS])
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

# *******************************
# User help
# *******************************

YELP_HELP_INIT

# ****************
# Distributor name
# ****************

AC_ARG_WITH([distributor-name],
  AS_HELP_STRING([--with-distributor-name=name],[Set the distributor name]),
  [LSB_DISTRIBUTOR="$withval"])

if test -z "$LSB_DISTRIBUTOR"; then
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

# ************
# Output files
# ************

AC_DEFINE([EPIPHANY_COMPILATION],[1],[Compiling epiphany])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/epiphany.desktop.in
data/pages/Makefile
doc/Makefile
help/Makefile
lib/Makefile
lib/egg/Makefile
lib/history/Makefile
lib/widgets/Makefile
embed/Makefile
embed/web-extension/Makefile
src/Makefile
src/bookmarks/Makefile
po/Makefile.in
tests/Makefile
tests/data/Makefile
],
[])

AC_OUTPUT

# *************************************
# *************************************

echo "
Epiphany was configured with the following options:
        
	Source code location	   : $srcdir
	Compiler		   : $CC
	Prefix			   : $prefix
	Extra debugging support	   : $enable_debug
	Build tests		   : $enable_tests
	Code coverage		   : $enable_code_coverage

	NSS support		   : $enable_nss
"
