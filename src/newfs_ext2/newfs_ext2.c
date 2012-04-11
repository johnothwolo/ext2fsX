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

#if !defined(lint)
static const char whatid[] __attribute__ ((unused)) =
"@(#) $Revision: 1.5 $ Built: " __DATE__ " " __TIME__;
#endif

/*
 This is a small wrapper around mke2fs to present the same interface as newfs_hfs,
 newfs_msdos, etc.
*/

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char *progname;

#define EXT_SPAWN_SUPRESS_FAILURES
#include <util/spawn.c>

#define MKE2FS_CMD "/usr/local/sbin/mke2fs"
#define MKE2FS_LABEL_ARG "-L"

int main(int argc, char *argv[])
{
   char *mke2args[argc+1];
   int i;
   
   bzero(mke2args, sizeof(char*) * (argc+1));
   
   progname = argv[0];
   
   mke2args[0] = MKE2FS_CMD;
   
   for (i=1; i < argc; ++i) {
      /* Remap -v to set the volume name */
      if (0 == strncmp(argv[i], "-v", 2)) {
         mke2args[i] = MKE2FS_LABEL_ARG;
         ++i;
         mke2args[i] = argv[i];
      } else {
         mke2args[i] = argv[i];
      }
   }
   
   safe_execv(mke2args);
   return (0);
}
