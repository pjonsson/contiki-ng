#!/bin/bash -ex

# Copyright (c) 2022, Research Institutes of Sweden
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# https://web.archive.org/web/20130816160212/http://sourceforge.net/apps/mediawiki/mspgcc/index.php?title=Gcc47:20-Bit_Design#ta_a20

LIBCVERSION=20120716
LIBCFILE=msp430-libc-$LIBCVERSION.tar.bz2

error() {
    echo "$*"
    exit 1
}

[ -z "$1" ] && error "No installation directory specified"

prefix=$1

startdir="$PWD"
tmpdir=$PWD
#tmpdir=$(mktemp -d -p "$startdir")

cd "$tmpdir"

wget -nv -nc http://sourceforge.net/projects/mspgcc/files/msp430-libc/$LIBCFILE || error "Could not download $LIBCFILE"

tar xjf "$LIBCFILE"

cd msp430-libc-$LIBCVERSION/src
perl -pi -e 's#msp430-#msp430-elf-#g' Makefile
perl -pi -e 's#/dev/null#/dev/null | grep 430#g' Makefile
make
#make PREFIX="$prefix" DESTDIR="$tmpdir" install
sudo make PREFIX="$prefix" install
echo "Results and temporary files are in '$tmpdir'"
