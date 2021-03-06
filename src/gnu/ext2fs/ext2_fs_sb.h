/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 *  linux/include/linux/ext2_fs_sb.h
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/include/linux/minix_fs_sb.h
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#ifndef _LINUX_EXT2_FS_SB
#define _LINUX_EXT2_FS_SB

/*
 * The following is not needed anymore since the descriptors buffer
 * heads are now dynamically allocated
 */
/* #define EXT2_MAX_GROUP_DESC	8 */

#define EXT2_MAX_GROUP_LOADED	8

#define MAXMNTLEN	512

#include <kern/locks.h>

/*
 * second extended-fs super-block data in memory
 */
struct m_ext2fs {
	unsigned long e2fs_fsize;	/* Size of a fragment in bytes */
	unsigned long e2fs_fpb;/* Number of fragments per block */
	unsigned long e2fs_ipb;/* Number of inodes per block */
	unsigned long e2fs_fpg;/* Number of fragments in a group */
	unsigned long e2fs_bpg;/* Number of blocks in a group */
	unsigned long e2fs_ipg;/* Number of inodes in a group */
	unsigned long e2fs_itpg;	/* Number of inode table blocks per group */
	unsigned long e2fs_dbpg;	/* Number of descriptor blocks per group */
	unsigned long s_desc_per_block;	/* Number of group descriptors per block */
	unsigned long s_groups_count;	/* Number of groups in the fs */
	unsigned long e2fs_isize;	  /* Size of inode */
	struct buffer_head * s_sbh;	/* Buffer containing the super block */
	struct ext2fs * s_es;	/* Pointer to the super block in the buffer */
	buf_t* s_group_desc;
	unsigned short s_loaded_inode_bitmaps;
	unsigned short s_loaded_block_bitmaps;
	unsigned long s_inode_bitmap_number[EXT2_MAX_GROUP_LOADED];
	buf_t s_inode_bitmap[EXT2_MAX_GROUP_LOADED];
	unsigned long s_block_bitmap_number[EXT2_MAX_GROUP_LOADED];
	buf_t s_block_bitmap[EXT2_MAX_GROUP_LOADED];
	unsigned long  s_mount_opt;
	unsigned short s_resuid;
	unsigned short s_resgid;
	unsigned short s_mount_state;
    u_int32_t s_hash_seed[4]; /* HTREE hash seed (byte swapped if necessary) */
	u_int8_t s_def_hash_version; /* Default hash version to use */
	/* 
	   stuff that FFS keeps in its super block or that linux
	   has in its non-ext2 specific super block and which is
	   generally considered useful 
	*/
	unsigned long s_blocksize;
	unsigned long s_blocksize_bits;
	unsigned int  s_bshift;			/* = log2(s_blocksize) */
	quad_t	 s_qbmask;			/* = s_blocksize - 1 */
    quad_t    s_maxfilesize;
	unsigned int  s_fsbtodb;		/* shift to get disk block */
	char    s_rd_only;                      /* read-only 		*/
	char    s_dirt;                         /* fs modified flag */
	char	s_wasvalid;			/* valid at mount time */
   
   quad_t s_dircount; /* In core count of all directories */
   u_int32_t s_d_blocksize; /* block size of underlying device */
   lck_mtx_t *s_lock; /* lock to protect access to in core sb */
   uid_t s_uid_noperm; /* used when mounted w/o permissions in effect */
   gid_t s_gid_noperm; /* ditto */
   int32_t  s_uhash;	  /* 3 if hash should be signed, 0 if not */

	char    fs_fsmnt[MAXMNTLEN];            /* name mounted on */
};

static inline int
e2fs_overflow(struct m_ext2fs *fs, off_t lower, off_t value)
{
	return (value < lower || value > fs->s_maxfilesize);
}

#endif	/* _LINUX_EXT2_FS_SB */
