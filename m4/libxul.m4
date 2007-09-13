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

# LIBXUL_INIT([DO-CHECK],[ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
#
# Checks for libxul, and aborts if it's not found
#
# Checks for -fshort-wchar compiler variable, and adds it to
# AM_CXXFLAGS if found
#
# Checks whether RTTI is enabled, and adds -fno-rtti to 
# AM_CXXFLAGS otherwise
#
# Checks whether the gecko build is a debug build, and adds
# debug flags to AM_CXXFLAGS if it is.

AC_DEFUN([LIBXUL_INIT],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl

PKG_CHECK_EXISTS([libxul],[libxul_cv_have_libxul=yes],[libxul_cv_have_libxul=no])
if test "$libxul_cv_have_libxul" != "yes"; then
  AC_MSG_ERROR([libxul not found])
fi

libxul_cv_prefix="$($PKG_CONFIG --variable=prefix libxul)"
libxul_cv_include_root="$($PKG_CONFIG --variable=includedir libxul)"
libxul_cv_sdkdir="$($PKG_CONFIG --variable=sdkdir libxul)"

# FIXMEchpe: this isn't right. The pc file seems buggy, but until
# I can figure this out, do it like this:
libxul_cv_libdir="$(readlink $($PKG_CONFIG --variable=sdkdir libxul)/bin)"

libxul_cv_includes="-I${libxul_cv_include_root}/stable -I${libxul_cv_include_root}/unstable"

AC_DEFINE([HAVE_LIBXUL],[1],[Define for libxul])

LIBXUL_PREFIX="$libxul_cv_prefix"
LIBXUL_INCLUDE_ROOT="$libxul_cv_include_root"
LIBXUL_INCLUDES="$libxul_cv_includes"
LIBXUL_LIBDIR="$libxul_cv_libdir"

LIBXUL_CXXCPPFLAGS=
LIBXUL_CXXFLAGS=
LIBXUL_LDFLAGS=

# Can't use the value from the .pc file, since it seems buggy
# Until I can figure it out, do this instead
LIBXUL_LIBS="-L${libxul_cv_sdkdir}/lib -lxpcomglue_s -L${libxul_cv_sdkdir}/bin -lxul -lxpcom"

# ***********************
# Check for -fshort-wchar
# ***********************

# NOTE: This is really gcc-only
# Do this test using CXX only since some versions of gcc
# 2.95-2.97 have a signed wchar_t in c++ only and some versions
# only have short-wchar support for c++.

AC_LANG_PUSH([C++])

_SAVE_CXXFLAGS=$CXXFLAGS
CXXFLAGS="$CXXFLAGS $LIBXUL_CXXFLAGS -fshort-wchar"

AC_CACHE_CHECK([for compiler -fshort-wchar option],
  libxul_cv_have_usable_wchar_option,
  [AC_RUN_IFELSE([AC_LANG_SOURCE(
		 [[#include <stddef.h>
		  int main () {
		    return (sizeof(wchar_t) != 2) || (wchar_t)-1 < (wchar_t) 0 ;
		  } ]])],
  [libxul_cv_have_usable_wchar_option="yes"],
  [libxul_cv_have_usable_wchar_option="no"],
  [libxul_cv_have_usable_wchar_option="maybe (cross-compiling)"])])

CXXFLAGS="$_SAVE_CXXFLAGS"

AC_LANG_POP([C++])

if test "$libxul_cv_have_usable_wchar_option" = "yes"; then
  LIBXUL_CXXFLAGS="$LIBXUL_CXXFLAGS -fshort-wchar"
fi

# **************
# Check for RTTI
# **************

AC_MSG_CHECKING([whether to enable C++ RTTI])
AC_ARG_ENABLE([cpp-rtti],
  AS_HELP_STRING([--enable-cpp-rtti],[Enable C++ RTTI]),
  [],[enable_cpp_rtti=no])
AC_MSG_RESULT([$enable_cpp_rtti])

if test "$enable_cpp_rtti" = "no"; then
  LIBXUL_CXXFLAGS="-fno-rtti $LIBXUL_CXXFLAGS"
fi

# *************
# Various tests
# *************

# FIXMEchpe: remove this test, it shouldn't be needed anymore thanks to static glue libs

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $LIBXUL_CXXCPPFLAGS $LIBXUL_INCLUDES"

AC_MSG_CHECKING([[whether we have a libxul debug build]])
AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([[#include <xpcom-config.h>
		    #if !defined(MOZ_REFLOW_PERF) || !defined(MOZ_REFLOW_PERF_DSP)
		    #error No
		    #endif]]
  )],
  [libxul_cv_have_debug=yes],
  [libxul_cv_have_debug=no])
AC_MSG_RESULT([$libxul_cv_have_debug])

CPPFLAGS="$_SAVE_CPPFLAGS"

AC_LANG_POP([C++])

if test "$libxul_cv_have_debug" = "yes"; then
	LIBXUL_CXXCPPFLAGS="$LIBXUL_CXXCPPFLAGS -DDEBUG -D_DEBUG"

	AC_DEFINE([HAVE_LIBXUL_DEBUG],[1],[Define if libxul is a debug build])
fi

# *************************************
# Check for C++ symbol visibility stuff
# *************************************

# Check for .hidden assembler directive and visibility attribute.
# Copied from mozilla's configure.in, which in turn was
# borrowed from glibc's configure.in

# Only do this for g++

if test "$GXX" = "yes"; then
  AC_CACHE_CHECK(for visibility(hidden) attribute,
                 libxul_cv_visibility_hidden,
                 [cat > conftest.c <<EOF
                  int foo __attribute__ ((visibility ("hidden"))) = 1;
EOF
                  libxul_cv_visibility_hidden=no
                  if ${CC-cc} -Werror -S conftest.c -o conftest.s >/dev/null 2>&1; then
                    if egrep '\.(hidden|private_extern).*foo' conftest.s >/dev/null; then
                      libxul_cv_visibility_hidden=yes
                    fi
                  fi
                  rm -f conftest.[cs]
                 ])
  if test "$libxul_cv_visibility_hidden" = "yes"; then
    AC_DEFINE([HAVE_VISIBILITY_HIDDEN_ATTRIBUTE],[1],[Define if the compiler supports the "hidden" visibility attribute])

    AC_CACHE_CHECK(for visibility(default) attribute,
                   libxul_cv_visibility_default,
                   [cat > conftest.c <<EOF
                    int foo __attribute__ ((visibility ("default"))) = 1;
EOF
                    libxul_cv_visibility_default=no
                    if ${CC-cc} -fvisibility=hidden -Werror -S conftest.c -o conftest.s >/dev/null 2>&1; then
                      if ! egrep '\.(hidden|private_extern).*foo' conftest.s >/dev/null; then
                        libxul_cv_visibility_default=yes
                      fi
                    fi
                    rm -f conftest.[cs]
                   ])
    if test "$libxul_cv_visibility_default" = "yes"; then
      AC_DEFINE([HAVE_VISIBILITY_ATTRIBUTE],[1],[Define if the compiler supports the "default" visibility attribute])

      AC_CACHE_CHECK(for visibility pragma support,
                     libxul_cv_visibility_pragma,
                     [cat > conftest.c <<EOF
#pragma GCC visibility push(hidden)
                      int foo_hidden = 1;
#pragma GCC visibility push(default)
                      int foo_default = 1;
EOF
                      libxul_cv_visibility_pragma=no
                      if ${CC-cc} -Werror -S conftest.c -o conftest.s >/dev/null 2>&1; then
                        if egrep '\.(hidden|private_extern).*foo_hidden' conftest.s >/dev/null; then
                          if ! egrep '\.(hidden|private_extern).*foo_default' conftest.s > /dev/null; then
                            libxul_cv_visibility_pragma=yes
                          fi
                        fi
                      fi
                      rm -f conftest.[cs]
                    ])
      if test "$libxul_cv_visibility_pragma" = "yes"; then
        AC_CACHE_CHECK(For gcc visibility bug with class-level attributes (GCC bug 26905),
                       libxul_cv_have_visibility_class_bug,
                       [cat > conftest.c <<EOF
#pragma GCC visibility push(hidden)
struct __attribute__ ((visibility ("default"))) TestStruct {
  static void Init();
};
__attribute__ ((visibility ("default"))) void TestFunc() {
  TestStruct::Init();
}
EOF
                       libxul_cv_have_visibility_class_bug=no
                       if ! ${CXX-g++} ${CXXFLAGS} ${DSO_PIC_CFLAGS} ${DSO_LDOPTS} -S -o conftest.S conftest.c > /dev/null 2>&1 ; then
                         libxul_cv_have_visibility_class_bug=yes
                       else
                         if test `egrep -c '@PLT|\\$stub' conftest.S` = 0; then
                           libxul_cv_have_visibility_class_bug=yes
                         fi
                       fi
                       rm -rf conftest.{c,S}
                       ])

        AC_CACHE_CHECK(For x86_64 gcc visibility bug with builtins (GCC bug 20297),
                       libxul_cv_have_visibility_builtin_bug,
                       [cat > conftest.c <<EOF
#pragma GCC visibility push(hidden)
#pragma GCC visibility push(default)
#include <string.h>
#pragma GCC visibility pop

__attribute__ ((visibility ("default"))) void Func() {
  char c[[100]];
  memset(c, 0, sizeof(c));
}
EOF
                       libxul_cv_have_visibility_builtin_bug=no
                       if ! ${CC-cc} ${CFLAGS} ${DSO_PIC_CFLAGS} ${DSO_LDOPTS} -O2 -S -o conftest.S conftest.c > /dev/null 2>&1 ; then
                         libxul_cv_have_visibility_builtin_bug=yes
                       else
                         if test `grep -c "@PLT" conftest.S` = 0; then
                           libxul_cv_visibility_builtin_bug=yes
                         fi
                       fi
                       rm -f conftest.{c,S}
                       ])
        if test "$libxul_cv_have_visibility_builtin_bug" = "no" -a \
                "$libxul_cv_have_visibility_class_bug" = "no"; then
          VISIBILITY_FLAGS='-I$(DIST)/include/system_wrappers -include $(topsrcdir)/config/gcc_hidden.h'
          WRAP_SYSTEM_INCLUDES=1
        else
          VISIBILITY_FLAGS='-fvisibility=hidden'
        fi # have visibility pragma bug
      fi   # have visibility pragma
    fi     # have visibility(default) attribute
  fi       # have visibility(hidden) attribute

  LIBXUL_CXXFLAGS="$LIBXUL_CXXFLAGS $VISIBILITY_FLAGS"
fi         # g++

# *********
# Finish up
# *********

AC_SUBST([LIBXUL_PREFIX])
AC_SUBST([LIBXUL_INCLUDE_ROOT])
AC_SUBST([LIBXUL_INCLUDES])
AC_SUBST([LIBXUL_LIBDIR])
AC_SUBST([LIBXUL_CXXCPPFLAGS])
AC_SUBST([LIBXUL_CXXFLAGS])
AC_SUBST([LIBXUL_LDFLAGS])
AC_SUBST([LIBXUL_LIBS])

])

