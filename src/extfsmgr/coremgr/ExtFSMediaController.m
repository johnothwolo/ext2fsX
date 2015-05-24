/*
* Copyright 2003-2004,2006 Brian Bergstrand.
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
"@(#) $Id: ExtFSMediaController.m,v 1.46 2006/11/28 22:13:43 bbergstrand Exp $";

#import <stdlib.h>
#import <string.h>
#import <sys/param.h>
#import <sys/ucred.h>
#import <sys/mount.h>

#import <mach/mach_port.h>
#import <mach/mach_interface.h>
#import <mach/mach_init.h>

#include <DiskArbitration/DiskArbitration.h>

#import <IOKit/IOMessage.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOCDMedia.h>
#import <IOKit/storage/IODVDMedia.h>
#import <IOKit/storage/IOStorageProtocolCharacteristics.h>

#import "ExtFSLock.h"
#import "ExtFSMedia.h"
#import "ExtFSMediaController.h"
#import "ExtFSMediaPrivate.h"

NSString * const ExtFSMediaNotificationAppeared = @"ExtFSMediaNotificationAppeared";
NSString * const ExtFSMediaNotificationDisappeared = @"ExtFSMediaNotificationDisappeared";
NSString * const ExtFSMediaNotificationMounted = @"ExtFSMediaNotificationMounted";
NSString * const ExtFSMediaNotificationUnmounted = @"ExtFSMediaNotificationUnmounted";
NSString * const ExtFSMediaNotificationCreationFailed = @"ExtFSMediaNotificationCreationFailed";
NSString * const ExtFSMediaNotificationOpFailure = @"ExtFSMediaNotificationOpFailure";
NSString * const ExtFSMediaNotificationClaimRequestDidComplete = @"ExtFSMediaNotificationClaimRequestDidComplete";
NSString * const ExtFSMediaNotificationDidReleaseClaim = @"ExtFSMediaNotificationDidReleaseClaim";
NSString * const ExtFSMediaNotificationApplicationWillMount = @"ExtFSMediaNotificationApplicationWillMount";
NSString * const ExtFSMediaNotificationApplicationWillUnmount = @"ExtFSMediaNotificationApplicationWillUnmount";

static ExtFSMediaController *e_instance;
static void* e_instanceLock = nil; // Ptr to global controller internal lock
static pthread_mutex_t e_initMutex = PTHREAD_MUTEX_INITIALIZER;
static IONotificationPortRef notify_port_ref=0;
static io_iterator_t notify_add_iter=0, notify_rem_iter=0;
static DASessionRef daSession = NULL;
static DAApprovalSessionRef daApprovalSession = NULL;

#ifdef EFSM_GLOBAL_DEVICE_ALLOW
static NSMutableSet *deviceMountBlockList = nil;
__private_extern__ void
EFSMBlockMountOfDevice(NSString *device)
{
    ewlock(e_instanceLock);
    [deviceMountBlockList addObject:device];
    eulock(e_instanceLock);
}

__private_extern__ void
EFSMAllowMountOfDevice(NSString *device)
{
    ewlock(e_instanceLock);
    [deviceMountBlockList removeObject:device];
    eulock(e_instanceLock);
}
#endif

static const char *e_fsNames[] = {
   EXT2FS_NAME,
   EXT3FS_NAME,
   HFS_NAME,
   HFS_NAME,
   HFS_NAME,
   HFS_NAME,
   UFS_NAME,
   CD9660_NAME,
   CDAUDIO_NAME,
   UDF_NAME,
   MSDOS_NAME,
   NTFS_NAME,
   "Unknown",
   nil
};

__private_extern__ NSDictionary *transportNameTypeMap = nil;

static void iomatch_add_callback(void *refcon, io_iterator_t iter);
static void iomatch_rem_callback(void *refcon, io_iterator_t iter);
static void DiskArbCallback_MountNotification(DADiskRef disk,
   DADissenterRef dissenter, void * context);
static void DiskArbCallback_UnmountNotification(DADiskRef disk,
   DADissenterRef dissenter, void * context);
static void DiskArbCallback_EjectNotification(DADiskRef disk,
   DADissenterRef dissenter, void * context);
static void DiskArbCallback_PathChangeNotification(DADiskRef disk,
   CFArrayRef keys, void * context);
static void DiskArbCallback_NameChangeNotification(DADiskRef disk,
   CFArrayRef keys, void * context);
static void DiskArbCallback_AppearedNotification(DADiskRef disk, void *context);
static DADissenterRef DiskArbCallback_ApproveMount(DADiskRef disk, void *context);
static void DiskArbCallback_Claimed(DADiskRef disk, DADissenterRef dissenter, void *context);
static DADissenterRef DiskArbCallback_ClaimRelease(DADiskRef disk, void *context);

static void DiskArb_CallFailed(const char *device, int type, int status);

enum {
    kPendingMounts = 0,
    kPendingUMounts = 1,
    kPendingCount
};


// Pre-Tiger Disk arb CallFailed types
#define kDiskArbUnmountAndEjectRequestFailed 1
#define kDiskArbUnmountRequestFailed 2
#define kDiskArbEjectRequestFailed 3
#define kDiskArbDiskChangeRequestFailed 4
#define EXT_DISK_ARB_MOUNT_FAILURE 5

#define EXT2_DISK_EJECT E2_BAD_ADDR

#define EXT_MOUNT_ERROR_DELAY 30.0

static NSDictionary *opticalMediaTypes = nil;
static NSDictionary *opticalMediaNames = nil;

@implementation ExtFSMediaController : NSObject

/* Private */

- (void)updateMountStatus
{
   int count, i;
   struct statfs *stats;
   NSString *device;
   ExtFSMedia *e2media;
   NSMutableArray *pMounts;
   
   count = getmntinfo(&stats, MNT_WAIT);
   if (!count)
      return;
   
   ewlock(e_lock);
   pMounts = [e_pending objectAtIndex:kPendingMounts];
   for (i = 0; i < count; ++i) {
      device = [[NSString stringWithUTF8String:stats[i].f_mntfromname] lastPathComponent];      
      e2media = [e_media objectForKey:device];
      if (!e2media)
         continue;
      
      [pMounts removeObject:e2media];
      
      [e2media setIsMounted:&stats[i]];
   }
   eulock(e_lock);
}

