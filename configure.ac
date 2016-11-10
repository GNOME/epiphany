# Copyright © 2000-2004 Marco Pesenti Gritti
# Copyright © 2003, 2004, 2005, 2006, 2007 Christian Persch
# Copyright © 2015-2016 Igalia S.L
#
# This file is part of Epiphany.
#
# Epiphany is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Epiphany is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Epiphany.  If not, see <http://www.gnu.org/licenses/>.

AX_REQUIRE_DEFINED([APPSTREAM_XML])
AX_REQUIRE_DEFINED([AX_CHECK_ENABLE_DEBUG])
AX_REQUIRE_DEFINED([AX_COMPILER_FLAGS])
AX_REQUIRE_DEFINED([AX_GENERATE_CHANGELOG])
AX_REQUIRE_DEFINED([AX_IS_RELEASE])
AX_REQUIRE_DEFINED([GLIB_GSETTINGS])
AX_REQUIRE_DEFINED([PKG_CHECK_EXISTS])
AX_REQUIRE_DEFINED([PKG_CHECK_MODULES])
AX_REQUIRE_DEFINED([YELP_HELP_INIT])

dnl Must be a valid git tag
m4_define([epiphany_changelog_start],[3.21.4])

AC_INIT([GNOME Web Browser],[3.23.1.2],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])
AX_IS_RELEASE([git-directory])

AC_PREREQ([2.69])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4 libgd])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

AX_CHECK_ENABLE_DEBUG([yes])
AX_CODE_COVERAGE
AX_COMPILER_FLAGS

AX_GENERATE_CHANGELOG
AC_SUBST([EPIPHANY_CHANGELOG_START],[epiphany_changelog_start])

AC_PROG_CC

AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

dnl Note: I want to enable subdir-objects here, to silence Automake's many
dnl warnings about not using it. But we can't do this until we require Automake
dnl 1.16 (which has not been released as of June 2015), due to GNU bug #13928.
dnl If Automake 2.0 comes out next instead of 1.16, we will need to bump our
dnl requirement straight to that, and then there is no need to use
dnl subdir-objects here as it will be implied. It is unfortunate that there is
dnl no way our build can be compatible with both future and older Automake, but
dnl it is a bug after all, and updating for bugfixes is just how software works.
dnl Note also: the issue is that a directory _literally_ named $(top_srcdir)
dnl will be created under embed/web-extension, and the build will fail due to
dnl misplaced .Plo files (used for dependency tracking).
AM_INIT_AUTOMAKE([1.11 foreign dist-xz no-dist-gzip tar-ustar])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([dlopen disable-static])

APPSTREAM_XML

GLIB_GSETTINGS

YELP_HELP_INIT

# Tests

AC_MSG_CHECKING([whether to build tests])
AC_ARG_ENABLE([tests],
	AS_HELP_STRING([--enable-tests],[Whether to build tests (default: yes)]),
	[], [enable_tests=yes])
AC_MSG_RESULT([$enable_tests])

AM_CONDITIONAL([ENABLE_TESTS],[test "$enable_tests" = "yes"])

# ************
# Dependencies
# ************

LIBGD_INIT([_view-common notification static])

LT_LIB_M

GLIB_REQUIRED=2.44.0
GTK_REQUIRED=3.22.0
WEBKITGTK_REQUIRED=2.15.1

PKG_CHECK_MODULES([CAIRO], [cairo >= 1.2])
PKG_CHECK_MODULES([GCR], [gcr-3 >= 3.5.5])
PKG_CHECK_MODULES([GDK], [gdk-3.0 >= $GTK_REQUIRED])
PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0 >= 2.14])
PKG_CHECK_MODULES([GIO_UNIX], [gio-unix-2.0 >= $GLIB_REQUIRED])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= $GLIB_REQUIRED])
PKG_CHECK_MODULES([GNOME_DESKTOP], [gnome-desktop-3.0 >= 2.91.2])
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= $GTK_REQUIRED])
PKG_CHECK_MODULES([GTK_UNIX_PRINT], [gtk+-unix-print-3.0 >= $GTK_REQUIRED])
PKG_CHECK_MODULES([HOGWEED], [hogweed >= 3.2])
PKG_CHECK_MODULES([HTTPSEVERYWHERE], [httpseverywhere-0.2 >= 0.2.2])
PKG_CHECK_MODULES([JSON_GLIB], [json-glib-1.0 >= 1.2.0])
PKG_CHECK_MODULES([LIBNOTIFY], [libnotify >= 0.5.1])
PKG_CHECK_MODULES([LIBSECRET], [libsecret-1 >= 0.14])
PKG_CHECK_MODULES([LIBSOUP], [libsoup-2.4 >= 2.48.0])
PKG_CHECK_MODULES([LIBXML], [libxml-2.0 >= 2.6.12])
PKG_CHECK_MODULES([LIBXSLT], [libxslt >= 1.1.7])
PKG_CHECK_MODULES([NETTLE], [nettle >= 3.2])
PKG_CHECK_MODULES([SQLITE3], [sqlite3 >= 3.0])
PKG_CHECK_MODULES([WEBKIT2GTK], [webkit2gtk-4.0 >= $WEBKITGTK_REQUIRED])
PKG_CHECK_MODULES([WEBKIT2GTK_WEB_EXTENSION], [webkit2gtk-web-extension-4.0 >= $WEBKITGTK_REQUIRED])

# ******************
# Portability checks
# ******************

# for backtrace()
AC_CHECK_HEADERS([execinfo.h])
AC_SEARCH_LIBS([backtrace], [c execinfo])

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

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])
AC_SUBST([GETTEXT_PACKAGE], [epiphany])

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

AC_CONFIG_FILES([
Makefile
data/Makefile
data/icons/Makefile
gvdb/Makefile
help/Makefile
libgd/Makefile
lib/Makefile
lib/egg/Makefile
lib/history/Makefile
lib/widgets/Makefile
embed/Makefile
embed/web-extension/Makefile
src/Makefile
src/profile-migrator/Makefile
src/search-provider/Makefile
po/Makefile.in
tests/Makefile
tests/data/Makefile
])

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
"
