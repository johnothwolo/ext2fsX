/* undeletes files, uses log file of deleted files written by libundel.so */

/* (C) Oliver Diedrich */
/* this file may be redistributed under the terms of the */
/* GNU Public License (GPL), see www.gnu.org */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>

#include "e2undel.h"
#ifdef WITH_FILE
  #include "file.h"
  #include "common.h"
#endif

#undef PROFILE                /* if defined, writes some timing information */
                              /* into PROF_NAME */
#ifdef PROFILE
  #include <sys/time.h>
  #define PROF_NAME "profile.e2undel"
#endif


/* global variables and flags */
char device[PATH_MAX];  /* file system where to look for deleted files (-d) */
char save[PATH_MAX];    /* path where to save recovered files (-s) */
int flag_magic = 0;     /* if set: use file_type() to determine file types (-t) */
int flag_all = 0;       /* if set: print all deleted files (-a) */
int flag_list = 0;      /* if set: just list deleted files in log file (-l) */
int flag_redir = 0;     /* if set: Output redirected to file */
int table[MAX_USER][MAX_INTERVAL];   /* # of deleted files for each user in */
                                     /* each time interval */
int uid[MAX_USER];      /* UIDs of users in table */


int interval[MAX_INTERVAL] = 
     {0,
      43200,            /* 12 h */
      172800,           /* 48 h */
      604800,           /* 7 d */
      2592000,          /* 30 d */
      31536000 };       /* 1 y */




void usage(char *myname)
/* gives a usage screen and exits */
{
  fprintf(stderr,
   "usage: %s -d device -s path [-a]", myname);
#ifdef WITH_FILE
  fprintf(stderr, " [-t]");
#endif
  fprintf(stderr,
   "\nusage: %s -l\n", myname);
  fprintf(stderr,
   "'-d': file system where to look for deleted files\n");
  fprintf(stderr,
   "'-s': directory where to save undeleted files\n");
  fprintf(stderr,
   "'-a': work on all files, not only on those listed in undel log file\n");
#ifdef WITH_FILE
  fprintf(stderr,
   "'-t': try to determine type of deleted files w/o names, works only with '-a'\n");
#endif
  fprintf(stderr,
   "'-l': just give a list of valid files in undel log file\n");
  fprintf(stderr,
   "'device' should not be mounted, and 'path' should not reside within 'device'.\n");
  exit(1);
}   /* usage */




void eval_args(int argc, char *argv[])
/* evaluates command line arguments. Sets global variables and flags. */
/* calls usage() in case of error */
{
  int i;
  char *c;
  
  device[0] = '\0';
  save[0] = '\0';

  for (i=1; i<argc; i++)
  {
    c = argv[i];
    if (*c != '-') usage(argv[0]); else c++ ;
    switch (*c)
    {
      case 't' : 
#ifdef WITH_FILE
                 flag_magic = 1;
#else
                 printf("\nnot built with -DWITH_FILE, file type detection ('-t') not possible\n\n");
                 usage(argv[0]);
#endif
                 break;
      case 'a' : 
                 flag_all = 1;
                 break;
      case 'l' : 
                 flag_list = 1;
                 break;
      case 'd' : 
                 if (i<argc-1) i++; else usage(argv[0]);
                 strncpy(device, argv[i], 255);
                 break;
      case 's' : 
                 if (i<argc-1) i++; else usage(argv[0]);
                 strncpy(save, argv[i], 255);
                 break;
      default :  
                 printf("\nargument '%s' invalid\n\n", argv[i]);
                 usage(argv[0]);
    }   /* switch */
  }   /* for i */
  if (!flag_list)
  {
    if ('\0' == device[0]) usage(argv[0]);
    if ('\0' == save[0]) usage(argv[0]);
  }
}   /* eval_args() */




