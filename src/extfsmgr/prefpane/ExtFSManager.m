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
"@(#) $Id: ExtFSManager.m,v 1.35 2006/12/03 22:37:18 bbergstrand Exp $";

#import <sys/types.h>
#import <sys/sysctl.h>
#import <unistd.h>

#import <ExtFSDiskManager/ExtFSDiskManager.h>
#import "ExtFSManager.h"
#import "ExtFSManagerAdditions.h"

/* Note: For the "Options" button, the image/title
      is the reverse of the actual action. */

#define EXT_TOOLBAR_ACTION_COUNT 3
#define EXT_TOOLBAR_ACTION_MOUNT @"Mount"
#define EXT_TOOLBAR_ACTION_EJECT @"Eject"
#define EXT_TOOLBAR_ACTION_INFO @"Info"

#define EXT_TOOLBAR_ALT_ACTION_MOUNT @"Unmount"
#define EXT_TOOLBAR_ALT_ACTION_INFO @"Options"

// Object property support
#define EXT_OBJ_PROPERTY_RESTORE_ACTIONS @"Restore"
#define EXT_OBJ_PROPERTY_TITLE_COLOR @"TitleColor"

static NSMutableDictionary* MediaProps(ExtFSMedia *m);

static void ExtSwapButtonState(id button, BOOL swapImage);
static id ExtMakeInfoTitle(NSString *title);
static void ExtSetPrefVal(ExtFSMedia *media, id key, id val);
static void AddRestoreAction(ExtFSMedia *m, id key, id val);
static BOOL PlayRestoreActions(ExtFSMedia *m);
static int FindPIDByPathOrName(const char *path, const char *name, uid_t user);

#import "extfsmgr.h"

static NSMutableDictionary *e_prefRoot = nil, *e_prefGlobal = nil,
    *e_prefMedia = nil, *e_prefMgr = nil, *e_iconCache = nil;;
static NSString *e_bundleid = nil;
static BOOL e_prefsChanged = NO;

/* Localized strings */
static NSString *e_yes, *e_no, *e_bytes;
static NSString *e_monikers[] =
      {@"bytes", @"KB", @"MB", @"GB", @"TB", @"PB", @"EB", @"ZB", @"YB", nil};
//     KiloByte, MegaByte, GigaByte, TeraByte, PetaByte, ExaByte, ZetaByte, YottaByte
//bytes  2^10,     2^20,     2^30,     2^40,     2^50,     2^60,    2^70,     2^80

static int e_operations = 0;
#define BeginOp() do { \
if (0 == e_operations) [e_opProgress startAnimation:nil]; \
++e_operations; \
} while(0)

#define EndOp() do { \
--e_operations; \
if (0 == e_operations) [e_opProgress stopAnimation:nil]; \
if (e_operations < 0) e_operations = 0; \
} while(0)

@implementation ExtFSManager : NSPreferencePane
@synthesize e_mountButton;
@synthesize e_ejectButton;
@synthesize e_infoButton;
@synthesize e_diskIconView;
@synthesize e_vollist;
@synthesize e_tabs;
@synthesize e_mountReadOnlyBox;
@synthesize e_dontAutomountBox;
@synthesize e_ignorePermsBox;
@synthesize e_indexedDirsBox;
@synthesize e_optionNoteText;
@synthesize e_copyrightText;
@synthesize e_infoText;
@synthesize e_startupProgress;
@synthesize e_startupText;
@synthesize e_opProgress;

/* Private */

- (int)probeDisks
{
   ExtFSMediaController *mc;
   mc = [ExtFSMediaController mediaController];
   
   e_volData = [[NSMutableArray alloc] initWithArray:
      [[mc rootMedia] sortedArrayUsingSelector:@selector(compare:)]];
   
   // Preload the icon data (this can take a few seconds if loading from disk)
   [e_volData makeObjectsPerformSelector:@selector(icon)];
   
   return (0);
}

- (void)updateMountState:(ExtFSMedia*)media
{
   NSString *tmp;
   
   tmp = [(NSButtonCell*)[e_mountButton cell] representedObject];
   if ([media canMount] && NO == [[media mountPoint] isEqualToString:@"/"]) {
      [e_mountButton setEnabled:YES];
      if ([media isMounted]) {
         if ([tmp isEqualTo:EXT_TOOLBAR_ACTION_MOUNT]) {
            ExtSwapButtonState(e_mountButton, YES);
            [(NSButtonCell*)[e_mountButton cell]
               setRepresentedObject:EXT_TOOLBAR_ALT_ACTION_MOUNT];
         }
      } else 
         goto unmount_state;
   } else {
      [e_mountButton setEnabled:NO];
unmount_state:
      if ([tmp isEqualTo:EXT_TOOLBAR_ALT_ACTION_MOUNT]) {
         ExtSwapButtonState(e_mountButton, YES);
         [(NSButtonCell*)[e_mountButton cell]
            setRepresentedObject:EXT_TOOLBAR_ACTION_MOUNT];
      }
   }
}

- (void)mediaAdd:(NSNotification*)notification
{
   ExtFSMedia *media, *parent;
   
   media = [notification object];
#ifdef DIAGNOSTIC
   NSLog(@"ExtFSM: **** Media '%s' appeared ***\n", BSDNAMESTR(media));
#endif

   if ((parent = [media parent])) {
      if (![e_volData containsObject:parent] && nil == [parent parent]) {
         [e_volData addObject:parent];
         goto exit;
      } else {
         if ([e_vollist isItemExpanded:parent])
            [e_vollist reloadItem:parent reloadChildren:YES];
         else
            [e_vollist reloadItem:parent reloadChildren:NO];
         return;
      }
   } else if (NO == [e_volData containsObject:media]){
      [e_volData addObject:media];
   }

exit:
   [e_volData sortUsingSelector:@selector(compare:)];
   [e_vollist reloadData];
}