# LIBXUL_DEFINES
#
# Automake defines for libxul. Not included in LIBXUL_INIT so that
# LIBXUL_INIT may be called conditionally. If you use LIBXUL_INIT,
# you _must_ call LIBXUL_DEFINES, unconditionally.

AC_DEFUN([LIBXUL_DEFINES],
[

AM_CONDITIONAL([HAVE_LIBXUL],[test "$libxul_cv_have_libxul" = "yes"])
AM_CONDITIONAL([HAVE_LIBXUL_DEBUG],[test "$libxul_cv_have_debug" = "yes"])

])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# _LIBXUL_DISPATCH(MACRO, INCLUDEDIRS, ...)

m4_define([_LIBXUL_DISPATCH],
[

AC_LANG_PUSH([C++])

_SAVE_CPPFLAGS="$CPPFLAGS"
_SAVE_CXXFLAGS="$CXXFLAGS"
_SAVE_LDFLAGS="$LDFLAGS"
_SAVE_LIBS="$LIBS"
CPPFLAGS="$CPPFLAGS $LIBXUL_CXXCPPFLAGS $LIBXUL_INCLUDES"
CXXFLAGS="$CXXFLAGS $LIBXUL_CXXFLAGS $($PKG_CONFIG --cflags-only-other libxul)"
LDFLAGS="$LDFLAGS $LIBXUL_LDFLAGS -Wl,--rpath=$LIBXUL_LIBDIR"
LIBS="$LIBS $($PKG_CONFIG --libs libxul)"

# FIXMEchpe: remove this, since the header layout is now flat (only stable and unstable)

_LIBXUL_DISPATCH_INCLUDEDIRS="$2"

_LIBXUL_DISPATCH_INCLUDEDIRS="$_LIBXUL_DISPATCH_INCLUDEDIRS dom necko pref"

# Now add them to CPPFLAGS
for i in $_LIBXUL_DISPATCH_INCLUDEDIRS; do
  CPPFLAGS="$CPPFLAGS -I$LIBXUL_INCLUDE_ROOT/$i"
done

m4_indir([$1],m4_shiftn(2,$@))

CPPFLAGS="$_SAVE_CPPFLAGS"
CXXFLAGS="$_SAVE_CXXFLAGS"
LDFLAGS="$_SAVE_LDFLAGS"
LIBS="$_SAVE_LIBS"

AC_LANG_POP([C++])

])# _LIBXUL_DISPATCH

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# LIBXUL_CHECK_HEADERS(INCLUDEDIRS, HEADERS, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [INCLUDES])

