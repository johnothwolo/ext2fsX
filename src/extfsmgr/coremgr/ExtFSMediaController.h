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

#import <Foundation/Foundation.h>

@class ExtFSMedia;
@protocol ExtFSMCP;

/*!
@class ExtFSMediaController
@abstract Handles interaction with Disk Arbitration and IOKit
to determine available media.
@discussion This is the central management point for all media.
It queries the system for all media, and determines their properties and mount status.
There should only be one instance of this class.
*/
@interface ExtFSMediaController : NSObject <ExtFSMCP>
{
@private
   void *e_lock;
   id e_media;
   id e_pending;
   id e_delegate;
   u_int64_t e_smonPollInterval;
   BOOL e_smonActive;
#ifndef NOEXT2
   unsigned char e_reserved[32];
#endif
}

/*!
@method mediaController
@abstract Class method to access the global controller instance.
@discussion This method should be called once before any threads are created because
it needs access to the main run-loop.
@result Global instance.
*/
+ (ExtFSMediaController*)mediaController;

/*!
@method mediaCount
@abstract Determine the number of media objects.
@discussion One media object is created for each
disk and each partition on a disk. This is a snapshot in time,
media could be added or removed the moment after return.
@result Count of media objects.
*/
- (unsigned)mediaCount;
/*!
@method media
@abstract Access all media objects.
@discussion This is a snapshot in time, media could be added
or removed the moment after return.
@result An array of media objects.
*/
- (NSArray*)media;
/*!
@method rootMedia
@abstract Access all media that is not a child of some other media.
@discussion This is a snapshot in time, media could be added
or removed the moment after return.
@result An array of media objects.
*/
- (NSArray*)rootMedia;
/*!
@method mediaWithFSType
@abstract Access all media with a specific filesystem type.
@discussion Only media that is mounted will be included.
This is a snapshot in time, media could be added
or removed the moment after return.
@param fstype Filesystem type to match.
@result An array of media objects, or nil if no matches were found.
*/
- (NSArray*)mediaWithFSType:(ExtFSType)fstype;
/*!
@method mediaWithIOTransportBus
@abstract Access all media on a specific I/O transport bus type.
@discussion This is a snapshot in time, media could be added
or removed the moment after return.
@param busType I/O bus transport type to match.
@result An array of media objects, or nil if no matches were found.
*/
- (NSArray*)mediaWithIOTransportBus:(ExtFSIOTransportType)busType;
/*!
@method mediaWithBSDName
@abstract Search for a media object by its BSD kernel name.
@param device A string containing the device name to search for
(excluding any paths).
@result A media object matching the device name, or nil if a match was not found.
*/
- (ExtFSMedia*)mediaWithBSDName:(NSString*)device;

/*!
@method mount:on:
@abstract Mount media on specified directory.
@discussion It is possible for this method to return success,
before the mount has completed. If the mount fails later, a
ExtFSMediaNotificationOpFailure notification will be sent.
@param media Media to mount.
@param on A string containing the absolute path to the directory the media should be mounted on.
@result 0 if successful or an error code (possibly from Disk Arbitration).
*/
- (int)mount:(ExtFSMedia*)media on:(NSString*)dir;
/*!
@method unmount:force:eject:
@abstract Unmount currently mounted media.
@discussion It is possible for this method to return success,
before the unmount has completed. If the unmount fails later, a
ExtFSMediaNotificationOpFailure notification will be sent.
@param media Media to unmount.
@param force Force the unmount even if the filesystem is in use?
@param eject Eject the media if possible?
@result 0 if successful or an error code (possibly from Disk Arbitration).
*/
- (int)unmount:(ExtFSMedia*)media force:(BOOL)force eject:(BOOL)eject;

/*!
@method claimMedia:
@abstract Use this to prevent DiskArb from attempting to recognize the disk.
@discussion This is an asynchronous method. The claim is not completed
until you receive an ExtFSMediaNotificationExclusiveRequestDidComplete notification.
@param media Media to claim
*/
- (void)claimMedia:(ExtFSMedia*)media;

/*!
@method releaseClaimOnMedia:
@abstract Release a previous claim granted by claimMedia:
@param media Media to release
*/
- (void)releaseClaimOnMedia:(ExtFSMedia*)media;

- (ExtFSOpticalMediaType)opticalMediaTypeForName:(NSString*)name;
- (NSString*)opticalMediaNameForType:(ExtFSOpticalMediaType)type;

- (id)delegate;
- (void)setDelegate:(id)obj;

#ifdef DIAGNOSTIC
- (void)dumpState;
#endif

@end

@interface NSObject (ExtFSMediaControllerDelegate)