#define ExtSetDefaultState() \
do { \
e_curSelection = nil; \
[e_mountButton setEnabled:NO]; \
[e_ejectButton setEnabled:NO]; \
[e_infoButton setEnabled:NO]; \
[e_diskIconView setImage:nil]; \
[self doOptions:self]; \
[e_tabs selectTabViewItemWithIdentifier:@"Startup"]; \
} while (0)

- (void)mediaRemove:(NSNotification*)notification
{
   ExtFSMedia *media, *parent;
   
   EndOp();
   media = [notification object];
   
#ifdef DIAGNOSTIC
   NSLog(@"ExtFSM: **** Media '%s' disappeared ***\n", BSDNAMESTR(media));
#endif

   if ((parent = [media parent])) {
      [e_vollist reloadItem:parent reloadChildren:YES];
      return;
   }
   
   if (YES == [e_volData containsObject:media]) {
      [e_volData removeObject:media];
      [e_vollist reloadData];
      if (-1 == [e_vollist selectedRow])
         ExtSetDefaultState();
   }
}

- (void)volMount:(NSNotification*)notification
{
   ExtFSMedia *media;
   
   EndOp();
   media = [notification object];
#ifdef DIAGNOSTIC
   NSLog(@"ExtFSM: **** Media '%s' mounted ***\n", BSDNAMESTR(media));
#endif
   // Replay any pref restore options, and save the modified prefs
   if (PlayRestoreActions(media))
      [self savePrefs];
   
   if (media == e_curSelection)
      [self doMediaSelection:media];
   [e_vollist reloadItem:media];
}

- (void)volUnmount:(NSNotification*)notification
{
   ExtFSMedia *media;
   
   EndOp();
   media = [notification object];
#ifdef DIAGNOSTIC
   NSLog(@"ExtFSM: **** Media '%s' unmounted ***\n", BSDNAMESTR([notification object]));
#endif
   if (media == e_curSelection)
      [self doMediaSelection:media];
   [e_vollist reloadItem:media];
}

- (void)volInfoUpdated:(NSNotification*)notification
{
    [e_vollist reloadItem:[notification object]];
}

- (void)childChanged:(NSNotification*)notification
{

}

- (void)mediaError:(NSNotification*)notification
{
   NSString *op, *device, *errStr, *msg, *errMsg;
   NSNumber *err;
   ExtFSMedia *media = [notification object];
   NSDictionary *info = [notification userInfo];
   NSWindow *win;
   
   // Replay any pref restore options, and save the modified prefs
   if (PlayRestoreActions(media))
      [self savePrefs];
   
   EndOp();
   device = [media bsdName];
   err = info[ExtMediaKeyOpFailureError];
   op = info[ExtMediaKeyOpFailureType];
   errStr = info[ExtMediaKeyOpFailureErrorString];
   errMsg = info[ExtMediaKeyOpFailureMsgString];
   
   msg = [NSString stringWithFormat:
            ExtLocalizedString(@"Command: %@\nDevice: %@\nMessage: %@\nError: 0x%X", ""),
            op, device, (errMsg ? [NSString stringWithFormat:@"%@ (%@)", errStr, errMsg] : errStr), [err intValue]];
   
   win = [NSApp keyWindow];
   if (nil == [win attachedSheet]) {
        NSBeginCriticalAlertSheet(ExtLocalizedString(@"Disk Error", ""), @"OK",
            nil, nil, win, nil, nil, nil, nil, @"%@", msg);
   } else {
        NSRunCriticalAlertPanel(ExtLocalizedString(@"Disk Error", ""),
            @"%@", @"OK", nil, nil, msg);
   }
}

- (void)startup
{
   NSString *title;
   ExtFSMedia *media;
   NSEnumerator *en;
   
   [e_vollist setEnabled:NO];
   
   (void)[self probeDisks];
   
   // Startup complete
   [e_vollist reloadData];
   
   // Register for notifications
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(mediaAdd:)
            name:ExtFSMediaNotificationAppeared
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(mediaRemove:)
            name:ExtFSMediaNotificationDisappeared
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(volMount:)
            name:ExtFSMediaNotificationMounted
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(volUnmount:)
            name:ExtFSMediaNotificationUnmounted
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(volInfoUpdated:)
            name:ExtFSMediaNotificationUpdatedInfo
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(childChanged:)
            name:ExtFSMediaNotificationChildChange
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:self
            selector:@selector(mediaError:)
            name:ExtFSMediaNotificationOpFailure
            object:nil];
   
   // Exapnd the root items
   en = [e_volData objectEnumerator];
   while ((media = [en nextObject])) {
      [e_vollist expandItem:media];
   }
   
   [e_startupProgress stopAnimation:self];
   [e_startupProgress displayIfNeeded];
   title = ExtLocalizedString(@"Please select a disk or volume",
      "Startup Message");
   [e_startupText setStringValue:title];
   
   [e_vollist setEnabled:YES];
}

// args are NSString*
#define ExtInfoInsert(title, value) \
do { \
line = ExtMakeInfoTitle((title)); \
data = [@" " stringByAppendingString:(value)]; \
data = [data stringByAppendingString:@"\n"]; \
[line appendAttributedString: \
   [[NSAttributedString alloc] initWithString:data]]; \
[e_infoText insertText:line]; \
} while(0)

#define ExtFmtInt(i) \
[intFmt stringForObjectValue:@(i)]
#define ExtFmtQuad(q) \
[intFmt stringForObjectValue:@(q)]
#define ExtFmtFloat(f) \
[floatFmt stringForObjectValue:@(f)]

