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
typedef NS_ENUM(NSInteger, ExtFSType) {
   fsTypeExt2 = 0,
   fsTypeExt3,
   fsTypeExt4,
   fsTypeHFS,
   fsTypeHFSPlus,
   fsTypeHFSJ,
   fsTypeHFSX,
   fsTypeUFS,
   fsTypeCD9660,
   fsTypeCDAudio,
   fsTypeUDF,
   fsTypeMSDOS,
   fsTypeNTFS,
   fsTypeExFat,
   fsTypeZFS,
   fsTypeXFS,
   fsTypeReiserFS,
   fsTypeReiser4,
   fsTypeUnknown,
   fsTypeNULL
};

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
typedef NS_OPTIONS(NSUInteger, ExtFSIOTransportType) {
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
   efsIOTransportTypePCIe     = (1<<16),
   efsIOTransportTypeSD       = (1<<17),
   efsIOTransportTypeUnknown  = (1<<31)
};

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
typedef NS_ENUM(short, ExtFSOpticalMediaType) {
   efsOpticalTypeCD = 0,
   efsOpticalTypeCDR,
   efsOpticalTypeCDRW,
   
   efsOpticalTypeDVD,
   efsOpticalTypeDVDDashR,
   efsOpticalTypeDVDDashRW,
   efsOpticalTypeDVDPlusR,
   efsOpticalTypeDVDPlusRW,
   efsOpticalTypeDVDRAM,
   
   efsOpticalTypeHDDVD,
   efsOpticalTypeHDDVDR,
   efsOpticalTypeHDDVDRW,
   efsOpticalTypeHDDVDRAM,
   
   efsOpticalTypeBD,
   efsOpticalTypeBDR,
   efsOpticalTypeBDRE,
   
   efsOpticalTypeUnknown   = 32767
};

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

static __inline__ BOOL
IsOpticalHDDVDMedia(ExtFSOpticalMediaType type)
{
	return (type >= efsOpticalTypeHDDVD && type <= efsOpticalTypeHDDVDRAM);
}

static __inline__ BOOL
IsOpticalBDMedia(ExtFSOpticalMediaType type)
{
   return (type >= efsOpticalTypeBD && type <= efsOpticalTypeBDRE);
}

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
}

/*!
@method initWithIORegProperties:
@abstract Preferred initializer method.
@param properties A dictionary of the device properties returned
from the IO Registry.
@result A new ExtFSMedia object or nil if there was an error.
The object will be returned auto-released.
*/
- (instancetype)initWithIORegProperties:(NSDictionary*)properties;

/*!
 @property representedObject
 @abstract Access associated context object.
 @result Context object or nil if a context object
 has not been set.
 @discussion The represented object is retained for the lifetime
 of the media object (or until replaced). Call with object == nil
 to release the represented object manually.

 */
@property (retain) id representedObject;

/*!
@property parent
@abstract Access the parent of the target object.
@discussion The parent object is determined from the IO Registry
hierarchy.
@result ExtFSMedia object that is the direct parent of
the target object -- nil is returned if the target has
no parent.
*/
@property (readonly, assign) ExtFSMedia *parent;
/*!
@property children
@abstract Access the children of the target object.
@discussion Children are determined from the IO Registry hierarchy.
This is a snapshot in time, the number of children
could possibly change the moment after return.
@result An array of ExtFSMedia objects that are directly descended
from the target object -- nil is returned if the target has
no descendants.
*/
@property (readonly, copy) NSArray *children;
/*!
@property childCount
@abstract Convenience method to obtain the child count of the target.
@discussion This is a snapshot in time, the number of children
could possibly change the moment after return.
@result Integer containing the number of children.
*/
@property (readonly) NSUInteger childCount;

/* Device */
/*!
@property ioRegistryName
@abstract Access the name of the object as the IO Registry
identifies it.
@result String containing the IOKit name.
*/
@property (readonly, copy) NSString *ioRegistryName;
/*!
@property bsdName
@abstract Access the device name of the object as the
BSD kernel identifies it.
@result String containing the kernel device name.
*/
@property (readonly, copy) NSString *bsdName;

/*!
@property icon
@abstract Access the icon of the object as determined by the IO Registry.
@result Preferred device image for the target object.
*/
@property (readonly, copy) NSImage *icon;

