#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/errno.h>

int main(int argc, char *argv[])
{
   int fd, bytes, i;
   long base;
   char buf[4096];
   struct dirent *pent;
   
   fd = open(argv[1], O_RDONLY, 0);
   
   if ((bytes = getdirentries(fd, buf, 4096, &base)) > 0) {
      for (i = 0; i < bytes; ) {
         pent = (struct dirent*)(buf+i);
         printf ("entry fnum = %lu, len = %u, namelen = %u, name = %s\n",
            pent->d_fileno, pent->d_reclen, pent->d_namlen, pent->d_name);
         i += pent->d_reclen;
      }
   } else {
      fprintf(stderr, "getdirentries() failed: %s\n", strerror(errno));
   }
}