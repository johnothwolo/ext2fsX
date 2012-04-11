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

#import <unistd.h>

#import <IOKit/storage/ata/ATASMARTLib.h>

#import "ExtFSMediaPrivate.h"
#import "ExtFSMediaSMART.h"
#import "ExtFSLock.h"

NSString * const ExtFSMediaNotificationSMARTStatus = @"ExtFSMediaNotificationSMARTStatus";
NSString * const ExtFSMediaKeySMARTStatus = @"ExtFSMediaKeySMARTStatus";
NSString * const ExtFSMediaKeySMARTStatusSeverity = @"ExtFSMediaKeySMARTStatusSeverity";
NSString * const ExtFSMediaKeySMARTStatusDescription = @"ExtFSMediaKeySMARTStatusDescription";
NSString * const ExtFSMediaKeySMARTStatusSeverityDescription = @"ExtFSMediaKeySMARTStatusSeverityDescription";

static const unsigned int e_SMARTErrorTxlate[] =
{
efsSMARTVerified, // kATASMARTSelfTestStatusNoError
efsSMARTTestAbort, // kATASMARTSelfTestStatusAbortedByHost
efsSMARTTestInterrupted, // kATASMARTSelfTestStatusInterruptedByReset
efsSMARTTestFatal, // kATASMARTSelfTestStatusFatalError
efsSMARTTestUnknownFail, // kATASMARTSelfTestStatusPreviousTestUnknownFailure
efsSMARTTestElectricFail, // kATASMARTSelfTestStatusPreviousTestElectricalFailure
efsSMARTTestServoFail, // kATASMARTSelfTestStatusPreviousTestServoFailure
efsSMARTTestReadFail, // kATASMARTSelfTestStatusPreviousTestReadFailure
0,
0,
0,
0,
0,
0,
0,
0,
efsSMARTTestInProgress, // kATASMARTSelfTestStatusInProgress
0
};

static NSDictionary *e_SMARTDescrip = nil, *e_SMARTSeverityDescrip = nil;

@implementation ExtFSMedia (ExtFSMediaSMART)

#ifndef SMART_CACHE_TIME
#define SMART_CACHE_TIME 300
#endif
- (ExtFSMARTStatus)SMARTStatus
{
    struct timeval now;
    gettimeofday(&now, nil);
    
    erlock(e_lock);
    if ((e_lastSMARTUpdate + SMART_CACHE_TIME) > now.tv_sec) {
        eulock(e_lock);
        return ((ExtFSMARTStatus)e_smartStatus);
    }
    eulock(e_lock);
    
    int smartStat = [[ExtFSMediaController mediaController] SMARTStatusForMedia:self parentDisk:nil];
    ewlock(e_lock);
    e_smartStatus = smartStat;
    if (now.tv_sec > e_lastSMARTUpdate)
        e_lastSMARTUpdate = now.tv_sec;
    eulock(e_lock);

    return ((ExtFSMARTStatus)e_smartStatus);
}

@end

@implementation ExtFSMediaController (ExtFSMediaControllerSMART)

