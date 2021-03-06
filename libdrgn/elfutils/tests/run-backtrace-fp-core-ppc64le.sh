#! /bin/bash
# Copyright (C) 2017 Red Hat, Inc.
# This file is part of elfutils.
#
# This file is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# elfutils is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. $srcdir/backtrace-subr.sh

# The binary is generated by compiling backtrace-child without unwind
# information, but with -fno-omit-frame-pointer.
#
# gcc -static -O2 -fno-omit-frame-pointer -fno-asynchronous-unwind-tables \
#     -D_GNU_SOURCE -pthread -o tests/backtrace.ppc64le.fp.exec -I. -Ilib \
#     tests/backtrace-child.c
#
# The core is generated by calling tests/backtrace.ppc64le.fp.exec --gencore

check_core ppc64le.fp