- (BOOL)volumeDidUnmount:(NSString*)device
{
   ewlock(e_lock);
   ExtFSMedia *e2media = [e_media objectForKey:device];
      NSMutableArray *pUMounts = [e_pending objectAtIndex:kPendingUMounts];
   BOOL isMounted = ([e2media isMounted] || [pUMounts containsObject:e2media]);
   if (e2media && isMounted) {
      [pUMounts removeObject:e2media];
      eulock(e_lock);
      [e2media setIsMounted:nil];
      return (YES);
   }
   eulock(e_lock);
#ifdef DIAGNOSTIC
   if (nil == e2media)
      E2Log(@"ExtFS: Oops! Received unmount for an unknown device: '%@'.\n", device);
   else if (NO == isMounted)
      E2Log(@"ExtFS: Oops! Received unmount for a device that is already unmounted: '%@'.\n", device);
#endif
   return (NO);
}

#define rmmedia(m, dev) do { \
ExtFSMedia *parent; \
if ((parent = [m parent])) \
    [parent remChild:m]; \
ewlock(e_lock); \
[e_media removeObjectForKey:dev]; \
eulock(e_lock); \
[self removePending:m]; \
} while(0)

- (ExtFSMedia*)createMediaWithIOService:(io_service_t)service properties:(NSDictionary*)props
{
   ExtFSMedia *e2media = [[ExtFSMedia alloc] initWithIORegProperties:props];
   if (e2media) {
      // It's possible for us to get in a race condition where we are created from a device that
      // has disappeared by the time we get to this point. updateAttributesFromIOService
      // will detect this and return false if the race occurs.
      BOOL good = [e2media updateAttributesFromIOService:service];
      if (!good) {
         [e2media release];
         return (nil);
      }
      NSString *device = [e2media bsdName];
      ewlock(e_lock);
      #ifdef DIAGNOSTIC
      if ([e_media objectForKey:device])
        trap();
      #endif
      [e_media setObject:e2media forKey:device];
      eulock(e_lock);
      
      E2DiagLog(@"ExtFS: Media %@ created with parent %@.\n", e2media, [e2media parent]);
      
      EFSMCPostNotification(ExtFSMediaNotificationAppeared, e2media, nil);
      [e2media release];
   } else {
      EFSMCPostNotification(ExtFSMediaNotificationCreationFailed,
        [props objectForKey:NSSTR(kIOBSDNameKey)], nil);
   }
   return (e2media);
}

- (void)removeMedia:(ExtFSMedia*)e2media device:(NSString *)device
{
  if ([e2media isMounted])
      E2DiagLog(@"ExtFS: Oops! Media '%@' removed while still mounted!\n", device);
   
   (void)[e2media retain];
   rmmedia(e2media, device);

   EFSMCPostNotification(ExtFSMediaNotificationDisappeared, e2media, nil);
   
   E2DiagLog(@"ExtFS: Media '%@' removed. Retain count = %lu.\n",
      e2media, (unsigned long)[e2media retainCount]);
   [e2media release];
}

- (int)updateMedia:(io_iterator_t)iter remove:(BOOL)remove
{
   io_service_t iomedia;
   kern_return_t kr = 0;
   ExtFSMedia *e2media;
   CFMutableDictionaryRef properties;
   NSString *device;
   NSMutableArray *needProbe;
   
    if (!remove)
        needProbe = [NSMutableArray array];
    else
        needProbe = nil;
   
   /* Init all media devices */
   while ((iomedia = IOIteratorNext(iter))) {
      kr = IORegistryEntryCreateCFProperties(iomedia, &properties,
         kCFAllocatorDefault, 0);
      if (0 == kr) {
         device = [(NSMutableDictionary*)properties objectForKey:NSSTR(kIOBSDNameKey)];
         erlock(e_lock);
         e2media = [[e_media objectForKey:device] retain];
         eulock(e_lock);
         if (e2media) {
            if (remove)
               [self removeMedia:e2media device:device];
            else {
               E2DiagLog(@"ExtFS: Existing media %@ appeared again.\n", e2media);
               [e2media updateProperties:(NSDictionary*)properties];
               #ifdef DIAGNOSTIC
               erlock(e_lock);
               kr = (e2media == [e_media objectForKey:device]) ? 0 : 1;
               eulock(e_lock);
               if (kr)
                  trap();
               #endif
               if ([e2media updateAttributesFromIOService:iomedia]) {
                   EFSMCPostNotification(ExtFSMediaNotificationUpdatedInfo, e2media, nil);
                   [needProbe addObject:e2media];
               } else {
                   // most likely, the service handle is simply stale, so don't rem
                   // [self removeMedia:e2media device:device];
                   E2DiagLog(@"ExtFS: failed to update existing media %@!\n", e2media);
               }
            
                [e2media release];
                CFRelease(properties);
                IOObjectRelease(iomedia);
                continue;
            }
         }
         
         if (!remove) {
            if ((e2media = [self createMediaWithIOService:iomedia properties:(NSDictionary*)properties]))
                [needProbe addObject:e2media];
         }
         
         CFRelease(properties);
      }
      IOObjectRelease(iomedia);
   }
   
   // XXX re-entry point (as NSTask can be called and re-enter the run loop)
   [needProbe makeObjectsPerformSelector:@selector(probe)];
   
   [self updateMountStatus];
   
   return (0);
}

- (void)mountDidTimeOut:(NSTimer*)timer
{
    NSMutableArray *pMounts;
    ExtFSMedia *media = [timer userInfo];
    
    erlock(e_lock);
    pMounts = [e_pending objectAtIndex:kPendingMounts];
    if ([pMounts containsObject:media]) {
        eulock(e_lock);
        // Send an error - this also clears the pending state
        DiskArb_CallFailed(BSDNAMESTR(media),
            EXT_DISK_ARB_MOUNT_FAILURE, ETIMEDOUT);
        return;
    }
    eulock(e_lock);
}

- (void)removePending:(ExtFSMedia*)media
{
    NSMutableArray *pMounts;
    NSMutableArray *pUMounts;
    
    ewlock(e_lock);
    pMounts = [e_pending objectAtIndex:kPendingMounts];
    pUMounts = [e_pending objectAtIndex:kPendingUMounts];
   
    E2DiagLog(@"Attempting to remove %@ from pending states.\n", media);
    [pMounts removeObject:media];
    [pUMounts removeObject:media];
    eulock(e_lock);
}

- (BOOL)allowMount:(NSString*)device
{
    erlock(e_lock);
    id delegate = [e_delegate retain];
    eulock(e_lock);
    
    (void)[delegate autorelease];
    if (delegate && [delegate respondsToSelector:@selector(allowMediaToMount:)]) {
        ExtFSMedia *media = [self mediaWithBSDName:device];
        if (!media) {
            // XXX we can get race condtions between IOKit and DiskArb (or even various DiskArb callbacks)
            media = [[[ExtFSMedia alloc] initWithDeviceName:device] autorelease];
        }
            return ([delegate allowMediaToMount:media]);
    }
    
    return (YES);
}