- (void)generateInfo:(ExtFSMedia*)media
{
   NSString *data;
   NSMutableAttributedString *line;
   NSNumberFormatter *floatFmt, *intFmt;
   ExtFSMediaController *mc = [ExtFSMediaController mediaController];
   uint64_t size;
   BOOL mounted;
   
   intFmt = [[NSNumberFormatter alloc] init];
   intFmt.formatterBehavior = NSNumberFormatterBehavior10_0;
   [intFmt setFormat:@",0"];
   [intFmt setLocalizesFormat:YES];
   floatFmt = [[NSNumberFormatter alloc] init];
   floatFmt.formatterBehavior = NSNumberFormatterBehavior10_0;
   [floatFmt setFormat:@",0.00"];
   [floatFmt setLocalizesFormat:YES];
   NSByteCountFormatter *byteFormatter = [[NSByteCountFormatter alloc] init];
   byteFormatter.includesActualByteCount = YES;
   NSByteCountFormatter *blockFormatter = [[NSByteCountFormatter alloc] init];
   blockFormatter.allowedUnits = NSByteCountFormatterUseBytes;

   mounted = [media isMounted];
   
   [e_infoText setEditable:YES];
   [e_infoText setString:@""];
   
   ExtInfoInsert(ExtLocalizedString(@"IOKit Name", ""),
      ExtLocalizedString([media ioRegistryName], ""));
   
   ExtInfoInsert(ExtLocalizedString(@"Device", ""),
      [media bsdName]);
   
   if (NO == mounted) {
      ExtInfoInsert(ExtLocalizedString(@"Connection Bus", ""),
         [media transportName]);
      ExtInfoInsert(ExtLocalizedString(@"Connection Type", ""),
         EFSIOTransportNameFromType([media transportType]));
   }
   
   if ([media isWholeDisk]) {
      NSDictionary *smartInfo =
         [mc SMARTStatusDescription:[media SMARTStatus]];
      ExtInfoInsert(ExtLocalizedString(@"S.M.A.R.T. Status", ""),
         smartInfo[ExtFSMediaKeySMARTStatusDescription]);
   }
   
   ExtInfoInsert(ExtLocalizedString(@"Ejectable", ""),
      ([media isEjectable] ? e_yes : e_no));
   
   ExtInfoInsert(ExtLocalizedString(@"DVD/CD", ""),
      ([media isOptical] ? [e_yes stringByAppendingFormat:@" (%@)", [mc opticalMediaNameForType:[media opticalMediaType]]] : e_no));
   
   data = ExtLocalizedString(@"Not Mounted", "");
   ExtInfoInsert(ExtLocalizedString(@"Mount Point", ""),
      (mounted ? [media mountPoint] : data));
   
   ExtInfoInsert(ExtLocalizedString(@"Writable", ""),
      ([media isWritable] ? e_yes : e_no));
   
   if ([media canMount]) {
      data = [media fsName];
      ExtInfoInsert(ExtLocalizedString(@"Filesystem", ""), data ? data : @"");
      
      data = [media uuidString];
      ExtInfoInsert(ExtLocalizedString(@"Volume UUID", ""),
         (data ? data : @""));
   }
   
   if (mounted) {
      data = [media volName];
      ExtInfoInsert(ExtLocalizedString(@"Volume Name", ""),
         (data ? data : @""));
      
      ExtInfoInsert(ExtLocalizedString(@"Permissions Enabled", ""),
         ([media hasPermissions] ? e_yes : e_no));
      
      ExtInfoInsert(ExtLocalizedString(@"Supports Journaling", ""),
         ([media hasJournal] ? e_yes : e_no));
      
      ExtInfoInsert(ExtLocalizedString(@"Journaled", ""),
         ([media isJournaled] ? e_yes : e_no));
      
      ExtInfoInsert(ExtLocalizedString(@"Supports Sparse Files", ""),
         ([media hasSparseFiles] ? e_yes : e_no));
      
      ExtInfoInsert(ExtLocalizedString(@"Case Sensitive Names", ""),
         ([media isCaseSensitive] ? e_yes : e_no));
      
      ExtInfoInsert(ExtLocalizedString(@"Case Preserved Names", ""),
         ([media isCasePreserving] ? e_yes : e_no));
      
      size = [media size]; // expensive conversion on G5
      data = [byteFormatter stringFromByteCount:size];
      ExtInfoInsert(ExtLocalizedString(@"Size", ""), data);
      
      size = [media availableSize];  // expensive conversion on G5
      data = [byteFormatter stringFromByteCount:size];
      ExtInfoInsert(ExtLocalizedString(@"Available Space", ""), data);
      
      data = [blockFormatter stringFromByteCount:[media blockSize]];
      ExtInfoInsert(ExtLocalizedString(@"Block Size", ""), data);
      
      data = [NSString stringWithFormat:@"%@", ExtFmtQuad([media blockCount])];
      ExtInfoInsert(ExtLocalizedString(@"Number of Blocks", ""), data);
      
      data = [NSString stringWithFormat:@"%@", ExtFmtQuad([media fileCount])];
      ExtInfoInsert(ExtLocalizedString(@"Number of Files", ""), data);
      
      data = [NSString stringWithFormat:@"%@", ExtFmtQuad([media dirCount])];
      ExtInfoInsert(ExtLocalizedString(@"Number of Directories", ""), data);
      
      if ([media isExtFS]) {
         [e_infoText insertText:@"\n"];
         line = ExtMakeInfoTitle(ExtLocalizedString(@"Ext2/3 Specific", ""));
         [line setAlignment:NSCenterTextAlignment range:NSMakeRange(0, [line length])];
         [e_infoText insertText:line];
         [e_infoText insertText:@"\n\n"];
         
         ExtInfoInsert(ExtLocalizedString(@"Indexed Directories", ""),
            ([media hasIndexedDirs] ? e_yes : e_no));
         
         ExtInfoInsert(ExtLocalizedString(@"Large Files", ""),
            ([media hasLargeFiles] ? e_yes : e_no));
      }
      
   } else {// mounted
      size = [media size]; // expensive conversion on G5
      data = [byteFormatter stringFromByteCount:size];
      ExtInfoInsert(ExtLocalizedString(@"Device Size", ""), data);
      
      data = [blockFormatter stringFromByteCount:[media blockSize]];
      ExtInfoInsert(ExtLocalizedString(@"Device Block Size", ""), data);
   }
   
   // Scroll back to the top
   [e_infoText scrollRangeToVisible:NSMakeRange(0,0)]; 
   
   [e_infoText setEditable:NO];
}

