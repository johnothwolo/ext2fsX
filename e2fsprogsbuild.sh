#!/bin/sh
# $Id: e2fsprogsbuild.sh,v 1.14 2006/07/01 20:16:57 bbergstrand Exp $
#
# Builds e2fsprogs
#
# Created for the ext2fsx project: http://sourceforge.net/projects/ext2fsx/
#
# Copyright 2003,2004,2006 Brian Bergstrand.
#
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
#
# 1.    Redistributions of source code must retain the above copyright notice, this list of
#     conditions and the following disclaimer.
# 2.    Redistributions in binary form must reproduce the above copyright notice, this list of
#     conditions and the following disclaimer in the documentation and/or other materials provided
#     with the distribution.
# 3.    The name of the author may not be used to endorse or promote products derived from this
#     software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
# OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

PATH="/usr/bin:/bin:/usr/sbin:/sbin"

#src root
if [ -z "${EXT2BUILD}" ]; then
	echo "EXT2BUILD is not defined, using current directory"
	EXT2BUILD="."
fi

if [ ! -d "${EXT2BUILD}/src/e2fsprogs" ]; then
	exit 1
fi

HOSTARCH=`uname -p`
if [ ${HOSTARCH} == "powerpc" ]; then
HOSTARCH="ppc"
fi

if [ ${HOSTARCH} != "ppc" ] && [ ${HOSTARCH} != "i386" ]; then
echo "Unknown host arch: ${HOSTARCH}"
exit 1
fi

cd "${EXT2BUILD}/src/e2fsprogs"

E2VER=`sed -n -e "s/^.*E2FSPROGS_VERSION[^0-9]*\([0-9]*\.[0-9]*\).*$/\1/p" ./version.h`

if [ -f ./.e2configdone ]; then
	#make sure the version is the same
	CONFVER=`cat ./.e2configdone`
	if [ "${E2VER}" != "${CONFVER}" ]; then
		rm ./.e2configdone
		make clean
	fi
fi

if [ ! -d build-ppc ]; then
mkdir build-ppc
fi

if [ ! -d build-i386 ]; then
mkdir build-i386
fi

#set no prebind
#since the exec's are linked against the shared libs using a relative path,
#pre-binding will never work anyway. Setting tells the loader to not
#even try pre-binding.
export LD_FORCE_NO_PREBIND=1

# As of 1.38, building with GCC4 produces a bunch of pointer sign warnings, so disable them
ECFLAGS="-DAPPLE_DARWIN=1 -DHAVE_EXT2_IOCTLS=1 -DSYS_fsctl=242 -pipe -Wno-pointer-sign"
buildarch()
{
	cd build-${1}
	
	if [ ! -f ../.e2configdone ]; then
		hostarg=""
		if [ ${HOSTARCH} != ${1} ]; then
			hostarg="--host=${1}-apple-darwin`uname -r`"
		fi
		# --with-included-gettext   -- this would be nice, but the po/ build fails
		export CFLAGS="-arch ${1} -isysroot /Developer/SDKs/MacOSX10.4u.sdk"
		../configure --prefix=/usr/local --mandir=/usr/local/share/man --disable-nls \
	--without-libintl-prefix --with-libiconv-prefix=/usr --disable-fsck --enable-bsd-shlibs \
	--with-ccopts="${ECFLAGS} ${CFLAGS}" ${hostarg}
		if [ $? -ne 0 ]; then
			echo "${1} configure failed!"
			exit $?
		fi
	fi
	
	makeargs=""
	if [ ${HOSTARCH} != ${1} ]; then
		# subst will get built with the target arch, however it needs to run on the host
		mv ./util/Makefile ./util/Makefile.ignore
		ln ../build-${HOSTARCH}/util/subst ./util/subst
		# This is fucked up, if we specify --with-ldflags to configure, config fails because
		# ld fails because CFLAGS and LDFLAGS contain the same args. However, during the linking
		# of some execs (mostly static) LDFLAGS is used w/o CFLAGS. So we run config w/o --with-ldargs,
		# and override LDARGS in the environment.
		export ALL_LDFLAGS="-arch ${1} -isysroot /Developer/SDKs/MacOSX10.4u.sdk"
		makeargs="-e"
	fi
	
	unset CFLAGS
	if [ -z ${makeargs} ]; then
		make
	else
		make "${makeargs}"
	fi
	
	unset ALL_LDFLAGS
	if [ ${HOSTARCH} != ${1} ]; then
		mv ./util/Makefile.ignore ./util/Makefile
	fi
	
	cd ..
}

buildarch ${HOSTARCH}

if [ ${HOSTARCH} == "ppc" ]; then
	buildarch i386
else
	buildarch ppc
fi

echo "${E2VER}" > ./.e2configdone

# create universal binaries
cp -pR build-ppc build-uni

cd build-uni
for i in `find . -type f \! -name "*.o" -print0 | xargs -0 file | grep Mach-O | awk -F: '{print $1}'`
do
	output=`dirname "${i}"`/`basename "${i}"`.uni
	lipo -create "${i}" ../build-i386/"${i}" -output "${output}"
	if [ -f "${output}" ]; then
		rm "${i}"
		mv "${output}" "${i}"
	fi
done

exit 0
