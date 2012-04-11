/*
* Copyright 2003 Brian Bergstrand.
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

#import <stdio.h>
#import <time.h>
#import <sys/sysctl.h>

#import <ExtFSDiskManager/ExtFSDiskManager.h>

@interface ListDisks : NSObject
{
   ExtFSMediaController *_mc;
}
- (void)list:(ExtFSMediaController*)mc;
- (void)mediaAdd:(NSNotification*)notification;
- (void)mediaRemove:(NSNotification*)notification;
- (void)volMount:(NSNotification*)notification;
- (void)volUnmount:(NSNotification*)notification;
@end

int threadID = 0;

#ifndef POUND
#define pinfo(f,args...) printf((f), args)
#else
#define pinfo(f,args...) {}
#endif

@implementation ListDisks

- (void)list:(ExtFSMediaController*)mc
{
   NSArray *disks;
   NSEnumerator *en;
   ExtFSMedia *obj, *parent;
   NSString *tmp;
   BOOL mounted;
   
   if (!_mc) _mc = mc;
   mc = _mc;
   
   disks = [mc media];
   en = [disks objectEnumerator];
   while ((obj = [en nextObject])) {
      mounted = [obj isMounted];
      pinfo ("Device: %s\n", [[obj bsdName] cString]);
      tmp = [obj ioRegistryName];
      if (tmp)
         pinfo ("\tIOKit Name: %s\n", [tmp cString]);
      parent = [obj parent];
      if (parent)
         pinfo ("\tParent Device: %s\n", [[parent bsdName] cString]);
      pinfo ("\tBus: %s\n", [[obj transportName] cString]);
      pinfo ("\tBus Type: %s\n", [EFSIOTransportNameFromType([obj transportType]) cString]);
      pinfo ("\tEjectable: %s\n", ([obj isEjectable] ? "Yes" : "No"));
      pinfo ("\tDVD/CD ROM: %s\n", (([obj isDVDROM] || [obj isCDROM]) ? "Yes" : "No"));
      pinfo ("\tFS Type: %s\n", EFSNameFromType([obj fsType]));
      pinfo ("\tMounted: %s\n", (mounted ? [[obj mountPoint] cString] : "Not mounted"));
      if ([obj canMount])
         pinfo("\tFilesystem: %s\n", [[obj fsName] UTF8String]);
      if (mounted) {
         tmp = [obj volName];
         if (tmp)
            pinfo ("\tVolume Name: %s\n", [tmp cString]);
         tmp = [obj uuidString];
         if (tmp)
            pinfo ("\tVolume UUID: %s\n", [tmp cString]);
         pinfo ("\tSupports Journaling: %s\n", ([obj hasJournal] ? "Yes" : "No"));
         pinfo ("\tJournaled: %s\n", ([obj isJournaled] ? "Yes" : "No"));
         pinfo ("\tIndexed Directories: %s\n", ([obj hasIndexedDirs] ? "Yes" : "No"));
         pinfo ("\tLarge Files: %s\n", ([obj hasLargeFiles] ? "Yes" : "No"));
         pinfo ("\tBlock size: %lu\n", [obj blockSize]);
         pinfo ("\tBlock count: %qu\n", [obj blockCount]);
         pinfo ("\tTotal bytes: %qu\n", [obj size]);
         pinfo ("\tAvailable bytes: %qu\n", [obj availableSize]);
         pinfo ("\tNumber of files: %qu\n", [obj fileCount]);
         pinfo ("\tNumber of directories: %qu\n", [obj dirCount]);
      }
      #if 0
      NSImage *icon = [obj icon];
      pinfo ("\tIcon name: %s\n", [[icon name] UTF8String]);
      #endif
   }
}

- (void)mediaAdd:(NSNotification*)notification
{
   printf ("**** Media '%s' appeared ***\n", BSDNAMESTR([notification object]));
}

- (void)mediaRemove:(NSNotification*)notification
{
   printf ("**** Media '%s' disappeared ***\n", BSDNAMESTR([notification object]));
}

- (void)volMount:(NSNotification*)notification
{
   printf ("**** Media '%s' mounted ***\n", BSDNAMESTR([notification object]));
}

- (void)volUnmount:(NSNotification*)notification
{
   printf ("**** Media '%s' unmounted ***\n", BSDNAMESTR([notification object]));
}

- (void)volSMARTStatus:(NSNotification*)notification
{
	NSDictionary *info = [notification userInfo];
	NSLog (@"**** Media '%@' ****\n\tS.M.A.R.T. status: %@, '%@' severity: %@, '%@'\n",
		[notification object],
		[info objectForKey:ExtFSMediaKeySMARTStatus],
		[info objectForKey:ExtFSMediaKeySMARTStatusDescription],
		[info objectForKey:ExtFSMediaKeySMARTStatusSeverity],
		[info objectForKey:ExtFSMediaKeySMARTStatusSeverityDescription]
		);
}

#define MINDOZE 130 // About 1 cycle on a Quicksilver G4
//#define MAXDOZE 500000000 // 1/2 second
#define MAXDOZE  1000000 // 1/100 second
- (void)threadCtl:(id)mc
{
	int doze, tid = threadID++;
	struct timespec rqt = {0,0}, rmt = rqt;
	do {
		NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
		
		[self list:mc];
		
		[pool release];
		doze = MINDOZE + (random() % MAXDOZE);
	#ifndef POUND
		rqt.tv_nsec = rmt.tv_nsec = doze;
		printf ("thread %d sleeping for %d ns\n", tid, doze);
		nanosleep(&rqt, &rmt);
	#endif
	} while(1);
	[NSThread exit];
}

@end

int main(int argc, const char *argv[])
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   ExtFSMediaController *mc;
   ListDisks *ld;
   int threads, mib[2];
   size_t len;
   struct timeval now;
   
   mc = [ExtFSMediaController mediaController];
   ld = [[ListDisks alloc] init];
   
   gettimeofday(&now, nil);
   srandom(now.tv_sec);
   
   threads = 2;
   mib[0] = CTL_HW;
   mib[1] = HW_NCPU;
   len = sizeof(threads);
   if (0 == sysctl(mib, 2, &threads, &len, nil, 0))
      ++threads;
   
   [[NSNotificationCenter defaultCenter] addObserver:ld
            selector:@selector(mediaAdd:)
            name:ExtFSMediaNotificationAppeared
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:ld
            selector:@selector(mediaRemove:)
            name:ExtFSMediaNotificationDisappeared
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:ld
            selector:@selector(volMount:)
            name:ExtFSMediaNotificationMounted
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:ld
            selector:@selector(volUnmount:)
            name:ExtFSMediaNotificationUnmounted
            object:nil];
   [[NSNotificationCenter defaultCenter] addObserver:ld
            selector:@selector(volSMARTStatus:)
            name:ExtFSMediaNotificationSMARTStatus
            object:nil];
   //[ld performSelector:@selector(list:) withObject:mc afterDelay:2.0];
   
   [mc monitorSMARTStatusWithPollInterval:EXTFS_DEFAULT_SMART_POLL flags:efsSMARTEventVerified];
   
   printf ("Creating %d threads...\n", threads);
   for (; threads > 0; --threads)
      [NSThread  detachNewThreadSelector:@selector(threadCtl:) toTarget:ld withObject:mc];
   
   [[NSRunLoop currentRunLoop] run];
   
   [ld release];
   [pool release];
   return (0);
}