#undef ExtInfoInsert
#undef ExtFmtInt
#undef ExtFmtQuad
#undef ExtFmtFloat

- (void)setOptionState:(ExtFSMedia*)media
{
   NSCellStateValue state;
   NSMutableDictionary *dict;
   NSNumber *boolVal;
   BOOL mediaRO;
   
   mediaRO = [media isOptical];
   
   dict = e_prefMedia[[media uuidString]];
   
   boolVal = dict[EXT_PREF_KEY_DIRINDEX];
   state = ([media hasIndexedDirs] || (boolVal && [boolVal boolValue]) ? NSOnState : NSOffState);
   [e_indexedDirsBox setState:state];
   if (NSOnState == state || mediaRO)
      [e_indexedDirsBox setEnabled:NO];
   else
      [e_indexedDirsBox setEnabled:YES];
   
   if (!dict)
      return;
   
   boolVal = dict[EXT_PREF_KEY_NOAUTO];
   state = ([boolVal boolValue] ? NSOnState : NSOffState);
   [e_dontAutomountBox setState:state];
   
   boolVal = dict[EXT_PREF_KEY_NOPERMS];
   state = ([boolVal boolValue] ? NSOnState : NSOffState);
   [e_ignorePermsBox setState:state];
   
   boolVal = dict[EXT_PREF_KEY_RDONLY];
   state = ([boolVal boolValue] || mediaRO ? NSOnState : NSOffState);
   [e_mountReadOnlyBox setState:state];
   if (!mediaRO)
      [e_mountReadOnlyBox setEnabled:YES];
   else
      [e_mountReadOnlyBox setEnabled:NO];
}

- (void)doMediaSelection:(ExtFSMedia*)media
{
   if (!media) {
      ExtSetDefaultState();
      return;
   }
   
   e_curSelection = media;
#ifdef TRACE
   NSLog(@"ExtFSM: media %@ selected, canMount=%d",
      [media bsdName], [media canMount]);
#endif
   
   /* Change state of buttons, options, info etc */
   [self generateInfo:media];
   
   [e_diskIconView setImage:[media icon]];
   
   if ([media isExtFS]) {
      if (nil != [media uuidString]) {
         [self setOptionState:media];
         [e_infoButton setEnabled:YES];
      } else {
         NSLog(@"ExtFSM: Options for '%@' disabled -- missing UUID.\n",
            [media bsdName]);
         [e_infoButton setEnabled:NO];
      }
   } else
      [e_infoButton setEnabled:NO];

   [self doOptions:self];
   
   [self updateMountState:media];
   
   if ([media isEjectable])
      [e_ejectButton setEnabled:YES];
   else
      [e_ejectButton setEnabled:NO];
}

/* Public */

- (IBAction)click_readOnly:(id)sender
{
   NSNumber *boolVal;
   
#ifdef TRACE
   NSLog(@"ExtFSM: readonly clicked.\n");
#endif
   
   boolVal =
      (NSOnState == [e_mountReadOnlyBox state] ? @YES : @NO);
   ExtSetPrefVal(e_curSelection, EXT_PREF_KEY_RDONLY, boolVal);
}

- (IBAction)click_automount:(id)sender
{
   NSNumber *boolVal;
   
#ifdef TRACE
   NSLog(@"ExtFSM: automount clicked.\n");
#endif

   boolVal =
      (NSOnState == [e_dontAutomountBox state] ? @YES : @NO);
   ExtSetPrefVal(e_curSelection, EXT_PREF_KEY_NOAUTO, boolVal);
}

- (IBAction)click_ignorePerms:(id)sender
{
    NSNumber *boolVal;
    
    boolVal =
       (NSOnState == [e_ignorePermsBox state] ? @YES : @NO);
    ExtSetPrefVal(e_curSelection, EXT_PREF_KEY_NOPERMS, boolVal);
}

- (IBAction)click_indexedDirs:(id)sender
{
   NSNumber *boolVal;
#ifdef TRACE
   NSLog(@"ExtFSM: indexed dirs clicked.\n");
#endif

   boolVal =
      (NSOnState == [e_indexedDirsBox state] ? @YES : @NO);
   ExtSetPrefVal(e_curSelection, EXT_PREF_KEY_DIRINDEX, boolVal);
}