- (void)claimDidCompleteWithDevice:(NSString*)device error:(int)error
{
    ExtFSMedia *media = [self mediaWithBSDName:device];
    if (media) {
        if (!error)
            [media setClaimedExclusive:YES];
        
        E2DiagLog(@"exclusive claim of '%@' %s\n", device, !error ? "granted" : "denied");
        
        NSDictionary *d = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithInt:error], ExtMediaKeyOpFailureError,
            nil];
        
        EFSMCPostNotification(ExtFSMediaNotificationClaimRequestDidComplete, media, d);
    }
}

- (BOOL)releaseClaimForDevice:(NSString*)device
{
    ExtFSMedia *media = [self mediaWithBSDName:device];
    if (media) {
        BOOL allow = ![media claimedExclusive];
        E2DiagLog(@"exclusive claim for '%@' %s\n", device, allow ? "released" : "retained");
        return (allow);
    }
    return (YES);
}

- (void)nameOfDevice:(NSString*)device didChangeTo:(NSString*)name
{
    ExtFSMedia *media = [self mediaWithBSDName:device];
    if (media) {
        (void)[media fsInfoUsingCache:NO];
    }
}

/* Public */

+ (ExtFSMediaController*)mediaController
{
   if (!e_instance) {
      (void)[[ExtFSMediaController alloc] init];
   }
   
   return (e_instance);
} 

- (NSUInteger)mediaCount
{
   NSUInteger ct;
   erlock(e_lock);
   ct = [e_media count];
   eulock(e_lock);
   return (ct);
}

- (NSArray*)media
{
   NSArray *m;
   erlock(e_lock);
   m = [e_media allValues];
   eulock(e_lock);
   return (m);
}

- (NSArray*)rootMedia
{
   NSMutableArray *array;
   NSEnumerator *en;
   ExtFSMedia *e2media;
   
   array = [NSMutableArray array];
   erlock(e_lock);
   en = [e_media objectEnumerator];
   while ((e2media = [en nextObject])) {
      if (nil == [e2media parent]) {
         [array addObject:e2media];
      }
   }
   eulock(e_lock);
   
   return (array);
}

- (NSArray*)mediaWithFSType:(ExtFSType)fstype
{
   NSMutableArray *array;
   NSEnumerator *en;
   ExtFSMedia *e2media;
   
   array = [NSMutableArray array];
   erlock(e_lock);
   en = [e_media objectEnumerator];
   while ((e2media = [en nextObject])) {
      if (fstype == [e2media fsType]) {
         [array addObject:e2media];
      }
   }
   eulock(e_lock);
   
   return (array);
}

- (NSArray*)mediaWithIOTransportBus:(ExtFSIOTransportType)busType
{
   NSMutableArray *array;
   NSEnumerator *en;
   ExtFSMedia *e2media;
   
   array = [NSMutableArray array];
   erlock(e_lock);
   en = [e_media objectEnumerator];
   while ((e2media = [en nextObject])) {
      if (busType == [e2media transportBus]) {
         [array addObject:e2media];
      }
   }
   eulock(e_lock);
   
   return (array);
}

- (ExtFSMedia*)mediaWithBSDName:(NSString*)device
{
   ExtFSMedia *m;
   erlock(e_lock);
   m = [[e_media objectForKey:device] retain];
   eulock(e_lock);
   return ([m autorelease]);
}

- (int)mount:(ExtFSMedia*)media on:(NSString*)dir
{
   NSMutableArray *pMounts;
   NSMutableArray *pUMounts;
   kern_return_t ke;
   
   ewlock(e_lock);
   pMounts = [e_pending objectAtIndex:kPendingMounts];
   pUMounts = [e_pending objectAtIndex:kPendingUMounts];
   if ([pMounts containsObject:media] || [pUMounts containsObject:media]) {
        eulock(e_lock);
        E2DiagLog(@"ExtFS: Can't mount '%@'. Operation is already in progress.",
            [media bsdName]);
        return (EINPROGRESS);
   }
   
   [pMounts addObject:media];// Add it while we are under the lock.
   eulock(e_lock);
   
   DADiskRef dadisk = DADiskCreateFromBSDName(kCFAllocatorDefault, daSession, BSDNAMESTR(media));
   if (dadisk) {
      ke = 0;
      EFSMCPostNotification(ExtFSMediaNotificationApplicationWillMount, media, nil);
      NSURL *url = dir ? [NSURL fileURLWithPath:dir] : nil;
      DADiskMount(dadisk, (CFURLRef)url, kDADiskMountOptionDefault, DiskArbCallback_MountNotification, NULL);
      CFRelease(dadisk);
   } else
      ke = ENOENT;
   if (0 == ke) {
        (void)[NSTimer scheduledTimerWithTimeInterval:EXT_MOUNT_ERROR_DELAY
            target:self selector:@selector(mountDidTimeOut:) userInfo:media
            repeats:NO];
   } else {
      // Re-acquire lock and remove it from pending
      ewlock(e_lock);
      if ([pMounts containsObject:media])
         [pMounts removeObject:media];
      eulock(e_lock);
   }
   return (ke);
}

- (int)unmount:(ExtFSMedia*)media force:(BOOL)force eject:(BOOL)eject
{
   int flags = kDADiskUnmountOptionDefault;
   NSMutableArray *pMounts;
   NSMutableArray *pUMounts;
   kern_return_t ke = 0;
   
   if (force)
      flags |= kDADiskUnmountOptionForce;
   
   if (![media isEjectable] && eject)
      eject = NO;
   
   if (eject) {
        // Find root media
        ExtFSMedia *parent = media;
        while ((parent = [parent parent])) {
            if ([parent isWholeDisk]) {
                media = parent;
                parent = nil;
                break;
            }
        }
        flags |= kDADiskUnmountOptionWhole;
   }
   
   ewlock(e_lock);
   pMounts = [e_pending objectAtIndex:kPendingMounts];
   pUMounts = [e_pending objectAtIndex:kPendingUMounts];
   if ([pMounts containsObject:media] || [pUMounts containsObject:media]) {
        eulock(e_lock);
        E2DiagLog(@"ExtFS: Can't unmount '%@'. Operation is already in progress.",
            [media bsdName]);
        return (EINPROGRESS);
   }
   
   [pUMounts addObject:media];
   eulock(e_lock);
   
   DADiskRef dadisk = DADiskCreateFromBSDName(kCFAllocatorDefault, daSession, BSDNAMESTR(media));
   if (!dadisk) {
      ke = ENOENT;
      goto unmount_failed;
   }
   
   if ([media isMounted] || (nil != [media children])) {
      EFSMCPostNotification(ExtFSMediaNotificationApplicationWillUnmount, media, nil);
      DADiskUnmount(dadisk, flags, DiskArbCallback_UnmountNotification, (eject ? (void*)EXT2_DISK_EJECT : NULL));
   } else if (eject)
      DADiskEject(dadisk, kDADiskEjectOptionDefault, DiskArbCallback_EjectNotification, NULL);
   else
      ke = EINVAL;
   
   CFRelease(dadisk);
   
unmount_failed:
    if (ke) {
        [self removePending:media];
    }
   
   return (ke);
}