AC_DEFUN([LIBXUL_CHECK_HEADERS],[_LIBXUL_DISPATCH([AC_CHECK_HEADERS],$@)])

# LIBXUL_COMPILE_IFELSE(INCLUDEDIRS, PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])

AC_DEFUN([LIBXUL_COMPILE_IFELSE],[_LIBXUL_DISPATCH([AC_COMPILE_IFELSE],$@)])

# LIBXUL_RUN_IFELSE(INCLUDEDIRS, PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])

AC_DEFUN([LIBXUL_RUN_IFELSE],[_LIBXUL_DISPATCH([AC_RUN_IFELSE],$@)])

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# LIBXUL_XPCOM_PROGRAM([PROLOGUE], [BODY])
#
# Produce a template C++ program which starts XPCOM up and shuts it down after
# the BODY part has run. In BODY, the the following variables are predeclared:
#
# nsresult rv
# int status = 1 (EXIT_FAILURE)
#
# The program's exit status will be |status|; set it to 0 (or EXIT_SUCCESS)
# to indicate success and to a value between 1 (EXIT_FAILURE) and 120 to
# indicate failure.
#
# To jump out of the BODY and exit the test program, you can use |break|.

AC_DEFUN([LIBXUL_XPCOM_PROGRAM],
[AC_LANG_PROGRAM([[
#include <xpcom-config.h>
#include <stdlib.h>
#include <stdio.h>
#include <nsXPCOM.h>
#include <nsCOMPtr.h>
#include <nsILocalFile.h>
#include <nsIServiceManager.h>
#include <nsStringGlue.h>
]]
[$1],
[[
// redirect unwanted mozilla debug output to the bit bucket
freopen ("/dev/null", "w", stdout);

nsresult rv;
nsCOMPtr<nsILocalFile> directory;
rv = NS_NewNativeLocalFile (NS_LITERAL_CSTRING("$_LIBXUL_HOME"), PR_FALSE,
			    getter_AddRefs (directory));
if (NS_FAILED (rv) || !directory) {
	exit (126);
}

rv = NS_InitXPCOM2 (nsnull, directory, nsnull);
if (NS_FAILED (rv)) {
	exit (125);
}

int status = EXIT_FAILURE;

// now put in the BODY, scoped with do...while(0) to ensure we don't hold a
// COMptr after XPCOM shutdown and so we can jump out with a simple |break|.
do {
]]
m4_shiftn(1,$@)
[[
} while (0);
	
NS_ShutdownXPCOM (nsnull);
exit (status);
]])
]) # LIBXUL_XPCOM_PROGRAM

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# LIBXUL_XPCOM_PROGRAM_CHECK([ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND], [ACTION-IF-CROSS-COMPILING])
#
# Checks whether we can build and run any XPCOM test programs at all