/*!
@property ejectable
@abstract Determine if the media is ejectable from its enclosure.
@result YES if the media can be ejected, otherwise NO.
*/
@property (readonly, getter=isEjectable) BOOL ejectable;
/*!
@property canMount
@abstract Determine if the media is mountable.
@result YES if the media can be mounted, otherwise NO.
*/
@property (readonly) BOOL canMount;
/*!
@property mounted
@abstract Determine if the media is currently mounted.
@result YES if the filesystem on the device is currently mounted, otherwise NO.
*/
@property (getter=isMounted, readonly) BOOL mounted;
/*!
@property writable
@abstract Determine if the media/filesystem is writable.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result YES if the filesystem or media is writable, otherwise NO.
*/
@property (getter=isWritable, readonly) BOOL writable;
/*!
@property wholeDisk
@abstract Determine if the target object represents the media
as a whole (ie the total disk, not a partition of the disk).
@result YES or NO.
*/
@property (getter=isWholeDisk, readonly) BOOL wholeDisk;
/*!
@property leafDisk
@abstract Determine if the media contains any partitions.
@result YES if the media does not contain partitions, otherwise NO.
*/
@property (getter=isLeafDisk, readonly) BOOL leafDisk;
/*!
@property optical
@abstract Determine if the media is any type of optical disc.
@result YES if the media is an optical disc, otherwise NO.
*/
@property (getter=isOptical, readonly) BOOL optical;
/*!
@property opticalMediaType
@abstract Determine specific optical media type.
@discussion If the media is not an optical disc, efsOpticalTypeUnknown is always returned.
@result Type of media.
*/
@property (readonly) ExtFSOpticalMediaType opticalMediaType;
/*!
@property usesDiskArbitration
@abstract Determine if the media is managed by the Disk Arbitration
daemon.
@result YES if the media is DA managed, otherwise NO.
*/
@property (readonly, getter=usesDiskArb) BOOL usesDiskArbitration;
/*!
@property size
@abstract Determine the size of the filesystem or media.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result Size of the filesystem or media in bytes.
*/
@property (readonly) u_int64_t size; /* bytes */
/*!
@property blockSize
@abstract Determine the block size of the filesystem or media.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result Size of the filesystem or media block size in bytes.
*/
@property (readonly) u_int32_t blockSize;

/*!
@property fsType
@abstract Determine the filesystem type.
@discussion If the media is not mounted, the result may
be fsTypeUnknown.
@result The filesystem type id.
*/
@property (readonly) ExtFSType fsType;
/*!
@property fsName
@abstract Get the filesystem name in a format suitable for display to a user.
@discussion If the media is not mounted, the result will always
be nil.
@result The filesystem name or nil if there was an error.
*/
@property (readonly, copy) NSString *fsName;
/*!
@property mountPoint
@abstract Determine the directory that the filesystem is mounted on.
@result String containing mount path, or nil if the media is not mounted.
*/
@property (readonly, copy) NSString *mountPoint;
/*!
@property availableSize
@abstract Determine the filesystem's available space.
@result Size of available space in bytes. Always 0 if the filesystem is
not mounted.
*/
@property (readonly) u_int64_t availableSize;
/*!
@property blockCount
@abstract Determine the block count of the filesystem or media.
@discussion If the media is mounted, then this applies to
the filesystem only. Otherwise, it applies only to the media.
@result Number of blocks in the filesystem or media.
*/
@property (readonly) u_int64_t blockCount;
/*!
@property fileCount
@abstract Determine the number of files in the filesystem.
@result The number of files, or 0 if the media is not mounted.
*/
@property (readonly) u_int64_t fileCount;
/*!
@property dirCount
@abstract Determine the number of directories in the filesystem.
@discussion The filesystem must support the getattrlist() sys call or
0 will be returned.
@result The number of directories, or 0 if the media is not mounted.
*/
@property (readonly) u_int64_t dirCount;
/*!
@property volName
@abstract Get the filesystem name.
@result String containing the filesystem name or nil if it
cannot be determined (ie the media is not mounted).
*/
@property (readonly, copy) NSString *volName;
/*!
@property hasPermissions
@abstract Determine if filesystem permissions are in effect.
@result YES if permissions are active, otherwise NO.
Always NO if the media is not mounted.
*/
@property (readonly) BOOL hasPermissions;
/*!
@property hasJournal
@abstract Determine if the filesystem has a journal log.
@result YES if a journal is present, otherwise NO.
Always NO if the media is not mounted.
*/
@property (readonly) BOOL hasJournal;
/*!
@property journaled
@abstract Determine if the filesystem journal is active.
@discussion A filesystem may have a journal on disk, but it
may not be currently in use.
@result YES if the journal is active, otherwise NO.
Always NO if the media is not mounted.
*/
@property (getter=isJournaled, readonly) BOOL journaled;
/*!
@property caseSensitive
@abstract Determine if the filesystem uses case-sensitive file names.
@result YES or NO. Always NO if the media is not mounted.
*/
@property (getter=isCaseSensitive, readonly) BOOL caseSensitive;
/*!
@property casePreserving
@abstract Determine if the filesystem preserves file name case.
@discussion HFS is case-preserving, but not case-sensitive, 
Ext2 and UFS are both and FAT12/16 are neither.
@result YES or NO. Always NO if the media is not mounted.
*/
@property (getter=isCasePreserving, readonly) BOOL casePreserving;
/*!
@property hasSparseFiles
@abstract Determine if the filesystem supports sparse files.
@discussion Sparse files are files with "holes" in them, the filesystem
will automatically return zero's when these "holes" are accessed. HFS does
not support sparse files, while Ext2 and UFS do.
@result YES if sparse files are supported, otherwise NO.
Always NO if the media is not mounted.
*/
@property (readonly) BOOL hasSparseFiles;
/*!
@property blessed
@abstract Determine if the volume contains an OS X blessed folder (always false for non-HFS+).
@result True if the volume is blessed (bootable).
*/
@property (getter=isBlessed, readonly) BOOL blessed;