- (void)claimMedia:(ExtFSMedia*)media
{
    DADiskRef dadisk = DADiskCreateFromBSDName(kCFAllocatorDefault, daSession, BSDNAMESTR(media));
    if (!dadisk) {
        return;
    }
    
    DADiskClaim(dadisk, kDADiskClaimOptionDefault, DiskArbCallback_ClaimRelease, NULL,
        DiskArbCallback_Claimed, NULL);
    
    CFRelease(dadisk);
}

- (void)releaseClaimOnMedia:(ExtFSMedia*)media
{
    if (![media claimedExclusive])
        return;
        
    DADiskRef dadisk = DADiskCreateFromBSDName(kCFAllocatorDefault, daSession, BSDNAMESTR(media));
    if (!dadisk) {
        return;
    }
    [media setClaimedExclusive:NO];
    
    DADiskUnclaim(dadisk);
    CFRelease(dadisk);
    
    EFSMCPostNotification(ExtFSMediaNotificationDidReleaseClaim, media, nil);
}

- (ExtFSOpticalMediaType)opticalMediaTypeForName:(NSString*)name
{
    return ([[opticalMediaTypes objectForKey:name] intValue]);
}

- (NSString*)opticalMediaNameForType:(ExtFSOpticalMediaType)type
{
    return ([opticalMediaNames objectForKey:[NSNumber numberWithInt:type]]);
}

- (id)delegate
{
    erlock(e_lock);
    id obj = [e_delegate retain];
    eulock(e_lock);
    return ([obj autorelease]);
}

- (void)setDelegate:(id)obj
{
    id tmp = nil;
    ewlock(e_lock);
    if (obj != e_delegate) {
        tmp = e_delegate;
        e_delegate = [obj retain];
    }
    eulock(e_lock);
    [tmp release];
}

#ifdef DIAGNOSTIC
- (void)dumpState
{
    NSMutableString *state = [NSMutableString string];
    
    [state appendString:@"\n== EFSM STATE ==\n"];
    [state appendString:@"media = {\n"];
    
    NSEnumerator *en;
    erlock(e_lock);
    en = [[[e_media allValues] sortedArrayUsingSelector:@selector(compare:)] objectEnumerator];
    eulock(e_lock);
    ExtFSMedia *m;
    while ((m = [en nextObject])) {
        [state appendFormat:@"\t'%@', children = %@\n", m, [m children]];
    }
    
    [state appendString:@"}\n"];
    
    erlock(e_lock);
    [state appendFormat:@"pending mounts = %@\n", [e_pending objectAtIndex:kPendingMounts]];
    [state appendFormat:@"pending unmounts = %@\n", [e_pending objectAtIndex:kPendingUMounts]];
    #ifdef EFSM_GLOBAL_DEVICE_ALLOW
    [state appendFormat:@"denied mounts = %@\n", deviceMountBlockList];
    #endif
    eulock(e_lock);
    
    [state appendString:@"== EFSM STATE END ==\n"];
    
    E2Log(@"%@", state);
}
#endif

/* Super */

- (void)processDiskNotifications:(id)arg
{
    pthread_mutex_lock(&e_initMutex);
    
    CFRunLoopSourceRef rlSource = IONotificationPortGetRunLoopSource(notify_port_ref);
    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CFRunLoopAddSource([[NSRunLoop currentRunLoop] getCFRunLoop], rlSource,
        kCFRunLoopCommonModes);
    
    /* Process the initial registry entrires */
    iomatch_add_callback(nil, notify_add_iter);
    iomatch_rem_callback(nil, notify_rem_iter);

    /* Init Disk Arb */
    if (NULL == (daSession = DASessionCreate(kCFAllocatorDefault))) {
        pthread_mutex_unlock(&e_initMutex);
        [NSThread exit];
    }
    DASessionScheduleWithRunLoop(daSession, [[NSRunLoop currentRunLoop] getCFRunLoop], kCFRunLoopCommonModes);

    // unmount detection
    DARegisterDiskDescriptionChangedCallback(daSession, NULL, kDADiskDescriptionWatchVolumePath,
        DiskArbCallback_PathChangeNotification, NULL);
    // mount detection
    DARegisterDiskAppearedCallback(daSession, NULL, DiskArbCallback_AppearedNotification, NULL);
    // rename detection
    DARegisterDiskDescriptionChangedCallback(daSession, NULL, kDADiskDescriptionWatchVolumeName,
        DiskArbCallback_NameChangeNotification, NULL);
    
    // approval callbacks
    if ((daApprovalSession = DAApprovalSessionCreate(kCFAllocatorDefault))) {
        DAApprovalSessionScheduleWithRunLoop(daApprovalSession, [[NSRunLoop currentRunLoop] getCFRunLoop],
            kCFRunLoopCommonModes);
        DARegisterDiskMountApprovalCallback(daApprovalSession, kDADiskDescriptionMatchVolumeMountable, DiskArbCallback_ApproveMount, NULL);
    } else {
        E2Log(@"ExtFS: Failed to create DAApprovalSession!\n");
    }
    
    pthread_mutex_unlock(&e_initMutex);
    
    [pool release];
    
    pool = [[NSAutoreleasePool alloc] init];
    [[NSRunLoop currentRunLoop] run];
    [pool release];
}