AC_DEFUN([LIBXUL_XPCOM_PROGRAM_CHECK],
[AC_REQUIRE([LIBXUL_INIT])dnl

AC_CACHE_CHECK([whether we can compile and run XPCOM programs],
[libxul_cv_xpcom_program_check],
[
libxul_cv_xpcom_program_check=no

LIBXUL_RUN_IFELSE([],
	[LIBXUL_XPCOM_PROGRAM([],[[status = EXIT_SUCCESS;]])],
	[libxul_cv_xpcom_program_check=yes],
	[libxul_cv_xpcom_program_check=no],
	[libxul_cv_xpcom_program_check=maybe])
])

if test "$libxul_cv_xpcom_program_check" = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_FAILURE([Cannot compile and run XPCOM programs])],
	[$3])
fi

]) # LIBXUL_XPCOM_PROGRAM_CHECK

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# LIBXUL_CHECK_CONTRACTID(CONTRACTID, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Checks wheter CONTRACTID is a registered contract ID

AC_DEFUN([LIBXUL_CHECK_CONTRACTID],
[AC_REQUIRE([LIBXUL_INIT])dnl

AS_VAR_PUSHDEF([libxul_cv_have_CID],[libxul_cv_have_$1])

AC_CACHE_CHECK([for the $1 XPCOM component],
libxul_cv_have_CID,
[
AS_VAR_SET(libxul_cv_have_CID,[no])

LIBXUL_RUN_IFELSE([],
[LIBXUL_XPCOM_PROGRAM([[
#include <nsIComponentRegistrar.h>
]],[[
status = 99;
nsCOMPtr<nsIComponentRegistrar> registrar;
rv = NS_GetComponentRegistrar (getter_AddRefs (registrar));
if (NS_FAILED (rv)) break;

status = 98;
PRBool isRegistered = PR_FALSE;
rv = registrar->IsContractIDRegistered ("$1", &isRegistered);
if (NS_FAILED (rv)) break;

status = isRegistered ? EXIT_SUCCESS : 97;
]])
],
[AS_VAR_SET(libxul_cv_have_CID,[yes])],
[AS_VAR_SET(libxul_cv_have_CID,[no])],
[AS_VAR_SET(libxul_cv_have_CID,[maybe])])

])

if test AS_VAR_GET(libxul_cv_have_CID) = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_ERROR([dnl
Contract ID "$1" is not registered, but $PACKAGE_NAME depends on it.])],
	[$3])
