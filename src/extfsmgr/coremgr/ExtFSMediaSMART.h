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

/*!
@header ExtFSMediaSMART
@abstract Disk S.M.A.R.T. status monitoring.
@discussion S.M.A.R.T. is only available with ATA disks.
*/

/*!
@enum ExtFSMARTStatus
@constant efsSMARTInvalidTransport Invalid transport error; S.M.A.R.T. is only supported by ATA.
@constant efsSMARTOSError An operating system error occurred trying to query the S.M.A.R.T. status.
@constant efsSMARTVerified No S.M.A.R.T. errors detected.
@constant efsSMARTTestInProgress The disk's S.M.A.R.T. test is currently in progress.
@constant efsSMARTTestAbort The last/current S.M.A.R.T. test was aborted by the kernel.
@constant efsSMARTTestInterrupted The last/current S.M.A.R.T. test was interrupted.
@constant efsSMARTTestFatal A fatal error was detected.
@constant efsSMARTTestUnknownFail An unknown error was detected.
@constant efsSMARTTestElectricFail A disk electrical failure was detected.
@constant efsSMARTTestServoFail A disk motor failure was detected.
@constant efsSMARTTestReadFail A disk read failure was detected.
*/
typedef enum {
    efsSMARTInvalidTransport = -2,
    efsSMARTOSError          = -1,
    efsSMARTVerified         = 0,
    efsSMARTTestInProgress   = 1,
    efsSMARTTestAbort        = 2,
    efsSMARTTestInterrupted  = 3,
    efsSMARTTestFatal        = 20,
    efsSMARTTestUnknownFail  = 21,
    efsSMARTTestElectricFail = 22,
    efsSMARTTestServoFail    = 23,
    efsSMARTTestReadFail     = 24,
    efsSMARTStatusMax
}ExtFSMARTStatus;

/*!
@enum ExtFSMARTStatusSeverity
@constant efsSMARTSeverityInformational The S.M.A.R.T. status is not an error condition.
@constant efsSMARTSeverityWarning The S.M.A.R.T. status is a possible error condition.
@constant efsSMARTSeverityCritical The S.M.A.R.T. status is a critical error condition, and disk failure is imminent.
*/
typedef enum {
    efsSMARTSeverityInformational = 0,
    efsSMARTSeverityWarning       = 1,
    efsSMARTSeverityCritical      = 2,
    efsSMARTSeverityMax
}ExtFSMARTStatusSeverity;

/*!
@enum ExtFSSMARTEventFlag
@constant efsSMARTEventFailed Only failure notifications should be sent.
@constant efsSMARTEventVerified Verification notifications should be sent in addition to failure notifications.
*/
typedef enum {
    efsSMARTEventFailed   = 0,
    efsSMARTEventVerified = (1<<0)
}ExtFSSMARTEventFlag;

/*!
@category ExtFSMedia (ExtFSMediaSMART)
@abstract Adds S.M.A.R.T. functionality to ExtFSMedia.
*/
@interface ExtFSMedia (ExtFSMediaSMART)

/*!
@method SMARTStatus
@abstract Obtain the S.M.A.R.T. status of the object.
@discussion This is a convenience method. For more information
see [ExtFSMediaController(ExtFSMediaControllerSMART) SMARTStatusForMedia:parentDisk:].
@result An ExtFSMARTStatus status code.
*/
- (ExtFSMARTStatus)SMARTStatus;

@end

/*!
@defined EXTFS_DEFAULT_SMART_POLL
@abstract Default S.M.A.R.T. status poll interval -- in milliseconds.
*/
#define EXTFS_DEFAULT_SMART_POLL 300000

/*!
@category ExtFSMediaController (ExtFSMediaControllerSMART)
@abstract Adds S.M.A.R.T. functionality to ExtFSMediaController.
*/
@interface ExtFSMediaController (ExtFSMediaControllerSMART)

/*!
@method SMARTStatusForMedia:parentDisk:
@abstract Obtain the S.M.A.R.T. status for a media object.
@discussion Disk partitions cannot be queried directly, so this
method will find the object representing the disk as a whole and
query that for its S.M.A.R.T. status.
@param media The object to query for its S.M.A.R.T. status.
@param disk The actual disk object queried. Can be nil, if the caller is not interested.
@result An ExtFSMARTStatus status code.
*/
- (ExtFSMARTStatus)SMARTStatusForMedia:(ExtFSMedia*)media parentDisk:(ExtFSMedia**)disk;
/*!
@method SMARTStatusDescription:
@abstract Given a ExtFSMARTStatus code, create a dictionary containing
information that can be presented to a user.
@param ExtFSMARTStatus code.
@result A dictionary, containing one or more ExtFSMediaKeySMARTStatus* keys.
*/
- (NSDictionary*)SMARTStatusDescription:(ExtFSMARTStatus)status;
/*!
@method monitorSMARTStatusWithPollInterval:flags:
@abstract This starts a background thread to monitor all ATA disk drives
for their S.M.A.R.T. status.
@discussion The disks are polled when the specified interval
expires. If an error condition is detected an ExtFSMediaNotificationSMARTStatus
event will be posted to the default Notification Center on the main thread.
@param interval The poll interval in milliseconds.
@param flags One or more ExtFSSMARTEventFlag flags. These can modify the
the criteria that triggers a notification.
@result YES if the monitor was started. NO if interval is 0, or the monitor
is already active.
*/
- (BOOL)monitorSMARTStatusWithPollInterval:(u_int64_t)interval flags:(ExtFSSMARTEventFlag)flags;
/*!
@method monitorSMARTStatusStop
@abstract Stop monitoring disk S.M.A.R.T. status.
*/
- (void)monitorSMARTStatusStop;

@end

/*!
@constant ExtFSMediaNotificationSMARTStatus
@abstract This event is sent by the disk monitor. The object
will be the affected media object, and the user info will be
a dictionary containing one or more ExtFSMediaKeySMARTStatus keys.
*/
extern NSString * const ExtFSMediaNotificationSMARTStatus;
/*!
@constant ExtFSMediaKeySMARTStatus
@abstract Key for ExtFSMediaNotificationSMARTStatus event dictionary.
@discussion An NSNumber containing the ExtFSMARTStatus code.
*/
extern NSString * const ExtFSMediaKeySMARTStatus;
/*!
@constant ExtFSMediaKeySMARTStatusSeverity
@abstract Key for ExtFSMediaNotificationSMARTStatus event dictionary.
@discussion An NSNumber containing the ExtFSMARTStatusSeverity
associated with the ExtFSMARTStatus code.
*/
extern NSString * const ExtFSMediaKeySMARTStatusSeverity;
/*!
@constant ExtFSMediaKeySMARTStatusDescription
@abstract Key for ExtFSMediaNotificationSMARTStatus event dictionary.
@discussion A string containing a description of the ExtFSMARTStatus
code suitable for presentation to a user. This key may not
exist if no suitable description could be found.
*/
extern NSString * const ExtFSMediaKeySMARTStatusDescription;
/*!
@constant ExtFSMediaKeySMARTStatusSeverityDescription
@abstract Key for ExtFSMediaNotificationSMARTStatus event dictionary.
@discussion A string containing a description of the ExtFSMARTStatusSeverity
code suitable for presentation to a user.
*/
extern NSString * const ExtFSMediaKeySMARTStatusSeverityDescription;