- (void)doMount:(id)sender
{
   int err, mount;
   
   mount = ![e_curSelection isMounted];
#ifdef TRACE
   NSLog(@"ExtFSM: %@ '%@'.\n", (mount ? @"Mounting" : @"Unmounting"),
      [e_curSelection bsdName]);
#endif
   
   if (mount) {
      // Make sure 'Don't Automount' is not set.
      NSNumber *val;
      NSMutableDictionary *dict = e_prefMedia[[e_curSelection uuidString]];
      val = dict[EXT_PREF_KEY_NOAUTO];
      if (dict && val && [val boolValue]) {
         ExtSetPrefVal(e_curSelection, EXT_PREF_KEY_NOAUTO, @NO);
         AddRestoreAction(e_curSelection, EXT_PREF_KEY_NOAUTO, val);
      }
   }
   
   // Save the prefs so mount will behave correctly if an Ext2 disk was changed.
   [self savePrefs];
   
   if (mount)
      err = [[ExtFSMediaController mediaController] mount:e_curSelection on:nil];
   else
      err = [[ExtFSMediaController mediaController] unmount:e_curSelection
         force:NO eject:NO];
   if (0 == err) {
      BeginOp();
      [self updateMountState:e_curSelection];
   } else {
      NSString *op = (mount ? @"mount" : @"unmount");
      op = ExtLocalizedString(op, "");
      NSBeginCriticalAlertSheet(ExtLocalizedString(@"Disk Error", ""),
         @"OK", nil, nil, [NSApp keyWindow], nil, nil, nil, nil,
         @"%@ %@ '%@'. Error = %d.",
            ExtLocalizedString(@"Could not", ""), op,
            [e_curSelection bsdName], err);
   }
}

- (IBAction)donate:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:@"http://www.bergstrand.org/brian/donate"]];
}

- (void)doEject:(id)sender
{
   int err;
#ifdef TRACE
   NSLog(@"ExtFSM: Ejecting '%@'.\n", [e_curSelection bsdName]);
#endif
   
   err = [[ExtFSMediaController mediaController] unmount:e_curSelection
      force:NO eject:YES];
   if (!err) {
      BeginOp();
   } else {
         NSBeginCriticalAlertSheet(ExtLocalizedString(@"Disk Error", ""),
         @"OK", nil, nil, [NSApp keyWindow], nil, nil, nil, nil,
         @"%@ '%@'",
            ExtLocalizedString(@"Could not eject", ""), [e_curSelection bsdName]);
   }
}

- (void)doOptions:(id)sender
{
   BOOL opts, info, startup, enabled;;
   
   startup = [[[e_tabs selectedTabViewItem] identifier] isEqualTo:@"Startup"];
   opts = [[[e_tabs selectedTabViewItem] identifier]
      isEqualTo:EXT_TOOLBAR_ALT_ACTION_INFO];
   info = !opts && !startup;
   enabled = [e_infoButton isEnabled];
   
#ifdef TRACE
   NSLog(@"ExtFSM: doOptions: start=%d, opts=%d, info=%d, enabled=%d, infoAlt=%d\n",
      startup, opts, info, enabled, e_infoButtonAlt);
#endif
   
   if (!info || !enabled) {
      if (!info)
         [e_tabs selectTabViewItemWithIdentifier:EXT_TOOLBAR_ACTION_INFO];
      if (enabled)
         goto info_alt_switch;
      else 
         goto info_alt_switch_back;
      return;
   }
   
   if (info && enabled) {
      if (self != sender) {
         // User action
         [e_tabs selectTabViewItemWithIdentifier:EXT_TOOLBAR_ALT_ACTION_INFO];
info_alt_switch_back:
         if (e_infoButtonAlt)
            ExtSwapButtonState(e_infoButton, YES);
         e_infoButtonAlt = NO;
         return;
      }
info_alt_switch:
      if (!e_infoButtonAlt)
         ExtSwapButtonState(e_infoButton, YES);
      e_infoButtonAlt = YES;
   }
}

