# Initialization
AC_INIT([tvcontrol],[0.1.0],[asierllano@gmail.com],[tvcontrol])
AC_CONFIG_SRCDIR([tvcontrold/tvcontrold.cpp])
AC_PREREQ([2.69])
AM_INIT_AUTOMAKE
AC_CONFIG_MACRO_DIR(m4)
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

# Check libcec dependencies
PKG_CHECK_MODULES([LIBCEC],  [libcec >= 1.6.0])
PKG_CHECK_MODULES([LIBAOSD], [libaosd])
PKG_CHECK_MODULES([LIBRSVG], [librsvg-2.0])
AC_SUBST([LIBCEC_CFLAGS])
AC_SUBST([LIBCEC_LIBS])
AC_SUBST([LIBAOSD_CFLAGS])
AC_SUBST([LIBAOSD_LIBS])
AC_SUBST([LIBRSVG_CFLAGS])
AC_SUBST([LIBRSVG_LIBS])

# Output generation
AC_CONFIG_FILES([Makefile tvcontrold/Makefile])
AC_OUTPUT