ext2_filsys open_fs(char *dev_name)
/* opens the file system dev_name */
/* NULL: error, else: file system handle */
{
  int err;
  ext2_filsys fs;
  char input[10];
  char buf[PATH_MAX+30];

  err = ext2fs_open(dev_name,   /* /dev/... */
                    0,          /* read_only, use superblock feature sets */
                    0,          /* primary superblock */
                    0,          /* block size  from superblock */
                    unix_io_manager,    /* unix_io IO manager */
                    &fs);       /* filesystem handle */
  if (err)
  {
    sprintf(buf, "while opening %s", dev_name);
    com_err(dev_name, err, buf);
    return NULL;
  }

  err = ext2fs_read_inode_bitmap(fs);
  if (err)
  {
    com_err(dev_name, err, "while reading inode bitmap");
    ext2fs_close(fs);
    return NULL;
  }
  err = ext2fs_read_block_bitmap(fs);
  if (err)
  {
    com_err(dev_name, err, "while reading block bitmap");
    ext2fs_close(fs);
    return NULL;
  }

  printf("%s opened for %s access\n", dev_name,
         fs->flags & EXT2_FLAG_RW ? "read-write" : "read-only");
  if (fs->flags &&  EXT2_FLAG_DIRTY)
  {
    printf("%s was not cleanly unmounted.\n", dev_name);
    printf("Do you want wo continue (y/n)? ");
    fgets(input, 8, stdin);
    if ('y' != input[0]) return NULL;
  }
  return fs;
}   /* open_fs() */




void show_fs_info(ext2_filsys fs)
/* print some superblock content of file system fs */
{
  printf("%d inodes (%d free)\n", fs->super->s_inodes_count,
         fs->super->s_free_inodes_count);
  printf("%d blocks of %d bytes (%d free)\n", fs->super->s_blocks_count,
         1024 << fs->super->s_log_block_size, fs->super->s_free_blocks_count);
  printf("last mounted on %s", ctime( (time_t *) &(fs->super->s_mtime)) );
}   /* show_fs_info() */




unsigned long check_path(char *dev_name, char *save_name)
/* does some sanity checks for device and save path */
/* 0: error, else: number of device to work on */
{
  FILE *f;
  char buf[PATH_MAX+30];
  char input[10];
  struct stat st_buf;
  int i;
  unsigned long devnr;

  sprintf(buf, "%s/undel.XXXXXX", save_name); /* save path writeable? */
  if (-1 == (i = mkstemp(buf)))
  {
    fprintf(stderr, "can't create files on %s\n", save_name);
    return 0;
  }
  else
  {
    close(i);
    unlink(buf);
  }
  
  if (stat(dev_name, &st_buf))        /* find device number of dev_name */
  {
    perror(dev_name);
    return 0;
  }
  if (S_ISBLK(st_buf.st_mode))          /* real device, not image file */
   devnr = st_buf.st_rdev;
  else devnr = 1;                       /* we return devnr, and 0 is error */

  if (devnr > 1)                        /* real device */
  {
    f = fopen("/etc/mtab", "r");        /* dev_name mounted? */
    if (f)
    {
      while (fgets(buf, PATH_MAX, f))
      {
        char *c = buf;
        while (*c != ' ') c++;
        *c = '\0';
        if (!strncmp(buf, dev_name, strlen(buf)))
        {
          fprintf(stderr, "\n%s is mounted. Do you want to continue (y/n)? ",
                  dev_name);
          fgets(input, 8, stdin);
          if ('y' != input[0])
          {
            fclose(f);
            return 0;
          }
        }
      }   /* while */
      fclose(f);
    }   /* if (f) */

    if (stat(save_name, &st_buf))       /* save path on dev_name? */
    {
      perror(save_name);
      return 0;
    }
    if (devnr == st_buf.st_dev)
    {
      fprintf(stderr, "%s is mounted on %s. Do you really want to continue (y/n)? ",
              save_name, dev_name);
      fgets(input, 8, stdin);
      if ('y' != input[0]) return 0;
    }
  }   /* if (devnr > 1) */
  return devnr;
}   /* check_path() */
  