- (void)mainViewDidLoad
{
   NSDictionary *plist;
   NSString *title, *tmp;
   NSButton *button;
   NSButtonCell *buttonCell;
   NSImage *image;
   NSButton *buttons[] = {e_mountButton, e_ejectButton, e_infoButton, nil};
   SEL tbactions[] = {@selector(doMount:), @selector(doEject:),
      @selector(doOptions:)};
   NSString *titles[] = {EXT_TOOLBAR_ACTION_MOUNT, EXT_TOOLBAR_ACTION_EJECT,
      EXT_TOOLBAR_ACTION_INFO, nil};
   NSString *alt_titles[] = {EXT_TOOLBAR_ALT_ACTION_MOUNT, nil,
      EXT_TOOLBAR_ALT_ACTION_INFO, nil};
   NSString *alt_images[] = {EXT_TOOLBAR_ALT_ACTION_MOUNT, nil, EXT_TOOLBAR_ALT_ACTION_INFO, nil};
   NSNumber *boolVal;
   int i;
   
   [self setValue:ExtLocalizedString(@"Donate", "") forKey:@"donateTitle"];
   
   e_infoButtonAlt = NO; // Used to deterimine state
   
   [[e_vollist tableColumnWithIdentifier:@"vols"]
      setDataCell:[[NSBrowserCell alloc] init]];
   
   // Change controls for startup
   [e_startupProgress setStyle:NSProgressIndicatorSpinningStyle];
   [e_startupProgress sizeToFit];
   [e_startupProgress setDisplayedWhenStopped:NO];
   [e_startupProgress setIndeterminate:YES];
   [e_startupProgress setUsesThreadedAnimation:YES];
   [e_startupProgress startAnimation:self]; // Stopped in [startup]
   [e_startupProgress display];
   [e_tabs selectTabViewItemWithIdentifier:@"Startup"];
   title = [ExtLocalizedString(@"Please wait, gathering disk information",
      "Startup Message") stringByAppendingFormat:@"%C", 0x2026 /*unicode elipses*/];
   [e_startupText setStringValue:title];
   [e_startupText display];
   
   /* Get our prefs */
   plist = [[self bundle] infoDictionary];
   e_bundleid = plist[@"CFBundleIdentifier"];
   
   e_prefRoot = [[NSMutableDictionary alloc] initWithDictionary:
      [[NSUserDefaults standardUserDefaults] persistentDomainForName:e_bundleid]];
   if (![e_prefRoot count]) {
   #ifdef TRACE
      NSLog(@"ExtFSM: Creating preferences\n");
   #endif
      // Create the defaults
      e_prefRoot[EXT_PREF_KEY_GLOBAL] = [NSMutableDictionary dictionary];
      e_prefRoot[EXT_PREF_KEY_MEDIA] = [NSMutableDictionary dictionary];
      e_prefRoot[EXT_PREF_KEY_MGR] = [NSMutableDictionary dictionary];
      // Save them
      [self didUnselect];
   }
   e_prefGlobal = [[NSMutableDictionary alloc] initWithDictionary:
      e_prefRoot[EXT_PREF_KEY_GLOBAL]];
   e_prefMedia = [[NSMutableDictionary alloc] initWithDictionary:
      e_prefRoot[EXT_PREF_KEY_MEDIA]];
   e_prefMgr = [[NSMutableDictionary alloc] initWithDictionary:
      e_prefRoot[EXT_PREF_KEY_MGR]];
   e_prefRoot[EXT_PREF_KEY_GLOBAL] = e_prefGlobal;
   e_prefRoot[EXT_PREF_KEY_MEDIA] = e_prefMedia;
   e_prefRoot[EXT_PREF_KEY_MGR] = e_prefMgr;
#ifdef notnow
   NSLog(@"ExtFSM: Prefs = %@\n", e_prefRoot);
#endif
   
   /* Setup localized string globals */
   e_yes = ExtLocalizedString(@"Yes", "");
   e_no = ExtLocalizedString(@"No", "");
   e_bytes = ExtLocalizedString(@"bytes", "");
   e_monikers[0] = e_bytes;
   
   [e_tabs setTabViewType:NSNoTabsNoBorder];
   
   button = buttons[0];
   for (i=0; nil != button; ++i, button = buttons[i]) {
      buttonCell = [button cell];
      
      [button setImagePosition:NSImageAbove];
      [button setBezelStyle:NSShadowlessSquareBezelStyle];
      [button setButtonType:NSMomentaryPushInButton];
      // [button setShowsBorderOnlyWhileMouseInside:YES];
      [buttonCell setAlignment:NSCenterTextAlignment];
      [buttonCell setTarget:self];
      [buttonCell setAction:tbactions[i]];
      [buttonCell sendActionOn:NSLeftMouseUp];
      [buttonCell setImageDimsWhenDisabled:YES];
      
      tmp = titles[i];
      [buttonCell setRepresentedObject:tmp]; //Used for state
      
      /* Setup primaries */
      title = ExtLocalizedString(tmp, "Toolbar Item Title");
      [button setTitle:title];
      
      image = [[self bundle] imageForResource:tmp];
      [button setImage:image];
      
      /* Setup alternates */
      tmp = alt_titles[i];
      if (tmp) {
         title = ExtLocalizedString(tmp, "Toolbar Item Title (Alternate)");
         [button setAlternateTitle:title];
      }
      tmp = alt_images[i];
      if (tmp) {
         image = [[self bundle] imageForResource:tmp];
         [button setAlternateImage:image];
      }
      
      [button setEnabled:NO];
   }
   
   [e_opProgress setUsesThreadedAnimation:YES];
   [e_opProgress setDisplayedWhenStopped:NO];
   [e_opProgress displayIfNeeded];
   e_operations = 0;
   
   [e_mountReadOnlyBox setTitle:
      ExtLocalizedString(@"Mount Read Only", "")];
   [e_dontAutomountBox setTitle:
      ExtLocalizedString(@"Don't Automount", "")];
   [e_ignorePermsBox setTitle:
      ExtLocalizedString(@"Ignore Permissions", "")]; 
   [e_indexedDirsBox setTitle:
      ExtLocalizedString(@"Enable Indexed Directories", "")];
   
   [e_optionNoteText setStringValue:
      ExtLocalizedString(@"Changes to these options will take effect during the next mount.", "")];
   
   [e_copyrightText setStringValue:[NSString stringWithFormat:@"%@ (%@)",
      [[self bundle] localizedInfoDictionary][@"CFBundleGetInfoString"],
      [[self bundle] objectForInfoDictionaryKey:@"ExtGlobalVersion"]]];
   [e_copyrightText setTextColor:[NSColor disabledControlTextColor]];
   
    // Install the SMART monitor into the user's login items
    boolVal = e_prefMgr[EXT_PREF_KEY_MGR_SMARTD_ADDED];
    tmp = [[self bundle] pathForResource:@"efssmartd" ofType:@"app"];
    if (nil == boolVal || NO == [boolVal boolValue]) {
        if ([self addLoginItem:tmp]) {
            boolVal = @YES;
            e_prefMgr[EXT_PREF_KEY_MGR_SMARTD_ADDED] = boolVal;
            e_prefsChanged = YES;
            [self savePrefs];
        }
    }
    if (boolVal && YES == [boolVal boolValue]) {
        // Launch it if it's not running
        if (0 == FindPIDByPathOrName(nil, "efssmartd", getuid()))
            if (NO == [[NSWorkspace sharedWorkspace] launchApplication:tmp])
                NSLog(@"ExtFSM: NSWorkspace faild to launch '%@'.\n", tmp);
    }

   [self performSelector:@selector(startup) withObject:nil afterDelay:0.3];
}

