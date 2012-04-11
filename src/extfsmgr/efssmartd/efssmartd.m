/*
* Copyright 2004 Brian Bergstrand.
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

#import <ExtFSDiskManager/ExtFSDiskManager.h>

#import "extfsmgr.h"
#import "SMARTAlertController.h"

@interface EAppDelegate : NSObject {
}

@end

@implementation EAppDelegate

- (void)volSMARTStatus:(NSNotification*)notification
{
    NSDictionary *info = [notification userInfo];
    int status = [[info objectForKey:ExtFSMediaKeySMARTStatus] intValue];
    if (status > efsSMARTVerified && status != efsSMARTTestInProgress)  {
        SMARTAlertController *win = 
        [[SMARTAlertController alloc] initWithMedia:[notification object]
           status:info];
        [win showWindow:nil];
        [NSApp requestUserAttention:NSCriticalRequest];
    } else {
        NSLog(@"efssmartd: %@ for disk %@: '%@'.",
            [info objectForKey:ExtFSMediaKeySMARTStatusSeverityDescription],
            [[notification object] ioRegistryName],
            [info objectForKey:ExtFSMediaKeySMARTStatusDescription]);
    }
}

#define EXT_MIN_SMART_POLL 30 * 1000
- (void)applicationWillFinishLaunching:(NSNotification *)notification
{
    u_int64_t interval = 0ULL;
    NSDictionary *prefs;
    ExtFSMediaController *mc = [ExtFSMediaController mediaController];
    
    [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(volSMARTStatus:)
            name:ExtFSMediaNotificationSMARTStatus
            object:nil];
    
    prefs = [[NSUserDefaults standardUserDefaults] objectForKey:EXT_PREF_KEY_SMARTD];
    if (prefs) {
        NSNumber *num = [prefs objectForKey:EXT_PREF_KEY_SMARTD_MON_INTERVAL];
        if (num)
            interval = [num unsignedIntValue] * 1000;
    }
    
    if (interval < EXT_MIN_SMART_POLL)
        interval = EXTFS_DEFAULT_SMART_POLL;
    
    // Start monitoring -- efsSMARTEventVerified for verification events
    [mc monitorSMARTStatusWithPollInterval:interval flags:efsSMARTEventFailed];
}

@end

@implementation NSApplication (NoWarnings)

- (void)buttonPressed:(id)sender
{
    // NSApplicationMain loads a default NIB/Menu bar when we don't provide one.
    // This nib expects this method to be present in the main class.
}

@end

int main (int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    // Add in global prefs
    [[NSUserDefaults standardUserDefaults] addSuiteNamed:EXT_PREF_ID];
    
    [[NSApplication sharedApplication] setDelegate:[[EAppDelegate alloc] init]];
    [pool release];
    
    return (NSApplicationMain(argc,argv));
}
