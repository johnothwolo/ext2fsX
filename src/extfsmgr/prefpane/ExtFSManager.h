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

@interface ExtFSManager : NSPreferencePane
{
   IBOutlet NSButton *e_mountButton;
   IBOutlet NSButton *e_ejectButton;
   IBOutlet NSButton *e_infoButton;
   IBOutlet NSImageView *e_diskIconView;
   IBOutlet NSOutlineView *e_vollist;
   IBOutlet NSTabView *e_tabs;
   
   IBOutlet id e_mountReadOnlyBox;
   IBOutlet id e_dontAutomountBox;
   IBOutlet id e_ignorePermsBox;
   IBOutlet id e_indexedDirsBox;
   IBOutlet id e_optionNoteText;
   
   IBOutlet id e_copyrightText;
   IBOutlet id e_infoText;
   
   IBOutlet id e_startupProgress;
   IBOutlet id e_startupText;
   
   IBOutlet id e_opProgress;
   
   id e_volData, e_curSelection;
   NSString *donateTitle;
   BOOL e_infoButtonAlt;
}

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

/* Delegate stuff */
- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item;
- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item;
- (id)outlineView:(NSOutlineView *)outlineView
   objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item;

@end

#define EXTFS_MGR_PREF_ID @"net.sourceforge.ext2fs.mgr"

#define ExtLocalizedString(key, comment) \
NSLocalizedStringFromTableInBundle(key,nil, [self bundle], comment)
