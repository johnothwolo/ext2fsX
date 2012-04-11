/*
* Copyright 2003-2006 Brian Bergstrand.
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

#import <Cocoa/Cocoa.h>

/*!
@enum ExtFSType
@abstract Filesystem type id's.
@constant fsTypeExt2 Ext2 filesystem id.
@constant fsTypeExt3 Ext3 journaled filesystem id.
@constant fsTypeHFS Macintosh HFS filesystem id.
@constant fsTypeHFSPlus Macintosh HFS Plus filesystem id.
@constant fsTypeHFSJ Macintosh HFS Plus Journaled filesystem id.
@constant fsTypeHFSX Macintosh HFS Extended (Extreme?) filesystem id.
@constant fsTypeUFS UFS (Unix)filesystem id.
@constant fsTypeCD9660 ISO 9660 filesystem id.
@constant fsTypeCDAudio CD Audio filesystem id.
@constant fsTypeUDF UDF (DVD) filesystem id.
@constant fsTypeMSDOS FAT filesystem id. Includes FAT12, FAT16 and FAT32
@constant fsTypeNTFS NTFS filesystem id.
@constant fsTypeUnknown Unknown filesystem id.
*/
typedef enum {
   fsTypeExt2 = 0,
   fsTypeExt3 = 1,
   fsTypeHFS  = 2,
   fsTypeHFSPlus = 3,
   fsTypeHFSJ = 4,
   fsTypeHFSX = 5,
   fsTypeUFS  = 6,
   fsTypeCD9660 = 7,
   fsTypeCDAudio = 8,
   fsTypeUDF   = 9,
   fsTypeMSDOS = 10,
   fsTypeNTFS  = 11,
   fsTypeUnknown = 12,
   fsTypeNULL
}ExtFSType;

/*!
@enum ExtFSType
@abstract I/O Transport type id's.
@constant efsIOTransportTypeInternal Internal transport type id.
@constant efsIOTransportTypeExternal External transport type id.
@constant efsIOTransportTypeVirtual Virtual (software) transport type id.
@constant efsIOTransportTypeATA ATA bus transport id.
@constant efsIOTransportTypeFirewire Firewire bus transport id.
@constant efsIOTransportTypeUSB USB bus transport id.
@constant efsIOTransportTypeSCSI SCSI bus transport id.
@constant efsIOTransportTypeImage Disk Image (virtual) transport id.
@constant efsIOTransportTypeSATA SATA bus transport id.
@constant efsIOTransportTypeFibreChannel FibreChannel bus transport id.
@constant efsIOTransportTypeUnknown Unknown bus transport id.
*/
typedef enum {
    efsIOTransportTypeInternal = (1<<0),
    efsIOTransportTypeExternal = (1<<1),
    efsIOTransportTypeVirtual  = (1<<2),
    efsIOTransportTypeATA      = (1<<8),
    efsIOTransportTypeATAPI    = (1<<9),
    efsIOTransportTypeFirewire = (1<<10),
    efsIOTransportTypeUSB      = (1<<11),
    efsIOTransportTypeSCSI     = (1<<12),
    efsIOTransportTypeImage    = (1<<13),
    efsIOTransportTypeSATA     = (1<<14),
    efsIOTransportTypeFibreChannel = (1<<15),
    efsIOTransportTypeUnknown  = (1<<31)
}ExtFSIOTransportType;

/*!
@enum ExtFSOpticalMediaType
@abstract Optical Media Type id's to identify read-only, write-once, re-writable media sepcifics.
@constant efsOpticalTypeCD Read only CD.
@constant efsOpticalTypeCDR CD-R
@constant efsOpticalTypeCDRW CD-RW
@constant efsOpticalTypeDVD Read only DVD.
@constant efsOpticalTypeDVDDashR DVD-R
@constant efsOpticalTypeDVDDashRW DVD-RW
@constant efsOpticalTypeDVDPlusR DVD+R
@constant efsOpticalTypeDVDPlusRW DVD+RW
@constant efsOpticalTypeDVDRAM DVD-RAM
@constant efsOpticalTypeUnknown Unknown optical disc.
*/
typedef enum {
    efsOpticalTypeCD        = 0,
    efsOpticalTypeCDR       = 1,
    efsOpticalTypeCDRW      = 2,
    efsOpticalTypeDVD       = 3,
    efsOpticalTypeDVDDashR  = 4,
    efsOpticalTypeDVDDashRW = 5,
    efsOpticalTypeDVDPlusR  = 6,
    efsOpticalTypeDVDPlusRW = 7,
    efsOpticalTypeDVDRAM    = 8,
    
    efsOpticalTypeUnknown   = 32767
    
}ExtFSOpticalMediaType;