int add_names(struct one_inode *del, struct e2undel_log *start)
/* adds names from log file list *start to inode list *del */
/* returns number of del list entries with name from log file list */
{
  struct e2undel_log *l;
  ext2_ino_t inode;
  int n = 0;
  int found = 0;

  while (del)
  {
    l = start;
    del->name = NULL;
    inode = del->inode;
    found = 0;
    while (!found)
    {
      if (l->inode == inode)
      {
        del->name = l->name;
        n++;
        found = 1;
      }
      else
      {
        if (inode < l->inode)
        {
          if (l->last) l = l->last;
          else found = 1;
        }
        else
        {
          if (l->next) l = l->next;
          else found = 1;
        }
      }
    }   /* while (!found) */
    del = del->next;
  }   /* while (del) */
  return n;
}   /* add_names() */




void build_table(struct one_inode *ino)
/* fills # of deleted files in table[][] and UIDs in uid[] */
{
  register int user, t;
  time_t tim = time(NULL);

  for (user=0; user <MAX_USER; user++)
  {
    uid[user] = -1;
    for (t=0; t<MAX_INTERVAL; t++) 
    {
      table[user][t] = 0;
    }
  }
      
  while (ino)
  {
    if (!flag_all && !ino->name)
    {
      ino = ino->next;
      continue;
    }
    if (tim - ino->dtime < interval[1]) t = 0;       /* < 12 h */
    else if (tim - ino->dtime < interval[2]) t = 1;  /* < 48 h */
    else if (tim - ino->dtime < interval[3]) t = 2;  /* < 7 days */
    else if (tim - ino->dtime < interval[4]) t = 3;  /* < 30 days */
    else if (tim - ino->dtime < interval[5]) t = 4;  /* < 365 days */
    else t = 5;                                      /* older */
    ino->interval = t;
    for (user=0; user<MAX_USER; user++)
    {
      if (uid[user] == ino->uid) 
      {
        table[user][t]++;
        break;
      }
      else
      {
        if (-1 == uid[user])
        {
          uid[user] = ino->uid;
          table[user][t]++;
          break;
        }
      }
    }   /* for user */
    ino = ino->next;
  }   /* while (ino) */
}   /* build_table() */




void print_table()
/* prints # of entries from table[][]; */
/* gets user names for UIDs in uid[] */
{
  register int user, t;
  struct passwd *pw;

  if (flag_redir)
  {
    printf("\n   user name |  < 12 h |  < 48 h |   < 7 d |  < 30 d |   < 1 y |   older\n");
    printf("-------------+---------+---------+---------+---------+---------+--------\n");
  }
  else
  {
    printf("\n   user name | \033[01;30m1\033[m <12 h | \033[01;30m2\033[m <48 h | \033[01;30m3\033[m  <7 d | \033[01;30m4\033[m <30 d | \033[01;30m5\033[m  <1 y | \033[01;30m6\033[m older\n");
    printf("-------------+---------+---------+---------+---------+---------+--------\n");
  }
  for (user=0; user<MAX_USER; user++)
  {
    if (uid[user] > -1)
    {
      pw = getpwuid(uid[user]);
      if (flag_redir)
      {
        if (!pw) printf("%12d", uid[user]);
        else printf("%12s", pw->pw_name);
      }
      else
      {
        if (!pw) printf("\033[01;30m%12d\033[m", uid[user]);
        else printf("\033[01;30m%12s\033[m", pw->pw_name);
      }
      for (t=0; t<MAX_INTERVAL; t++) printf(" | %7d", table[user][t]);
      printf("\n");
    }
    else return;
  }   /* for user */
}   /* print_table() */




