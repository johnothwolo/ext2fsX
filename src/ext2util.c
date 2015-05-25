/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
* Copyright 2003,2006 Brian Bergstrand.
*
* Redistribution and use in source and binary forms, with or without modification, 
* are permitted provided that the following conditions are met:
*
* 1.	Redistributions of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer.
* 2.	Redistributions in binary form must reproduce the above copyright notice, this list of
*     conditions and the following disclaimer in the documentation and/or other materials provided
*     with the distribution.
* 3.	The name of the author may not be used to endorse or promote products derived from this
*     software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
* AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Revision: 1.12 $ Built: " __DATE__ " " __TIME__;

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/loadable_fs.h>

//#include <dev/disk.h>

#include <machine/byte_order.h>

#include <err.h>
#include <sysexits.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFStringEncodingExt.h>
#include <CoreFoundation/CFUUID.h>

#include "ext2_apple.h"
#include "ext2_fs.h"
#include <fs.h>
#include "ext2_byteorder.h"

#ifndef FSUC_ADOPT
#define FSUC_ADOPT 'a'
#endif

#ifndef FSUC_DISOWN
#define FSUC_DISOWN 'd'
#endif

#ifndef FSUC_GETUUID
#define FSUC_GETUUID 'k'
#endif

#ifndef FSUC_SETUUID
#define FSUC_SETUUID 's'
#endif

#ifndef FSUC_MKJNL
#define FSUC_MKJNL   'J'
#endif

#ifndef FSUC_UNJNL
#define FSUC_UNJNL   'U'
#endif

#ifndef FSUC_LABEL
#define FSUC_LABEL		'n'
#endif

#define FS_TYPE			EXT2FS_NAME
#define FS_NAME_FILE		EXT2FS_NAME
#define FS_BUNDLE_NAME		"ext2fs.kext"
#define FS_KEXT_DIR			"/Library/Extensions/ext2fs.kext"
#define FS_KMOD_DIR			"/Library/Extensions/ext2fs.kext/Contents/MacOS/ext2fs"
#define RAWDEV_PREFIX		"/dev/r"
#define BLOCKDEV_PREFIX		"/dev/"
#define MOUNT_COMMAND		"/sbin/mount"
#define UMOUNT_COMMAND		"/sbin/umount"
#define KEXTLOAD_COMMAND	"/sbin/kextload"
//#define KMODLOAD_COMMAND	"/sbin/kmodload"
#define READWRITE_OPT		"-w"
#define READONLY_OPT		"-r"
#define SUID_OPT			"suid"
#define NOSUID_OPT			"nosuid"
#define DEV_OPT				"dev"
#define NODEV_OPT			"nodev"

#define FS_LABEL_COMMAND "/usr/local/sbin/e2label"

#define UNKNOWN_LABEL		"UNTITLED"

#define	DEVICE_SUID			"suid"
#define	DEVICE_NOSUID		"nosuid"

#define	DEVICE_DEV			"dev"
#define	DEVICE_NODEV		"nodev"

#define VOL_UUID_STR_LEN	36

#ifdef DEBUG
#undef DEBUG
#endif

/* globals */
const char	*progname;	/* our program name, from argv[0] */
int		debug;	/* use -D to enable debug printfs */

/*
 * The following code is re-usable for all FS_util programs
 */
void usage(void);

static int fs_probe(char *devpath, int removable, int writable);
static int fs_mount(char *devpath, char *mount_point, int removable, 
	int writable, int suid, int dev);
static int fs_unmount(char *devpath);
static int fs_label(char *devpath, char *volName);
static void fs_set_label_file(char *labelPtr);

#ifdef DEBUG
static void report_exit_code(int ret);
#endif

extern int checkLoadable();
#if 0
static void mklabel(u_int8_t *dest, const char *src);
#endif

int ret = 0;
char	diskLabel[EXT2_VOL_LABEL_LENGTH + 1];

#define EXT_SUPER_UUID
#include "util/spawn.c"
#include "util/super.c"

