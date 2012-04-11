/*
* Copyright 2004,2006 Brian Bergstrand.
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

#import <stdio.h>
#import <sys/types.h>
#import <sys/mount.h>
#import <sys/mman.h>
#import <unistd.h>
#import <fcntl.h>

#import <ufs/ufs/dinode.h>
#import <ufs/ffs/fs.h>

#import <ext2_byteorder.h>
#ifndef NOEXT2
#import <gnu/ext2fs/ext2_fs.h>
#endif

#ifndef EFSM_PRIVATE
#define EFSM_PRIVATE
#endif
// We don't want HFSVolumes.h from CarbonCore
#define __HFSVOLUMES__
#import "ExtFSMedia.h"
#undef __HFSVOLUMES__
#import <hfs/hfs_format.h>

char *progname;

#ifndef NOEXT2
struct superblock {
    struct ext2_super_block *s_es;
};
#endif

struct efsattrs {
    u_int64_t fsBlockCount;
    u_int32_t fsBlockSize;
    NSString *name;
    NSString *uuid;
    BOOL isJournaled, blessed;
};

#define EXT_SUPER_SIZE 32768
#define EXT_SUPER_OFF 1024
#define HFS_SUPER_OFF 1024
static ExtFSType efs_getdevicefs (const char *device, struct efsattrs *fsa)
{
    int fd, bytes;
    char *buf;
    #ifndef NOEXT2
    struct superblock sb;
    #endif
    HFSPlusVolumeHeader *hpsuper;
    HFSMasterDirectoryBlock *hsuper;
    struct fs *usuper;
    ExtFSType type = fsTypeUnknown; 
    char path[PATH_MAX] = "/dev/r";
    BOOL slocked = NO;
    
    buf = malloc(EXT_SUPER_SIZE);
    if (!buf) {
        fprintf(stderr, "%s: malloc failed, %s\n", progname,
            strerror(ENOMEM));
        return (-ENOMEM);
    }
    
    bytes = strlen(path);
    snprintf((path+bytes), PATH_MAX-1-bytes, "%s", device);
    path[PATH_MAX-1] = 0;
    
    fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        fprintf(stderr, "%s: open '%s' failed, %s\n", progname, path,
            strerror(errno));
        free(buf);
        return (-errno);
    }
    
    if (0 == geteuid()) {
        mprotect(buf, EXT_SUPER_SIZE, PROT_EXEC | PROT_WRITE | PROT_READ);
        slocked = (0 == mlock(buf, EXT_SUPER_SIZE));
    }
    
    bytes = read (fd, buf, EXT_SUPER_SIZE);

    if (EXT_SUPER_SIZE != bytes) {
        fprintf(stderr, "%s: device read '%s' failed, %s\n", progname, path,
            strerror(errno));
        bzero(buf, EXT_SUPER_SIZE);
        if (slocked)
            (void)munlock(buf, EXT_SUPER_SIZE);
        free(buf);
        close(fd);
        return (-errno);
    }

    unsigned char *uuidbytes = NULL;
    unsigned char *hfsuidbytes = NULL;
    
    /* Ext2 and HFS(+) Superblocks start at offset 1024 (block 2). */
    #ifndef NOEXT2
    sb.s_es = (struct ext2_super_block*)(buf+EXT_SUPER_OFF);
    if (EXT2_SUPER_MAGIC == le16_to_cpu(sb.s_es->s_magic)) {
        type = fsTypeExt2;
        if (0 != EXT2_HAS_COMPAT_FEATURE(&sb, EXT3_FEATURE_COMPAT_HAS_JOURNAL))
            type = fsTypeExt3;
        uuidbytes = sb.s_es->s_uuid;
        fsa->fsBlockCount = le32_to_cpu(sb.s_es->s_blocks_count);
        fsa->fsBlockSize = EXT2_MIN_BLOCK_SIZE << le32_to_cpu(sb.s_es->s_log_block_size);
    } else
    #endif
    {
efs_hfs:
        hpsuper = (HFSPlusVolumeHeader*)(buf+HFS_SUPER_OFF);
        if (kHFSPlusSigWord == be16_to_cpu(hpsuper->signature)
            || (kHFSXSigWord == be16_to_cpu(hpsuper->signature))) {
            type = fsTypeHFSPlus;
            if (be32_to_cpu(hpsuper->attributes) & kHFSVolumeJournaledMask) {
                type = fsTypeHFSJ;
                fsa->isJournaled = YES;
            }
            if (kHFSXSigWord == be16_to_cpu(hpsuper->signature))
                type = fsTypeHFSX;
            // unique id is in the last 8 bytes of the finder info storage
            // this is obviously not a UUID
            hfsuidbytes = &hpsuper->finderInfo[24];
            fsa->fsBlockCount = be32_to_cpu(hpsuper->totalBlocks);
            fsa->fsBlockSize = be32_to_cpu(hpsuper->blockSize);
            /* TN1150:
               finderInfo[5] contains the directory ID of a bootable Mac OS X system
               (the /System/Library/CoreServices directory), or zero if there is no bootable Mac OS X system on the volume.
            */
            fsa->blessed = ((u_int32_t*)&hpsuper->finderInfo[0])[5] > 0 ? YES : NO;
        }
        else if (kHFSSigWord == be16_to_cpu(hpsuper->signature)) {
            type = fsTypeHFS;
            hsuper = (HFSMasterDirectoryBlock*)(buf+HFS_SUPER_OFF);
            if (0 != hsuper->drEmbedSigWord) {
                // read the embedded volume header
                bytes = be32_to_cpu(hsuper->drAlBlkSiz);
                off_t offset = be16_to_cpu(hsuper->drAlBlSt) * 512 /*XXX*/;
                offset += (off_t)be16_to_cpu(hsuper->drEmbedExtent.startBlock) * (off_t)bytes;
                if (bytes < (sizeof(HFSPlusVolumeHeader) + HFS_SUPER_OFF))
                    bytes = EXT_SUPER_SIZE;
                if (bytes == pread(fd, buf, bytes, offset)) {
                    goto efs_hfs;
                }
            } else {
                fsa->fsBlockCount = be16_to_cpu(hsuper->drNmAlBlks);
                fsa->fsBlockSize = be32_to_cpu(hsuper->drAlBlkSiz);
            }
        } else {
            usuper = (struct fs*)(buf+SBOFF);
            if (FS_MAGIC == be32_to_cpu(usuper->fs_magic)) {
                struct ufslabel *ulabel = (struct ufslabel*)(buf+UFS_LABEL_OFFSET);
                const u_char lmagic[] = UFS_LABEL_MAGIC;
                type = fsTypeUFS;
                if (*((u_int32_t*)&lmagic[0]) == be32_to_cpu(ulabel->ul_magic))
                    fsa->uuid = [[NSString alloc] initWithFormat:@"%qX", be64_to_cpu(ulabel->ul_uuid)];
            fsa->fsBlockCount = be32_to_cpu(usuper->fs_size);
            fsa->fsBlockSize = be32_to_cpu(usuper->fs_bsize);
        }
    }
    }
    
    close(fd);
    
    if (uuidbytes) {
        CFUUIDRef cuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault,
            *((CFUUIDBytes*)uuidbytes));
        if (cuuid) {
            fsa->uuid = (NSString*)CFUUIDCreateString(kCFAllocatorDefault, cuuid);
            CFRelease(cuuid);
        }
    } else if (hfsuidbytes) {
        fsa->uuid = [[NSString alloc] initWithFormat:@"%qX", be64_to_cpu(*((u_int64_t*)hfsuidbytes))];
    }

    bzero(buf, EXT_SUPER_SIZE);
    if (slocked)
        (void)munlock(buf, EXT_SUPER_SIZE);
    free(buf);
    return (type);
}

