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

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ExtFSMedia.m,v 1.41 2006/10/14 20:47:56 bbergstrand Exp $";

#import <stdlib.h>
#import <unistd.h>
#import <sys/param.h>
#import <sys/ucred.h>
#import <sys/mount.h>
#import <sys/attr.h>
#import <sys/ioctl.h>
#import <sys/syscall.h>
#import <pthread.h>
#import <unistd.h>

#import <ext2_byteorder.h>
#ifndef NOEXT2
#import <gnu/ext2fs/ext2_fs.h>
#endif

#ifndef EFSM_PRIVATE
#define EFSM_PRIVATE
#endif
#import "ExtFSLock.h"
#import "ExtFSMedia.h"
#import "ExtFSMediaController.h"
#import "ExtFSMediaPrivate.h"

#import <IOKit/storage/IOMedia.h>
#import <IOKit/IOBSD.h>

NSString * const ExtFSMediaNotificationUpdatedInfo = @"ExtFSMediaNotificationUpdatedInfo";
NSString * const ExtFSMediaNotificationChildChange = @"ExtFSMediaNotificationChildChange";

struct attr_volinfo {
   size_t v_size;
   /* Fixed storage */
   u_long v_signature;
   off_t v_availspace;
   u_long v_filecount;
   u_long v_dircount;
   attrreference_t v_volref;
   vol_capabilities_attr_t v_caps;
   /* Variable storage */
   char v_name[256];
};

union volinfo {
   struct attr_volinfo vinfo;
   struct statfs vstat;
};
#define VOL_INFO_CACHE_TIME 15

static NSMutableDictionary *e_mediaIconCache = nil;
static void *e_mediaIconCacheLck = nil;

#ifndef SYS_fsctl
#define SYS_fsctl 242
#endif

#ifndef NOEXT2
struct superblock {
   struct ext2_super_block *s_es;
};
#define e2super e_sb
#define e2sblock e2super->s_es

// Must be called with the write lock held
#define e2super_alloc() \
do { \
   e_sb = malloc(sizeof(struct superblock)); \
   if (e_sb) { \
      e2sblock = malloc(sizeof(struct ext2_super_block)); \
      if (!e2sblock) { free(e_sb); e_sb = nil; } \
   } \
} while(0)

// Must be called with the write lock held
#define e2super_free() \
do { \
   if (e_sb) { free(e2sblock); free(e_sb); e_sb = nil; } \
} while(0)
#else
#define e2super_free()
#endif

#ifndef __HFS_FORMAT__
#define kHFSPlusSigWord 0x482B
#define kHFSXSigWord 0x4858
#endif

@implementation ExtFSMedia
{
@private
	id e_children;
	
	id e_object;
	NSString *e_uuid;
	struct superblock *e_sb;
#ifndef NOEXT2
	unsigned char e_reserved[32];
#endif

}
/* Private */

- (void)postNotification:(NSArray*)args
{
    NSDictionary *info = ([args count] == 2) ? args[1] : nil;
    [[NSNotificationCenter defaultCenter]
         postNotificationName:args[0] object:self userInfo:info];
}
#define EFSMPostNotification(note, info) do {\
NSArray *args = [[NSArray alloc] initWithObjects:note, info, nil]; \
[self performSelectorOnMainThread:@selector(postNotification:) withObject:args waitUntilDone:NO]; \
[args release]; \
} while(0)


