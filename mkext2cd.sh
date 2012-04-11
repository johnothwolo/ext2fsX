#!/bin/sh

#must be run as root

TEMPFILE=/tmp/burnfs
MNT=/mnt/burn

if [ ! -d $MNT ]; then
	mkdir -p $MNT
fi

dd if=/dev/zero of=$TEMPFILE bs=1024k count=220

/sbin/mkfs -t ext2 -L "EXT2 TEST" -F -b 2048 $TEMPFILE
/sbin/tune2fs $TEMPFILE -c 0 -i 0
mount -w -t ext2 -o loop=/dev/loop0 $TEMPFILE $MNT

# bunch of little files
cp -dR /usr/src/linux-2.4.18-14/kernel $MNT/
cp -dR /usr/src/linux-2.4.18-14/fs $MNT/
# couple of large(r) files
dd if=/dev/urandom of=$MNT/8MB.binary bs=1024k count=8
openssl md5 $MNT/8MB.binary > $MNT/8MB.binary.md5

dd if=/dev/urandom of=$MNT/20MB.binary bs=1024k count=20
openssl md5 $MNT/20MB.binary > $MNT/20MB.binary.md5

if [ -f /usr/X11R6/bin/XFree86 ]; then
 cp /usr/X11R6/bin/XFree86 $MNT/
 openssl md5 $MNT/XFree86 > $MNT/XFree86.md5
fi

sleep 15
umount $MNT
if [ $? -ne 0 ]; then
 umount -f $MNT
fi

# system dependent settings
DEVICEID="CRX195E1"
# BASH trick: x is an array holding the words from the command results
x=(`cdrecord -scanbus | grep $DEVICEID`)
DEV=${x[0]}
if [ -z "$DEV" ]
then
    echo CD writer device not found
    exit 1
fi

# burn CD
x=(`ls -l $TEMPFILE`)
SIZE=${x[4]}
cdrecord -vv speed=12 dev=$DEV -tsize=$SIZE -data -multi -eject $TEMPFILE