static __inline__ BOOL
IsOpticalCDMedia(ExtFSOpticalMediaType type)
{
    return (type >= efsOpticalTypeCD && type <= efsOpticalTypeCDRW);
}

static __inline__ BOOL
IsOpticalDVDMedia(ExtFSOpticalMediaType type)
{
    return (type >= efsOpticalTypeDVD && type <= efsOpticalTypeDVDRAM);
}

// Forward declaration for an ExtFSMedia private type.
struct superblock;

/*!
@class ExtFSMedia
@abstract Representation of filesystem and/or device.
@discussion Instances of this class can be used to query
a filesystem or device for its properties.
*/
@interface ExtFSMedia : NSObject
{
@protected
    NSDictionary *e_probedAttributes;
@private
   void *e_lock; // lock storage
   ExtFSMedia *e_parent;
   id e_children;
   
   id e_media, e_iconDesc, e_object;
   NSString *e_where, *e_ioregName, *e_volName, *e_uuid, *e_bsdName;
   struct superblock *e_sb;
   u_int64_t e_size, e_blockCount, e_blockAvail;
   u_int32_t e_devBlockSize, e_fsBlockSize, e_attributeFlags,
      e_volCaps, e_lastFSUpdate, e_fileCount, e_dirCount;
   ExtFSType e_fsType;
   ExtFSIOTransportType e_ioTransport;
   NSImage *e_icon;
   ExtFSOpticalMediaType e_opticalType;
   unsigned int e_smartService;
   u_int32_t e_lastSMARTUpdate;
   int e_smartStatus;
#ifndef NOEXT2
   unsigned char e_reserved[32];
#endif
}

/*!
@method initWithIORegProperties:
@abstract Preferred initializer method.
@param properties A dictionary of the device properties returned
from the IO Registry.
@result A new ExtFSMedia object or nil if there was an error.
The object will be returned auto-released.
*/
- (ExtFSMedia*)initWithIORegProperties:(NSDictionary*)properties;

/*!
@method representedObject
@abstract Access associated context object.
@result Context object or nil if a context object
has not been set.
*/
- (id)representedObject;
/*!
@method setRepresentedObject
@abstract Associate some object with the target ExtFSMedia object.
@discussion The represented object is retained for the lifetime
of the media object (or until replaced). Call with object == nil
to release the represented object manually.
*/
- (void)setRepresentedObject:(id)object;

/*!
@method parent
@abstract Access the parent of the target object.
@discussion The parent object is determined from the IO Registry
hierarchy.
@result ExtFSMedia object that is the direct parent of
the target object -- nil is returned if the target has
no parent.
*/
- (ExtFSMedia*)parent;
/*!
@method children
@abstract Access the children of the target object.
@discussion Children are determined from the IO Registry hierarchy.
This is a snapshot in time, the number of children
could possibly change the moment after return.
@result An array of ExtFSMedia objects that are directly descended
from the target object -- nil is returned if the target has
no descendants.
*/
- (NSArray*)children;
/*!
@method childCount
@abstract Convenience method to obtain the child count of the target.
@discussion This is a snapshot in time, the number of children
could possibly change the moment after return.
@result Integer containing the number of children.
*/
- (unsigned)childCount;

/* Device */
/*!
@method ioRegistryName
@abstract Access the name of the object as the IO Registry
identifies it.
@result String containing the IOKit name.
*/
- (NSString*)ioRegistryName;
/*!
@method bsdName
@abstract Access the device name of the object as the
BSD kernel identifies it.
@result String containing the kernel device name.
*/
- (NSString*)bsdName;

/*!
@method icon
@abstract Access the icon of the object as determined by the IO Registry.
@result Preferred device image for the target object.
*/
- (NSImage*)icon;