- (int)fsInfoUsingCache:(BOOL)cache
{
   struct attrlist alist;
   union volinfo vinfo;
   struct timeval now;
   int err;
   char *path;
   
   path = MOUNTPOINTSTR(self);
   if (!path)
      return (EINVAL);
   
   #ifndef NOEXT2
   NSString *bsdName = [self bsdName];
   #endif
   
   gettimeofday(&now, nil);
   ewlock(e_lock);
   
   #ifndef NOEXT2
   /* Get the superblock if we don't have it */
   if ((fsTypeExt2 == e_fsType || fsTypeExt3 == e_fsType) && !e_sb) {
      e2super_alloc();
      if (e_sb)
         err = syscall(SYS_fsctl, path, EXT2_IOC_GETSBLOCK, e2sblock, 0);
      else
         err = ENOMEM;
      if (0 == err) {
         // Make sure our UUID copy is correct (ie a re-format since last mount)
         [e_uuid release];
         e_uuid = nil;
         eulock(e_lock);
         NSString *tmp = [self uuidString];
         ewlock(e_lock);
         e_uuid = [tmp retain];
      } else {
         E2Log(@"ExtFS: Failed to load superblock for device '%@' mounted on '%s' (%d).\n",
            bsdName, path, err);
         e2super_free();
      }
   }
   #endif
   
   if (cache && (e_lastFSUpdate + VOL_INFO_CACHE_TIME) > now.tv_sec) {
      eulock(e_lock);
      return (0);
   }
   e_lastFSUpdate = now.tv_sec;
   
   eulock(e_lock); // drop the lock while we do the I/O
   
   bzero(&vinfo, sizeof(vinfo));
   bzero(&alist, sizeof(alist));
   alist.bitmapcount = ATTR_BIT_MAP_COUNT;
   alist.volattr = ATTR_VOL_INFO|ATTR_VOL_SIGNATURE|ATTR_VOL_SPACEAVAIL|
      ATTR_VOL_FILECOUNT|ATTR_VOL_DIRCOUNT|ATTR_VOL_CAPABILITIES|ATTR_VOL_NAME;
   
   err = getattrlist(path, &alist, &vinfo.vinfo, sizeof(vinfo.vinfo), 0);
   if (!err) {
      ewlock(e_lock);
      e_fileCount = vinfo.vinfo.v_filecount;
      e_dirCount = vinfo.vinfo.v_dircount;
      e_blockAvail = vinfo.vinfo.v_availspace / e_fsBlockSize;
      if (0 != vinfo.vinfo.v_name[VOL_CAPABILITIES_FORMAT]) {
         [e_volName release];
         e_volName = [[NSString alloc] initWithUTF8String:vinfo.vinfo.v_name];
      }
      if (alist.volattr & ATTR_VOL_CAPABILITIES)
         e_volCaps = vinfo.vinfo.v_caps.capabilities[0];
      if (fsTypeHFS == e_fsType && kHFSPlusSigWord == vinfo.vinfo.v_signature)
            e_fsType = fsTypeHFSPlus;
      else if (fsTypeHFS == e_fsType && kHFSXSigWord == vinfo.vinfo.v_signature)
         e_fsType = fsTypeHFSX;
      eulock(e_lock);
      goto eminfo_exit;
   }
    #ifdef DIAGNOSTIC
    if (ENOENT == err)
        trap();
    #endif
   
   /* Fall back to statfs to get the info. */
   err = statfs(path, &vinfo.vstat);
   ewlock(e_lock);
   if (!err) {
      e_blockAvail = vinfo.vstat.f_bavail;
      e_fileCount = vinfo.vstat.f_files - vinfo.vstat.f_ffree;
   } else {
      e_lastFSUpdate = 0;
   }
   
   e_attributeFlags &= ~kfsGetAttrlist;
   eulock(e_lock);
   
eminfo_exit:
   if (!err) {
      EFSMPostNotification(ExtFSMediaNotificationUpdatedInfo, nil);
   }
   return (err);
}

- (int)fsInfo
{
    return ([self fsInfoUsingCache:YES]);
}

- (void)probe
{
    int type;
    NSString *uuid, *tmp;
     
    if (0 == (e_attributeFlags & kfsNoMount)) {
        // Query the raw disk for its filesystem type
        NSString *probePath;
        probePath = [[ExtFSMediaController mediaController] pathForResource:EFS_PROBE_RSRC];
        if (probePath) {
            NSTask *probe = [[NSTask alloc] init];
            NSPipe *output = [[NSPipe alloc] init];
            [probe setStandardOutput:output];
            [probe setArguments:@[[self bsdName]]];
            [probe setLaunchPath:probePath];
            @try {
            [probe launch];
            [probe waitUntilExit];
            type = [probe terminationStatus];
            } @catch (NSException *e) {
            // Launch failed
            type = -1;
            }
            if (type >= 0 && type < fsTypeNULL) {
                NSFileHandle *f;
                NSData *d;
                NSUInteger len;
                
                // See if there was any attributes output
                f = [output fileHandleForReading];
                uuid = nil;
                NSDictionary *plist = nil, *prevPlist = nil;
                @try {
                if ((d = [f availableData]) && (len = [d length]) > 0) {
                    plist = [NSPropertyListSerialization propertyListFromData:d
                        mutabilityOption:NSPropertyListImmutable format:NULL errorDescription:nil];
                    
                    uuid = [plist[EPROBE_KEY_UUID] retain];
                    if ([plist[EPROBE_KEY_JOURNALED] boolValue])
                        e_volCaps |= (VOL_CAP_FMT_JOURNAL|VOL_CAP_FMT_JOURNAL_ACTIVE);
                    else
                        e_volCaps &= ~(VOL_CAP_FMT_JOURNAL|VOL_CAP_FMT_JOURNAL_ACTIVE);
                    
                }
                } @catch (NSException *e) {}
                
                tmp = nil;
                ewlock(e_lock);
                e_fsType = type;
                if (uuid) {
                    tmp = e_uuid;
                    e_uuid = uuid;
                }
                if (plist) {
                    prevPlist = e_probedAttributes;
                    e_probedAttributes = [plist retain];
                }
                eulock(e_lock);
                [prevPlist release];
                [tmp release];
            }
            else
                E2DiagLog(@"ExtFS: efsprobe failed with '%d'.\n", type);
            [output release];
            [probe release];
        }
    }
}

