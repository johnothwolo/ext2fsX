/* log.c: part of e2undel since version 0.31 */
/* functions to read, compact, and print log file entries */

/* (C) Oliver Diedrich */
/* this file may be redistributed under the terms of the */
/* GNU Public License (GPL), see www.gnu.org */


#include "e2undel.h"
#include "math.h"


struct e2undel_log *read_log(unsigned long devnr, int *n)
/* reads log file, discards entries with device number different from devnr */
/* if devnr==0, all entries are read */
/* NULL: error, else: pointer to start of ring list of log file entries */
/* used only in list mode with devnr==0, but maybe some version later ... */
{
  char buf[PATH_MAX+20];                /* file name + device and inode # */
  unsigned long dev;                    /* device # of entry */
  char *c;
  struct e2undel_log *l, *start, *previous;
  int i = 0;                            /* counts entries matching devnr */
  int lines = 0;                        /* counts all entries */

  FILE *f = fopen(LOGNAME, "r");
  if (!f)
  {
    perror("opening log file");
    return(NULL);
  }
  previous = NULL;
  l = NULL;
  start = NULL;

  while (fgets(buf, PATH_MAX+20, f))
  {
    lines++;
    if (!flag_redir && (0 == (lines % 2000)))
    {
      printf(".");
      fflush(stdout);
    }
    c = buf;
    while (*c != '\n') c++;
    *c = '\0';
    c = buf;
    dev = atol(c);                                /* major # */
    dev = dev << 8;
    while ( (*c >= '0') && (*c <= '9') ) c++;
    c++;                                          /* , */
    dev += atol(c);                               /* minor # */
    while ( (*c >= '0') && (*c <= '9') ) c++;
    c++; c++;                                     /* :: */
    if (devnr && (devnr != dev)) continue;        /* device number matters, */
                                                  /* but does not match */
    if (NULL == (l = malloc(sizeof(struct e2undel_log))))
    {
      perror("reading log file");
      fclose(f);
      return(NULL);
    }
    i++;
    l->dev = dev;
    l->inode = atol(c);                           /* inode # */
    while ( (*c >= '0') && (*c <= '9') ) c++;
    c++; c++;                                     /* :: */
    if (NULL == (l->name = malloc(strlen(c)+1)))
    {
      perror("reading log file");
      fclose(f);
      return(NULL);
    }
    sprintf(l->name, "%s", c);
    l->last = previous;
    if (previous) previous->next = l;
    else start = l;
    previous = l;
  }   /* while */
  fclose(f);
  if (!flag_redir) printf("\n");
  if (l)                                /* close the ring */
  {
    l->next = start;
    start->last = l;
  }
  *n = i;
  return start;
}   /* read_log() */