- (id)init
{
   kern_return_t kr = 0;
   id obj;
   int i;
   
   pthread_mutex_lock(&e_initMutex);
   
   if (e_instance) {
      pthread_mutex_unlock(&e_initMutex);
      [super release];
      return (e_instance);
   }
   
   if (!(self = [super init])) {
      pthread_mutex_unlock(&e_initMutex);
      return (nil);
   }
      
   if (0 != eilock(&e_lock)) {
      pthread_mutex_unlock(&e_initMutex);
      E2DiagLog(@"ExtFS: Failed to allocate lock for media controller!\n");
      [super release];
      return (nil);
   }
   
   e_instance = self;
    
    opticalMediaTypes = [[NSDictionary alloc] initWithObjectsAndKeys:
        [NSNumber numberWithInt:efsOpticalTypeCD], NSSTR(kIOCDMediaTypeROM),
        [NSNumber numberWithInt:efsOpticalTypeCDR], NSSTR(kIOCDMediaTypeR),
        [NSNumber numberWithInt:efsOpticalTypeCDRW], NSSTR(kIOCDMediaTypeRW),
        [NSNumber numberWithInt:efsOpticalTypeDVD], NSSTR(kIODVDMediaTypeROM),
        [NSNumber numberWithInt:efsOpticalTypeDVDDashR], NSSTR(kIODVDMediaTypeR),
        [NSNumber numberWithInt:efsOpticalTypeDVDDashRW], NSSTR(kIODVDMediaTypeRW),
        [NSNumber numberWithInt:efsOpticalTypeDVDPlusR], NSSTR(kIODVDMediaTypePlusR),
        [NSNumber numberWithInt:efsOpticalTypeDVDPlusRW], NSSTR(kIODVDMediaTypePlusRW),
        [NSNumber numberWithInt:efsOpticalTypeDVDRAM], NSSTR(kIODVDMediaTypeRAM),
        nil];
        
    NSBundle *me = [NSBundle bundleWithIdentifier:EXTFS_DM_BNDL_ID];
    opticalMediaNames = [[NSDictionary alloc] initWithObjectsAndKeys:
        NSSTR(kIOCDMediaTypeROM), [NSNumber numberWithInt:efsOpticalTypeCD],
        NSSTR(kIOCDMediaTypeR), [NSNumber numberWithInt:efsOpticalTypeCDR],
        NSSTR(kIOCDMediaTypeRW), [NSNumber numberWithInt:efsOpticalTypeCDRW],
        NSSTR(kIODVDMediaTypeROM), [NSNumber numberWithInt:efsOpticalTypeDVD],
        NSSTR(kIODVDMediaTypeR), [NSNumber numberWithInt:efsOpticalTypeDVDDashR],
        NSSTR(kIODVDMediaTypeRW), [NSNumber numberWithInt:efsOpticalTypeDVDDashRW],
        NSSTR(kIODVDMediaTypePlusR), [NSNumber numberWithInt:efsOpticalTypeDVDPlusR],
        NSSTR(kIODVDMediaTypePlusRW), [NSNumber numberWithInt:efsOpticalTypeDVDPlusRW],
        NSSTR(kIODVDMediaTypeRAM), [NSNumber numberWithInt:efsOpticalTypeDVDRAM],
        [me localizedStringForKey:@"Unknown Optical Disc" value:nil table:nil], [NSNumber numberWithInt:efsOpticalTypeUnknown],
        nil];
    
    transportNameTypeMap = [[NSDictionary alloc] initWithObjectsAndKeys:
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeInternal],
            NSSTR(kIOPropertyInternalKey),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeExternal],
            NSSTR(kIOPropertyExternalKey),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeInternal|efsIOTransportTypeExternal],
            NSSTR(kIOPropertyInternalExternalKey),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeVirtual],
            NSSTR(kIOPropertyInterconnectFileKey),
        
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeATA],
            NSSTR(kIOPropertyPhysicalInterconnectTypeATA),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeATAPI],
            NSSTR(kIOPropertyPhysicalInterconnectTypeATAPI),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeFirewire],
            NSSTR(kIOPropertyPhysicalInterconnectTypeFireWire),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeUSB],
            NSSTR(kIOPropertyPhysicalInterconnectTypeUSB),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeSCSI],
            NSSTR(kIOPropertyPhysicalInterconnectTypeSCSIParallel),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeSCSI],
            NSSTR(kIOPropertyPhysicalInterconnectTypeSerialAttachedSCSI),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeFibreChannel],
            NSSTR(kIOPropertyPhysicalInterconnectTypeFibreChannel),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeSATA],
            NSSTR(kIOPropertyPhysicalInterconnectTypeSerialATA),
        [NSNumber numberWithUnsignedInt:efsIOTransportTypeImage],
            NSSTR(kIOPropertyPhysicalInterconnectTypeVirtual),
        nil];
    
   e_pending = [[NSMutableArray alloc] initWithCapacity:kPendingCount];
   for (i=0; i < kPendingCount; ++i) {
        obj = [[NSMutableArray alloc] init];
        [e_pending addObject:obj];
        [obj release];
   }
   
   notify_port_ref = IONotificationPortCreate(kIOMasterPortDefault);
	if (0 == notify_port_ref) {
		kr = 1;
      goto exit;
	}
   
   /* Setup callbacks to get notified of additions/removals. */
   kr = IOServiceAddMatchingNotification (notify_port_ref,
      kIOMatchedNotification, IOServiceMatching(kIOMediaClass),
      iomatch_add_callback, nil, &notify_add_iter);
   if (kr || 0 == notify_add_iter) {
      goto exit;
   }
   kr = IOServiceAddMatchingNotification (notify_port_ref,
      kIOTerminatedNotification, IOServiceMatching(kIOMediaClass),
      iomatch_rem_callback, nil, &notify_rem_iter);
   if (kr || 0 == notify_rem_iter) {
      goto exit;
   }
   
#ifdef EFSM_1030_SUPPORT
    if (floor(NSFoundationVersionNumber) <= NSFoundationVersionNumber10_3) {
        // We are running on 10.3
        PantherInitSMART();
    }
#endif
   
   e_media = [[NSMutableDictionary alloc] init];
   
    #ifdef EFSM_GLOBAL_DEVICE_ALLOW
    deviceMountBlockList = [[NSMutableSet alloc] init];
    #endif
    e_instanceLock = e_lock;
   
   pthread_mutex_unlock(&e_initMutex);
   
    // Start the thread that will receive all non-approval notifications
    [NSThread detachNewThreadSelector:@selector(processDiskNotifications:) toTarget:self withObject:nil];
   
   return (self);

exit:
   e_instance = nil;
   if (e_lock) edlock(e_lock);
   [e_media release];
   [e_pending release];
   if (notify_add_iter) IOObjectRelease(notify_add_iter);
   if (notify_rem_iter) IOObjectRelease(notify_rem_iter);
   if (notify_port_ref) IONotificationPortDestroy(notify_port_ref);
   pthread_mutex_unlock(&e_initMutex);
   [super release];
   return (nil);
}

