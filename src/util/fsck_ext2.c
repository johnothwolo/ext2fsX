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

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Revision: 1.2 $ Built: " __DATE__ " " __TIME__;

/*
 This is a small wrapper around e2fsck to present the same interface as fsck_hfs.
 In particular, it handles the '-q' option.
*/

#include <sys/param.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gnu/ext2fs/ext2_fs.h>

static char *progname;

#define EXT_SPAWN_SUPRESS_FAILURES
#include <util/spawn.c>
#define EXT_SUPER_ISCLEAN
#include <util/super.c>

#define E2FSCK_CMD "/usr/local/sbin/e2fsck"

int main(int argc, char *argv[])
{
   char *e2fsck[argc+1];
   int i;

   progname = argv[0];

   bzero(e2fsck, sizeof(char*) * (argc+1));
   e2fsck[0] = E2FSCK_CMD;
   
   for (i=1; i < argc; ++i) {
      if (0 == strncmp(argv[i], "-q", 2)) {
         return (extsuper_isclean(argv[i+1]));
      }
      
      e2fsck[i] = argv[i];
   }
   
   safe_execv(e2fsck);
   return (0);
}