int choose_user()
/* lets user select user name */
/* -1: user exit, else selected UID */
{
  char input[80];
  char *c, *tail;
  int user, i;
  struct passwd *pw;

  printf("Select user name from table or press enter to exit: ");
  fflush(stdout);
  while (1)
  {
    fgets(input, 78, stdin);
    c = input;
    do
    {
      if ('\n' == *c) *c = '\0';
    } while ( (*c != '\0') && c++);
    if (c == input) return -1;                 /* just enter */
    pw = getpwnam(input);
    if (pw) return pw->pw_uid;
    else                                       /* numeric UID without name? */
    {
      i = strtol(input, &tail, 10);
      if (tail != input)
      {
        for (user=0; user<MAX_USER; user++)
        {
          if (i == uid[user]) return i;
        }
      }
    }
    printf("User %s does not exists. Select user name from table: ", input);
  }   /* while (1) */
}   /* choose_user() */




int choose_interval()
/* lets user select a time interval */
/* -1: user exit, else selected interval (0-5) */
{
  char input[80];
  char *c;
  int i;

  printf("Select time interval (1 to 6) or press enter to exit: ");
  fflush(stdout);
  while (1)
  {
    fgets(input, 78, stdin);
    c = input;
    do
    {
      if ('\n' == *c) *c = '\0';
    } while ( (*c != '\0') && c++);
    if (c == input) return -1;                 /* just enter */
    i = atoi(input);
    if ( (i>=1) && (i<=6) ) return i-1;
    else printf("Invalid time interval. Select a value between 1 and 6: ");
  }   /* while (1) */
}   /* choose_interval() */




#ifdef WITH_FILE
int file_type(ext2_filsys fs, struct one_inode *ino, char **c)
/* dumps first kByte of inode *ino on fs and determines file type */
/* file type is copied to *c */
/* 0: error, else: ok */
{
  errcode_t err;
  struct ext2_inode inode_struct;
  ext2_file_t e2_file;
  char buf[1024];
  unsigned int n;

  err = ext2fs_read_inode(fs, ino->inode, &inode_struct);
  if (err)
  {
    com_err("file_type()", err, "while reading inode %ld", ino->inode);
    return 0;
  }
  err = ext2fs_file_open(fs, ino->inode, 0, &e2_file);
  if (err)
  {
    com_err("file_type()", err, "while opening ext2 file");
    return 0;
  }
  err = ext2fs_file_read(e2_file, buf, sizeof(buf), &n);
  if (err)
  {
    com_err("file_type()", err, "while reading ext2 file");
    return 0;
  }
  err = ext2fs_file_close(e2_file);
  if (err)
  {
    com_err("file_type()", err, "while closing ext2 file");
    return 0;
  }

  if (0 == n)
  {
    if (NULL == (*c = malloc(10)))
    {
      fprintf(stderr, "file_type(): out of memory\n");
      return 0;
    }
    sprintf(*c, "* empty");
    return 1;
  }

  if (softmagic(buf, n)) 
  {
    if (NULL == (*c = malloc(strlen(buf)+3)))
    {
      fprintf(stderr, "file_type(): out of memory\n");
      return 0;
    }
    else
    {
      sprintf(*c, "* %s", buf);
      return 1;
    }
  }

  else
  {
    if (ascmagic(buf, n)) 
    {
      if (NULL == (*c = malloc(strlen(buf)+3)))
      {
        fprintf(stderr, "file_type(): out of memory\n");
        return 0;
      }
      else
      {
        sprintf(*c, "* %s", buf);
        return 1;
      }
    }
    else
    {
      if (NULL == (*c = malloc(8)))
      {
        fprintf(stderr, "file_type(): out of memory\n");
        return 0;
      }
      else
      {
        sprintf(*c, "* data");
        return 1;
      }
    }
  }
}   /* file_type() */
#endif