/* This singleton lives forever. */
- (void)dealloc
{
   E2Log(@"ExtFS: Oops! Somebody released the global media controller!\n");
#if 0
    DAUnregisterApprovalCallback(daApprovalSession, DiskArbCallback_ApproveMount, NULL);
    DAApprovalSessionUnscheduleFromRunLoop(daApprovalSession, [[NSRunLoop currentRunLoop] getCFRunLoop], kCFRunLoopCommonModes);
    CFRelease(daApprovalSession);
    
   DAUnregisterCallback(DiskArbCallback_PathChangeNotification, NULL);
   DAUnregisterCallback(DiskArbCallback_NameChangeNotification, NULL);
   DAUnregisterCallback(DiskArbCallback_AppearedNotification, NULL);
   DASessionUnscheduleFromRunLoop(daSession, [[NSRunLoop currentRunLoop] getCFRunLoop], kCFRunLoopCommonModes);
   CFRelease(daSession);
   
   [e_media release];
   [e_pending release];
   pthread_mutex_lock(&e_initMutex);
   e_instanceLock = nil;
   pthread_mutex_unlock(&e_initMutex);
   edlock(e_lock);
#endif
   [super dealloc];
}

@end

/* Helpers */

// e_fsNames is read-only, so there is no need for a lock
const char* EFSNameFromType(ExtFSType type)
{
   if (type > fsTypeUnknown)
      type = fsTypeUnknown;
   return (e_fsNames[(type)]);
}

NSString* EFSNSNameFromType(ExtFSType type)
{
   if (type > fsTypeUnknown)
      type = fsTypeUnknown;
   return @(e_fsNames[(type)]);
}

static NSDictionary *e_fsPrettyNames = nil, *e_fsTransportNames = nil;
static pthread_mutex_t e_fsTableInitMutex = PTHREAD_MUTEX_INITIALIZER;

NSString* EFSNSPrettyNameFromType(ExtFSType type)
{
    if (nil == e_fsPrettyNames) {
        NSBundle *me;
        pthread_mutex_lock(&e_fsTableInitMutex);
        // Make sure the global is still nil once we hold the lock
        if (nil != e_fsPrettyNames) {
            pthread_mutex_unlock(&e_fsTableInitMutex);
            goto fspretty_lookup;
        }
        
        me = [NSBundle bundleWithIdentifier:EXTFS_DM_BNDL_ID];
        if (nil == me) {
            pthread_mutex_unlock(&e_fsTableInitMutex);
            E2Log(@"ExtFS: Could not find bundle!\n");
            return (nil);
        }

        /* The correct way to get these names is to enum the FS bundles and
          get the FSName value for each personality. This turns out to be more
          work than it's worth though. */
        e_fsPrettyNames = [[NSDictionary alloc] initWithObjectsAndKeys:
            @"Linux Ext2", [NSNumber numberWithUnsignedInt:fsTypeExt2],
            [me localizedStringForKey:@"Linux Ext3 (Journaled)" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:fsTypeExt3],
            @"Mac OS Standard", [NSNumber numberWithUnsignedInt:fsTypeHFS],
            [me localizedStringForKey:@"Mac OS Extended" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:fsTypeHFSPlus],
            [me localizedStringForKey:@"Mac OS Extended (Journaled)" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:fsTypeHFSJ],
            [me localizedStringForKey:@"Mac OS Extended (Journaled/Case Sensitive)" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:fsTypeHFSX],
            @"UNIX", [NSNumber numberWithUnsignedInt:fsTypeUFS],
            @"ISO 9660", [NSNumber numberWithUnsignedInt:fsTypeCD9660],
            [me localizedStringForKey:@"CD Audio" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:fsTypeCDAudio],
            @"Universal Disk Format (UDF)", [NSNumber numberWithUnsignedInt:fsTypeUDF],
            @"MSDOS (FAT)", [NSNumber numberWithUnsignedInt:fsTypeMSDOS],
            @"Windows NTFS", [NSNumber numberWithUnsignedInt:fsTypeNTFS],
            [me localizedStringForKey:@"Unknown" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:fsTypeUnknown],
          nil];
        pthread_mutex_unlock(&e_fsTableInitMutex);
    }

fspretty_lookup:
    // Once allocated, the name table is considered read-only
    if (type > fsTypeUnknown)
      type = fsTypeUnknown;
    return ([e_fsPrettyNames objectForKey:[NSNumber numberWithUnsignedInt:type]]);
}

NSString* EFSIOTransportNameFromType(unsigned long type)
{
    if (nil == e_fsTransportNames) {
        NSBundle *me;
        pthread_mutex_lock(&e_fsTableInitMutex);
        // Make sure the global is still nil once we hold the lock
        if (nil != e_fsTransportNames) {
            pthread_mutex_unlock(&e_fsTableInitMutex);
            goto fstrans_lookup;
        }
        
        me = [NSBundle bundleWithIdentifier:EXTFS_DM_BNDL_ID];
        if (nil == me) {
            pthread_mutex_unlock(&e_fsTableInitMutex);
            E2Log(@"ExtFS: Could not find bundle!\n");
            return (nil);
        }

        e_fsTransportNames = [[NSDictionary alloc] initWithObjectsAndKeys:
            [me localizedStringForKey:NSSTR(kIOPropertyInternalKey) value:nil table:nil],
                [NSNumber numberWithUnsignedInt:efsIOTransportTypeInternal],
            [me localizedStringForKey:NSSTR(kIOPropertyExternalKey) value:nil table:nil],
                [NSNumber numberWithUnsignedInt:efsIOTransportTypeExternal],
            [me localizedStringForKey:NSSTR(kIOPropertyInternalExternalKey) value:nil table:nil],
                [NSNumber numberWithUnsignedInt:efsIOTransportTypeInternal|efsIOTransportTypeExternal],
            [me localizedStringForKey:@"Virtual" value:nil table:nil],
                [NSNumber numberWithUnsignedInt:efsIOTransportTypeVirtual],
            NSSTR(kIOPropertyPhysicalInterconnectTypeATA), [NSNumber numberWithUnsignedInt:efsIOTransportTypeATA],
            NSSTR(kIOPropertyPhysicalInterconnectTypeATAPI), [NSNumber numberWithUnsignedInt:efsIOTransportTypeATAPI],
            NSSTR(kIOPropertyPhysicalInterconnectTypeFireWire), [NSNumber numberWithUnsignedInt:efsIOTransportTypeFirewire],
            NSSTR(kIOPropertyPhysicalInterconnectTypeUSB), [NSNumber numberWithUnsignedInt:efsIOTransportTypeUSB],
            @"SCSI", [NSNumber numberWithUnsignedInt:efsIOTransportTypeSCSI],
            @"Fibre Channel", @(efsIOTransportTypeFibreChannel),
            NSSTR(kIOPropertyPhysicalInterconnectTypeSerialATA), @(efsIOTransportTypeSATA),
            [me localizedStringForKey:@"Disk Image" value:nil table:nil],
                @(efsIOTransportTypeImage),
            [me localizedStringForKey:@"Unknown" value:nil table:nil],
                @(efsIOTransportTypeUnknown),
            nil];
        pthread_mutex_unlock(&e_fsTableInitMutex);
    }

fstrans_lookup:
    // Once allocated, the name table is considered read-only
    if (type > efsIOTransportTypeUnknown)
      type = efsIOTransportTypeUnknown;
    return ([e_fsTransportNames objectForKey:@(type)]);
}