/* Public */

- (ExtFSMedia*)initWithIORegProperties:(NSDictionary*)properties
{
   if ((self = [super init])) {
      if (nil == e_mediaIconCacheLck) {
         // Just to make sure someone else doesn't get in during creation...
         e_mediaIconCacheLck = (void*)E2_BAD_ADDR;
         if (0 != eilock(&e_mediaIconCacheLck)) {// This is never released
             E2Log(@"ExtFS: Failed to allocate media icon cache lock!\n");
            e_mediaIconCacheLck = nil;
init_err:
            [super release];
            return (nil);
         }
      }
      if (0 != eilock(&e_lock)) {
        E2Log(@"ExtFS: Failed to allocate media object lock!\n");
        goto init_err;
      }
      
      e_attributeFlags = kfsDiskArb | kfsGetAttrlist;
      e_bsdName = [properties[@kIOBSDNameKey] retain];
      e_fsType = fsTypeUnknown;
      [self updateProperties:properties];
      
      ewlock(e_mediaIconCacheLck);
      if (nil != e_mediaIconCache)
         (void)[e_mediaIconCache retain];
      else
         e_mediaIconCache = [[NSMutableDictionary alloc] init];
      eulock(e_mediaIconCacheLck);
   }
   return (self);
}

- (id)representedObject
{
   id tmp;
   erlock(e_lock);
   tmp = [e_object retain];
   eulock(e_lock);
   return ([tmp autorelease]);
}

- (void)setRepresentedObject:(id)object
{
   id tmp;
   (void)[object retain];
   ewlock(e_lock);
   tmp = e_object;
   e_object = object;
   eulock(e_lock);
   [tmp release];
}

- (ExtFSMedia*)parent
{
    erlock(e_lock);
    ExtFSMedia *p = [e_parent retain];
    eulock(e_lock);
    return ([p autorelease]);
}

- (NSArray*)children
{
   NSArray *c;
   erlock(e_lock);
   /* We have to return a copy, so the caller
      can use the array in a thread safe context.
    */
   c = [e_children copy];
   eulock(e_lock);
   return ([c autorelease]);
}

- (NSUInteger)childCount
{
   NSUInteger ct;
   erlock(e_lock);
   ct  = [e_children count];
   eulock(e_lock);
   return (ct);
}

- (void)addChild:(ExtFSMedia*)media
{
   NSString *myname = [self bsdName], *oname = [media bsdName];
   ewlock(e_lock);
   if (!e_children)
      e_children = [[NSMutableArray alloc] init];

   NSUInteger idx = [e_children indexOfObject:media];
   if (NSNotFound != idx) {
      (void)[media retain]; // Just in case the ptr is the same as that at idx
      e_children[idx] = media;
      eulock(e_lock);
      [media release];
      E2DiagLog(@"ExtFS: Oops! Parent '%@' already contains child '%@'.\n",
         myname, oname);
      return;
   };

   [e_children addObject:media];
   eulock(e_lock);
   EFSMPostNotification(ExtFSMediaNotificationChildChange, nil);
}

- (void)remChild:(ExtFSMedia*)media
{
   NSString *myname = e_bsdName, *oname = [media bsdName];
   ewlock(e_lock);
   NSUInteger idx = [e_children indexOfObject:media];
   if (nil == e_children || NSNotFound == idx) {
      eulock(e_lock);
      E2DiagLog(@"ExtFS: Oops! Parent '%@' does not contain child '%@'.\n",
         myname, oname);
      return;
   };

   [e_children removeObjectAtIndex:idx];
   eulock(e_lock);
   EFSMPostNotification(ExtFSMediaNotificationChildChange, nil);
}

