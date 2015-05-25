/*
* Copyright 2003-2004 Brian Bergstrand.
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

#import <PreferencePanes/PreferencePanes.h>

@class ExtFSMedia;

@interface ExtFSManager : NSPreferencePane <NSOutlineViewDataSource, NSOutlineViewDelegate>
{
   NSMutableArray *e_volData;
   __weak ExtFSMedia *e_curSelection;
   NSString *donateTitle;
   BOOL e_infoButtonAlt;
}
@property (weak) IBOutlet NSButton *e_mountButton;
@property (weak) IBOutlet NSButton *e_ejectButton;
@property (weak) IBOutlet NSButton *e_infoButton;
@property (weak) IBOutlet NSImageView *e_diskIconView;
@property (weak) IBOutlet NSOutlineView *e_vollist;
@property (weak) IBOutlet NSTabView *e_tabs;

@property (weak) IBOutlet NSButton *e_mountReadOnlyBox;
@property (weak) IBOutlet NSButton *e_dontAutomountBox;
@property (weak) IBOutlet NSButton *e_ignorePermsBox;
@property (weak) IBOutlet NSButton *e_indexedDirsBox;
@property (weak) IBOutlet NSTextField *e_optionNoteText;

@property (weak) IBOutlet NSTextField *e_copyrightText;
@property (unsafe_unretained) IBOutlet NSTextView *e_infoText;

@property (weak) IBOutlet NSProgressIndicator *e_startupProgress;
@property (weak) IBOutlet NSTextField *e_startupText;

@property (weak) IBOutlet NSProgressIndicator *e_opProgress;

- (IBAction)click_readOnly:(id)sender;
- (IBAction)click_automount:(id)sender;
- (IBAction)click_ignorePerms:(id)sender;
- (IBAction)click_indexedDirs:(id)sender;
- (IBAction)donate:(id)sender;

- (void)doMount:(id)sender;
- (void)doEject:(id)sender;
- (void)doOptions:(id)sender;

- (void)savePrefs;

- (void)doMediaSelection:(ExtFSMedia*)media;

@end

#define EXTFS_MGR_PREF_ID @"net.sourceforge.ext2fs.mgr"

#define ExtLocalizedString(key, comment) \
NSLocalizedStringFromTableInBundle(key,nil, [self bundle], comment)