int print_inode_list(ext2_filsys fs, struct one_inode *ino, int uid, int tim)
/* prints entries with UID uid and time interval tim from inode list; */
/* if flag_all is set, all list entries are shown, else only entries with */
/* name from log file are printed */
/* if flag_magic ist set, file types are determined via file_type() */
/* returns number of matching inodes */
{
  char *timestr, *c1, *c2;
  int n = 0;

  printf("\n  inode     size  deleted at        name\n");
  printf("-----------------------------------------------------------\n");
  while (NULL != ino)
  {
    if ( (ino->uid != uid) || (ino->interval != tim) ) 
    {
      ino = ino->next;
      continue;
    }
    if (!flag_all && !ino->name)
    {
      ino = ino->next;
      continue;
    }
    if (ino->free_blocks < ino->blocks) printf("\033[01;31m");
    timestr = ctime( (time_t *) &(ino->dtime));
    timestr += 4;                       /* remove day of week */
    c1 = &timestr[12];
    *c1 = ' ';
    c1++;
    c2 = &timestr[16];
    while (*c2 != '\n')                 /* remove seconds and century */
    {
      *c1 = *c2;
      c1++; c2++;
    }
    *c1 = '\0';
#ifdef WITH_FILE
    if (flag_magic)
    {
      if (!ino->name) 
      {
        if (!file_type(fs, ino, &ino->name))
        {
          ino->name = malloc(40);
          sprintf(ino->name, "(error while determining file type)");
        }
      }
    }
#endif
    n++;
    printf("%7lu%9u  %s %s", (long) ino->inode, ino->size, timestr, 
           ino->name ? ino->name : "(not in log file)");
    if (ino->free_blocks < ino->blocks) printf("\033[m\n");
    else printf("\n");
    ino = ino->next;
  }
  return n;
}   /* print_inode_list() */




struct one_inode *choose_one_inode(struct one_inode *ino)
/* lets user select one inode to recover; checks if it is in inode list *ino */
/* NULL: user exit, else selected one_inode struct */
{
  char input[80];
  char *c;
  long i;
  struct one_inode *inode;

  printf("Select an inode listed above or press enter to go back: ");
  fflush(stdout);
  while (1)
  {
    fgets(input, 78, stdin);
    c = input;
    do
    {
      if ('\n' == *c) *c = '\0';
    } while ( (*c != '\0') && c++);
    if (c == input) return NULL;                  /* just enter */
    i = atoi(input);
    if (!i)
    {
      printf("Invalid entry. Select on of the inodes listed above: ");
      fflush(stdout);
    }
    else
    {
      inode = ino;
      while (inode)
      {
        if (inode->inode == i) return inode;
        inode = inode->next;
      }
      printf("Invalid inode. Select on of the inodes listed above: ");
      fflush(stdout);
    }
  }   /* while (1) */
}   /* choose_one_inode() */




void name_inode(struct one_inode *ino, char *fname)
/* determines name for inode ino when recovering */
{
  char buf[PATH_MAX];
  char *c;

  if (ino->name)
  {
    if ('*' == ino->name[0])            /* file type instead of name */
    {
      sprintf(buf, "inode-%lu-%s", (long) ino->inode, ino->name+2);   /* w/o "* " */
      c = buf;
      while (*c != '\0')
      {
        if ('/' == *c) *c = '_';
        if (' ' == *c) *c = '_';
        c++;
      }
    }
    else
    {
      sprintf(buf, "%s", ino->name);
      c = buf;
      while (*c != '\0')
      {
        if ('/' == *c) *c = '_';
        c++;
      }
    }
  }
  else sprintf(buf, "inode-%lu", (long) ino->inode);
  sprintf(fname, "%s/%s", save, buf);
}   /* name_inode() */