/*!
@method isEjectable
@abstract Determine if the media is ejectable from its enclosure.
@result YES if the media can be ejected, otherwise NO.
*/
- (BOOL)isEjectable;
/*!
@method canMount
@abstract Determine if the media is mountable.
@result YES if the media can be mounted, otherwise NO.
*/
- (BOOL)canMount;
/*!
@method isMounted
@abstract Determine if the media is currently mounted.
@result YES if the filesystem on the device is currently mounted, otherwise NO.
*/
- (BOOL)isMounted;
/*!
@method isWritable
@abstract Determine if the media/filesystem is writable.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result YES if the filesystem or media is writable, otherwise NO.
*/
- (BOOL)isWritable;
/*!
@method isWholeDisk
@abstract Determine if the target object represents the media
as a whole (ie the total disk, not a partition of the disk).
@result YES or NO.
*/
- (BOOL)isWholeDisk;
/*!
@method isLeafDisk
@abstract Determine if the media contains any partitions.
@result YES if the media does not contain partitions, otherwise NO.
*/
- (BOOL)isLeafDisk;
/*!
@method isOptical
@abstract Determine if the media is any type of optical disc.
@result YES if the media is an optical disc, otherwise NO.
*/
- (BOOL)isOptical;
/*!
@method opticalMediaType
@abstract Determine specific optical media type.
@discussion If the media is not an optical disc, efsOpticalTypeUnknown is always returned.
@result Type of media.
*/
- (ExtFSOpticalMediaType)opticalMediaType;
/*!
@method usesDiskArb
@abstract Determine if the media is managed by the Disk Arbitration
daemon.
@result YES if the media is DA managed, otherwise NO.
*/
- (BOOL)usesDiskArb;
/*!
@method size
@abstract Determine the size of the filesystem or media.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result Size of the filesystem or media in bytes.
*/
- (u_int64_t)size; /* bytes */
/*!
@method blockSize
@abstract Determine the block size of the filesystem or media.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result Size of the filesystem or media block size in bytes.
*/
- (u_int32_t)blockSize;

/*!
@method fsType
@abstract Determine the filesystem type.
@discussion If the media is not mounted, the result may
be fsTypeUnknown.
@result The filesystem type id.
*/
- (ExtFSType)fsType;
/*!
@method fsName
@abstract Get the filesystem name in a format suitable for display to a user.
@discussion If the media is not mounted, the result will always
be nil.
@result The filesystem name or nil if there was an error.
*/
- (NSString*)fsName;
/*!
@method mountPoint
@abstract Determine the directory that the filesystem is mounted on.
@result String containing mount path, or nil if the media is not mounted.
*/
- (NSString*)mountPoint;
/*!
@method availableSize
@abstract Determine the filesystem's available space.
@result Size of available space in bytes. Always 0 if the filesystem is
not mounted.
*/
- (u_int64_t)availableSize;
/*!
@method blockCount
@abstract Determine the block count of the filesystem or media.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result Number of blocks in the filesystem or media.
*/
- (u_int64_t)blockCount;
/*!
@method fileCount
@abstract Determine the number of files in the filesystem.
@result The number of files, or 0 if the media is not mounted.
*/
- (u_int64_t)fileCount;
/*!
@method dirCount
@abstract Determine the number of directories in the filesystem.
@discussion The filesystem must support the getattrlist() sys call or
0 will be returned.
@result The number of directories, or 0 if the media is not mounted.
*/
- (u_int64_t)dirCount;
/*!
@method volName
@abstract Get the filesystem name.
@result String containing the filesystem name or nil if it
cannot be determined (ie the media is not mounted).
*/
- (NSString*)volName;
/*!
@method hasPermissions
@abstract Determine if filesystem permissions are in effect.
@result YES if permissions are active, otherwise NO.
Always NO if the media is not mounted.
*/
- (BOOL)hasPermissions;
/*!
@method hasJournal
@abstract Determine if the filesystem has a journal log.
@result YES if a journal is present, otherwise NO.
Always NO if the media is not mounted.
*/
- (BOOL)hasJournal;
/*!
@method isJournaled
@abstract Determine if the filesystem journal is active.
@discussion A filesystem may have a journal on disk, but it
may not be currently in use.
@result YES if the journal is active, otherwise NO.
Always NO if the media is not mounted.
*/
- (BOOL)isJournaled;
/*!
@method isCaseSensitive
@abstract Determine if the filesystem uses case-sensitive file names.
@result YES or NO. Always NO if the media is not mounted.
*/
- (BOOL)isCaseSensitive;
/*!
@method isCasePreserving
@abstract Determine if the filesystem preserves file name case.
@discussion HFS is case-preserving, but not case-sensitive, 
Ext2 and UFS are both and FAT12/16 are neither.
@result YES or NO. Always NO if the media is not mounted.
*/
- (BOOL)isCasePreserving;
/*!
@method hasSparseFiles
@abstract Determine if the filesystem supports sparse files.
@discussion Sparse files are files with "holes" in them, the filesystem
will automatically return zero's when these "holes" are accessed. HFS does
not support sparse files, while Ext2 and UFS do.
@result YES if sparse files are supported, otherwise NO.
Always NO if the media is not mounted.
*/
- (BOOL)hasSparseFiles;
/*!
@method isBlessed
@abstract Determine if the volume contains an OS X blessed folder (always false for non-HFS+).
@result True if the volume is blessed (bootable).
*/
- (BOOL)isBlessed;