/* Callbacks */

static void iomatch_add_callback(void *refcon, io_iterator_t iter)
{
    @autoreleasepool {
    [[ExtFSMediaController mediaController] updateMedia:iter remove:NO];
    }
}

static void iomatch_rem_callback(void *refcon, io_iterator_t iter)
{
    @autoreleasepool {
    [[ExtFSMediaController mediaController] updateMedia:iter remove:YES];
    }
}

static void DiskArbCallback_MountNotification(DADiskRef disk,
   DADissenterRef dissenter, void * context __unused)
{
    @autoreleasepool {
    if (NULL == dissenter)
        (void)[[ExtFSMediaController mediaController] updateMountStatus];
    else
        DiskArb_CallFailed(DADiskGetBSDName(disk), EXT_DISK_ARB_MOUNT_FAILURE,
                           DADissenterGetStatus(dissenter));
    }
}

static void DiskArbCallback_UnmountNotification(DADiskRef disk,
   DADissenterRef dissenter, void * context)
{
    const char *device = DADiskGetBSDName(disk);
    @autoreleasepool {
    if (NULL == dissenter) {
        (void)[[ExtFSMediaController mediaController] volumeDidUnmount:NSSTR(device)];
        if (EXT2_DISK_EJECT == (unsigned)context)
            DADiskEject(disk, kDADiskEjectOptionDefault, DiskArbCallback_EjectNotification, NULL);
    } else
        DiskArb_CallFailed(device, kDiskArbUnmountRequestFailed,
                           DADissenterGetStatus(dissenter));
    }
}

static void DiskArbCallback_EjectNotification(DADiskRef disk,
   DADissenterRef dissenter, void * context __unused)
{
    @autoreleasepool {
    const char *device = DADiskGetBSDName(disk);
    ExtFSMedia *emedia = [[ExtFSMediaController mediaController] mediaWithBSDName:NSSTR(device)];
    
    if (NULL == dissenter && emedia && [emedia isMounted])
        (void)[[ExtFSMediaController mediaController] volumeDidUnmount:NSSTR(device)];
    else if (dissenter)
        DiskArb_CallFailed(device, kDiskArbUnmountAndEjectRequestFailed,
                           DADissenterGetStatus(dissenter));
    }
}

static void DiskArbCallback_PathChangeNotification(DADiskRef disk,
   CFArrayRef keys __unused, void * context __unused)
{
    @autoreleasepool {
    NSDictionary *d = CFBridgingRelease(DADiskCopyDescription(disk));
    NSURL *path = [d objectForKey:(NSString*)kDADiskDescriptionVolumePathKey];
    
    /* Apparently, despite the "watching volume mount changes" documentation,
     DARegisterDiskDescriptionChangedCallback() is not meant to notice mounts.
     But it does work (and is in fact needed) in some circumstances where a device is attached
     and then some time later actually mounted.
     This of course is all very stupid. Why can't DA just provide a mount and unmount callback?
     */
    if (d && [[NSFileManager defaultManager] fileExistsAtPath:[path path]]) {
        (void)[[ExtFSMediaController mediaController] updateMountStatus];
    } else if (d && (!path || NO == [[NSFileManager defaultManager] fileExistsAtPath:[path path]])) {
        (void)[[ExtFSMediaController mediaController] volumeDidUnmount:NSSTR(DADiskGetBSDName(disk))];
    }
    }
}

static void DiskArbCallback_NameChangeNotification(DADiskRef disk,
   CFArrayRef keys __unused, void * context __unused)
{
    @autoreleasepool {
    NSDictionary *d = CFBridgingRelease(DADiskCopyDescription(disk));
    NSString *name = [d objectForKey:(NSString*)kDADiskDescriptionVolumeNameKey];
    
    if (d && name)
        (void)[[ExtFSMediaController mediaController] nameOfDevice:NSSTR(DADiskGetBSDName(disk)) didChangeTo:name];
    }
}

static void DiskArbCallback_AppearedNotification(DADiskRef disk, void *context __unused)
{
    @autoreleasepool {
	NSDictionary *d = CFBridgingRelease(DADiskCopyDescription(disk));
    NSURL *path;
    const char *bsd = DADiskGetBSDName(disk);
    E2DiagLog(@"%s: %p - %s\n", __FUNCTION__,  disk, bsd ? bsd : "NULL DEVICE!");
    if (d && (path = [d objectForKey:(NSString*)kDADiskDescriptionVolumePathKey])
        /* && [[NSFileManager defaultManager] fileExistsAtPath:[path path]]*/) {
        (void)[[ExtFSMediaController mediaController] updateMountStatus];
    }
    }
}

static DADissenterRef DiskArbCallback_ApproveMount(DADiskRef disk, void *context)
{
    @autoreleasepool {
    NSString *dev = NSSTR(DADiskGetBSDName(disk));
    BOOL allow = YES;
    
    #ifdef EFSM_GLOBAL_DEVICE_ALLOW
    NSRange r = [dev rangeOfString:@"disk"];
    r.length++; // include the disk #
    NSString *rootDev = [dev substringWithRange:r];
    erlock(e_instanceLock);
    if ([deviceMountBlockList containsObject:dev] || [deviceMountBlockList containsObject:rootDev])
        allow = NO;
    eulock(e_instanceLock);
    if (allow)
    #endif
    allow = [[ExtFSMediaController mediaController] allowMount:dev];
    
    E2DiagLog(@"ExtFS: Mount '%s' %s.\n", DADiskGetBSDName(disk), allow ? "allowed" : "denied");
    if (allow)
        return (NULL);
    
    return (DADissenterCreate(kCFAllocatorDefault, kDAReturnExclusiveAccess, NULL));
    }
}

static void DiskArbCallback_Claimed(DADiskRef disk, DADissenterRef dissenter, void *context)
{
    @autoreleasepool {
    [[ExtFSMediaController mediaController] claimDidCompleteWithDevice:NSSTR(DADiskGetBSDName(disk))
                                                                 error:!dissenter ? 0 : DADissenterGetStatus(dissenter)];
    }
}

