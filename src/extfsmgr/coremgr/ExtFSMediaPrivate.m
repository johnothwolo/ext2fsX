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

#import <string.h>
#import <sys/param.h>
#import <sys/ucred.h>
#import <sys/mount.h>

#import <mach/mach_port.h>
#import <mach/mach_interface.h>
#import <mach/mach_init.h>

#import <IOKit/IOMessage.h>
#import <IOKit/IOBSD.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOCDMedia.h>
#import <IOKit/storage/IODVDMedia.h>
#import <IOKit/storage/IOStorageProtocolCharacteristics.h>
#import <IOKit/storage/ata/ATASMARTLib.h>

#import "ExtFSMediaPrivate.h"
#import "ExtFSLock.h"

enum {
    kNoteArgName = 0,
    kNoteArgObject = 1,
    kNoteArgInfo = 2
};

__private_extern__ NSDictionary *transportNameTypeMap;

static
#ifndef EFSM_1030_SUPPORT
inline __attribute__((always_inline))
#endif
BOOL IsSMARTCapableTiger(io_service_t is)
{
    CFMutableDictionaryRef props;
    BOOL smart = NO;
    if (kIOReturnSuccess == IORegistryEntryCreateCFProperties(is, &props, kCFAllocatorDefault, 0)) {
        smart = (nil != [(NSDictionary*)props objectForKey:NSSTR(kIOPropertySMARTCapableKey)]);
        CFRelease(props);
    }
    return (smart);
}

#ifndef EFSM_1030_SUPPORT
#define IsSMARTCapable IsSMARTCapableTiger
#else
static BOOL (*IsSMARTCapable)(io_service_t) = IsSMARTCapableTiger;

static
BOOL IsSMARTCapablePan(io_service_t is)
{
    io_name_t class;
    if (kIOReturnSuccess == IOObjectGetClass(is, class)) {
        if (0 == strncmp(class, "IOATABlockStorageDevice", sizeof(class)))
            return (YES);
    }
    return (NO);
}

__private_extern__ void PantherInitSMART()
{
    IsSMARTCapable = IsSMARTCapablePan;
}
#endif

@implementation ExtFSMediaController (Private)

- (void)postNotification:(NSArray*)args
{
    unsigned ct = [args count];
    id obj = (ct >= kNoteArgObject+1) ? [args objectAtIndex:kNoteArgObject] : nil;
    NSDictionary *info = (ct >= kNoteArgInfo+1) ? [args objectAtIndex:kNoteArgInfo] : nil;
    [[NSNotificationCenter defaultCenter]
         postNotificationName:[args objectAtIndex:kNoteArgName] object:obj userInfo:info];
}

- (NSString*)pathForResource:(NSString*)resource
{
    NSString *path = nil;
    
    ewlock(e_lock);
    NSBundle *me = [NSBundle bundleWithIdentifier:EXTFS_DM_BNDL_ID];
    if (me) {
        path = [me pathForResource:resource ofType:nil];
    } else {
        E2Log(@"ExtFS: Could not find bundle!\n");
    }
    eulock(e_lock);
    return (path);
}

@end

#define MAX_PARENT_ITERS 10
@implementation ExtFSMedia (ExtFSMediaController)