- (NSString*)ioRegistryName
{
   NSString *name;
   erlock(e_lock);
   name = [e_ioregName retain];
   eulock(e_lock);
   return ([name autorelease]);
}

- (NSString*)bsdName
{
   return ([[e_bsdName retain] autorelease]);
}

- (NSImage*)icon
{
   NSString *bundleid, *iconName, *cacheKey;
   NSImage *ico;
   FSRef ref;
   int err;
   
   erlock(e_lock);
   if (e_icon) {
      eulock(e_lock);
      return ([[e_icon retain] autorelease]);
   }
   euplock(e_lock); // Upgrade to write lock
   
   if (e_parent && (e_icon = [e_parent icon])) {
      (void)[e_icon retain]; // For ourself
      eulock(e_lock);
      E2DiagLog(@"ExtFS: Retrieved icon %@ from %@ (%lu).\n", [e_icon name], e_parent, [e_icon retainCount]);
      return ([[e_icon retain] autorelease]);
   }
   
   if (e_attributeFlags & kfsIconNotFound) {
      eulock(e_lock);
      return (nil);
   }
   
   bundleid = e_iconDesc[(NSString*)kCFBundleIdentifierKey];
   iconName = e_iconDesc[@kIOBundleResourceFileKey];
   cacheKey = [NSString stringWithFormat:@"%@.%@", bundleid, [iconName lastPathComponent]];
   if (!bundleid || !iconName) {
      eulock(e_lock);
      E2DiagLog(@"ExtFS: Could not find icon path for %@.\n", [self bsdName]);
      return (nil);
   }
   
   /* Try the global cache */
   erlock(e_mediaIconCacheLck);
   if ((e_icon = e_mediaIconCache[cacheKey])) {
      (void)[e_icon retain]; // For ourself
      eulock(e_mediaIconCacheLck);
      eulock(e_lock);
      E2DiagLog(@"ExtFS: Retrieved icon %@ from icon cache (%lu).\n", cacheKey, [e_icon retainCount]);
      return ([[e_icon retain] autorelease]);
   }
   eulock(e_mediaIconCacheLck);
   
   /* Load the icon from disk */
   eulock(e_lock); // Drop the lock, as this can take many seconds
   ico = nil;
   if (noErr == (err = FSFindFolder (kLocalDomain,
      kKernelExtensionsFolderType, NO, &ref))) {
      CFURLRef url;
      
      url = CFURLCreateFromFSRef (nil, &ref);
      if (url) {
         CFArrayRef kexts;
         
         kexts = CFBundleCreateBundlesFromDirectory(nil, url, nil);
         if (kexts) {
            CFBundleRef iconBundle;
            
            iconBundle = CFBundleGetBundleWithIdentifier((CFStringRef)bundleid);
            if (iconBundle) {
               CFURLRef iconurl;
               
               iconurl = CFBundleCopyResourceURL(iconBundle,
                  (CFStringRef)[iconName stringByDeletingPathExtension],
                  (CFStringRef)[iconName pathExtension],
                  nil);
               if (iconurl) {
                  ico = [[NSImage alloc] initWithContentsOfURL:(NSURL*)iconurl];
                  CFRelease(iconurl);
               }
            }
            CFRelease(kexts);
         } /* kexts */
         CFRelease(url);
      } /* url */
   } /* FSFindFolder */
   
   ewlock(e_lock);
   // Now that we are under the lock again,
   // check to make sure no one beat us to the punch.
   if (nil != e_icon) {
      eulock(e_lock);
      [ico release];
      goto emicon_exit;
   }
   
   if (ico) {
      ewlock(e_mediaIconCacheLck);
      // Somebody else could have added us to the cache
      if (nil == (e_icon = e_mediaIconCache[cacheKey])) {
         e_icon = ico;
         [e_icon setName:cacheKey];
         e_mediaIconCache[cacheKey] = e_icon;
         E2DiagLog(@"ExtFS: Added icon %@ to icon cache (%lu).\n", cacheKey, [e_icon retainCount]);
      } else {
         // Use the cached icon instead of the one we found
         [ico release];
         (void)[e_icon retain];
      }
      eulock(e_mediaIconCacheLck);
   } else {
      e_attributeFlags |= kfsIconNotFound;
   }
   eulock(e_lock);

emicon_exit:
   return ([[e_icon retain] autorelease]);
}