static DADissenterRef DiskArbCallback_ClaimRelease(DADiskRef disk, void *context)
{
    BOOL allow;
    @autoreleasepool {
    allow = [[ExtFSMediaController mediaController] releaseClaimForDevice:NSSTR(DADiskGetBSDName(disk))];
    }
    if (allow)
        return (NULL);
    
    return (DADissenterCreate(kCFAllocatorDefault, kDAReturnExclusiveAccess, NULL));
}

NSString * const ExtMediaKeyOpFailureID = @"ExtMediaKeyOpFailureID";
NSString * const ExtMediaKeyOpFailureType = @"ExtMediaKeyOpFailureType";
NSString * const ExtMediaKeyOpFailureDevice = @"ExtMediaKeyOpFailureBSDName";
NSString * const ExtMediaKeyOpFailureError = @"ExtMediaKeyOpFailureError";
NSString * const ExtMediaKeyOpFailureErrorString = @"ExtMediaKeyOpFailureErrorString";
NSString * const ExtMediaKeyOpFailureMsgString = @"ExtMediaKeyOpFailureMsgString";

// Error codes are defined in DiskArbitration/DADissenter.h
static NSString * const e_DiskArbErrorTable[] = {
   @"", // Empty
   @"Unknown Error", // kDAReturnError
   @"Device is busy", // kDAReturnBusy
   @"Bad argument", // kDAReturnBadArgument
   @"Device is locked", // kDAReturnExclusiveAccess
   @"Resource shortage", // kDAReturnNoResources
   @"Device not found", // kDAReturnNotFound
   @"Device not mounted", // kDAReturnNotMounted
   @"Operation not permitted", // kDAReturnNotPermitted
   @"Not authorized", // kDAReturnNotPrivileged
   @"Device not ready", // kDAReturnNotReady
   @"Device is read only", // kDAReturnNotWritable
   @"Unsupported request", // kDAReturnUnsupported
   nil
};
#define DA_ERR_TABLE_LAST 0x0C

static int const e_DiskArbUnixErrorTable[] = {
    0, // no err
    (kDAReturnNotPermitted & 0xFF), // EPERM
    (kDAReturnNotFound & 0xFF), // ENOENT
    1, // ESRCH
    1, // EINTR
    1, // EIO
    (kDAReturnNotReady & 0xFF), // ENXIO
    1, // E2BIG
    1, // ENOEXEC
    1, // EBADF
    1, // ECHILD
    1, // EDEADLK
    1, // ENOMEM
    1, // EACCES
    1, // EFAULT
    1, // ENOTBLK
    (kDAReturnBusy & 0xFF), // EBUSY
    1, // EEXIST
    1, // EXDEV
    1, // ENODEV
    1, // ENOTDIR
    1, // EISDIR
    (kDAReturnBadArgument & 0xFF), // EINVAL
};
#define DA_UNIX_ERR_TABLE_LAST EINVAL
#define MACH_ERR_UNIX_DOMAIN 3

static void DiskArb_CallFailed(const char *device, int type, int status)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   
   NSString *bsd = NSSTR(device), *err = nil, *op, *msg, *opid;
   ExtFSMedia *emedia;
   NSBundle *me;
   ExtFSMediaController *mc = [ExtFSMediaController mediaController];
   
   if ((emedia = [mc mediaWithBSDName:bsd])) {
      short idx = (status & 0xFF);
      if (MACH_ERR_UNIX_DOMAIN == err_get_sub(status)) {
         if (idx <= DA_UNIX_ERR_TABLE_LAST)
            idx = e_DiskArbUnixErrorTable[idx];
         else
            idx = 1;
      }
      
      NSDictionary *dict;
      
      [mc removePending:emedia];
      
      if (idx > DA_ERR_TABLE_LAST) {
         if (EBUSY == idx) // This can be returned instead of kDAReturnBusy
            idx = 2;
         else if (ETIMEDOUT == idx)
            err = @"Operation timed out";
         else
            idx = 1;
      }
      // Table is read-only, no need to protect it.
      if (!err)
         err = e_DiskArbErrorTable[idx];
      
      msg = nil;
      switch (type) {
        /* XXX - No mount errors from Disk Arb??? */
        case EXT_DISK_ARB_MOUNT_FAILURE:
            if ([emedia isMounted]) // make sure it's not mounted
                return;
            opid = ExtFSMediaNotificationMounted;
            op = @"Mount";
            msg = @"The filesystem may need repair. Please use Disk Utility to check the filesystem.";
            break;
        case kDiskArbUnmountRequestFailed:
            opid = ExtFSMediaNotificationUnmounted;
            op = @"Unmount";
            msg = @"The disk may be in use by an application.";
            break;
        case kDiskArbUnmountAndEjectRequestFailed:
        case kDiskArbEjectRequestFailed:
            opid = ExtFSMediaNotificationUnmounted;
            op = @"Eject";
            break;
        case kDiskArbDiskChangeRequestFailed:
            opid = @"";
            op = @"Change Request";
            break;
        default:
            opid = @"";
            op = @"Unknown";
            break;
      }
      
      // Get a ref to ourself so we can load our localized strings.
      erlock(e_instanceLock);
      me = [NSBundle bundleWithIdentifier:EXTFS_DM_BNDL_ID];
      if (me) {
        NSString *errl, *opl, *msgl;
        
        errl = [me localizedStringForKey:err value:nil table:nil];
        if (errl)
            err = errl;
        
        opl = [me localizedStringForKey:op value:nil table:nil];
        if (opl)
            op = opl;
        
        if (msg) {
            if ((msgl = [me localizedStringForKey:msg value:nil table:nil]))
                msg = msgl;
        }
      }
      eulock(e_instanceLock);
      
      dict = [NSDictionary dictionaryWithObjectsAndKeys:
         opid, ExtMediaKeyOpFailureID,
         op, ExtMediaKeyOpFailureType,
         bsd, ExtMediaKeyOpFailureDevice,
         [NSNumber numberWithInt:status], ExtMediaKeyOpFailureError,
         err, ExtMediaKeyOpFailureErrorString,
         msg, (msg ? ExtMediaKeyOpFailureMsgString : nil),
         nil];

      EFSMCPostNotification(ExtFSMediaNotificationOpFailure, emedia, dict);
      
      E2Log(@"ExtFS: DiskArb failure for device '%s', with type %d and status 0x%X\n",
         device, type, status);
   }
   
   [pool release];
}
