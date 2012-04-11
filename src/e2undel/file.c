#include "file.h"
#include "common.h"

/* (C) Oliver Diedrich */
/* this file may be redistributed under the terms of the */
/* GNU Public License (GPL), see www.gnu.org */


int main(int argc, char *argv[])
{
  char fname[PATH_MAX];
  unsigned char buf[FSIZE+1];
  int f;
  int n, i;
  
  if (argc < 2) return -1;

  if (!apprentice())
  {
    fprintf(stderr, "trouble with magic types.\n");
    return 0;
  }

  for (i=1; i<argc; i++)
  {
    strncpy(fname, argv[i], PATH_MAX);
    f = open(fname, O_RDONLY);
    if (-1 == f)
    {
      perror(fname);
      return 0;
    }
    n = read(f, buf, FSIZE);
    if (-1 == n)
    {
      perror(fname);
      close(f);
      continue;
    }

    fprintf(stderr, "%s: ", argv[i]);
    if (0 == n)
    {
      fprintf(stderr, "empty\n");
      close(f);
      continue;
    }
    if (softmagic(buf, n)) fprintf(stderr, "%s", buf);
    else
    {
      if (ascmagic(buf, n)) fprintf(stderr, "%s", buf);
      else fprintf(stderr, "data");
    }
    fprintf(stderr, "\n");
    close(f);
  }   /* for i */
  return 1;
}   /* main() */
