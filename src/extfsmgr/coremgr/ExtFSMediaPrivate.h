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

/*
 * This is a private implementation file. It should never be accessed by ExtFSDiskManager clients.
 */

#import "ExtFSMedia.h"
#import "ExtFSMediaController.h"

@protocol ExtFSMCP
- (void)updateMountStatus;
- (ExtFSMedia*)createMediaWithIOService:(io_service_t)service properties:(NSDictionary*)props;
- (int)updateMedia:(io_iterator_t)iter remove:(BOOL)remove;
- (BOOL)volumeDidUnmount:(NSString*)name;
- (void)removePending:(ExtFSMedia*)media;
@end

@interface ExtFSMediaController (Private)
- (void)postNotification:(NSArray*)args;
- (NSString*)pathForResource:(NSString*)resource;
@end

#define EFSMCPostNotification(note, obj, info) do {\
NSArray *args = [[NSArray alloc] initWithObjects:note, obj, info, nil]; \
[[ExtFSMediaController mediaController] performSelectorOnMainThread:@selector(postNotification:) \
withObject:args waitUntilDone:NO]; \
[args release]; \
} while(0)

#ifndef EXTFS_DM_BNDL_ID
#define EXTFS_DM_BNDL_ID @"net.sourceforge.ext2fsx.ExtFSDiskManager"
#endif
#define EFS_PROBE_RSRC @"efsprobe"

@interface ExtFSMedia (ExtFSMediaControllerPrivate)
- (BOOL)updateAttributesFromIOService:(io_service_t)service;
- (void)updateProperties:(NSDictionary*)properties;
- (void)setIsMounted:(struct statfs*)stat;
- (NSDictionary*)iconDescription;
- (void)addChild:(ExtFSMedia*)media;
- (void)remChild:(ExtFSMedia*)media;
- (io_service_t)copyIOService;
- (io_service_t)copySMARTIOService; // Get SMART capable service
/* Implemented in ExtFSMedia.m -- this just gets rid of the compiler warnings. */
- (int)fsInfoUsingCache:(BOOL)cache;
- (int)fsInfo;
- (void)probe;
- (io_service_t)SMARTService;
- (void)setSMARTService:(io_service_t)is;
- (BOOL)isSMART;
- (void)setNotSMART;
- (BOOL)claimedExclusive;
- (void)setClaimedExclusive:(BOOL)claimed;
- (BOOL)loadCustomVolumeIcon;
// This is a special init used for one specific purpose ([ExtFSMediaController allowMount]).
- (ExtFSMedia*)initWithDeviceName:(NSString*)device;
@end

#ifndef EXT2FS_NAME
#define EXT2FS_NAME "ext2"
#endif
#ifndef EXT3FS_NAME
#define EXT3FS_NAME "ext3"
#endif

#define HFS_NAME "hfs"
#define UFS_NAME "ufs"
#define CD9660_NAME "cd9660"
#define CDAUDIO_NAME "cddafs"
#define UDF_NAME "udf"
#define MSDOS_NAME "msdos"
#define NTFS_NAME "ntfs"

#define efsIOTransportTypeMask 0x00000007
#define efsIOTransportBusMask  0xFFFFFFF8

#ifdef __ppc__
#define E2_BAD_ADDR 0xdeadbeef
#ifndef trap
#define trap() asm volatile("trap")
#endif
#elif defined(__i386__)
#define E2_BAD_ADDR 0xbaadf00d
#ifndef trap
#define trap() asm volatile("int $3")
#endif
#endif

#ifndef E2DiagLog
#ifdef DEBUG
#define E2DiagLog NSLog
#else
#define E2DiagLog()
#endif
#endif

#ifndef E2Log
#define E2Log NSLog
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_3
#define EFSM_1030_SUPPORT
__private_extern__ void PantherInitSMART();
#endif

/*!
@enum ExtFSMediaFlags
@abstract ExtFSMedia internal bit flags.
@discussion These flags should not be used by ExtFSMedia clients.
@constant kfsDiskArb Media is managed by Disk Arb.
@constant kfsMounted Media is mounted.
@constant kfsWritable Media or filesystem is writable.
@constant kfsEjectable Media is ejectable.
@constant kfsWholeDisk Media represents a whole disk.
@constant kfsLeafDisk Media contains no partititions.
@constant kfsCDROM Media is a CD.
@constant kfsDVDROM Media is a DVD.
@constant kfsGetAttrlist Filesystem supports getattrlist() sys call.
@constant kfsIconNotFound No icon found for the media.
@constant kfsNoMount Media cannot be mounted (partition map, driver partition, etc).
@constant kfsPermsEnabled Filesystem permissions are in effect.
*/
enum {
   kfsDiskArb		= (1<<0), /* Mount/unmount with Disk Arb */
   kfsMounted		= (1<<1),
   kfsWritable		= (1<<2),
   kfsEjectable	    = (1<<3),
   kfsWholeDisk	    = (1<<4),
   kfsLeafDisk		= (1<<5),
   kfsCDROM			= (1<<6),
   kfsDVDROM		= (1<<7),
   kfsGetAttrlist	= (1<<9),  /* getattrlist supported */
   kfsIconNotFound	= (1<<10),
   kfsNoMount		= (1<<11),
   kfsPermsEnabled  = (1<<12),
   kfsNotSMART      = (1<<13),
   kfsClaimedWithDA = (1<<14),
   kfsHasCustomIcon = (1<<15)
};
