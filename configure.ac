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

# This adds the option of compiling without using CEC
AC_ARG_ENABLE(cec,
  [  --disable-cec           disable CEC support],
  [case "${enableval}" in
     yes | no ) WITH_CEC="${enableval}" ;;
     *) AC_MSG_ERROR(Bad value ${enableval} for --disable-cec) ;;
   esac],
  [WITH_CEC="yes"]
)
AM_CONDITIONAL([WITH_CEC], [test "x$WITH_CEC" = "xyes"])

# Define ENABLE_CEC in config.h if we're going to compile against it
if test "x$WITH_CEC" = "xyes"; then
    AC_DEFINE([ENABLE_CEC], [], [Build with CEC support])
    AC_MSG_NOTICE([CEC support enabled])
    PKG_CHECK_MODULES([LIBCEC],  [libcec >= 1.6.0])
    AC_SUBST([LIBCEC_CFLAGS])
    AC_SUBST([LIBCEC_LIBS])
else
    AC_MSG_NOTICE([CEC support disabled])
fi

# Check libcec dependencies
PKG_CHECK_MODULES([LIBAOSD], [libaosd])
PKG_CHECK_MODULES([LIBRSVG], [librsvg-2.0])
AC_SUBST([LIBAOSD_CFLAGS])
AC_SUBST([LIBAOSD_LIBS])
AC_SUBST([LIBRSVG_CFLAGS])
AC_SUBST([LIBRSVG_LIBS])

# Output generation
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile tvcontrold/Makefile])
AC_OUTPUT