- (void)didSelect
{
}

- (void)didUnselect
{
   [self savePrefs];
}

// Private
- (void)savePrefs
{
   NSArray *paths;
   NSString *path;
   
   if (!e_prefsChanged)
      return;
   
   // Save the prefs
   [[NSUserDefaults standardUserDefaults]
      removePersistentDomainForName:e_bundleid];
   [[NSUserDefaults standardUserDefaults] setPersistentDomain:e_prefRoot
      forName:e_bundleid];
   
   e_prefsChanged = NO;
   
   /* Copy the plist to the /Library/Preferences dir. This is needed,
      because mount will run as root 99% of the time, and therefore won't
      have access to the user specific prefs. */
   paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSLocalDomainMask, NO);
   path = [paths[0] stringByAppendingPathComponent:
      @"Preferences/" EXT_PREF_ID @".plist"];
   if ([[NSFileManager defaultManager] fileExistsAtPath:path]) {
      // Remove it since we may not have write access to the file itself
      (void)[[NSFileManager defaultManager] removeFileAtPath:path handler:nil];
   }
   if (![e_prefRoot writeToFile:path atomically:YES]) {
      NSLog(@"ExtFSM: Could not copy preferences to %@."
         @" This is most likely because you do not have write permissions."
         @" Changes made will have no effect when mounting volumes.", path);
      NSBeep();
   }
}

/* Delegate */

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
#ifdef TRACE
   NSLog(@"ExtFSM: outline # children for %@", item);
#endif
   if (!item)
      return ([e_volData count]); // Return # of children for top-level item
      
   return ([item childCount]);
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
#ifdef TRACE
   NSLog(@"ExtFSM: outline expand for %@", item);
#endif
   if ([item childCount])
      return (YES);
   
   return (NO);
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
   NSArray *children;
   
#ifdef TRACE
   NSLog(@"ExtFSM: outline get child at index %d for %@", index, item);
#endif
   if (nil == item && index < [e_volData count]) {
      return (e_volData[index]);
   }
   
   children = [item children];
   if (index < [children count])
      return (children[index]);
   
   NSLog(@"ExtFSM: index is outside of range for object %@\n", item);
   return (nil);
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification
{
#ifdef TRACE
   NSLog(@"ExtFSM: outline selection row = %u", [e_vollist selectedRow]);
#endif
   [self doMediaSelection:[e_vollist itemAtRow:[e_vollist selectedRow]]];
}

- (id)outlineView:(NSOutlineView *)outlineView
   objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    return (nil);
}

- (void)outlineView:(NSOutlineView *)outlineView willDisplayCell:(id)cell
    forTableColumn:(NSTableColumn *)tableColumn item:(id)item
{    
    NSString *name;
    NSAttributedString *value;
    NSImage *icon;

    @try {
        name = [item volName];
        if (!name)
            name = [[item mountPoint] lastPathComponent];
    } @catch (NSException *e) {
#ifdef DIAGNOSTIC
        NSLog(@"ExtFSM: Item (0x%08lX) is not a valid media object.\n", (uintptr_t)item);
#endif
        return;
    }
    if (nil == name)
        name = [item ioRegistryName];
   
    icon = [item icon];
    if (nil != icon) {
        NSImage *cellIcon = nil;
        NSString *iname = [[icon name] stringByAppendingString:@".small"];
        
        if (nil != e_iconCache)
            cellIcon = e_iconCache[iname];
        else
            e_iconCache = [[NSMutableDictionary alloc] init];
        
        if (nil == cellIcon) {
            // Not found in cache
            cellIcon = [icon copy];
            [cellIcon setName:iname];
            [cellIcon setSize:NSMakeSize(16,16)];
            e_iconCache[iname] = cellIcon;
        }
        [cell setImage:cellIcon];
    }
    
    if ([e_volData containsObject:item]) {
        // Root disks get a bold name, and are colored red for SMART failures.
        NSMutableDictionary *d = MediaProps(item);
        NSColor *color;
        
        if (nil == (color = d[EXT_OBJ_PROPERTY_TITLE_COLOR])) {
            if ([item SMARTStatus] >= efsSMARTTestFatal)
                color = [NSColor redColor];
            else
                color = [NSColor blackColor];
            d[EXT_OBJ_PROPERTY_TITLE_COLOR] = color;
        }
        value = [[NSAttributedString alloc] initWithString:name attributes:
                    @{NSFontAttributeName: [[NSFontManager sharedFontManager] convertFont:
                            [cell font] toHaveTrait:NSBoldFontMask],
                        NSForegroundColorAttributeName: color}];
        [cell setAttributedStringValue:value];
    } else {
        [cell setLeaf:YES];
        if (YES == [item isMounted])
            [cell setStringValue:name];
        else {
            value = [[NSAttributedString alloc] initWithString:name attributes:
                    @{NSForegroundColorAttributeName: [NSColor disabledControlTextColor]}];
            [cell setAttributedStringValue:value];
        }
    }
}

@end

static NSMutableDictionary* MediaProps(ExtFSMedia *m)
{
    NSMutableDictionary *d = [m representedObject];
    
    if (nil == d) {
        d = [[NSMutableDictionary alloc] init];
        [m setRepresentedObject:d];
    }
    return (d);
}

static void ExtSwapButtonState(id button, BOOL swapImage)
{
   NSString *title;
   NSImage *image;
   
   title = [button alternateTitle];
   [button setAlternateTitle:[button title]];
   [button setTitle:title];
   
   if (swapImage) {
      image = [button alternateImage];
      [button setAlternateImage:[button image]];
      [button setImage:image];
   }
}