int dump_inode(ext2_filsys fs, struct one_inode *ino, char *fname)
/* prints content of inode ino on fs into file fname */
/* -1: error, else: number of bytes written */
{
  errcode_t err;
  struct ext2_inode inode_struct;
  int f;
  ext2_file_t e2_file;
  char buf[8192];
  int written, all_written;
  unsigned int n;

  f = open(fname, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
  if (-1 == f) 
  {
    perror(fname);
    return -1;
  }

  err = ext2fs_read_inode(fs, ino->inode, &inode_struct);
  if (err)
  {
    com_err("dump_inode()", err, "while reading inode %u", ino->inode);
    return -1;
  }
  err = ext2fs_file_open(fs, ino->inode, 0, &e2_file);
  if (err)
  {
    com_err("dump_inode()", err, "while opening ext2 file");
    return -1;
  }

  all_written = 0;
  while (1)
  {
    err = ext2fs_file_read(e2_file, buf, sizeof(buf), &n);
    if (err)
    {
      com_err("dump_inode()", err, "while reading ext2 file");
      return -1;
    }
    if (0 == n) break;
    written = write(f, buf, n);
    if (written != n)
    {
      perror(fname);
      return -1;
    }
    all_written += written;
  }   /* while (1) */
  err = ext2fs_file_close(e2_file);
  if (err)
  {
    com_err("dump_inode()", err, "while closing ext2 file");
    return -1;
  }
  close(f);
  printf("%d bytes written to %s\n", all_written, fname);
  return all_written;
}   /* dump_inode() */




void list_inodes_by_device(struct e2undel_log *start)
/* lists entries from e2undel_log list *start, sorted by device number */
/* an entry is listed only if its inode is marked as deleted in the */
/* file system */
{
  unsigned long devs[MAX_DEVICE];
  register int n, count;
  struct e2undel_log *log = start;
  int major, minor;
  char *c1, *c2, *timestr;
  char fname[PATH_MAX];
  errcode_t err;
  ext2_filsys fs;
  struct ext2_inode inode_struct;
  struct passwd *pw;
  
  for (n=0; n<MAX_DEVICE; n++) devs[n] = 0;
  do
  {
    n = 0;
    while (devs[n])
    {
      if (devs[n] == log->dev) break;
      n++;
    }
    if (0 == devs[n]) devs[n] = log->dev;
    log = log->next;
  } while (start != log);

  n = 0;
  while(devs[n])
  {
    count = 0;
    minor = (int) (devs[n] % 0x100);
    major = (int) (devs[n] / 0x100);

    switch (major)
    {
      case  3: sprintf(fname, "%s%d",
                      (minor < 64) ? "/dev/hda" : "/dev/hdb",
                      (minor < 64) ? minor : minor-64);
               break;
      case 22: sprintf(fname, "%s%d",
                      (minor < 64) ? "/dev/hdc" : "/dev/hdd",
                      (minor < 64) ? minor : minor-64);
               break;
      case 33: sprintf(fname, "%s%d",
                      (minor < 64) ? "/dev/hde" : "/dev/hdf",
                      (minor < 64) ? minor : minor-64);
               break;
      case 34: sprintf(fname, "%s%d",
                      (minor < 64) ? "/dev/hdg" : "/dev/hdh",
                      (minor < 64) ? minor : minor-64);
               break;
      case  8: sprintf(fname, "/dev/sd%c%d", (char) (minor/16 + 97),
                      minor % 16);
               break;
      default: sprintf(fname, "0x%lx", devs[n]);
    }   /* switch (major) */
    printf("\n%s (%d, %d)\n", fname, major, minor);
    if ('/' == fname[0])
    {
      if (!(fs = open_fs(fname))) fs = 0;
    }
    else fs = 0;

    if (fs) 
    {
      printf("  inode   owner    size     deleted at  name\n");
      printf("---------------------------------------------------------------------\n");
    }
    else
    {
      printf("  inode  name\n");
      printf("----------------------------\n");
    }
    
    log = start;
    do
    {
      if (log->dev != devs[n]) 
      {
        log = log->next;
        continue;
      }
      if (fs)
      {
        err = ext2fs_read_inode(fs, log->inode, &inode_struct);
        if (err)
        {
          printf("%7lu %s (error while reading inode)\n", (long) log->inode, log->name);
          log = log->next;
          continue;
        }
        if (!(inode_struct.i_dtime && inode_struct.i_size))
        {
          log = log->next;
          continue;
        }

        timestr = ctime( (time_t *) &(inode_struct.i_dtime));
        timestr += 4;                       /* remove day of week */
        c1 = &timestr[12];
        *c1 = ' ';
        c1++;
        c2 = &timestr[18];
        while (*c2 != '\n')                 /* remove seconds and century */
        {
          *c1 = *c2;
          c1++; c2++;
        }
        *c1 = '\0';
        printf("%7lu", (long) log->inode);
        pw = getpwuid(inode_struct.i_uid);
        pw ? printf("%8s", pw->pw_name) : printf("%8d", inode_struct.i_uid);
        printf(" %8d %s %s\n", inode_struct.i_size, timestr, log->name);
      }   /* if (fs) */
      else printf("%7lu  %s\n", (long) log->inode, log->name);
      log = log->next;
      count++;
    } while (start != log);

    if (fs)
       printf("---------------------------------------------------------------------\n");
    else printf("----------------------------\n");
    printf("%d named files %s be recovered on %s\n",
           count, fs ? "can" : "might", fname);
    n++;
  }   /* while (devs[n]) */
}   /* list_inodes_by_device() */




int main(int argc, char *argv[])
{
  ext2_filsys fs;             /* ext2 file system, for low level access */
  unsigned long dev_nr;       /* device number of device to work on */
  struct e2undel_log *log;    /* list of log file entries */
  int n_log;                  /* number of entries in log */
  struct one_inode *del;      /* list of deleted inodes */
  int n_del;                  /* number of deleted inodes */
  int n_del_name;             /* # deleted inodes with name from log file */
  int uid = -1, tim = -1;     /* selected UID and time interval */
  struct one_inode *inode;    /* selected inode to recover */
  char save_name[PATH_MAX];   /* name of revovered file */
  int n_undel_list;           /* # of matching entries from print_inode_list() */

#ifdef PROFILE
  FILE *prof;
  struct timeval tv;
  double last_time = 0.0;
  prof = fopen(PROF_NAME, "a");
  if (!prof) fprintf(stderr, "\ncan't open profile.e2undel, profiling disabled\n\n");
#endif

  initialize_ext2_error_table();
  printf("%s %s\n", argv[0], VERSION);
  eval_args(argc, argv);
  if (isatty(STDOUT_FILENO)) flag_redir = 0; else flag_redir = 1;


  if (flag_list)    /* print contents of log file, then exit */
  {

#ifdef PROFILE
  if (prof)
  {
    if (-1 != gettimeofday(&tv, NULL))
    {
      last_time = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "\nlist mode started at %sread_log(): ", 
              ctime((time_t *) &tv.tv_sec));
      fflush(prof);
    }
  }
#endif

    if (!flag_redir)
    {
      printf("\nreading log file: ");
      fflush(stdout);
    }

    if (NULL == (log = read_log(0, &n_log)))
    {
      printf("no entries in log file\n");
      return 0;
    }
    else
    {

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      last_time = d;
      fprintf(prof, "compact_log() with %d entries: ", n_log); fflush(prof);
    }
  }
#endif

      if (!flag_redir)
      {
        printf("checking log file: ");
        fflush(stdout);
      }
      log = compact_log(log, &n_log);
      if (NULL == log) 
      {
        printf("no entries found in log file\n");
        return 0;
      }
      printf("found %d entries in log file\n", n_log); fflush(stdout);
    } /* else */

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      last_time = d;
      fprintf(prof, "list_inodes_by_device() for remaining %d entries: ",
              n_log);  fflush(prof);
    }
  }