- (BOOL)isEjectable
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsEjectable));
   eulock(e_lock);
   return (test);
}

- (BOOL)canMount
{
   erlock(e_lock);
   if ((e_attributeFlags & kfsNoMount)) {
      eulock(e_lock);
      return (NO);
   }
   eulock(e_lock);
   
   return (YES);
}

- (BOOL)isMounted
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsMounted));
   eulock(e_lock);
   return (test);
}

- (BOOL)isWritable
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsWritable));
   eulock(e_lock);
   return (test);
}

- (BOOL)isWholeDisk
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsWholeDisk));
   eulock(e_lock);
   return (test);
}

- (BOOL)isLeafDisk
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsLeafDisk));
   eulock(e_lock);
   return (test);
}

- (BOOL)isOptical
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsDVDROM)) || (0 != (e_attributeFlags & kfsCDROM));
   eulock(e_lock);
   return (test);
}

@synthesize opticalMediaType = e_opticalType;

- (BOOL)usesDiskArb
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsDiskArb));
   eulock(e_lock);
   return (test);
}

#ifdef notyet
- (void)setUsesDiskArb:(BOOL)diskarb
{
   ewlock(e_lock);
   if (diskarb)
      e_attributeFlags |= kfsDiskArb;
   else
      e_attributeFlags &= ~kfsDiskArb;
   eulock(e_lock);
}
#endif

- (u_int64_t)size
{
   return (e_size);
}

- (u_int32_t)blockSize
{
   u_int32_t sz;
   erlock(e_lock);
   sz = (e_attributeFlags & kfsMounted) ? e_fsBlockSize : e_devBlockSize;
   eulock(e_lock);
   return (sz);
}

- (ExtFSType)fsType
{
   return (e_fsType);
}

- (NSString*)fsName
{
    return (EFSNSPrettyNameFromType(e_fsType));
}

- (NSString*)mountPoint
{
    erlock(e_lock);
    NSString *s = [e_where retain];
    eulock(e_lock);
    return ([s autorelease]);
}

- (u_int64_t)availableSize
{
   u_int64_t sz;
   
   (void)[self fsInfo];
   
   erlock(e_lock);
   sz = e_blockAvail * e_fsBlockSize;
   eulock(e_lock);
   
   return (sz);
}

- (u_int64_t)blockCount
{
    erlock(e_lock);
    u_int64_t ct = e_blockCount ? e_blockCount : e_size / e_devBlockSize;
    eulock(e_lock);
    return (ct);
}

- (u_int64_t)fileCount
{
   (void)[self fsInfo];
   
   return (e_fileCount);
}

- (u_int64_t)dirCount
{
   erlock(e_lock);
   if (e_attributeFlags & kfsGetAttrlist) {
      eulock(e_lock);
      (void)[self fsInfo];
   } else {
      eulock(e_lock);
   }
      
   return (e_dirCount);
}

- (NSString*)volName
{
   erlock(e_lock);
   NSString *s = [e_volName retain];
   eulock(e_lock);
   return ([s autorelease]);
}

- (BOOL)hasPermissions
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_attributeFlags & kfsPermsEnabled));
   eulock(e_lock);
   return (test);
}

- (BOOL)hasJournal
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_volCaps & VOL_CAP_FMT_JOURNAL));
   eulock(e_lock);
   return (test);
}

- (BOOL)isJournaled
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_volCaps & VOL_CAP_FMT_JOURNAL_ACTIVE));
   eulock(e_lock);
   return (test);
}

- (BOOL)isCaseSensitive
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_volCaps & VOL_CAP_FMT_CASE_SENSITIVE));
   eulock(e_lock);
   return (test);
}

- (BOOL)isCasePreserving
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_volCaps & VOL_CAP_FMT_CASE_PRESERVING));
   eulock(e_lock);
   return (test);
}

- (BOOL)isBlessed
{
   BOOL test;
   erlock(e_lock);
   test = [e_probedAttributes[EPROBE_KEY_BLESSED] boolValue];
   eulock(e_lock);
   return (test);
}

- (BOOL)hasSparseFiles
{
   BOOL test;
   erlock(e_lock);
   test = (0 != (e_volCaps & VOL_CAP_FMT_SPARSE_FILES));
   eulock(e_lock);
   return (test);
}

