/* find_del.c: part of e2undel.c since version 0.2 */
/* gathers list of deleted inode */
/* heavily based on lsdel.c from Ted T'so's debugfs */

/* (C) Oliver Diedrich */
/* this file may be redistributed under the terms of the */
/* GNU Public License (GPL), see www.gnu.org */


#include "e2undel.h"



static int check_bl(ext2_filsys fs, blk_t *block, int block_count, void *p)
/* this is called from find_del() via ext2fs_block_iterate() for each block */
/* belongig to an inode */
{
  struct one_inode *ino = (struct one_inode *) p;

  ino->blocks++;
  if (*block < fs->super->s_first_data_block ||
      *block >= fs->super->s_blocks_count) 
  {
    ino->bad_blocks++;
    return BLOCK_ABORT;
  }
  if (fs->block_map)          /* just to be sure ... */
  {
    if (!ext2fs_test_block_bitmap(fs->block_map, *block)) ino->free_blocks++;
  }
  else printf("\n>>> very strange\n");
  return 0;
}   /* check_bl() */



struct one_inode *find_del(ext2_filsys fs, int *n_deleted)
/* scans file system fs for deleted inodes and returns list of them */
/* sets *n_deleted to number of deleted files */
/* return: list of deleted files; NULL: error */
{
  errcode_t err;
  ext2_inode_scan scan;
  ext2_ino_t inode;
  struct ext2_inode inode_struct;
  struct one_inode *ino = NULL;
  struct one_inode *inode_list;
  struct one_inode ino_tmp;        /* given to check_bl() for block counting */
  int n, n_del;                    /* counts all and deleted inodes on fs */
  char buf[255];
  char *blockbuf;
  int div = 0;

  blockbuf = malloc(fs->blocksize * 3);
  if (!blockbuf)
  {
    fprintf(stderr, "out of memory while allocating block buffer\n");
    return NULL;
  }

  err = ext2fs_open_inode_scan(fs,      /* file system */
                               0,       /* use suitable value for block buffer */
                               &scan);  /* inode iteration variable */
  if (err)
  {
    com_err("find_del()", err, "while initializing inode scan");
    return NULL;
  }

  if (!flag_redir)
  {
    div = fs->super->s_inodes_count / 50;
    for (n=0; n<51; n++) fprintf(stderr, "-");
    fprintf(stderr, "|"); fflush(stderr);
    fprintf(stderr, "\r|");
  }

  n_del = 0;
  n = 0;
  inode_list = NULL;
  do
  {
    n++;
    if (!flag_redir && (0 == n % div))
    {
      fprintf(stderr, "=");
      fflush(stderr);
    }
    err = ext2fs_get_next_inode(scan, &inode, &inode_struct);
    if (err)             /* only EXT2_ET_BAD_BLOCK_IN_INODE_TABLE possible? */
    {
      sprintf(buf, "while getting inode %d", n-1);
      com_err("find_del()", err, buf);
      continue;
    }
    if (inode && inode_struct.i_dtime)  /* inode found, and it is deleted */
    {
      ino_tmp.blocks = 0;
      ino_tmp.free_blocks = 0;
      ino_tmp.bad_blocks = 0;
      err = ext2fs_block_iterate(fs, inode, 0, blockbuf, check_bl, &ino_tmp);
      if (err)
      {
        com_err("find_del()", err, "in ext2_block_iterate()");
        ext2fs_close_inode_scan(scan);
        return NULL;
      }

      if (ino_tmp.free_blocks && !ino_tmp.bad_blocks)
      {                                 /* free blocks, and no bad blocks */
        n_del++;
        if (!inode_list)
        {
          ino = malloc(sizeof(struct one_inode));
          if (!ino)
          {
            fprintf(stderr, "out of memory when allocating inode list\n");
            ext2fs_close_inode_scan(scan);
            return NULL;
          }
          inode_list = ino;
          ino->next = NULL;
        }
        else
        {
          ino->next = malloc(sizeof(struct one_inode));
          if (!ino->next)
          {
            fprintf(stderr, "out of memory when allocating inode list\n");
            ext2fs_close_inode_scan(scan);
            return NULL;
          }
          ino = ino->next;
          ino->next = NULL;
        }

        ino->inode = inode;
        ino->mode = inode_struct.i_mode;
        ino->uid = inode_struct.i_uid;
        ino->size = inode_struct.i_size;
        ino->mtime = inode_struct.i_mtime;
        ino->dtime = inode_struct.i_dtime;
        ino->blocks = ino_tmp.blocks;
        ino->free_blocks = ino_tmp.free_blocks;
        ino->bad_blocks = ino_tmp.bad_blocks;
      }   /* if (free_blocks && !bad_blocks) */
    }   /* if (inode_struct.i_dtime) */
  } while (inode);
  if (!flag_redir) fprintf(stderr, "\n");

  printf("%d inodes scanned, %d deleted files found\n", n-1, n_del);
  ext2fs_close_inode_scan(scan);
  *n_deleted = n_del;
  return inode_list;
}
