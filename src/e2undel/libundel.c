/* overwrites unlink(2) and remove(3) via $LD_PRELOAD */

/* (C) Oliver Diedrich */
/* this file may be redistributed under the terms of the */
/* GNU Public License (GPL), see www.gnu.org */

#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>

/* 
compile as Position-Independent Code:
gcc -Wall -fPIC -c libundel.c
gcc -shared -Wl,-soname=libundel.so.1 -o libundel.so.1 libundel.o
oder
ld -shared -soname libundel.so.1 -ldl -o libundel.so.1.0 libundel.o
*/


#define VERSION 0.2

FILE *f = NULL;

void _init()
{
  f = fopen("/var/e2undel/e2undel", "a");
  if (!f)
    fprintf(stderr, "libundel: can't open log file, undeletion disabled\n");
}


int unlink(const char *pathname)
{
  int err;
  struct stat buf;
  char pwd[PATH_MAX];
  int (*unlinkp)(char *) = dlsym(RTLD_NEXT, "unlink");

  if (NULL != pathname)
  {
    if (__lxstat(3, pathname, &buf)) buf.st_ino = 0;
    if (!realpath(pathname, pwd)) pwd[0] = '\0';
  }
  err = (*unlinkp)((char *) pathname);
  if (err) return err;      /* unlink() did not succeed */
  if (f)
  {
    if (!S_ISLNK(buf.st_mode))  /* !!! should we check for other file types? */
    {                         /* don't log deleted symlinks */
      fprintf(f, "%ld,%ld::%ld::%s\n",
              (long) (buf.st_dev & 0xff00) / 256,
              (long) buf.st_dev & 0xff,
              (long) buf.st_ino, pwd[0] ? pwd : pathname);
      fflush(f);
    }
  }   /* if (f) */
    return err;
}   /* unlink() */


int remove(const char *pathname)
{
  int err;
  struct stat buf;
  char pwd[PATH_MAX];
  int (*removep)(char *) = dlsym(RTLD_NEXT, "remove");

  if (NULL != pathname)
  {
    if (__lxstat(3, pathname, &buf)) buf.st_ino = 0;
    if (!realpath(pathname, pwd)) pwd[0] = '\0';
  }
  err = (*removep)((char *) pathname);
  if (err) return err;      /* remove() did not succeed */
  if (f)
  {
    if (!S_ISLNK(buf.st_mode))  /* !!! should we check for other file types? */
    {                         /* don't log deleted symlinks */
      fprintf(f, "%ld,%ld::%ld::%s\n",
              (long) (buf.st_dev & 0xff00) / 256,
              (long) buf.st_dev & 0xff,
              (long) buf.st_ino, pwd[0] ? pwd : pathname);
      fflush(f);
    }
  }   /* if (f) */
    return err;
}   /* remove() */


void _fini()
{
  if (f) fclose(f);
}