#endif

    list_inodes_by_device(log);

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      fprintf(prof, "finished\n");
      fclose(prof);
    }
  }
#endif

    return 0;
  }   /* if (flag_list) */


/* undelete files */

  printf("Trying to recover files on %s, saving them on %s\n", 
         device, save);
  if (!flag_all) 
   printf("Checking only for named deleted files (use '-a' for all files)\n");
#ifdef WITH_FILE
  else
  {
    if (!flag_magic)
     printf("Use '-t' to get some information about the content of deleted files\n");
  }
#endif
  printf("\n");


/* open device for low level reading */

  if (!(fs = open_fs(device))) return 1;
  show_fs_info(fs);

  dev_nr = check_path(device, save);
  if (!dev_nr)
  {
    ext2fs_close(fs);
    return 1;
  }

#ifdef PROFILE
  if (prof)
  {
    if (-1 != gettimeofday(&tv, NULL))
    {
      last_time = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "\n%sread_log_tree() on device %s, saving on %s: ", 
              ctime((time_t *) &tv.tv_sec), device, save);
      fflush(prof);
    }
  }
#endif


/* read log file if existing */

  if (!flag_redir)
  {
    printf("\nreading log file: ");
    fflush(stdout);
  }
  if (NULL == (log = read_log_tree(dev_nr, &n_log)))
  {
    printf("no entries for %s in log file\n", device);
    if (!flag_all)
    {
      ext2fs_close(fs);
      return 0;
    }
  }
  else printf("found %d entries for %s in log file\n", n_log, device);

