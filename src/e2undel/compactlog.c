/* compactlog.c: compacts libundel log file */
/* not specifically tested yet, but seems to work */

/* (C) Oliver Diedrich */
/* this file may be redistributed under the terms of the */
/* GNU Public License (GPL), see www.gnu.org */


#include <stdio.h>
#include "e2undel.h"

int flag_redir = 1;   /* avoids printing progress points when reading */

int main(int argc, char **argv)
{
  int n_log;
  struct e2undel_log *log;    /* list of log file entries */

  if (argc > 1)
  {
    printf("compacts libundel log file %s to stdout\n", LOGNAME);
    printf("usage: %s [> TMP]\n", argv[0]);
    printf("[cp TMP %s]\n", LOGNAME);
    return 1;
  }
  
  fprintf(stderr, "reading log file: ");
  fflush(stderr);
  if (NULL == (log = read_log(0, &n_log)))
  {
    printf("no entries in log file\n");
    return 1;
  }
  fprintf(stderr, "%d entries\n", n_log);
  fprintf(stderr, "compacting log file: ");
  fflush(stderr);
  log = compact_log(log, &n_log);
  fprintf(stderr, "%d entries remaining\n", n_log);
  print_log(log);
  return 0;
}