- (ExtFSMARTStatus)SMARTStatusForMedia:(ExtFSMedia*)media parentDisk:(ExtFSMedia**)disk
{
    ExtFSMARTStatus status;
    
    if (disk)
        *disk = nil;
    // Find the root disk object
    ExtFSMedia *parent = media;
    while ((parent = [parent parent])) {
        if ([parent isWholeDisk]) {
            media = parent;
            parent = nil;
            break;
        }
    }
    
    io_service_t service = [media SMARTService];
    BOOL setService = NO;
    if (service) {
        if (kIOReturnSuccess != IOObjectRetain(service))
            service = nil;
    } else if ([media isSMART]) {
        service = [media copySMARTIOService];
        setService = YES;
    } else
        service = nil;
    
    if (service) {
        kern_return_t kr;
        IOCFPlugInInterface **cfipp;
        IOATASMARTInterface **sipp;
        SInt32 score;
        Boolean exceeded = NO;
        
        // Create the intermediate plug-in interface
        status = efsSMARTVerified;
        cfipp = nil;
        sipp = nil;
        kr = IOCreatePlugInInterfaceForService(service, kIOATASMARTUserClientTypeID,
            kIOCFPlugInInterfaceID, (IOCFPlugInInterface***)&cfipp, &score);
        if (kIOReturnSuccess == kr && cfipp) {
            // Create the SMART plug-in interface
            kr = (*cfipp)->QueryInterface(cfipp, CFUUIDGetUUIDBytes(kIOATASMARTInterfaceID), (LPVOID)&sipp);
        }
        if (S_OK == kr && sipp) {
            // Query for SMART status
            kr = (*sipp)->SMARTReturnStatus(sipp, &exceeded);
            if (kIOReturnSuccess == kr && setService) {
                [media setSMARTService:service];
            }
            if (kIOReturnSuccess == kr && exceeded) {
                ATASMARTData data;
                status = efsSMARTTestUnknownFail;
                // Get details
                kr = (*sipp)->SMARTReadData(sipp, &data);
                if (kIOReturnSuccess == kr &&
                     data.selfTestExecutionStatus < (sizeof(e_SMARTErrorTxlate) / sizeof(unsigned int)))
                    status = e_SMARTErrorTxlate[data.selfTestExecutionStatus];
            } else if (kr) {
                status = efsSMARTOSError;
                E2Log(@"ExtFS: SMART status query failed with: 0x%x!\n", kr);
            }
            kr = (*sipp)->Release(sipp);
        } else {
            status = efsSMARTOSError;
            E2Log(@"ExtFS: Could not create SMART plug-in interface! (0x%x).\n", kr);
        }
        
        if (cfipp)
            kr = IODestroyPlugInInterface(cfipp);
        
        IOObjectRelease(service);
        
        if (disk)
            *disk = media;
    } else {
        status = efsSMARTInvalidTransport;
        if (setService) {
            [media setNotSMART];
        }
    }
    
    return (status);
}