fi

AS_VAR_POPDEF([libxul_cv_have_CID])

]) # LIBXUL_CHECK_CONTRACTID

# LIBXUL_CHECK_CONTRACTIDS(CONTRACTID, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
#
# Checks wheter CONTRACTIDs are registered contract IDs.
# If ACTION-IF-NOT-FOUND is given, it is executed when one of the contract IDs
# is not found and the missing contract ID is in the |as_contractid| variable.

AC_DEFUN([LIBXUL_CHECK_CONTRACTIDS],
[AC_REQUIRE([LIBXUL_INIT])dnl

result=yes
as_contractid=
for as_contractid in $1
do
	LIBXUL_CHECK_CONTRACTID([$as_contractid],[],[result=no; break;])
done

if test "$result" = "yes"; then
	ifelse([$2],,[:],[$2])
else
	ifelse([$3],,[AC_MSG_ERROR([dnl
Contract ID "$as_contractid" is not registered, but $PACKAGE_NAME depends on it.])],
	[$3])
fi

]) # LIBXUL_CHECK_CONTRACTIDS

# ***************************************************************************
# ***************************************************************************
# ***************************************************************************

# LIBXUL_XPIDL([ACTION-IF-FOUND],[ACTION-IF-NOT-FOUND])
#
# Checks for xpidl program and include directory
#
# Variables set:
# XPIDL:        the xpidl program
# XPIDL_IDLDIR: the xpidl include directory

AC_DEFUN([LIBXUL_XPIDL],
[AC_REQUIRE([LIBXUL_INIT])dnl

_C_PATH_PROG([XPIDL],[xpidl],[no],[$LIBXUL_LIBDIR:$PATH])

XPIDL_IDLDIR="$($PKG_CONFIG --variable=idldir libxul)"

if test "$XPIDL" != "no" -a -n "$XPIDL_IDLDIR" -a -f "$XPIDL_IDLDIR/nsISupports.idl"; then
	ifelse([$1],,[:],[$1])
else
	ifelse([$2],,[AC_MSG_FAILURE([XPIDL program or include directory not found])],[$2])
fi

])
