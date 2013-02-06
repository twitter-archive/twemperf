# Define the package version numbers and the bug reporting address
m4_define([MCP_MAJOR], 0)
m4_define([MCP_MINOR], 1)
m4_define([MCP_PATCH], 1)
m4_define([MCP_BUGS], [manj@twitter.com])

# Initialize autoconf
AC_PREREQ([2.64])
AC_INIT([mcperf], [MCP_MAJOR.MCP_MINOR.MCP_PATCH], [MCP_BUGS])
AC_CONFIG_SRCDIR([src/mcp.c])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_HEADERS([config.h:config.h.in])
AC_CONFIG_MACRO_DIR([m4])

# Initialize automake
AM_INIT_AUTOMAKE([1.9 foreign])

# Define macro variables for the package version numbers
AC_DEFINE(MCP_VERSION_MAJOR, MCP_MAJOR, [Define the major version number])
AC_DEFINE(MCP_VERSION_MINOR, MCP_MINOR, [Define the minor version number])
AC_DEFINE(MCP_VERSION_PATCH, MCP_PATCH, [Define the patch version number])
AC_DEFINE(MCP_VERSION_STRING, "MCP_MAJOR.MCP_MINOR.MCP_PATCH", [Define the version string])

# Checks for language
AC_LANG([C])

# Checks for programs
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for typedefs, structures, and compiler characteristics
AC_C_INLINE
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INTMAX_T
AC_TYPE_INTPTR_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINTMAX_T
AC_TYPE_UINTPTR_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for header files
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([fcntl.h float.h stddef.h stdlib.h string.h unistd.h])
AC_CHECK_HEADERS([inttypes.h stdint.h])
AC_CHECK_HEADERS([sys/ioctl.h sys/time.h sys/uio.h])
AC_CHECK_HEADERS([sys/socket.h sys/un.h netinet/in.h arpa/inet.h netdb.h])
AC_CHECK_HEADERS([sys/epoll.h], [], [AC_MSG_ERROR([required sys/epoll.h header file is missing])])

# Checks for library functions
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([gettimeofday memchr memmove memset socket strchr strerror strndup strtoul])

AC_CACHE_CHECK([if epoll works], [ac_cv_epoll_works],
  AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
int
main(int argc, char **argv)
{
    int fd;

    fd = epoll_create(256);
    if (fd < 0) {
        perror("epoll_create:");
        exit(1);
    }
    exit(0);
}
  ], [ac_cv_epoll_works=yes], [ac_cv_epoll_works=no]))
AS_IF([test "x$ac_cv_epoll_works" = "xyes"], [], [AC_MSG_FAILURE([Linux epoll(7) API is missing])])

# Checks for libraries
AC_CHECK_LIB([m], [pow])

# Package options
AC_MSG_CHECKING([whether to enable debug logs and asserts])
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING(
    [--enable-debug],
    [enable debug logs and asserts @<:@default=no@:>@])
  ],
  [],
  [enable_debug=no])
AS_CASE([x$enable_debug],
  [xyes], [AC_DEFINE([HAVE_ASSERT_PANIC], [1],
                     [Define to 1 if panic on assert is enabled])
           AC_DEFINE([HAVE_DEBUG_LOG], [1], [Define to 1 if debug log is enabled])
          ],
  [xno], [],
  [AC_MSG_FAILURE([invalid value ${enable_debug} for --enable-debug])])
AC_MSG_RESULT($enable_debug)

# Define Makefiles
AC_CONFIG_FILES([Makefile
                 src/Makefile
                 src/gen/Makefile
                 src/stats/Makefile])

# Generate the "configure" script
AC_OUTPUT
