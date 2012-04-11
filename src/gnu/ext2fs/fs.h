/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fs.h	8.7 (Berkeley) 4/19/94
 * $FreeBSD: src/sys/gnu/ext2fs/fs.h,v 1.11 2002/05/16 19:07:59 iedowse Exp $
 */

/*
 * Each disk drive contains some number of file systems.
 * A file system consists of a number of cylinder groups.
 * Each cylinder group has inodes and data.
 *
 * A file system is described by its super-block, which in turn
 * describes the cylinder groups.  The super-block is critical
 * data and is replicated in each cylinder group to protect against
 * catastrophic loss.  This is done at `newfs' time and the critical
 * super-block data does not change, so the copies need not be
 * referenced further unless disaster strikes.
 *
 * The first boot and super blocks are given in absolute disk addresses.
 * The byte-offset forms are preferred, as they don't imply a sector size.
 */
#define SBSIZE		1024
#define SBLOCK		2

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in 
 * the super block for this name.
 */
#define MAXMNTLEN 512

/*
 * Macros for access to superblock array structures
 */

/*
 * Convert cylinder group to base address of its global summary info.
 */
#define fs_cs(fs, cgindx)      (((struct ext2_group_desc *) \
        (buf_dataptr(fs->s_group_desc[cgindx / EXT2_DESC_PER_BLOCK(fs)]))) \
		[cgindx % EXT2_DESC_PER_BLOCK(fs)])

/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define fsbtodb(fs, b)	((b) << ((fs)->s_fsbtodb))
#define	dbtofsb(fs, b)	((b) >> ((fs)->s_fsbtodb))

/* get group containing inode */
#define ino_to_cg(fs, x)	(((x) - 1) / EXT2_INODES_PER_GROUP(fs))

/* get block containing inode from its number x */
#define	ino_to_fsba(fs, x)	le32_to_cpu(fs_cs(fs, ino_to_cg(fs, x)).bg_inode_table) + \
	(((x)-1) % EXT2_INODES_PER_GROUP(fs))/EXT2_INODES_PER_BLOCK(fs)

/* get offset for inode in block */
#define	ino_to_fsbo(fs, x)	((x-1) % EXT2_INODES_PER_BLOCK(fs))

/*
 * Give cylinder group number for a file system block.
 * Give cylinder group block number for a file system block.
 */
#define	dtog(fs, d)	(((d) - fs->s_es->s_first_data_block) / \
			EXT2_BLOCKS_PER_GROUP(fs))
#define	dtogd(fs, d)	(((d) - fs->s_es->s_first_data_block) % \
			EXT2_BLOCKS_PER_GROUP(fs))

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */
#define blkoff(fs, loc)		/* calculates (loc % fs->fs_bsize) */ \
	((loc) & (fs)->s_qbmask)

#define lblktosize(fs, blk)	/* calculates (blk * fs->fs_bsize) */ \
	((blk) << (fs->s_bshift))

#define lblkno(fs, loc)		/* calculates (loc / fs->fs_bsize) */ \
	((loc) >> (fs->s_bshift))

/* no fragments -> logical block number equal # of frags */
#define numfrags(fs, loc)	/* calculates (loc / fs->fs_fsize) */ \
	((loc) >> (fs->s_bshift))

#define fragroundup(fs, size)	/* calculates roundup(size, fs->fs_fsize) */ \
	roundup(size, fs->s_frag_size)
	/* was (((size) + (fs)->fs_qfmask) & (fs)->fs_fmask) */

/*
 * Determining the size of a file block in the file system.
 * easy w/o fragments
 */
#define blksize(fs, ip, lbn) ((fs)->s_frag_size)

/*
 * INOPB is the number of inodes in a secondary storage block.
 */
#define	INOPB(fs)	EXT2_INODES_PER_BLOCK(fs)

/*
 * NINDIR is the number of indirects in a file system block.
 */
#define	NINDIR(fs)	(EXT2_ADDR_PER_BLOCK(fs))

extern int inside[], around[];
extern u_char *fragtbl[];

/* a few remarks about superblock locking/unlocking
 * Linux provides special routines for doing so
 * FreeBSD uses VOP_LOCK/VOP_UNLOCK on the device vnode
 * For Darwin, the in-core sb contains its own lock
 */
#define lock_super(sbp) lck_mtx_lock((sbp)->s_lock)
#define unlock_super(sbp) lck_mtx_unlock((sbp)->s_lock)

