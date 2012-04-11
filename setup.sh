#! /bin/sh
#
# This shell script is released as part of the ext2fsx project under the GNU
# GPL.  For more information on the GPL please visit
# http://www.fsf.org/licenses/gpl.html for more information.
#
# For more information about the ext2fsx project please visit:
# http://sourceforge.net/projects/ext2fsx/
#
# Script written by Brian Bergstrand
# @(#) $Id: setup.sh,v 1.9 2006/02/04 21:17:10 bbergstrand Exp $

#perms

echo -n "Setting permissions... "
chmod u+x ./MakeInstall.sh ./Resources/post* ./Resources/pre* ./e2fsprogsbuild.sh \
./test/mkksym.sh
echo "done"

#links

cd ./src

if [ ! -e linux ]; then
    echo -n "Creating linux softlink... "
	ln -s ./gnu/ext2fs ./linux
    echo "done"
fi

cd ./gnu/ext2fs

if [ ! -e ext3_fs.h ]; then
    echo -n "Creating ext3_fs.h softlink... "
    ln ext2_fs.h ext3_fs.h
    echo "done"
fi

echo "End of setup script"
exit 0

# journal source

ln ./journal/jbd.h jbd.h
