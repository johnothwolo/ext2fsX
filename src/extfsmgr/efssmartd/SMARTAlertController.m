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

#import "SMARTAlertController.h"

static NSPoint lastDisplay = {0.0};

@implementation SMARTAlertController

- (id)initWithMedia:(ExtFSMedia*)media status:(NSDictionary*)dict
{
    if ((self = [super initWithWindowNibName:@"SMARTAlert" owner:self])) {
        e_media = [media retain];
        e_status = [dict retain];
    }
    return (self);
}

- (IBAction)performClose:(id)sender
{
    [super close];
}

- (IBAction)showWindow:(id)sender
{
    NSWindow *me;
    
    me = [super window];
    // If we have another on screen window, offset ourself from that
    if (lastDisplay.x > 0.0 || lastDisplay.x < 0.0) {
        NSRect r = [me frame];
        lastDisplay.x += 25.0;
        lastDisplay.y -= 25.0;
        r.origin = lastDisplay;
        [me setFrame:r display:NO];
    } else {
        [me center];
        lastDisplay = [me frame].origin;
    }
    [me setLevel:kCGStatusWindowLevel];
    [super showWindow:sender];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [e_media release]; e_media = nil;
    [e_status release]; e_status = nil;
    if (nil == [NSApp windows])
        lastDisplay = NSMakePoint(0.0,0.0);
    [self autorelease];
}

- (void)windowDidLoad
{
    NSString *title;
    
    [[super window] setTitle:[e_status objectForKey:ExtFSMediaKeySMARTStatusSeverityDescription]];
    [e_icon setImage:[e_media icon]];
    [e_text setStringValue:[e_status objectForKey:ExtFSMediaKeySMARTStatusDescription]];
    
    if (nil == (title = [e_media volName]))
        if (nil == (title = [e_media mountPoint]))
            if (nil == (title = [e_media ioRegistryName]))
                // Last resort is the device name
                title = [e_media bsdName];
    [e_title setStringValue:title];
}

@end