/*!
@method isExtFS
@abstract Convenience method to determine if a filesystem is Ext2 or Ext3.
@result YES if the filesystem is Ext2/3, otherwise NO.
*/
- (BOOL)isExtFS;
/*!
@method uuidString
@abstract Get the filesystem UUID as a string.
@discussion This is only supported by Ext2/3 and UFS currrently.
@result String containing UUID or nil if a UUID is not present.
This may be nil if the media is not mounted.
*/
- (NSString*)uuidString;
/*!
@method hasIndexedDirs
@abstract Determine if directory indexing is active.
@discussion Directory indexing is an Ext2/3 specific option.
It greatly speeds up file name lookup for large directories.
@result YES if indexing is active, otherwise NO.
Always NO if the media is not mounted or the filesystem is not Ext2/3.
*/
- (BOOL)hasIndexedDirs;
/*!
@method hasLargeFiles
@abstract Determine if the filesystem supports large files (> 2GB).
@discussion This only works with Ext2/3 filesystems currently.
@result YES if large files are supported, otherwise NO.
Always NO if the media is not mounted or the filesystem is not Ext2/3.
*/
- (BOOL)hasLargeFiles;
/*!
@method maxFileSize
@abstract Determine the max file size supported by the filesystem.
@discussion Currently, this method always returns 0.
@result Max file size or 0 if the filesystem is not mounted.
*/
- (u_int64_t)maxFileSize;

/*!
@method transportType
@abstract Determine how the device is connected.
@result An ExtFSConnectionType id.
*/
- (ExtFSIOTransportType)transportType;
/*!
@method transportBus
@abstract Determine the type of bus the device is connected to.
@result An ExtFSConnectionType id.
*/
- (ExtFSIOTransportType)transportBus;
/*!
@method transportName
@abstract Determine the name of the device bus.
@result A string containing a name suitable for display to a user
or nil if there was an error.
*/
- (NSString*)transportName;

/*!
@method compare:
@abstract Determine the equality of one object compared to another.
@param media The object to compare the target object to.
@result NSOrderedSame if the objects are equal.
*/
- (NSComparisonResult)compare:(ExtFSMedia *)media;

@end

/*!
@category ExtFSMedia (ExtFSMediaLockMgr)
@abstract Locking methods for use by ExtFSMedia sub-classers only.
@discussion It is up to the sub-class to avoid deadlock with ExtFSMedia.
When calling any ExtFSMedia method, the lock should not be held.
*/
@interface ExtFSMedia (ExtFSMediaLockMgr)

/*!
@method lock:
@abstract Lock the object.
@discussion Must be balanced with a call to [unlock].
@param exclusive If YES, acquire exclusive access, otherwise shared
access is acquired.
*/
- (void)lock:(BOOL)exclusive;

/*!
@method unlock
@abstract Release object lock.
*/
- (void)unlock;

@end

/*!
@const ExtFSMediaNotificationUpdatedInfo
@abstract This notification is sent to the default Notification center
when the filesystem information has been updated (available space, file count, etc).
The changed media object is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationUpdatedInfo;
/*!
@const ExtFSMediaNotificationChildChange
@abstract This notification is sent to the default Notification center
when a child is added or removed. The parent object is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationChildChange;

/*!
@defined BSDNAMESTR
@abstract Convenience macro to get an object's device name as a C string.
*/
#define BSDNAMESTR(media) (char *)[[(media) bsdName] UTF8String]
/*!
@defined MOUNTPOINTSTR
@abstract Convenience macro to get an object's mount point name as a C string.
*/
#define MOUNTPOINTSTR(media) (char *)[[(media) mountPoint] UTF8String]

/*!
@defined NSSTR
@abstract Convenience macro to convert a C string to an NSString.
*/
#define NSSTR(cstr) [NSString stringWithUTF8String:(cstr)]

#define EPROBE_KEY_NAME @"name"
#define EPROBE_KEY_UUID @"uuid"
#define EPROBE_KEY_JOURNALED @"journaled"
#define EPROBE_KEY_FSBLKSIZE @"fsBlockSize"
#define EPROBE_KEY_FSBLKCOUNT @"fsBlockCount"
#define EPROBE_KEY_BLESSED @"blessed"