struct e2undel_log *read_log_tree(unsigned long devnr, int *n)
/* reads log file into a binary tree sorted by inode numbers */
/* discards entries with device number different from devnr */
/* NULL: error, else: pointer to start of tree of log file entries */
{
  char buf[PATH_MAX+16];
  unsigned long dev;
  char *c;
  struct e2undel_log *l, *log, *start;
  int i = 0;                            /* counts entries matching devnr */
  int lines = 0;                        /* counts all entries */
  int found = 0;                        /* breaks tree searching loop */

  FILE *f = fopen(LOGNAME, "r");
  if (!f)
  {
    perror("opening log file");
    return(NULL);
  }
  l = NULL;
  start = NULL;

  while (fgets(buf, PATH_MAX+16, f))
  {
    lines++;
    if (!flag_redir && (0 == (lines % 2000)))
    {
      printf("."); fflush(stdout);
    }
    c = buf;
    while (*c != '\n') c++;
    *c = '\0';

    c = buf;                                 /* extract device number */
    dev = atol(c);
    dev = dev << 8;
    while ( (*c >= '0') && (*c <= '9') ) c++;
    c++;
    dev += atol(c);
    while ( (*c >= '0') && (*c <= '9') ) c++;
    c++; c++;
    if (devnr != dev) continue;              /* device number does not match */
    if (NULL == (l = malloc(sizeof(struct e2undel_log))))
    {
      perror("reading log file");
      fclose(f);
      return(NULL);
    }
    else memset(l, 0, sizeof(struct e2undel_log));
    i++;
    l->dev = dev;
    l->inode = atol(c);
    while ( (*c >= '0') && (*c <= '9') ) c++;
    c++; c++;
    if (NULL == (l->name = malloc(strlen(c)+1)))
    {
      perror("reading log file");
      fclose(f);
      return(NULL);
    }
    sprintf(l->name, "%s", c);

    if (!start) start = l;
    else
    {
      log = start;
      found = 0;
      while (!found)
      {
        if (l->inode == log->inode)     /* replace old entry by newer one */
        {
          log->dev = l->dev;
          free(log->name);
          log->name = l->name;
          free(l);
          i--;
          found = 1;
        }
        else 
        {
          if (l->inode < log->inode)
          {
            if (log->last) log = log->last;
            else
            {
              log->last = l;
              found = 1;
            }
          }
          else
          {
            if (log->next) log = log->next;
            else
            {
              log->next = l;
              found = 1;
            }
          }
        }
      }   /* while (!found) */
    }   /* if (start) */
  }   /* while (fgets(...)) */
  fclose(f);
  if (!flag_redir) printf("\n");
  *n = i;
  return start;
}   /* read_log_tree() */



struct e2undel_log *compact_log(struct e2undel_log *start, int *n)
/* eliminates double inode numbers: only the newest entry is valid */
/* only needed (and called) in list mode */
/* returns new start of ring list or NULL in case of error (out of mem) */
{
  struct e2undel_log *log, *l, *end;
  ext2_ino_t inode;
  unsigned long devs[MAX_DEVICE];
  unsigned long device;
  int i;
  ext2_ino_t max_inode;
  unsigned char *bitmap;
  
  for (i=0; i<MAX_DEVICE; i++) devs[i] = 0;
  l = start;
  max_inode = 0;
  do                          /* find different devices and max inode # */
  {
    i = 0;
    while (devs[i])
    {
      if (devs[i] == l->dev) break;
      i++;
    }
    if (0 == devs[i]) devs[i] = l->dev;
    if (l->inode > max_inode) max_inode = l->inode;
    l = l->next;
  } while (start != l);
  if (NULL == (bitmap = malloc(max_inode / 8)))
  {
    perror("checking log file");
    return NULL;
  }

  end = start->last;          /* last entry in ring list */
  i = 0;                      /* enumerates devices */
  while(devs[i])
  {
    device = devs[i];
    bzero(bitmap, max_inode/8);
    log = end;
    do
    {
      if (log->dev != device)      /* device # doesn't match */
      {
        log = log->last;
        continue;
      }
      inode = log->inode/8;
      if (bitmap[inode] & (1 << (log->inode % 8)))
      {                            /* inode already in list */
        log->next->last = log->last;
        log->last->next = log->next;
        l = log;
        log = log->last;
        free(l);
        (*n)--;
      }
      else
      {
        bitmap[inode] |= (1 << (log->inode % 8));
        log = log->last;
      }
    } while (end != log);
    i++;
  }   /* while devs[n] */
  free(bitmap);
  return(end->next);
}   /* compact_log() */



void print_log(struct e2undel_log *start)
/* needed by compactlog and for test purposes */
{
 struct  e2undel_log *l = start;
  do
  {
    printf("%lu,%lu::%lu::%s\n", l->dev >> 8, l->dev & 0xff, 
                                 (long) l->inode, l->name);
    l = l->next;
  } while (start != l);
}   /* print_log() */



void print_tree(struct e2undel_log *start)
/* just for test purposes */
{
  struct e2undel_log *l = start;
  if (l->last) print_tree(l->last);
  printf("%lu: %s\n", (long) l->inode, l->name);
  if (l->next) print_tree(l->next);
}