/*!
@property extFS
@abstract Convenience method to determine if a filesystem is Ext2 or Ext3.
@result YES if the filesystem is Ext2/3, otherwise NO.
*/
@property (getter=isExtFS, readonly) BOOL extFS;
/*!
@property uuidString
@abstract Get the filesystem UUID as a string.
@discussion This is only supported by Ext2/3 and UFS currrently.
@result String containing UUID or nil if a UUID is not present.
This may be nil if the media is not mounted.
*/
@property (readonly, copy) NSString *uuidString;
/*!
@property hasIndexedDirs
@abstract Determine if directory indexing is active.
@discussion Directory indexing is an Ext2/3 specific option.
It greatly speeds up file name lookup for large directories.
@result YES if indexing is active, otherwise NO.
Always NO if the media is not mounted or the filesystem is not Ext2/3.
*/
@property (readonly) BOOL hasIndexedDirs;
/*!
@property hasLargeFiles
@abstract Determine if the filesystem supports large files (> 2GB).
@discussion This only works with Ext2/3 filesystems currently.
@result YES if large files are supported, otherwise NO.
Always NO if the media is not mounted or the filesystem is not Ext2/3.
*/
@property (readonly) BOOL hasLargeFiles;
/*!
@property maxFileSize
@abstract Determine the max file size supported by the filesystem.
@discussion Currently, this method always returns 0.
@result Max file size or 0 if the filesystem is not mounted.
*/
@property (readonly) u_int64_t maxFileSize;

/*!
@property transportType
@abstract Determine how the device is connected.
@result An ExtFSConnectionType id.
*/
@property (readonly) ExtFSIOTransportType transportType;
/*!
@property transportBus
@abstract Determine the type of bus the device is connected to.
@result An ExtFSConnectionType id.
*/
@property (readonly) ExtFSIOTransportType transportBus;
/*!
@property transportName
@abstract Determine the name of the device bus.
@result A string containing a name suitable for display to a user
or nil if there was an error.
*/
@property (readonly, copy) NSString *transportName;

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
#define NSSTR(cstr) @(cstr)

#define EPROBE_KEY_NAME @"name"
#define EPROBE_KEY_UUID @"uuid"
#define EPROBE_KEY_JOURNALED @"journaled"
#define EPROBE_KEY_FSBLKSIZE @"fsBlockSize"
#define EPROBE_KEY_FSBLKCOUNT @"fsBlockCount"
#define EPROBE_KEY_BLESSED @"blessed"