#ifdef notyet
- (BOOL)hasSuper
{
   BOOL test;
   erlock(e_lock);
   test = (nil != e_sb);
   eulock(e_lock);
   return (test);
}
#endif

/* Private (for now) -- caller must release returned object */
- (CFUUIDRef)uuid
{
   CFUUIDRef uuid = nil;
   #ifndef NOEXT2
   CFUUIDBytes *bytes;
   
   erlock(e_lock);
   if (e_sb) {
      bytes = (CFUUIDBytes*)&e2sblock->s_uuid[0];
      uuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *bytes);
   }
   eulock(e_lock);
   #endif
   
   return (uuid);
}

- (BOOL)isExtFS
{
   BOOL test;
   erlock(e_lock);
   test = (e_fsType == fsTypeExt2 || e_fsType == fsTypeExt3);
   eulock(e_lock);
   return (test);
}

- (NSString*)uuidString
{
   CFUUIDRef uuid;
   NSString *str;
   
   uuid = [self uuid];
   if (uuid) {
      str = (NSString*)CFUUIDCreateString(kCFAllocatorDefault, uuid);
      CFRelease(uuid);
      return ([str autorelease]);
   } else if (e_uuid) {
      return ([[e_uuid retain] autorelease]);
   }
   
   return (nil);
}

- (BOOL)hasIndexedDirs
{
   BOOL test = NO;
   #ifndef NOEXT2
   erlock(e_lock);
   if (e_sb)
      test = (0 != EXT3_HAS_COMPAT_FEATURE(e2super, EXT3_FEATURE_COMPAT_DIR_INDEX));
   eulock(e_lock);
   #endif
   return (test);
}

- (BOOL)hasLargeFiles
{
   BOOL test = NO;
   #ifndef NOEXT2
   erlock(e_lock);
   if (e_sb)
      test = (0 != EXT2_HAS_RO_COMPAT_FEATURE(e2super, EXT2_FEATURE_RO_COMPAT_LARGE_FILE));
   eulock(e_lock);
   #endif
   return (test);
}

- (u_int64_t)maxFileSize
{
    return (0);
}

- (ExtFSIOTransportType)transportType
{
    return (e_ioTransport & efsIOTransportTypeMask);
}

- (ExtFSIOTransportType)transportBus
{
    return (e_ioTransport & efsIOTransportBusMask);
}

- (NSString*)transportName
{
    return (EFSIOTransportNameFromType((e_ioTransport & efsIOTransportBusMask)));
}

- (NSComparisonResult)compare:(ExtFSMedia *)media
{
   return ((self != media) ? [[self bsdName] compare:[media bsdName]] : NSOrderedSame);
}

/* Super */

- (NSString*)description
{
   return ([NSString stringWithFormat:@"<%@, %p>", e_bsdName, self]);
}

- (NSUInteger)hash
{
   return ([e_bsdName hash]);
}

- (BOOL)isEqual:(id)obj
{
   if ([obj respondsToSelector:@selector(bsdName)])
      return ((self == obj) || [e_bsdName isEqualToString:[obj bsdName]]);
   
   return (NO);
}

- (void)dealloc
{
   NSUInteger count;
   E2DiagLog(@"ExtFS: Media <%@, %p> dealloc.\n",
      e_media[@kIOBSDNameKey], self);
   
   // Icon cache
   ewlock(e_mediaIconCacheLck);
   count = [e_mediaIconCache retainCount];
   [e_mediaIconCache release];
   if (1 == count)
      e_mediaIconCache = nil;
   eulock(e_mediaIconCacheLck);
   // The mediaIconCache lock is never released.
   
   e2super_free();
   [e_object release];
   [e_icon release];
   [e_iconDesc release];
   [e_ioregName release];
   [e_volName release];
   [e_where release];
   [e_media release];
   [e_uuid release];
   [e_children release];
   [e_parent release];
   [e_probedAttributes release];
   [e_bsdName release];
   
   if (e_smartService)
      IOObjectRelease((io_service_t)e_smartService);
   
   edlock(e_lock);
   
   [super dealloc];
}

@end

@implementation ExtFSMedia (ExtFSMediaLockMgr)

- (void)lock:(BOOL)exclusive
{
    exclusive ? ewlock(e_lock) : erlock(e_lock);
}

- (void)unlock
{
    eulock(e_lock);
}

@end