static id ExtMakeInfoTitle(NSString *title)
{
   NSMutableAttributedString *str;
   str = [[NSMutableAttributedString alloc] initWithString:
      [title stringByAppendingString:@":"]];
   [str applyFontTraits:NSBoldFontMask range:NSMakeRange(0, [str length])];
   return (str);
}

static void ExtSetPrefVal(ExtFSMedia *media, id key, id val)
{
    NSMutableDictionary *dict;
    NSString *uuid = [media uuidString];
    if (nil == (dict = [e_prefMedia[uuid] mutableCopy])) {
        dict = [[NSMutableDictionary alloc] init];
    }
    dict[key] = val;
    e_prefMedia[uuid] = dict;
    e_prefsChanged = YES;
}

static void AddRestoreAction(ExtFSMedia *m, id key, id val)
{
    NSMutableDictionary *objd = MediaProps(m);
    NSMutableArray *actions;
    
    actions = objd[EXT_OBJ_PROPERTY_RESTORE_ACTIONS];
    if (nil == actions) {
        actions = [[NSMutableArray alloc] init];
        objd[EXT_OBJ_PROPERTY_RESTORE_ACTIONS] = actions;
    }

#ifdef TRACE
    NSLog(@"Adding restore action for media '%@' with key '%@' and val '%@'.\n",
        [m bsdName], key, val);
#endif
    [actions addObject:
        @{key: val}];
}

static BOOL PlayRestoreActions(ExtFSMedia *m)
{
    NSMutableDictionary *objd = [m representedObject];
    NSMutableArray *actions;
    BOOL played = NO;
    if (objd) {
        actions = objd[EXT_OBJ_PROPERTY_RESTORE_ACTIONS];
        if (actions && [actions count] > 0) {
            NSDictionary *obj;
            id key, val;
            NSEnumerator *en = [actions objectEnumerator];
            while ((obj = [en nextObject])) {
                key = [obj allKeys][0];
                val = [obj allValues][0];
        #ifdef TRACE
                NSLog(@"Playing restore action for media '%@' with key '%@' and val '%@'.\n",
                    [m bsdName], key, val);
        #endif
                ExtSetPrefVal(m, key, val);
            }
            played = YES;
        }
        if (actions)
            [objd removeObjectForKey:EXT_OBJ_PROPERTY_RESTORE_ACTIONS];
    }
    return (played);
}

#define ELOG(f) NSLog(@"EFSM: %@\n", f)
#define ELOG1(f,a) NSLog((f), "EFSM", __LINE__, (a))
#define STRING_ERROR_ALLOC_MEM @"Memory alloc failed"

static int FindPIDByPathOrName(const char *path, const char *name, uid_t user)
{
   int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
   struct kinfo_proc *procs;
   struct extern_proc *proc;
   char *pargs, *cmdpath;
   size_t plen, argmax, arglen;
   int err, i, argct, retry = 0;
   pid_t found = (pid_t)0, me;
   
   procs = nil;
   do {
      // Get needed buffer size
      plen = 0;
      err = sysctl(mib, 3, nil, &plen, nil, 0);
      if (err) {
         ELOG1(@"%s:%d get process size failed with: %d", err == -1 ? errno : err);
         break;
      }
      
      procs = (struct kinfo_proc *)malloc(plen);
      if (!procs) {
         ELOG(STRING_ERROR_ALLOC_MEM);
         break;
      }
      
      // Get the process list
      err = sysctl(mib, 3, procs, &plen, nil, 0);
      if (-1 == err || (0 != err && ENOMEM == err)) {
         ELOG1(@"%s:%d get process list failed with: %d", err == -1 ? errno : err);
         break;
      }
      if (ENOMEM == err) {
         // buffer too small, try again
         free(procs);
         procs = nil;
         err = 0;
         retry++;
         if (retry < 3)
            continue;
         ELOG1(@"%s:%d Failed to obtain process list after %d tries.", retry);
         break;
      }
      
      // Get the max arg size for a command
      mib[1] = KERN_ARGMAX;
      mib[2] = 0;
      arglen  = sizeof(argmax);
      err = sysctl(mib, 2, &argmax, &arglen, nil, 0);
      if (err) {
         ELOG1(@"%s:%d get process arg size failed with: %d", err == -1 ? errno : err);
         break;
      }
      
      // Allocate space for args
      pargs = malloc(argmax);
      if (!pargs) {
         ELOG(STRING_ERROR_ALLOC_MEM);
         break;
      }
      
      // Walk the list and find the process
      me = getpid();
      for (i=0; i < plen / sizeof(struct kinfo_proc); ++i) {
         proc = &(procs+i)->kp_proc;
         if (proc->p_pid == me)
            continue;
         if ((procs+i)->kp_eproc.e_pcred.p_ruid != user)
            continue;
         
         // Get the process arguments
         #if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3)
         mib[1] = KERN_PROCARGS2;
         #else
         mib[1] = KERN_PROCARGS;
         #endif
         mib[2] = proc->p_pid;
         arglen = argmax;
         if (0 == sysctl(mib, 3, pargs, &arglen, nil, 0)) {
            #if (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_3)
            // argc is first for KERN_PROCARGS2
            bcopy(pargs, &argct, sizeof(argct));
            // the command path is next
            cmdpath = (pargs+sizeof(argct));
            #else
            argct = 0;
            cmdpath = pargs;
            #endif
            *(pargs+(arglen-1)) = '\0'; // Just in case
            if ((path && 0 == strcmp(cmdpath, path)) ||
                (name && strstr(cmdpath, name))) {
               found = proc->p_pid;
               break;
            }
         }
      }
      free(pargs);
      break;
      
   } while(1);
   
   if (procs)
      free(procs);
   return (found);
}
