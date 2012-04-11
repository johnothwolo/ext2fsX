#!/bin/sh

PATH="/usr/bin:/bin:/usr/sbin:/sbin"

tst=`kextstat | grep "net\.sourceforge\.ext2fs\.fs\.ext2"`
if [ "$tst" != "" ]; then
sudo kextunload /Library/Extensions/ext2fs.kext
# Why the hell does kextunload not return an error if the unload failed?!
#if [ $? -ne 0 ]; then
sleep 1
tst=`kextstat | grep "net\.sourceforge\.ext2fs\.fs\.ext2"`
if [ "$tst" != "" ]; then
echo "Failed to unload kernel extension. Please unmount all Ext2 volumes."
exit 1
fi
fi

sudo rm -rf /Library/Extensions/ext2fs.kext /Library/Filesystems/ext2.fs \
/Library/PreferencePanes/ExtFSManager.prefPane /System/Library/Filesystems/ext2.fs
sudo rm /sbin/mount_ext2 /usr/share/man/man8/mount_ext2.8

#e2fsprogs, e2undel
sudo rm /sbin/fsck_ext2 /sbin/newfs_ext2
cd /usr/local/bin
sudo rm ./chattr ./lsattr ./uuidgen
cd ../info
sudo rm ./libext2fs.info.gz
cd ../lib
sudo rm ./libblkid* ./libcom_err* ./libe2p* ./libext2fs* ./libss* ./libuuid*
cd ../sbin
sudo rm ./badblocks ./debugfs ./dumpe2fs ./e2fsck ./e2image ./e2label ./findfs ./fsck.ext2 \
./blkid ./fsck.ext3 ./mke2fs ./mkfs.ext* ./mklost+found ./resize2fs ./tune2fs ./logsave \
./e2undel
cd ../share/doc
sudo rm E2FSPROGS_COPYRIGHT E2UNDEL_README

MAN1="./chattr.1 ./lsattr.1 ./uuidgen.1"
MAN8="./badblocks.8 ./debugfs.8 ./dumpe2fs.8 ./e2fsck.8 ./e2image.8 ./e2label.8 ./findfs.8 \
./fsck_ext2.8 ./newfs_ext2.8 ./blkid.8 ./fsck.ext2.8 ./fsck.ext3.8 ./mke2fs.8 ./mkfs.ext* \
./mklost+found.8 ./resize2fs.8 ./tune2fs.8 ./logsave.8"
cd /usr/local/share/man/man1
sudo rm ${MAN1}
cd ../man8
sudo rm ${MAN8}
cd /usr/local/man/man1
sudo rm ${MAN1}
cd ../man8
sudo rm ${MAN8}

#Rebuild the kext cache
if [ -f /Library/Extensions.kextcache ]; then
	sudo rm /Library/Extensions.kextcache
	sudo kextcache -k /Library/Extensions
fi

sudo rm -rf /Library/Receipts/Ext2FS.pkg

echo "Uninstall successful. A reboot is not required, but is recommended."
