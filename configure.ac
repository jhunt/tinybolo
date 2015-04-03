#
#  Copyright 2015 James Hunt <james@jameshunt.us>
#
#  This file is part of tinybolo.
#
#  tinybolo is free software: you can redistribute it and/or modify it under the
#  terms of the GNU General Public License as published by the Free Software
#  Foundation, either version 3 of the License, or (at your option) any later
#  version.
#
#  tinybolo is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
#  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
#  details.
#
#  You should have received a copy of the GNU General Public License along
#  with tinybolo.  If not, see <http://www.gnu.org/licenses/>.
#

AC_PREREQ(2.68)
AC_INIT([tinybolo], [1.0.1], [bugs@niftylogic.com])

AC_CONFIG_SRCDIR([src/tinybolo.c])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build])
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_PROG_CC

AC_HAVE_LIBRARY(pthread,,  AC_MSG_ERROR(Missing pthread library))
AC_HAVE_LIBRARY(zmq,,      AC_MSG_ERROR(Missing 0MQ library))

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
