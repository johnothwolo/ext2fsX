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

static const char me_whatid[] __attribute__ ((unused)) =
"@(#) $Id: mount_extmgr.c,v 1.4 2006/07/01 20:46:18 bbergstrand Exp $";

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <CoreFoundation/CoreFoundation.h>

#include "extfsmgr.h"

#include <gnu/ext2fs/ext2_fs.h>

#define EXT_SUPER_UUID
#include <util/super.c>

void extmgr_mntopts (const char *device, int *mopts, int *eopts, int *nomount)
{
   CFPropertyListRef mediaRoot;
   CFDictionaryRef media;
   CFStringRef uuid;
   CFBooleanRef boolVal;
   
   *nomount = 0;
   
   mediaRoot = CFPreferencesCopyAppValue(EXT_PREF_KEY_MEDIA, EXT_PREF_ID);
   if (mediaRoot && CFDictionaryGetTypeID() == CFGetTypeID(mediaRoot)) {
      uuid = extsuper_uuid(device);
      if (uuid) {
         media = CFDictionaryGetValue(mediaRoot, uuid);
         if (media && CFDictionaryGetTypeID() == CFGetTypeID(media)) {
            boolVal = CFDictionaryGetValue(media, EXT_PREF_KEY_NOAUTO);
            if (boolVal && CFBooleanGetValue(boolVal)) {
               *nomount = 1;
               goto out;
            }
            
            boolVal = CFDictionaryGetValue(media, EXT_PREF_KEY_RDONLY);
            if (boolVal && CFBooleanGetValue(boolVal)) {
               *mopts |= MNT_RDONLY;
            }
            
            boolVal = CFDictionaryGetValue(media, EXT_PREF_KEY_NOPERMS);
            if (boolVal && CFBooleanGetValue(boolVal)) {
               *mopts |= MNT_IGNORE_OWNERSHIP;
            }
            
            /* Ext2/3 specific */
            
            boolVal = CFDictionaryGetValue(media, EXT_PREF_KEY_DIRINDEX);
            if (boolVal && CFBooleanGetValue(boolVal)) {
               *eopts |= EXT2_MNT_INDEX;
            }
         }
out:
         CFRelease(uuid);
      }
   }
   if (mediaRoot)
      CFRelease(mediaRoot);
}
