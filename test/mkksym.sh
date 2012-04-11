#!/bin/sh
#
# @(#) $Id: mkksym.sh,v 1.1 2004/03/04 20:52:38 bbergstrand Exp $
#
# Creates a kext symbol file for debugging
#
# Created for the ext2fsx project: http://sourceforge.net/projects/ext2fsx/
#
# Copyright 2004 Brian Bergstrand.
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

PATH=/usr/bin:/bin:/sbin:/usr/sbin

KSYM_CREATE=kextload
KSYM_KERNEL=/mach_kernel
KSYM_BUNDLEID=
KSYM_LOADADDR=
KSYM_MODULE=

usage() {
	echo "Usage:"
	echo "$0 [-k kernel path] -b bundle id -a load address -m kext name"
	exit 22
}

#Process args
if [ $# -eq 0 ]; then
	usage
fi

while : ; do
	
	case "${1}" in
		-a )
			KSYM_LOADADDR=${2}
			shift
			shift # once more for the address
		;;
		-b )
			KSYM_BUNDLEID=${2}
			shift
			shift # once more for the path
		;;
		-k )
			KSYM_KERNEL=${2}
			shift
			shift # once more for the path
		;;
		-m )
			KSYM_MODULE=${2}
			shift
			shift # once more for the path
		;;
		* )
			break
		;;
	esac
done


if [ -z ${KSYM_BUNDLEID} ] || [ -z ${KSYM_LOADADDR} ] || [ -z ${KSYM_MODULE} ]; then
	usage
fi

if [ ! -f "${KSYM_KERNEL}" ]; then
	echo "kernel file '${KSYM_KERNEL}' does not exist"
	exit 2
fi

if [ ! -d "./${KSYM_MODULE}" ]; then
	echo "kext bundle '${KSYM_MODULE}' does not exist in the current directory"
	exit 2
fi

${KSYM_CREATE} -c -k "${KSYM_KERNEL}" -n -r . -s . -z -a ${KSYM_BUNDLEID}@${KSYM_LOADADDR} \
"${KSYM_MODULE}"