#ifdef WITH_FILE
  if (flag_magic)
  {
    if (!apprentice())
    {
      fprintf(stderr, 
              "Trouble with magic types, file type detection disabled.\n");
      flag_magic = 0;
    }
  }
#endif

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      last_time = d;
      fprintf(prof, "find_del(): ");
    }
  }
#endif


/* scan device for deleted files */

  printf("searching for deleted inodes on %s:\n", device); fflush(stdout);
  del = find_del(fs, &n_del);
  if (NULL == del)              /* error or no deleted inodes found */
  {
    ext2fs_close(fs);
    return 0;
  }


/* match deleted files with names from log; build user x time table */

  if (log)
  {

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      last_time = d;
      fprintf(prof, "add_names() for %d log entries and %d deleted inodes: ",
            n_log, n_del);
    }
  }
#endif

    printf("checking names from log file for deleted files: "); fflush(stdout);
    n_del_name = add_names(del, log);
    printf("%d deleted files with names\n", n_del_name);

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      last_time = d;
      fprintf(prof, "build_table(), %s files (%d): ",
             flag_all ? "all" : "only named", flag_all ? n_del : n_del_name);
    }
  }
#endif

  }   /* if (log) */
  else n_del_name = 0;
  
  if (n_del_name || flag_all) build_table(del);
  else
  {
    printf("use '-a' for all deleted files\n");
    ext2fs_close(fs);
    return 0;
  }

#ifdef PROFILE
  if (prof)
  {
    double d;
    if (-1 != gettimeofday(&tv, NULL))
    {
      d = (double) tv.tv_sec + (double) tv.tv_usec / 1000000.0;
      fprintf(prof, "%6.3f s\n", d - last_time);
      fprintf(prof, "entering intercative part: \n");
    }
  }
#endif


/* the interactive part: ask for user name and time interval, print matching */
/* entries, ask for inode numbers to undelete, dump file contents */

  while (1)
  {
    print_table();
    if (flag_redir)
    {
      ext2fs_close(fs);
      return 0;
    }
    uid = choose_user();
    if (-1 == uid)
    {
      ext2fs_close(fs);
      return 0;
    }
    tim = choose_interval();
    if (-1 == tim)
    {
      ext2fs_close(fs);
      return 0;
    }
    n_undel_list = print_inode_list(fs, del, uid, tim);
    if (n_undel_list)
    {
      do
      {
        inode = choose_one_inode(del);
        if (inode) 
        {
          name_inode(inode, save_name);
          dump_inode(fs, inode, save_name);
        }
      } while (inode);
    }
  }   /* while (1) */

  if (ext2fs_close(fs))
  {
    fprintf(stderr, " %s: error while closing\n", device);
    return 1;
  }
  else return 0;
}