#if defined(E2_BUF_CHECK) && defined(obsolete)
TAILQ_HEAD(bqueues, buf);
extern struct bqueues bufqueues[];
extern unsigned	splbio(void);
extern void	splx(unsigned level);

#define e2_decl_buf_cp struct buf obuf;
#define e2_buf_cp(b) \
obuf = *(b); \
obuf.b_hash = (b)->b_hash; \
obuf.b_vnbufs = (b)->b_vnbufs; \
obuf.b_freelist = (b)->b_freelist;

static __inline__ void e2_chk_buf_lck(buf_t  bp, buf_t  obp)
{
   buf_t  ent;
   struct bqueues *bqh;
   int found = 0, s, idx;
   char *bufnames[BQUEUES+1] = {"LOCKED","LRU","AGE","EMPTY","META","LAUNDRY",""};
   
   s = splbio();
   for(bqh = bufqueues, idx=-1; bqh <= &bufqueues[BQUEUES-1] && !found; bqh++, idx++) {
      TAILQ_FOREACH(ent, bqh, b_freelist) {
         if (ent == bp) {
            found = 1;
            break;
         }
      }
   }
   
   if (!found) {
      panic("ext2: locked buf 0x%x not found on any list! "
      "pre-brelse buf = 0x%x, pre-brelse flags = %d, pre-brelse q = %ld",
      bp, obp, obp->b_flags, obp->b_whichq);
   }
   
   if (idx != BQ_LOCKED) {
      panic("ext2: locked buf 0x%x is not on locked list! "
      "Actual list = %s, pre-brelse buf = 0x%x, pre-brelse flags = %d, pre-brelse q = %ld",
      bp, bufnames[idx], obp, obp->b_flags, obp->b_whichq);
   }
   splx(s);
}
#else
#define e2_chk_buf_lck(bp,obp)
#define e2_decl_buf_cp
#define e2_buf_cp(bp)
#endif /* E2_BUF_CHECK */

/* FS Meta data only! */
#define EXT_BUF_DIRTY 0x00000001

#define BMETA_DIRTY(bp) do { \
    uintptr_t flags = (uintptr_t)buf_fsprivate(bp); \
    flags |= EXT_BUF_DIRTY; \
    buf_setfsprivate(bp, (void*)flags); \
} while(0)

/* this is supposed to mark a buffer dirty on ready for delayed writing */
#define mark_buffer_dirty(bp) BMETA_DIRTY(bp)

#define BMETA_CLEAN(bp) do { \
    uintptr_t flags = (uintptr_t)buf_fsprivate(bp); \
    flags &= ~EXT_BUF_DIRTY; \
    buf_setfsprivate(bp, (void*)flags); \
} while(0)

#define BMETA_ISDIRTY(bp) (0 != ((uintptr_t)buf_fsprivate(bp) & EXT_BUF_DIRTY))

/*
 * To lock a buffer, set the B_LOCKED flag and then brelse() it. To unlock,
 * reset the B_LOCKED flag and brelse() the buffer back on the LRU list
 */ 
#define LCK_BUF(bp) do { \
	buf_setflags(bp, B_LOCKED); \
	buf_brelse(bp); \
} while(0)

//xxx More nastiness because of KPI limitations. We have to get the buf marked as busy to write/relse it.
#define ULCK_BUF(bp) do { \
    daddr64_t blk = buf_blkno(bp); \
    vnode_t vp = buf_vnode(bp); \
    int bytes = (int)buf_count(bp); \
	buf_clearflags(bp, (B_LOCKED)); \
    if (buf_getblk(vp, blk, bytes, 0, 0, BLK_META|BLK_ONLYVALID) != bp) \
        panic("ext2: cached group buffer not found!"); \
	if (BMETA_ISDIRTY(bp)) { \
		BMETA_CLEAN(bp); \
		buf_bwrite(bp); \
	} else \
		buf_brelse(bp); \
} while(0)

/* Copied from ufs/ufs/dir.h */
/*
 * Template for manipulating directories.  Should use struct direct's,
 * but the name field is MAXNAMLEN - 1, and this just won't do.
 */
struct dirtemplate {
	u_int32_t	dot_ino;
	int16_t		dot_reclen;
	u_int8_t	dot_type;
	u_int8_t	dot_namlen;
	char		dot_name[4];	/* must be multiple of 4 */
	u_int32_t	dotdot_ino;
	int16_t		dotdot_reclen;
	u_int8_t	dotdot_type;
	u_int8_t	dotdot_namlen;
	char		dotdot_name[4];	/* ditto */
};