#define EXTFS_REPLACE_DRIVE_MSG @"The disk should be immediately replaced after copying all data."
- (NSDictionary*)SMARTStatusDescription:(ExtFSMARTStatus)status
{
    ExtFSMARTStatusSeverity severity;
    NSDictionary *info;
    NSString *statusStr, *severityStr;
    
    if (nil == e_SMARTDescrip) {
        NSBundle *me;
        ewlock(e_lock);
        // Make sure the global is still nil once we hold the lock
        if (nil != e_SMARTDescrip) {
            eulock(e_lock);
            goto fssmart_lookup;
        }
        
        me = [NSBundle bundleWithIdentifier:EXTFS_DM_BNDL_ID];
        if (nil == me) {
            eulock(e_lock);
            E2Log(@"ExtFS: Could not find bundle!\n");
            return (nil);
        }

        e_SMARTDescrip = [[NSDictionary alloc] initWithObjectsAndKeys:
            [me localizedStringForKey:@"The S.M.A.R.T. service is not supported by this disk." value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTInvalidTransport],
            [me localizedStringForKey:@"The status query failed due to an OS error." value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTOSError],
            [me localizedStringForKey:@"Disk verified." value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTVerified],
            [me localizedStringForKey:@"Test aborted." value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestAbort],
            [me localizedStringForKey:@"Test interrupted." value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestInterrupted],
            [me localizedStringForKey:
                @"A fatal disk error was detected! " EXTFS_REPLACE_DRIVE_MSG value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestFatal],
            [me localizedStringForKey:
                @"An unknown disk error was detected! " EXTFS_REPLACE_DRIVE_MSG value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestUnknownFail],
            [me localizedStringForKey:
                @"A disk electrical failure was detected! " EXTFS_REPLACE_DRIVE_MSG value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestElectricFail],
            [me localizedStringForKey:
                @"A disk motor failure was detected! " EXTFS_REPLACE_DRIVE_MSG value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestServoFail],
            [me localizedStringForKey:
                @"A disk read failure was detected! " EXTFS_REPLACE_DRIVE_MSG value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestReadFail],
            [me localizedStringForKey:
                @"The disk test is currently in progress." EXTFS_REPLACE_DRIVE_MSG value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTTestInProgress],
            nil];
            
        e_SMARTSeverityDescrip = [[NSDictionary alloc] initWithObjectsAndKeys:
            [me localizedStringForKey:@"Informational Status" value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTSeverityInformational],
            [me localizedStringForKey:@"Warning Status" value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTSeverityWarning],
            [me localizedStringForKey:@"!! Critical Error !!" value:nil table:nil],
                [NSNumber numberWithInt:efsSMARTSeverityCritical],
            nil];
        eulock(e_lock);
    }
    
fssmart_lookup:
    switch (status) {
        case efsSMARTInvalidTransport:
        case efsSMARTVerified:
        case efsSMARTTestInProgress:
        case efsSMARTTestInterrupted:
            severity = efsSMARTSeverityInformational;
            break;
        case efsSMARTOSError:
        case efsSMARTTestAbort:
            severity = efsSMARTSeverityWarning;
            break;
        default:
            severity = efsSMARTSeverityCritical;
            break;
    };
    
    // Once allocated, the tables are considered read-only
    statusStr = nil;
    if (status < efsSMARTStatusMax)
        statusStr = [e_SMARTDescrip objectForKey:[NSNumber numberWithInt:status]];
    severityStr = [e_SMARTSeverityDescrip objectForKey:[NSNumber numberWithInt:severity]];
    
    info = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt:status], ExtFSMediaKeySMARTStatus,
        [NSNumber numberWithInt:severity], ExtFSMediaKeySMARTStatusSeverity,
        severityStr, ExtFSMediaKeySMARTStatusSeverityDescription,
        statusStr, (statusStr ? ExtFSMediaKeySMARTStatusDescription : nil),
        nil];
    
    return (info);
}

- (void)monitorSMARTStatusThread:(NSArray*)args
{
    NSAutoreleasePool *tpool = [[NSAutoreleasePool alloc] init];
    ExtFSSMARTEventFlag flags = efsSMARTEventFailed;
    
    e_smonActive = YES;
    if ([args count] > 0)
        flags = [[args objectAtIndex:0] unsignedLongValue];
    do {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        NSArray *media = [self mediaWithIOTransportBus:efsIOTransportTypeATA];
        NSEnumerator *en;
        ExtFSMedia *obj;
        ExtFSMARTStatus status;
        
        en = [media objectEnumerator];
        while ((obj = [en nextObject])) {
            if (NO == [obj isWholeDisk])
                continue;
            status = [self SMARTStatusForMedia:obj parentDisk:nil];
            if (efsSMARTVerified != status ||
                 (efsSMARTVerified == status && (flags & efsSMARTEventVerified))) {
                EFSMCPostNotification(ExtFSMediaNotificationSMARTStatus, obj,
                    [self SMARTStatusDescription:status]);
            }
        }
        
        [pool release];
        
        usleep(e_smonPollInterval * 1000);
    } while (0 != e_smonPollInterval);
    [tpool release];
    e_smonActive = NO;
    [NSThread exit];
}

- (BOOL)monitorSMARTStatusWithPollInterval:(u_int64_t)interval flags:(ExtFSSMARTEventFlag)flags
{
    if (interval != 0 && NO == e_smonActive) {
        NSArray *args = [[NSArray alloc] initWithObjects:[NSNumber numberWithUnsignedLong:flags], nil];
        e_smonPollInterval = interval;
        [NSThread  detachNewThreadSelector:@selector(monitorSMARTStatusThread:) toTarget:self withObject:args];
        [args release];
        return (YES);
    }
    return (NO);
}

- (void)monitorSMARTStatusStop
{
    e_smonPollInterval = 0;
}

@end