int main (int argc, char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    int type;
    
    if (2 != argc)
        return (-1);
    
    progname = strrchr(argv[0], '/');
    if (!progname)
        progname = argv[0];
    
    struct efsattrs fsa = {0};
    type = efs_getdevicefs(argv[1], &fsa);
    if (type > -1 && fsTypeUnknown != type) {
        NSMutableDictionary *d = [[NSMutableDictionary alloc] init];
        if (fsa.name) {
            [d setObject:fsa.name forKey:EPROBE_KEY_NAME];
        }
        if (fsa.uuid) {
            [d setObject:fsa.uuid forKey:EPROBE_KEY_UUID];
        }
        [d setObject:[NSNumber numberWithBool:fsa.isJournaled] forKey:EPROBE_KEY_JOURNALED];
        [d setObject:[NSNumber numberWithUnsignedInt:fsa.fsBlockSize] forKey:EPROBE_KEY_FSBLKSIZE];
        [d setObject:[NSNumber numberWithUnsignedLongLong:fsa.fsBlockCount] forKey:EPROBE_KEY_FSBLKCOUNT];
        [d setObject:[NSNumber numberWithBool:fsa.blessed] forKey:EPROBE_KEY_BLESSED];
        
        id plist = [NSPropertyListSerialization dataFromPropertyList:d
            format:NSPropertyListXMLFormat_v1_0
        	errorDescription:nil];
        if (plist)
            plist = [[NSString alloc] initWithData:plist encoding:NSUTF8StringEncoding]; 
        if (plist)
            printf ("%s", [plist  UTF8String]);
    }
    [pool release];
    return (type);
}
