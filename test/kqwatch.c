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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

int main (int argc, char *argv[])
{
   int i, kfd;
   struct kevent *kep, kout;
   char *path;
   struct stat sb;
   
   if (argc != 2) {
      errno = EINVAL;
      perror(argv[0]);
      exit(errno);
   }
   
   path = malloc(PATH_MAX);
   realpath(argv[1], path);
   
   i = stat(path, &sb);
   if (i) {
      perror(argv[0]);
      exit(errno);
   }
   
   kep = (struct kevent*)malloc(sizeof(struct kevent));
   bzero(kep, sizeof(struct kevent));
   kep->ident = open (path, O_EVTONLY, 0);
   if (kep->ident < 0) {
      perror(argv[0]);
   }
   
   kep->filter = EVFILT_VNODE;
   kep->flags = EV_ADD | EV_CLEAR;
   kep->fflags = NOTE_DELETE | NOTE_WRITE;
   kep->data = NULL;
   kep->udata = path;
   
   kfd = kqueue();
   if (-1 == kfd) {
      perror(argv[0]);
      exit (errno);
   }
   
   kevent (kfd, kep, 1, NULL, 0, NULL);
   
   while(1) {
      printf ("waiting for events...\n");
      i = kevent(kfd, NULL, 0, &kout, 1, NULL);
      if (0 == i)
         continue;
      if (-1 == i) {
         perror(argv[0]);
         exit(errno);
      }
      
      if (kout.fflags & NOTE_DELETE)
         printf ("+ delete event for %s\n", kout.udata);
      if (kout.fflags & NOTE_WRITE)
         printf ("+ write event for %s\n", kout.udata);
      if (kout.fflags & NOTE_EXTEND)
         printf ("+ size increase event for %s\n", kout.udata);
      if (kout.fflags & NOTE_ATTRIB)
         printf ("+ attribute change event for %s\n", kout.udata);
      if (kout.fflags & NOTE_LINK)
         printf ("+ link count change event for %s\n", kout.udata);
      if (kout.fflags & NOTE_RENAME)
         printf ("+ rename event for %s\n", kout.udata);
   }
   
   return (0);
}
