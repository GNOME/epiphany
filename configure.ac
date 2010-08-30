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
m4_define([epiphany_version_minor],[30])
m4_define([epiphany_version_micro],[4])
m4_define([epiphany_version],[epiphany_version_major.epiphany_version_minor.epiphany_version_micro])

# This can be either "trunk" or "branches/gnome-x-y"
m4_define([epiphany_branch],["trunk"])

AC_INIT([GNOME Web Browser],[epiphany_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=epiphany],[epiphany])

GNOME_COMMON_INIT

AC_PREREQ([2.59])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])

EPIPHANY_API_VERSION=2.29

# This should stay at the major version of the latest stable releases
EPIPHANY_UA_VERSION=2.30

AC_SUBST([EPIPHANY_MAJOR],[epiphany_version_major.epiphany_version_minor])
AC_SUBST([EPIPHANY_API_VERSION])
AC_SUBST([EPIPHANY_UA_VERSION])

# for EPHY_CHECK_VERSION
AC_SUBST([EPIPHANY_MAJOR_VERSION], [epiphany_version_major])
AC_SUBST([EPIPHANY_MINOR_VERSION], [epiphany_version_minor])
AC_SUBST([EPIPHANY_MICRO_VERSION], [epiphany_version_micro])

# Build ID
BUILDID="$(TZ=UTC0 date +'%Y%m%d')"
AC_SUBST([BUILDID])

AM_INIT_AUTOMAKE([1.9 foreign dist-bzip2 no-dist-gzip tar-ustar])

# Use AM_SILENT_RULES if present
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

AC_ENABLE_SHARED([yes])
AC_ENABLE_STATIC([no])

AC_LIBTOOL_DLOPEN
LT_PREREQ(2.2)
LT_INIT

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])

IT_PROG_INTLTOOL([0.40.0])

PKG_PROG_PKG_CONFIG

GNOME_DEBUG_CHECK
GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

MORE_WARN_FLAGS=
DEPRECATION_FLAGS=

if test "$enable_maintainer_mode" = "yes"; then
	AC_DEFINE([MAINTAINER_MODE],[1],[Define to enable 'maintainer-only' behaviour])
	enable_debug=yes
	DEPRECATION_FLAGS="-DG_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED -DGDK_PIXBUF_DISABLE_DEPRECATED -DGCONF_DISABLE_DEPRECATED -DPANGO_DISABLE_DEPRECATED -DGNOME_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE"
fi

GLIB_REQUIRED=2.19.7
GTK_REQUIRED=2.19.5
LIBXML_REQUIRED=2.6.12
LIBXSLT_REQUIRED=1.1.7
LIBSTARTUP_NOTIFICATION_REQUIRED=0.5
LIBNOTIFY_REQUIRED=0.4
DBUS_GLIB_REQUIRED=0.35
WEBKIT_GTK_REQUIRED=1.2.3
LIBSOUP_GNOME_REQUIRED=2.29.91
GNOME_KEYRING_REQUIRED=2.26.0

PKG_CHECK_EXISTS([libnotify >= $LIBNOTIFY_REQUIRED],[have_libnotify=yes],[have_libnotify=no])

LIBNOTIFY_PACKAGE=
if test "$have_libnotify" = "yes"; then
  LIBNOTIFY_PACKAGE="libnotify >= $LIBNOTIFY_REQUIRED"
  AC_DEFINE([HAVE_LIBNOTIFY],[1],[Define if libnotify is available])
fi

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
		  gtk+-2.0 >= $GTK_REQUIRED
		  gtk+-unix-print-2.0 >= $GTK_REQUIRED
		  ice
		  x11
		  sm
		  libxml-2.0 >= $LIBXML_REQUIRED
		  libxslt >= $LIBXSLT_REQUIRED
		  gconf-2.0
		  libstartup-notification-1.0 >= $LIBSTARTUP_NOTIFICATION_REQUIRED
		  $LIBNOTIFY_PACKAGE
		  webkit-1.0 >= $WEBKIT_GTK_REQUIRED
		  libsoup-gnome-2.4 >= $LIBSOUP_GNOME_REQUIRED
		  gnome-keyring-1 >= $GNOME_KEYRING_REQUIRED
		  ])

# ****
# DBUS
# ****

PKG_CHECK_MODULES([DBUS],[dbus-glib-1 >= $DBUS_GLIB_REQUIRED])

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

AC_CHECK_FUNCS([localtime_r memchr memmove memset mkdir mkdtemp realpath sqrt strchr strcspn strdup strerror strrchr strtol strtoul strstr])

# for backtrace()
AC_CHECK_HEADERS([execinfo.h fcntl.h])
AC_C_INLINE
AC_FUNC_MKTIME
AC_FUNC_STRTOD
AC_TYPE_SIZE_T

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