void usage()
{
        fprintf(stderr, "usage: %s action_arg device_arg [mount_point_arg] [Flags]\n", progname);
        fprintf(stderr, "action_arg:\n");
        fprintf(stderr, "\t-%c (Probe)\n", FSUC_PROBE);
        fprintf(stderr, "\t-%c (Mount)\n", FSUC_MOUNT);
        fprintf(stderr, "\t-%c (Unmount)\n", FSUC_UNMOUNT);
        fprintf(stderr, "\t-%c (Set Name)\n", FSUC_LABEL);
        fprintf(stderr, "\t-%c (Get UUID)\n", FSUC_GETUUID);
    fprintf(stderr, "device_arg:\n");
    fprintf(stderr, "\tdevice we are acting upon (for example, 'disk0s2')\n");
    fprintf(stderr, "mount_point_arg:\n");
    fprintf(stderr, "\trequired for Mount and Force Mount \n");
    fprintf(stderr, "Flags:\n");
    fprintf(stderr, "\trequired for Mount, Force Mount and Probe\n");
    fprintf(stderr, "\tindicates removable or fixed (for example 'fixed')\n");
    fprintf(stderr, "\tindicates readonly or writable (for example 'readonly')\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "\t%s -p disk0s2 fixed writable\n", progname);
        exit(FSUR_INVAL);
}

int main(int argc, char **argv)
{
    char		rawdevpath[MAXPATHLEN];
    char		blockdevpath[MAXPATHLEN];
    char		opt;
    struct stat	sb;
    int			ret = FSUR_INVAL;

    /* save & strip off program name */
    progname = argv[0];
    argc--;
    argv++;

    /* secret debug flag - must be 1st flag */
    debug = (argc > 0 && !strcmp(argv[0], "-D"));
    if (debug) { /* strip off debug flag argument */
        argc--;
        argv++;
    }

    if (argc < 2 || argv[0][0] != '-')
        usage();
    opt = argv[0][1];
    if (opt != FSUC_PROBE && opt != FSUC_MOUNT && opt != FSUC_UNMOUNT && opt != FSUC_LABEL
         && FSUC_GETUUID != opt) {
        usage(); /* Not supported action */
    }
    if ((opt == FSUC_MOUNT || opt == FSUC_UNMOUNT || opt == FSUC_LABEL) && argc < 3)
        usage(); /* mountpoint arg missing! */

    snprintf(rawdevpath, MAXPATHLEN, "%s%s", RAWDEV_PREFIX, argv[1]);
    if (stat(rawdevpath, &sb) != 0) {
        fprintf(stderr, "%s: stat %s failed, %s\n", progname, rawdevpath,
                strerror(errno));
        exit(FSUR_INVAL);
    }

    snprintf(blockdevpath, MAXPATHLEN, "%s%s", BLOCKDEV_PREFIX, argv[1]);
    if (stat(blockdevpath, &sb) != 0) {
        fprintf(stderr, "%s: stat %s failed, %s\n", progname, blockdevpath,
                strerror(errno));
        exit(FSUR_INVAL);
    }

    switch (opt) {
        case FSUC_PROBE: {
            if (argc != 4)
                usage();
            ret = fs_probe(rawdevpath,
                        strcmp(argv[2], DEVICE_FIXED),
                        strcmp(argv[3], DEVICE_READONLY));
            break;
        }

        case FSUC_MOUNT:
        case FSUC_MOUNT_FORCE:
            if (argc != 7)
                usage();
            if (strcmp(argv[3], DEVICE_FIXED) && strcmp(argv[3], DEVICE_REMOVABLE)) {
                printf("ext2fs.util: ERROR: unrecognized flag (removable/fixed) argv[%d]='%s'\n",3,argv[3]);
                usage();
            }
                if (strcmp(argv[4], DEVICE_READONLY) && strcmp(argv[4], DEVICE_WRITABLE)) {
                    printf("ext2fs.util: ERROR: unrecognized flag (readonly/writable) argv[%d]='%s'\n",4,argv[4]);
                    usage();
                }
                if (strcmp(argv[5], DEVICE_SUID) && strcmp(argv[5], DEVICE_NOSUID)) {
                    printf("ext2fs.util: ERROR: unrecognized flag (suid/nosuid) argv[%d]='%s'\n",5,argv[5]);
                    usage();
                }
                if (strcmp(argv[6], DEVICE_DEV) && strcmp(argv[6], DEVICE_NODEV)) {
                    printf("ext2fs.util: ERROR: unrecognized flag (dev/nodev) argv[%d]='%s'\n",6,argv[6]);
                    usage();
                }
                ret = fs_mount(blockdevpath,
                            argv[2],
                            strcmp(argv[3], DEVICE_FIXED),
                            strcmp(argv[4], DEVICE_READONLY),
                            strcmp(argv[5], DEVICE_NOSUID),
                            strcmp(argv[6], DEVICE_NODEV));
            break;
        case FSUC_UNMOUNT:
            ret = fs_unmount(rawdevpath);
            break;
        case FSUC_LABEL:
            if (argc != 3)
               usage();
               
            ret = fs_label(blockdevpath, argv[2]);
            break;
         case FSUC_GETUUID:
            {
               CFStringRef uuid;
               
               if (argc != 2)
                  usage();
               
               ret = FSUR_INVAL;
               uuid = extsuper_uuid(blockdevpath);
               if (uuid) {
                  char pruuid[40];
                  ret = FSUR_IO_SUCCESS;
                  /* Convert to UTF8 */
                  (void) CFStringGetCString(uuid, pruuid, sizeof(pruuid),
                     kCFStringEncodingUTF8);
                  CFRelease(uuid);
                  /* Write to stdout */
                  write(1, pruuid, strlen(pruuid));
               }
            }
            break;
         case FSUC_SETUUID:
            /* This is done at volume creation time */
         case FSUC_ADOPT:
         case FSUC_DISOWN:
         case FSUC_MKJNL:
         case FSUC_UNJNL:
            ret = FSUR_INVAL;
            break;
        default:
            usage();
    }

    #ifdef DEBUG
    report_exit_code(ret);
    #endif
    exit(ret);

    return(ret);
}

static int fs_mount(char *devpath, char *mount_point, int removable, int writable, int suid, int dev) {
    char *kextargs[] = {KEXTLOAD_COMMAND, FS_KEXT_DIR, NULL};
    char *mountargs[] = {MOUNT_COMMAND, READWRITE_OPT, "-o", SUID_OPT, "-o",
        DEV_OPT, "-t", FS_TYPE, devpath, mount_point, NULL};

    if (! writable)
        mountargs[1] = READONLY_OPT;

    if (! suid)
        mountargs[3] = NOSUID_OPT;

    if (! dev)
        mountargs[5] = NODEV_OPT;

    if (checkLoadable())
        safe_execv(kextargs); /* better here than in mount_udf */
    safe_execv(mountargs);
    ret = FSUR_IO_SUCCESS;

    return ret;
}

static int fs_unmount(char *devpath) {
        char *umountargs[] = {UMOUNT_COMMAND, devpath, NULL};

        safe_execv(umountargs);
        return(FSUR_IO_SUCCESS);
}

/*
 * Check a volume label.
 */
static int
oklabel(const char *src)
{
    int c, i;

    for (i = 0, c = 0; i < EXT2_VOL_LABEL_LENGTH; i++) {
        c = (u_char)*src++;
        if (c < ' ' + !i || strchr(EXT2_VOL_LABEL_INVAL_CHARS, c))
            break;
    }
    return i && !c;
}

#if 0
/*
 * Make a volume label.
 */
static void
mklabel(u_int8_t *dest, const char *src)
{
    int c, i;

    for (i = 0; i < EXT2_VOL_LABEL_LENGTH; i++) {
	c = *src ? toupper(*src++) : ' ';
	*dest++ = !i && c == '\xe5' ? 5 : c;
    }
}
#endif

#ifdef DEBUG
static void
report_exit_code(int ret)
{
    printf("...ret = %d\n", ret);

    switch (ret) {
    case FSUR_RECOGNIZED:
	printf("File system recognized; a mount is possible.\n");
	break;
    case FSUR_UNRECOGNIZED:
	printf("File system unrecognized; a mount is not possible.\n");
	break;
    case FSUR_IO_SUCCESS:
	printf("Mount, unmount, or repair succeeded.\n");
	break;
    case FSUR_IO_FAIL:
	printf("Unrecoverable I/O error.\n");
	break;
    case FSUR_IO_UNCLEAN:
	printf("Mount failed; file system is not clean.\n");
	break;
    case FSUR_INVAL:
	printf("Invalid argument.\n");
	break;
    case FSUR_LOADERR:
	printf("kern_loader error.\n");
	break;
    case FSUR_INITRECOGNIZED:
	printf("File system recognized; initialization is possible.\n");
	break;
    }
}
#endif

/* Set the name of this file system */
static void fs_set_label_file(char *labelPtr)
{
    int 			fd;
    char	filename[MAXPATHLEN];
    char	label[EXT2_VOL_LABEL_LENGTH],
        			labelUTF8[EXT2_VOL_LABEL_LENGTH*3],
        			*tempPtr;
    off_t			offset;
    CFStringRef 	cfstr;

    snprintf(filename, MAXPATHLEN, "%s/%s%s/%s.label", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    unlink(filename);

    snprintf(filename, MAXPATHLEN, "%s/%s%s/%s.name", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    unlink(filename);

    if((labelPtr[0] != '\0') && oklabel(labelPtr)) {
        /* Remove any trailing white space*/
        labelPtr[EXT2_VOL_LABEL_LENGTH] = '\0';
        tempPtr = &labelPtr[EXT2_VOL_LABEL_LENGTH - 1];
        offset = EXT2_VOL_LABEL_LENGTH;
        while(((*tempPtr == '\0')||(*tempPtr == ' ')) && (offset--)) {
            if(*tempPtr == ' ') {
                *tempPtr = '\0';
            }
            tempPtr--;
        }

        /* remove any embedded spaces (mount doesn't like them) */
        tempPtr = labelPtr;
        while(*tempPtr != '\0') {
            if(*tempPtr == ' ') {
                *tempPtr = '_';
            }
            tempPtr++;
        }

        if(labelPtr[0] == '\0') {
            strncpy(label, UNKNOWN_LABEL, EXT2_VOL_LABEL_LENGTH);
        } else
            strncpy(label, labelPtr, EXT2_VOL_LABEL_LENGTH);

    } else {
        strncpy(label, UNKNOWN_LABEL, EXT2_VOL_LABEL_LENGTH);
    }

#if	D2U_LOWER_CASE
    msd_str_to_lower(label);
#endif	/* D2U_LOWER_CASE */

    /* Convert it to UTF-8 */
    cfstr = CFStringCreateWithCString(kCFAllocatorDefault, label, kCFStringEncodingWindowsLatin1);
    if (cfstr == NULL)
        cfstr = CFStringCreateWithCString(kCFAllocatorDefault, label, kCFStringEncodingDOSJapanese);
    if (cfstr == NULL)
        cfstr = CFStringCreateWithCString(kCFAllocatorDefault, UNKNOWN_LABEL, kCFStringEncodingWindowsLatin1);
    (void) CFStringGetCString(cfstr, labelUTF8, sizeof(labelUTF8), kCFStringEncodingUTF8);
    CFRelease(cfstr);

    /* At this point, label should contain a correct formatted name */
    write(1, labelUTF8, strlen(labelUTF8));

    /* backwards compatibility */
    /* write the .label file */
    snprintf(filename, MAXPATHLEN, "%s/%s%s/%s.label", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, 0755);
    if (fd >= 0) {
	write(fd, labelUTF8, strlen(labelUTF8) + 1);
	close(fd);
    }
    /* write the .name file */
    snprintf(filename, MAXPATHLEN, "%s/%s%s/%s.name", FS_DIR_LOCATION,
            FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
    fd = open(filename, O_WRONLY|O_CREAT|O_EXCL, 0755);
    if (fd >= 0) {
	write(fd, FS_NAME_FILE, 1 + strlen(FS_NAME_FILE));
	close(fd);
    }
}

/*
 * Begin Filesystem-specific code
 */
static int fs_probe(char *devpath, int removable, int writable)
{
   char *buf;
   struct ext2_super_block *sbp;
   int ret;
   
   bzero(diskLabel, EXT2_VOL_LABEL_LENGTH+1);
   
   ret = extsuper_read(devpath, &buf, &sbp);
   if (ret)
      return (FSUR_UNRECOGNIZED);
   
   /* Get the volume name */
   if (0 != sbp->s_volume_name[0])
      bcopy(sbp->s_volume_name, diskLabel, EXT2_VOL_LABEL_LENGTH);
   
   fs_set_label_file(diskLabel);

   free(buf);
   return(FSUR_RECOGNIZED);
}

static int fs_label(char *devpath, char *volName)
{
   char *labelargs[] = {FS_LABEL_COMMAND, devpath, volName, NULL};
               
   safe_execv(labelargs);
   return (FSUR_IO_SUCCESS);
}