- (BOOL)updateAttributesFromIOService:(io_service_t)service
{
   ExtFSMediaController *mc;
   NSString *regName = nil;
   ExtFSMedia *parent = nil;
   CFTypeRef iconDesc;
   io_name_t ioname;
   io_service_t ioparent, ioparentold;
#ifdef notyet
   io_iterator_t piter;
#endif
   int iterations;
   kern_return_t kr;
   ExtFSIOTransportType transType;
   BOOL dvd, cd, wholeDisk;
   
   mc = [ExtFSMediaController mediaController];
   
   erlock(e_lock);
   wholeDisk = (e_attributeFlags & kfsWholeDisk);
   eulock(e_lock);
   
   /* Get IOKit name */
   if (0 == (kr = IORegistryEntryGetNameInPlane(service, kIOServicePlane, ioname)))
      regName = NSSTR(ioname);
   else {
      if (kr)
        E2DiagLog(@"ExtFS: IORegistryEntryGetNameInPlane (%@) error: 0x%X\n", self, kr);
      return (NO);
   }
   
   /* Get Parent */
   CFMutableDictionaryRef props;
#ifdef notyet
   /* This seems to only return the first parent, don't know what's going on */
   if (0 == IORegistryEntryGetParentIterator(service, kIOServicePlane, &piter)) {
      while ((ioparent = IOIteratorNext(piter))) {
#endif
   if (NO == wholeDisk) {
      ioparent = nil;
      iterations = 0;
      kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &ioparent);
      while (kIOReturnSuccess == kr && ioparent && iterations < MAX_PARENT_ITERS) {
         /* Break on the first IOMedia parent */
         if (IOObjectConformsTo(ioparent, kIOMediaClass))
            break;
         /* Otherwise release it */
         ioparentold = ioparent;
         ioparent = nil;
         kr = IORegistryEntryGetParentEntry(ioparentold, kIOServicePlane, &ioparent);
         IOObjectRelease(ioparentold);
         iterations++;
      }
     if (kr)
        E2DiagLog(@"ExtFS: IORegistryEntryGetParentEntry (%@) error: 0x%X\n", self, kr);
#ifdef DIAGNOSTIC
     if (iterations >= MAX_PARENT_ITERS)
        E2Log(@"ExtFS: IORegistryEntryGetParentEntry (%@) error: max iterations exceeded\n", self);
#endif
#ifdef notyet
      IOObjectRelease(piter);
#endif
      if (kIOReturnNoDevice == kr && 0 == iterations) {
         return (NO);
      }
      
      if (ioparent) {
         kr = IORegistryEntryCreateCFProperties(ioparent, &props,
            kCFAllocatorDefault, 0);
         if (!kr) {
            NSString *pdevice;
            
            /* See if the parent object exists */
            pdevice = [(NSDictionary*)props objectForKey:NSSTR(kIOBSDNameKey)];
            parent = [mc mediaWithBSDName:pdevice];
            if (!parent) {
               /* Parent does not exist */
               parent = [mc createMediaWithIOService:ioparent properties:(NSDictionary*)props];
            }
            CFRelease(props);
         }
         
         IOObjectRelease(ioparent);
      }
   }
   
   // Get transport properties
    transType = efsIOTransportTypeUnknown;
    if (nil != parent) {
        transType = [parent transportType] | [parent transportBus];
        dvd = IsOpticalDVDMedia([parent opticalMediaType]);
        cd = IsOpticalCDMedia([parent opticalMediaType]);
    } else {
        ioparent = nil;
        kr = IORegistryEntryGetParentEntry(service, kIOServicePlane, &ioparent);
        while (kIOReturnSuccess == kr && ioparent &&
             (efsIOTransportTypeUnknown == (transType & efsIOTransportBusMask) ||
             0 == (transType & efsIOTransportTypeMask))) {
            
            if (kIOReturnSuccess == IORegistryEntryCreateCFProperties(ioparent, &props, kCFAllocatorDefault, 0)) {
                NSDictionary *protocolSpecs = [(NSDictionary*)props objectForKey:
                    NSSTR(kIOPropertyProtocolCharacteristicsKey)];
                NSString *type, *location;
                NSNumber *foundType;
                
                if ((type = [protocolSpecs objectForKey:NSSTR(kIOPropertyPhysicalInterconnectTypeKey)])
                    || (type = [(NSDictionary*)props objectForKey:NSSTR(kIOPropertyPhysicalInterconnectTypeKey)])) {
                    if ((foundType = [transportNameTypeMap objectForKey:type])
                        && (efsIOTransportTypeUnknown == (transType & efsIOTransportBusMask)))
                        transType = [foundType unsignedIntValue] | (transType & efsIOTransportTypeMask);
                }
                
                if ((location = [protocolSpecs objectForKey:NSSTR(kIOPropertyPhysicalInterconnectLocationKey)])
                    || (location = [(NSDictionary*)props objectForKey:NSSTR(kIOPropertyPhysicalInterconnectLocationKey)])) {
                    if ((foundType = [transportNameTypeMap objectForKey:location])
                        && 0 == (transType & efsIOTransportTypeMask))
                        transType |= [foundType unsignedIntValue];
                }
                CFRelease(props);
            }
            ioparentold = ioparent;
            ioparent = nil;
            kr = IORegistryEntryGetParentEntry(ioparentold, kIOServicePlane, &ioparent);
            IOObjectRelease(ioparentold);
        }
        if (ioparent)
            IOObjectRelease(ioparent);
        if (0 == (transType & efsIOTransportTypeMask))
            transType |= efsIOTransportTypeInternal;
        dvd = IOObjectConformsTo(service, kIODVDMediaClass);
        cd = IOObjectConformsTo(service, kIOCDMediaClass);
    }
   
   /* Find the icon description */
   if (!parent || !(iconDesc = [parent iconDescription]))
      iconDesc = IORegistryEntrySearchCFProperty(service, kIOServicePlane,
         CFSTR(kIOMediaIconKey), kCFAllocatorDefault,
         kIORegistryIterateParents | kIORegistryIterateRecursively);
   
   ExtFSMedia *oldParent = nil;
   BOOL addToParent = NO;
   ewlock(e_lock);
   
   e_ioTransport = transType;
   [e_ioregName release];
      e_ioregName = [regName retain];
   if (e_parent != parent) {
       oldParent = e_parent;
      e_parent = [parent retain];
       addToParent = YES;
   }
   if (iconDesc) {
      [e_iconDesc release];
      e_iconDesc = [(NSDictionary*)iconDesc retain];
   }
   
   NSString * opticalType;
   if (dvd) {
      e_attributeFlags |= kfsDVDROM;
      opticalType = [e_media objectForKey:NSSTR(kIODVDMediaTypeKey)];
   } else if (cd) {
      e_attributeFlags |= kfsCDROM;
      opticalType = [e_media objectForKey:NSSTR(kIOCDMediaTypeKey)];
   } else
      opticalType = nil;

    if (opticalType)
        e_opticalType = [mc opticalMediaTypeForName:opticalType];
    else if (parent)
        e_opticalType = [parent opticalMediaType];
    else
        e_opticalType = efsOpticalTypeUnknown;

   eulock(e_lock);
   
    // Update child status
    if (oldParent) {
        [oldParent remChild:self];
        E2DiagLog(@"ExtFS: '%@' removing self from parent '%@'\n", self, oldParent);
        [oldParent release];
    }
    if (addToParent)
        [e_parent addChild:self];
    
    return (YES);
}

- (void)updateProperties:(NSDictionary*)properties
{
    id oldprops = nil;
    ewlock(e_lock);
    if (properties != e_media) {
        oldprops = e_media;
        e_media = [properties retain];
        
        #ifdef DEBUG
        if (NO == [e_bsdName isEqualToString:[e_media objectForKey:NSSTR(kIOBSDNameKey)]])
            trap(); // XXX This should NEVER happen
        #endif
        
        e_size = [[e_media objectForKey:NSSTR(kIOMediaSizeKey)] unsignedLongLongValue];
        e_devBlockSize = [[e_media objectForKey:NSSTR(kIOMediaPreferredBlockSizeKey)] unsignedLongValue];
        e_opticalType = efsOpticalTypeUnknown;

        e_attributeFlags &= ~(kfsEjectable|kfsWritable|kfsWholeDisk|kfsLeafDisk|kfsNoMount);
        if ([[e_media objectForKey:NSSTR(kIOMediaEjectableKey)] boolValue])
            e_attributeFlags |= kfsEjectable;
        if ([[e_media objectForKey:NSSTR(kIOMediaWritableKey)] boolValue])
            e_attributeFlags |= kfsWritable;
        if ([[e_media objectForKey:NSSTR(kIOMediaWholeKey)] boolValue])
            e_attributeFlags |= kfsWholeDisk;
        if ([[e_media objectForKey:NSSTR(kIOMediaLeafKey)] boolValue])
            e_attributeFlags |= kfsLeafDisk;
        
        NSSet *gptIgnore = [NSSet setWithObjects:
            @"024DEE41-33E7-11D3-9D69-0008C781F39F", // MBR
            @"C12A7328-F81F-11D2-BA4B-00A0C93EC93B", // EFI
            @"0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", // Linux Swap
            @"8DA63339-0007-60C0-C436-083AC8230908", // Linxu reserved
            nil];

        NSString *hint = [e_media objectForKey:NSSTR(kIOMediaContentKey)];
        if ([gptIgnore containsObject:hint]
            || NSNotFound != [hint rangeOfString:@"partition"].location
            || NSNotFound != [hint rangeOfString:@"Boot"].location
            || NSNotFound != [hint rangeOfString:@"Driver"].location
            || NSNotFound != [hint rangeOfString:@"Patches"].location
            || NSNotFound != [hint rangeOfString:@"CD_DA"].location) {
            e_attributeFlags |= kfsNoMount;
        } else {
        hint = [e_media objectForKey:NSSTR(kIOMediaContentKey)];
            if ([gptIgnore containsObject:hint]
                || NSNotFound != [hint rangeOfString:@"partition"].location) {
            e_attributeFlags |= kfsNoMount;
    }
        }
    }
    eulock(e_lock);
    
    [oldprops release];
}

- (void)setIsMounted:(struct statfs*)stat
{
   NSDictionary *fsTypes;
   NSNumber *fstype;
   NSString *tmpstr;
   id tmp;
   ExtFSType ftype;
   BOOL hasJournal, isJournaled;
   
   if (!stat) {
      ewlock(e_lock);
      e_attributeFlags &= ~(kfsMounted|kfsPermsEnabled);
      e_fsBlockSize = 0;
      e_blockCount = 0;
      e_blockAvail = 0;
      e_lastFSUpdate = 0;
      //e_fsType = fsTypeUnknown;
      [e_where release]; e_where = nil;
      [e_volName release]; e_volName = nil;
      
      /* Reset the write flag in case the device is writable,
         but the mounted filesystem was not */
      if ([[e_media objectForKey:NSSTR(kIOMediaWritableKey)] boolValue])
         e_attributeFlags |= kfsWritable;

        if (e_attributeFlags & kfsHasCustomIcon) {
            e_attributeFlags &= ~kfsHasCustomIcon;
            tmp = e_icon;
            e_icon = nil;
        } else
            tmp = nil;
      eulock(e_lock);
      
      EFSMCPostNotification(ExtFSMediaNotificationUnmounted, self, nil);
      [tmp release];
      return;
   }
   
   fsTypes = [[NSDictionary alloc] initWithObjectsAndKeys:
      [NSNumber numberWithInt:fsTypeExt2], NSSTR(EXT2FS_NAME),
      [NSNumber numberWithInt:fsTypeExt3], NSSTR(EXT3FS_NAME),
      [NSNumber numberWithInt:fsTypeHFS], NSSTR(HFS_NAME),
      [NSNumber numberWithInt:fsTypeUFS], NSSTR(UFS_NAME),
      [NSNumber numberWithInt:fsTypeCD9660], NSSTR(CD9660_NAME),
      [NSNumber numberWithInt:fsTypeCDAudio], NSSTR(CDAUDIO_NAME),
      [NSNumber numberWithInt:fsTypeUDF], NSSTR(UDF_NAME),
      [NSNumber numberWithInt:fsTypeMSDOS], NSSTR(MSDOS_NAME),
      [NSNumber numberWithInt:fsTypeNTFS], NSSTR(NTFS_NAME),
      [NSNumber numberWithInt:fsTypeUnknown], NSSTR("Unknown"),
      nil];
   
   fstype = [fsTypes objectForKey:NSSTR(stat->f_fstypename)];
   ftype = fsTypeUnknown;
   if (fstype)
      ftype = [fstype intValue];
   else
      E2Log(@"ExtFS: Unknown filesystem '%@'.\n", fstype);
   [fsTypes release];
   tmpstr = [[NSString alloc] initWithUTF8String:stat->f_mntonname];
   
   ewlock(e_lock);
   e_fsType = ftype;
   
   BOOL wasMounted = (0 != (e_attributeFlags & kfsMounted));
   e_attributeFlags |= kfsMounted;
   // CD-Audio needs the following
   e_attributeFlags &= ~kfsNoMount;
   if (stat->f_flags & MNT_RDONLY)
      e_attributeFlags &= ~kfsWritable;
   
   if (0 == (stat->f_flags & MNT_UNKNOWNPERMISSIONS))
      e_attributeFlags |= kfsPermsEnabled;
   
   e_fsBlockSize = stat->f_bsize;
   e_blockCount = stat->f_blocks;
   e_blockAvail = stat->f_bavail;
   tmp = e_where;
   e_where = tmpstr;
   eulock(e_lock);
   [tmp release];
   tmp = nil;
   tmpstr = nil;
   
    if (!wasMounted) {
       // Attemp to load custom icon
       (void)[self loadCustomVolumeIcon];
    }
   
   (void)[self fsInfoUsingCache:NO];
   hasJournal = [self hasJournal];
   isJournaled = [self isJournaled];
   
   ewlock(e_lock);
   if (fsTypeExt2 == e_fsType && hasJournal)
      e_fsType = fsTypeExt3;
   
   if (fsTypeHFSPlus == e_fsType) {
      if (isJournaled) {
         e_fsType = fsTypeHFSJ;
      }
   }
   eulock(e_lock);
   
   if (!wasMounted)
   EFSMCPostNotification(ExtFSMediaNotificationMounted, self, nil);
}

- (NSDictionary*)iconDescription
{
   return ([[e_iconDesc retain] autorelease]);
}

- (io_service_t)copyIOService
{
    const char *device = BSDNAMESTR(self);
    CFMutableDictionaryRef dict;
    
    dict = IOBSDNameMatching(kIOMasterPortDefault, 0, device);
    if (dict)
        return (IOServiceGetMatchingService(kIOMasterPortDefault, dict));
    
    return (nil);
}

- (io_service_t)copySMARTIOService
{
    io_service_t me = nil, tmp, parent = nil;
    
    if ((me = [self copyIOService])) {
        kern_return_t kr = IORegistryEntryGetParentEntry(me, kIOServicePlane, &parent);
        while (kIOReturnSuccess == kr && parent) {
            if (IsSMARTCapable(parent))
                    break;
            tmp = parent;
            parent = nil;
            kr = IORegistryEntryGetParentEntry(tmp, kIOServicePlane, &parent);
            IOObjectRelease(tmp);
        }
        IOObjectRelease(me);
    }
    
    return (parent);
}

- (io_service_t)SMARTService
{
    erlock(e_lock);
    io_service_t is = (io_service_t)e_smartService;
    eulock(e_lock);
    return (is);
}

- (void)setSMARTService:(io_service_t)is
{
    ewlock(e_lock);
    if (0 == e_smartService && kIOReturnSuccess == IOObjectRetain(is)) {
        e_smartService = is;
    }
    eulock(e_lock);
}

- (BOOL)isSMART
{
    erlock(e_lock);
    BOOL test = (0 == (e_attributeFlags & kfsNotSMART));
    eulock(e_lock);
    return (test);
}

- (void)setNotSMART
{
    ewlock(e_lock);
    if (0 == e_smartService) {
        e_attributeFlags |= kfsNotSMART;
    }
    eulock(e_lock);
}

- (BOOL)claimedExclusive
{
    erlock(e_lock);
    BOOL claimed = (0 != (e_attributeFlags & kfsClaimedWithDA));
    eulock(e_lock);
    return (claimed);
}

- (void)setClaimedExclusive:(BOOL)claimed
{
    ewlock(e_lock);
    if (claimed)
        e_attributeFlags |= kfsClaimedWithDA;
    else
        e_attributeFlags &= ~kfsClaimedWithDA;
    eulock(e_lock);
}

- (BOOL)loadCustomVolumeIcon
{
    NSString *path;
    NSImage *customIcon;
    id tmp;
    BOOL found = NO;
    
    erlock(e_lock);
    path = [e_where retain];
    eulock(e_lock);
    
    FSRef fref;
    CFURLRef url = path ? (CFURLRef)[NSURL fileURLWithPath:path] : NULL;
    if (url)
        CFURLGetFSRef(url, &fref);
    FSCatalogInfo cinfo;
    ((FolderInfo*)&cinfo.finderInfo)->finderFlags = 0;
    (void)FSGetCatalogInfo(&fref, kFSCatInfoFinderInfo, &cinfo, NULL, NULL, NULL);
    if (((FolderInfo*)&cinfo.finderInfo)->finderFlags & kHasCustomIcon) {        
        customIcon = [[NSImage alloc] initWithContentsOfFile:[path stringByAppendingPathComponent:@".VolumeIcon.icns"]];
        if (!customIcon)
            customIcon = [[NSImage alloc] initWithContentsOfFile:[path stringByAppendingPathComponent:@"Icon\r"]];
        if (customIcon) {
            [customIcon setName:[self bsdName]];
            ewlock(e_lock);
            tmp = e_icon;
            e_icon = customIcon;
            e_attributeFlags |= kfsHasCustomIcon;
            eulock(e_lock);
            [tmp release];
            found  = YES;
        }
    }
    [path release];
    return (found);
}

- (ExtFSMedia*)initWithDeviceName:(NSString*)device
{
    if (0 != eilock(&e_lock)) {
        E2Log(@"ExtFS: Failed to allocate media object lock!\n");
        [super release];
        return (nil);
    }
    e_media = [[NSDictionary alloc] initWithObjectsAndKeys:device, NSSTR(kIOBSDNameKey), nil];
    e_bsdName = [device retain];
    return (self);
}

@end