# ***************
# Multimedia keys
# ***************

AC_CHECK_HEADERS([X11/XF86keysym.h])

EPIPHANY_FEATURES=

# *********************
# GObject Introspection
# *********************

GOBJECT_INTROSPECTION_CHECK([0.6.7])

if test "$found_introspection" = "yes"; then
  EPIPHANY_FEATURES="$EPIPHANY_FEATURES introspection"
  AC_DEFINE([ENABLE_INTROSPECTION], [1], [Define to enable introspection support])
fi

# ****
# Seed
# ****

AC_MSG_CHECKING([whether Seed support is requested])
AC_ARG_ENABLE([seed],
  [AS_HELP_STRING([--enable-seed],[Enable Seed support (default: disabled)])],
  [],[enable_seed=no])
AC_MSG_RESULT([$enable_seed])

if test "$enable_seed" = "yes" -a "$found_introspection" != "yes"; then
  AC_MSG_ERROR([GObject introspection support must be enabled for Seed])
fi

if test "$enable_seed" = "yes"; then
  EPIPHANY_FEATURES="$EPIPHANY_FEATURES seed"

  SEED_REQUIRED=2.27.91
  PKG_CHECK_MODULES([SEED],[seed >= $SEED_REQUIRED])

  AC_DEFINE([ENABLE_SEED],[1],[Define to compile with Seed support])
fi

AM_CONDITIONAL([ENABLE_SEED],[test "$enable_seed" = "yes"])

# *********************
# CA Certificate file
# Stolen from GIO's TLS
# *********************

AC_MSG_CHECKING([location of system Certificate Authority list])
AC_ARG_WITH(ca-file,
            [AC_HELP_STRING([--with-ca-file=@<:@path@:>@],
                            [path to system Certificate Authority list])])
if test "$with_ca_file" = "no"; then
    AC_MSG_RESULT([disabled])
else
    if test -z "$with_ca_file"; then
        for f in /etc/pki/tls/certs/ca-bundle.crt \
                 /etc/ssl/certs/ca-certificates.crt; do
            if test -f "$f"; then
                with_ca_file="$f"
            fi
        done
        if test -z "$with_ca_file"; then
            AC_MSG_ERROR([could not find. Use --with-ca-file=path to set, or --without-ca-file to disable])
        fi
    fi

    AC_MSG_RESULT($with_ca_file)
    if ! test -f "$with_ca_file"; then
        AC_MSG_ERROR([No such file '$with_ca_file'. Use --with-ca-file=path to set, or --without-ca-file to disable])
    fi
    GTLS_SYSTEM_CA_FILE="$with_ca_file"

    AC_DEFINE_UNQUOTED([GTLS_SYSTEM_CA_FILE], ["$GTLS_SYSTEM_CA_FILE"], [path to system Certificate Authority list])
fi

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

AC_SUBST([EPIPHANY_FEATURES])

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

	PKG_CHECK_MODULES(AVAHI,
                         [avahi-gobject >= $AVAHI_REQUIRED
                          avahi-client >= $AVAHI_REQUIRED]
                         ,[have_avahi=yes],[have_avahi=no])

        if test "$enable_zeroconf" = "yes" -a "$have_avahi" = "no"; then
                AC_MSG_ERROR([zeroconf support was requested but avahi not found])
        elif test "$have_avahi" = "yes"; then
                enable_zeroconf=yes
        else
                enable_zeroconf=no
        fi
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
	PKG_CHECK_MODULES(NETWORK_MANAGER, NetworkManager, [have_network_manager=yes],[have_network_manager=no])

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

AM_CONDITIONAL([ENABLE_SPELLCHECKER],[false])

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

# *****************
# API Documentation
# *****************

GNOME_DOC_INIT
GTK_DOC_CHECK([1.0])

# ************
# Output files
# ************

AC_DEFINE([EPIPHANY_COMPILATION],[1],[Compiling epiphany])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/epiphany-${EPIPHANY_API_VERSION}.pc:data/epiphany.pc.in
data/epiphany.desktop.in
data/bme.desktop.in
data/glade/Makefile
data/icons/Makefile
data/art/Makefile
data/ui/Makefile
doc/Makefile
doc/reference/Makefile
lib/Makefile
lib/egg/Makefile
lib/widgets/Makefile
embed/Makefile
src/Makefile
src/bookmarks/Makefile
src/epiphany.h
help/Makefile
po/Makefile.in
tests/Makefile
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
 	GObject introspection      : $found_introspection
 	Seed support               : $enable_seed
 	NSS support                : $enable_nss
	Build tests		   : $enable_tests
	CA Certificates file	   : $GTLS_SYSTEM_CA_FILE
"