- (BOOL)allowMediaToMount:(ExtFSMedia*)media;

@end

/*!
@const ExtFSMediaNotificationAppeared
@abstract This notification is sent to the default Notification center
when new media appears. The new media object is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationAppeared;
/*!
@const ExtFSMediaNotificationDisappeared
@abstract This notification is sent to the default Notification center
when new media disappears (ie it's ejected). The vanishing 
media object is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationDisappeared;
/*!
@const ExtFSMediaNotificationMounted
@abstract This notification is sent to the default Notification center
when a device has been mounted (either by explicit request of
the application or by some other means). The mounted media object
is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationMounted;
/*!
@const ExtFSMediaNotificationUnmounted
@abstract This notification is sent to the default Notification center
when a device has been unmounted (either by explicit request of
the application or by some other means). The unmounted media object
is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationUnmounted;
/*!
@const ExtFSMediaNotificationApplicationWillMount
@abstract This notification is sent to the default Notification center
when the current application has requested a mount. This notification
does not apply to mount requests made by other applications.
The mounted media object is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationApplicationWillMount;
/*!
@const ExtFSMediaNotificationApplicationWillUnmount
@abstract This notification is sent to the default Notification center
when the current application has requested a unmount. This notification
does not apply to unmount requests made by other applications.
The mounted media object is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationApplicationWillUnmount;
/*!
@const ExtFSMediaNotificationOpFailure
@abstract This notification is sent to the default Notification center
when an asynchronous operation fails. The media object that failed is
attached and the user info dictionary contains the error information.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationOpFailure;
/*!
@const ExtFSMediaNotificationCreationFailed
@abstract This notification is sent to the default Notification center
when ExtFSMediaController fails to create a media object for a device.
A string containing the device name is attached.
@discussion This is guarranteed to be delivered on the main thread.
*/
extern NSString * const ExtFSMediaNotificationCreationFailed;

extern NSString * const ExtFSMediaNotificationClaimRequestDidComplete;

extern NSString * const ExtFSMediaNotificationDidReleaseClaim;

/*!
@const ExtMediaKeyOpFailureID
@abstract Key for ExtFSMediaNotificationOpFailure that contains one
of the operation notification types above (ExtFSMediaNotificationMounted, etc).
@discussion Will be an empty string if the operation is unknown.
*/
extern NSString * const ExtMediaKeyOpFailureID;
/*!
@const ExtMediaKeyOpFailureType
@abstract Key for ExtFSMediaNotificationOpFailure that gives a localized
name for the failure (as an NSString -- Unknown, Unmount, Eject, etc).
*/
extern NSString * const ExtMediaKeyOpFailureType;
/*!
@const ExtMediaKeyOpFailureError
@abstract Key for ExtFSMediaNotificationOpFailure to determine
the error code (as an NSNumber).
*/
extern NSString * const ExtMediaKeyOpFailureError;
/*!
@const ExtMediaKeyOpFailureErrorString
@abstract Key for ExtFSMediaNotificationOpFailure to access
a human-readable description of the error suitable for display
to a user.
*/
extern NSString * const ExtMediaKeyOpFailureErrorString;
/*!
@const ExtMediaKeyOpFailureMsgString
@abstract Key for ExtFSMediaNotificationOpFailure to access
a human-readable message describing further details about an
error code. This key may not be available for all errors.
*/
extern NSString * const ExtMediaKeyOpFailureMsgString;

/*!
@function EFSNameFromType
@abstract Converts a filesystem type id to a C string
containing the filesystem name.
@param type A valid ExtFSType id.
@result Filesystem name or nil if the type is invalid.
*/
const char* EFSNameFromType(int type);
/*!
@function EFSNSNameFromType
@abstract Converts a filesystem type id to a NSString
containing the filesystem name.
@param type A valid ExtFSType id.
@result Filesystem name or nil if the type is invalid.
*/
NSString* EFSNSNameFromType(unsigned long type);

/*!
@function EFSNSPrettyNameFromType
@abstract Converts a filesystem type id to a NSString
containing the filesystem name in a suitable format for display
to a user.
@param type A valid ExtFSType id.
@result Filesystem name or nil if the type is invalid.
*/
NSString* EFSNSPrettyNameFromType(unsigned long type);

/*!
@function EFSIOTransportNameFromType
@abstract Converts an I/O transport type id to an NSString
containing the transport name in a suitable format for display
to a user.
@param type A valid ExtFSIOTransportType id.
@result I/O transport name or nil if the type is invalid.
*/
NSString* EFSIOTransportNameFromType(unsigned long type);
