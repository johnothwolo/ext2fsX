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

#import "ExtFSManagerAdditions.h"

@implementation ExtFSManager (Additions)

/* Note: This method contains internal knowledge of the loginwindow preferences file.
 * This is usually a bad thing, but Apple's own LoginItems API is just a C source file that
 * does the same as below, only using CoreFoundataion.
 */
 
#define LOGINWINDOW_DOMAIN @"loginwindow"
#define LOGINITEMS_KEY @"AutoLaunchedApplicationDictionary"
#define HIDE_KEY @"Hide"
#define PATH_KEY @"Path"

- (BOOL)addLoginItem:(NSString *)path
{
    NSDictionary *myEntry = nil;
    NSMutableDictionary *loginDict;
    NSMutableArray *entries;
    NSUserDefaults *ud = [[NSUserDefaults alloc] init];
    NSEnumerator *en;
    BOOL added = NO;

    // Make sure we are up to date with what's on disk
    [ud synchronize];
    // Get the loginwindow dict
    loginDict = [[ud persistentDomainForName:LOGINWINDOW_DOMAIN] mutableCopyWithZone:nil];
    if (nil == loginDict)
        loginDict = [[NSMutableDictionary alloc] initWithCapacity:1];

    // Get the login items array
    entries = [[loginDict objectForKey:LOGINITEMS_KEY] mutableCopyWithZone:nil];
    if (nil == entries)
        entries = [[NSMutableArray alloc] initWithCapacity:1];

    // Make sure the entry does not exist
    en = [entries objectEnumerator];
    while ((myEntry = [en nextObject])) {
        if ([path isEqualToString:[myEntry objectForKey:PATH_KEY]]) {
            added = YES;
            myEntry = nil;
            goto add_exit;
        }
    }

    // Build our entry
    myEntry = [[NSDictionary alloc] initWithObjectsAndKeys:
         [NSNumber numberWithBool:NO], HIDE_KEY,
         path, PATH_KEY,
         nil];

    if (loginDict && entries && myEntry) {
        // Add our entry
        [entries insertObject:myEntry atIndex:0];

        [loginDict setObject:entries forKey:LOGINITEMS_KEY];

        // Update the loginwindow prefs
        [ud removePersistentDomainForName:LOGINWINDOW_DOMAIN];
        [ud setPersistentDomain:loginDict forName:LOGINWINDOW_DOMAIN];
        [ud synchronize];
        added = YES;
    }

add_exit:
    // Release everything
    [myEntry release];
    [entries release];
    [loginDict release];
    [ud release];
    return (added);
}

@end