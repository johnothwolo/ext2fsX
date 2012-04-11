/* e2undel.h */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <linux/ext2_fs.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_io.h>
#include <time.h>


#define VERSION "0.81"
#define LOGNAME "/var/e2undel/e2undel"
/* for user X interval table of recoverable files */
#define MAX_USER 500
#define MAX_INTERVAL 6
/* max # of recoverable files in one table cell */
#define MAX_UNDEL_LIST 500000
/* max # of devices in list mode */
#define MAX_DEVICE 30

/* this is not perfectly correct: ext2_types.h was introduced with
   e2fsprogs 1.21, but the old ino_t (til e2fsprogs 1.19) was renamed to
   ext2_ino_t in e2fsprogs 1.20; so this probably will yield an error
   with e2fsprogs 1.20 
   ext2_ino_t is unsigned int in ext2_types.h, but ino_t is unsigned long;
   we print each ext2_ino_t as long with a (long) type casting. Ugly hack. */

#ifndef _EXT2_TYPES_H
typedef ino_t ext2_ino_t;
#endif


struct e2undel_log
{
  ext2_ino_t inode;
  unsigned long dev;                 /* device number of file system */
  char *name;
  struct e2undel_log *next, *last;   /* next and previous entry in ring list */
};                                   /* in list mode; next entry with larger */
                                     /* smaller inode # in binary tree */
struct one_inode
{
  ext2_ino_t inode;           /* inode number */
  unsigned short mode;        /* rwx; not used yet */
  unsigned short uid;         /* user ID */
  unsigned int size;          /* file size */
  unsigned int mtime;         /* modification time; not used yet */
  unsigned int dtime;         /* deletion time */
  unsigned int blocks;        /* counted total # of blocks */
  unsigned int free_blocks;   /* free blocks; if == 0, inode is discarded in find_del() */
  unsigned int bad_blocks;    /* bad blocks; if > 0, inode is discarded in find_del() */
  char *name;                 /* file name or type */
  int interval;               /* time interval since deletion from interval[] */
  struct one_inode *next;
};

extern int flag_redir;        /* if != 0, output is sended to file or pipe */

struct one_inode *find_del(ext2_filsys fs, int *n_deleted);
struct e2undel_log *read_log(unsigned long devnr, int *n);
struct e2undel_log *read_log_tree(unsigned long devnr, int *n);
struct e2undel_log *compact_log(struct e2undel_log *log, int *n);
void print_log(struct e2undel_log *l);
void print_tree(struct e2undel_log *l);
