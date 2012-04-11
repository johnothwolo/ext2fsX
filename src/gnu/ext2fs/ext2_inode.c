/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ffs_inode.c	8.5 (Berkeley) 12/30/93
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_inode.c,v 1.37 2002/10/14 03:20:34 mckusick Exp $
 */

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_inode.c,v 1.23 2006/08/27 15:32:09 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/namei.h> /* cache_purge() */
#include <sys/ubc.h>
//#include <sys/trace.h>
static int prtactive = 0;
static void e2vprint(const char *label, vnode_t vp);
#define vprint e2vprint

#include "ext2_apple.h"

#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>

static int ext2_indirtrunc(struct inode *, int32_t, int32_t, int32_t, int,
	    long *);

/*
 * Update the access, modified, and inode change times as specified by the
 * IN_ACCESS, IN_UPDATE, and IN_CHANGE flags respectively.  Write the inode
 * to disk if the IN_MODIFIED flag is set (it may be set initially, or by
 * the timestamp update).  The IN_LAZYMOD flag is set to force a write
 * later if not now.  If we write now, then clear both IN_MODIFIED and
 * IN_LAZYMOD to reflect the presumably successful write, and if waitfor is
 * set, then wait for the write to complete.
 */
int
ext2_update(vp, waitfor)
	vnode_t vp;
	int waitfor;
{
	struct ext2_sb_info *fs;
	buf_t  bp;
	struct inode *ip;
	int error;
	
	if (vnode_vfsisrdonly(vp))
		return (0);

	ext2_itimes(vp);
	ip = VTOI(vp);
	IXLOCK(ip);
	if ((ip->i_flag & IN_MODIFIED) == 0) {
		IULOCK(ip);
		return (0);
	}
	fs = ip->i_e2fs;
	IULOCK(ip);
	if ((error = buf_meta_bread(ip->i_devvp,
	    (daddr64_t)fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
		(int)fs->s_blocksize, NOCRED, &bp)) != 0) {
		buf_brelse(bp);
		return (error);
	}
	IXLOCK(ip);
	ext2_i2ei(ip, (struct ext2_inode *)((char *)buf_dataptr(bp) +
	    EXT2_INODE_SIZE * ino_to_fsbo(fs, ip->i_number)));
	ip->i_flag &= ~(IN_LAZYMOD | IN_MODIFIED);
	IULOCK(ip);
	if (waitfor && (vfs_flags(vnode_mount(vp)) & MNT_ASYNC) == 0)
		return (buf_bwrite(bp));
   
   buf_bdwrite(bp);
   return (0);
}

#define	SINGLE	0	/* index of single indirect block */
#define	DOUBLE	1	/* index of double indirect block */
#define	TRIPLE	2	/* index of triple indirect block */
/*
 * Truncate the inode oip to at most length size, freeing the
 * disk blocks.
 */
int
ext2_truncate(vp, length, flags, cred, p)
	vnode_t vp;
	off_t length;
	int flags;
	ucred_t cred;
	proc_t p;
{
	vnode_t ovp = vp;
	int32_t lastblock;
	struct inode *oip;
	int32_t bn, lbn, lastiblock[NIADDR], indir_lbn[NIADDR];
	int32_t oldblks[NDADDR + NIADDR], newblks[NDADDR + NIADDR];
	struct ext2_sb_info *fs;
	buf_t  bp;
	int offset, size, level;
	long count, nblocks, blocksreleased = 0;
	int aflags, error, i, allerror;
	int devBlockSize, vflags;
	off_t osize;

#if 0
	ext2_trace(" inode %d to %qu\n", VTOI(ovp)->i_number, length);
#endif	
	
	oip = VTOI(ovp);
	fs = oip->i_e2fs;
	
	/* 
	 * negative file sizes will totally break the code below and
	 * are not meaningful anyways.
	 */
	if (length < 0 || length > fs->s_maxfilesize)
	    return (EFBIG);
   
	/* Note, it is possible to get here with an inode that is not completely
	   initialized. This can happen when ext2_vget() fails and calls vput() on
	   an inode that is only partially setup.
	 */
   
	IXLOCK(oip);
	if (VLNK == vnode_vtype(ovp) &&
	    oip->i_size < vnode_vfsmaxsymlen(ovp)) {
#if DIAGNOSTIC
		if (length != 0)
			panic("ext2_truncate: partial truncate of symlink");
#endif
		bzero((char *)&oip->i_shortlink, (u_int)oip->i_size);
		oip->i_size = 0;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		IULOCK(oip);
		return (ext2_update(ovp, 1));
	}
	if (oip->i_size == length) {
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		IULOCK(oip);
		return (ext2_update(ovp, 0));
	}
	devBlockSize = fs->s_d_blocksize;
	osize = oip->i_size;
	ext2_discard_prealloc(oip);
	/*
	 * Lengthen the size of the file. We must ensure that the
	 * last byte of the file is allocated. Since the smallest
	 * value of osize is 0, length will be at least 1.
	 */
	if (osize < length) {
		offset = blkoff(fs, length - 1);
		lbn = lblkno(fs, length - 1);
		aflags = B_CLRBUF;
		if (flags & IO_SYNC)
			aflags |= B_SYNC;
		if ((error = ext2_balloc(oip, lbn, offset + 1, cred, &bp, aflags)) != 0) {
			IULOCK(oip);
			return (error);
		}
		oip->i_size = length;
		oip->i_flag |= IN_CHANGE | IN_UPDATE;
		IULOCK(oip);
		
		if (UBCINFOEXISTS(ovp)) {
			buf_markinvalid(bp);
			buf_bwrite(bp);
			ubc_setsize(ovp, (off_t)length); 
		} else {
			if (aflags & B_SYNC)
				buf_bwrite(bp);
			else
				buf_bawrite(bp);
		}
		return (ext2_update(ovp, 1));
	}
	/*
	 * Shorten the size of the file. If the file is not being
	 * truncated to a block boundry, the contents of the
	 * partial block following the end of the file must be
	 * zero'ed in case it ever become accessible again because
	 * of subsequent file growth.
	 */
	/* I don't understand the comment above */
   IULOCK(oip);
   
   if (UBCINFOEXISTS(ovp))
		ubc_setsize(ovp, (off_t)length); 

	vflags = ((length > 0) ? BUF_WRITE_DATA : 0) | BUF_SKIP_META;
	allerror = buf_invalidateblks(ovp, vflags, 0, 0);
   
	IXLOCK(oip);
	offset = blkoff(fs, length);
	if (offset == 0) {
		oip->i_size = length;
	} else {
		lbn = lblkno(fs, length);
		aflags = B_CLRBUF;
		if (flags & IO_SYNC)
			aflags |= B_SYNC;
		if ((error = ext2_balloc(oip, lbn, offset, cred, &bp, aflags)) != 0) {
			IULOCK(oip);
			return (error);
		}
		oip->i_size = length;
		size = blksize(fs, oip, lbn);
		bzero((char *)buf_dataptr(bp) + offset, (u_int)(size - offset));
        IULOCK(oip);
		if (UBCINFOEXISTS(ovp)) {
			buf_markinvalid(bp);
			buf_bwrite(bp);
		} else {
			if (aflags & B_SYNC)
				buf_bwrite(bp);
			else
				buf_bawrite(bp);
        }
	    IXLOCK(oip);
	}
	/*
	 * Calculate index into inode's block list of
	 * last direct and indirect blocks (if any)
	 * which we want to keep.  Lastblock is -1 when
	 * the file is truncated to 0.
	 */
	lastblock = lblkno(fs, length + fs->s_blocksize - 1) - 1;
	lastiblock[SINGLE] = lastblock - NDADDR;
	lastiblock[DOUBLE] = lastiblock[SINGLE] - NINDIR(fs);
	lastiblock[TRIPLE] = lastiblock[DOUBLE] - NINDIR(fs) * NINDIR(fs);
    nblocks = btodb(fs->s_blocksize, devBlockSize);
   
	/*
	 * Update file and block pointers on disk before we start freeing
	 * blocks.  If we crash before free'ing blocks below, the blocks
	 * will be returned to the free list.  lastiblock values are also
	 * normalized to -1 for calls to ext2_indirtrunc below.
	 */
	bcopy((caddr_t)&oip->i_db[0], (caddr_t)oldblks, sizeof oldblks);
	for (level = TRIPLE; level >= SINGLE; level--)
		if (lastiblock[level] < 0) {
			oip->i_ib[level] = 0;
			lastiblock[level] = -1;
		}
	for (i = NDADDR - 1; i > lastblock; i--)
		oip->i_db[i] = 0;
	oip->i_flag |= IN_CHANGE | IN_UPDATE;
	IULOCK(oip);
	allerror = ext2_update(ovp, 1);

	/*
	 * Having written the new inode to disk, save its new configuration
	 * and put back the old block pointers long enough to process them.
	 * Note that we save the new block configuration so we can check it
	 * when we are done.
	 */
	IXLOCK(oip);
	bcopy((caddr_t)&oip->i_db[0], (caddr_t)newblks, sizeof newblks);
	bcopy((caddr_t)oldblks, (caddr_t)&oip->i_db[0], sizeof oldblks);
	oip->i_size = osize;
    vflags = ((length > 0) ? BUF_WRITE_DATA : 0) | BUF_SKIP_META;
	IULOCK(oip);
    allerror = buf_invalidateblks(ovp, vflags, 0, 0);

	/*
	 * Indirect blocks first.
	 */
	IXLOCK(oip);
	indir_lbn[SINGLE] = -NDADDR;
	indir_lbn[DOUBLE] = indir_lbn[SINGLE] - NINDIR(fs) - 1;
	indir_lbn[TRIPLE] = indir_lbn[DOUBLE] - NINDIR(fs) * NINDIR(fs) - 1;
	for (level = TRIPLE; level >= SINGLE; level--) {
		bn = oip->i_ib[level];
		if (bn != 0) {
			error = ext2_indirtrunc(oip, indir_lbn[level],
			    fsbtodb(fs, bn), lastiblock[level], level, &count);
			if (error)
				allerror = error;
			blocksreleased += count;
			if (lastiblock[level] < 0) {
				oip->i_ib[level] = 0;
				ext2_blkfree(oip, bn, fs->s_frag_size);
				blocksreleased += nblocks;
			}
		}
		if (lastiblock[level] >= 0)
			goto done; // lock still held
	}

	/*
	 * All whole direct blocks or frags.
	 */
	for (i = NDADDR - 1; i > lastblock; i--) {
		long bsize;

		bn = oip->i_db[i];
		if (bn == 0)
			continue;
		oip->i_db[i] = 0;
		bsize = blksize(fs, oip, i);
		ext2_blkfree(oip, bn, bsize);
		blocksreleased += btodb(bsize, devBlockSize);
	}
	if (lastblock < 0)
		goto done;

	/*
	 * Finally, look for a change in size of the
	 * last direct block; release any frags.
	 */
	bn = oip->i_db[lastblock];
	if (bn != 0) {
		long oldspace, newspace;

		/*
		 * Calculate amount of space we're giving
		 * back as old block size minus new block size.
		 */
		oldspace = blksize(fs, oip, lastblock);
		oip->i_size = length;
		newspace = blksize(fs, oip, lastblock);
		if (newspace == 0)
			panic("ext2_itrunc: newspace");
		if (oldspace - newspace > 0) {
			/*
			 * Block number of space to be free'd is
			 * the old block # plus the number of frags
			 * required for the storage we're keeping.
			 */
			bn += numfrags(fs, newspace);
			ext2_blkfree(oip, bn, oldspace - newspace);
			blocksreleased += btodb(oldspace - newspace, devBlockSize);
		}
	}
done:
#if DIAGNOSTIC
	for (level = SINGLE; level <= TRIPLE; level++)
		if (newblks[NDADDR + level] != oip->i_ib[level])
			panic("ext2: itrunc1");
	for (i = 0; i < NDADDR; i++)
		if (newblks[i] != oip->i_db[i])
			panic("ext2: itrunc2");
   if (length == 0 && (vnode_hasdirtyblks(ovp) /* ??? || vnode_hascleanblks(ovp) */))
		panic("ext2: itrunc3");
#endif /* DIAGNOSTIC */
	/*
	 * Put back the real size.
	 */
	oip->i_size = length;
	oip->i_blocks -= blocksreleased;
	if (oip->i_blocks < 0)			/* sanity */
		oip->i_blocks = 0;
	oip->i_flag |= IN_CHANGE;
	IULOCK(oip);
   
	return (allerror);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block bn.  Blocks are free'd in LIFO order up to (but not including)
 * lastbn.  If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 *
 * NB: triple indirect blocks are untested.
 */

static int
ext2_indirtrunc(ip, lbn, dbn, lastbn, level, countp)
	struct inode *ip;
	int32_t lbn, lastbn;
	int32_t dbn;
	int level;
	long *countp;
{
	buf_t  bp;
	struct ext2_sb_info *fs = ip->i_e2fs;
	vnode_t vp;
	ext2_daddr_t *bap, *copy, nb, nlbn, last;
	long blkcount, factor;
	int i, nblocks, blocksreleased = 0;
	int error = 0, allerror = 0;
    u_int32_t devBlockSize=0;
    buf_t  tbp;
   
    devBlockSize = ip->i_e2fs->s_d_blocksize;
   
    /* Doing a MALLOC here is asking for trouble. We can still
	 * deadlock on pagerfile lock, in case we are running
	 * low on memory and block in MALLOC
	 */

	tbp = buf_geteblk(fs->s_blocksize);
	copy = (int32_t *)buf_dataptr(tbp);

	/*
	 * Calculate index in current block of last
	 * block to be kept.  -1 indicates the entire
	 * block so we need not calculate the index.
	 */
	factor = 1;
	for (i = SINGLE; i < level; i++)
		factor *= NINDIR(fs);
	last = lastbn;
	if (lastbn > 0)
		last /= factor;
	nblocks = btodb(fs->s_blocksize, devBlockSize);
   
	/*
	 * Get buffer of block pointers, zero those entries corresponding
	 * to blocks to be free'd, and update on disk copy first.  Since
	 * double(triple) indirect before single(double) indirect, calls
	 * to bmap on these blocks will fail.  However, we already have
	 * the on disk address, so we have to set the b_blkno field
	 * explicitly instead of letting bread do everything for us.
	 */
	vp = ITOV(ip);
	vnode_t devvp = ip->i_devvp;
	IULOCK(ip); // unlock for IO
	
	bp = buf_getblk(vp, (daddr64_t)lbn, (int)fs->s_blocksize, 0, 0, BLK_META);
#ifdef obsolete
	if (buf_valid(bp)) {
		trace(TR_BREADHIT, pack(vp, fs->fs_bsize), lbn);
	} else {
		trace(TR_BREADMISS, pack(vp, fs->fs_bsize), lbn);
		current_proc()->p_stats->p_ru.ru_inblock++;	/* pay for read */
#endif
	if (0 == buf_valid(bp)) {
		// cache miss
		buf_setflags(bp, B_READ);
		if (buf_count(bp) > buf_size(bp))
			panic("ext2_indirtrunc: bad buffer size");
		buf_setblkno(bp, (daddr64_t)dbn);
		
		struct vnop_strategy_args vsargs;
		vsargs.a_desc = &vnop_strategy_desc;
		vsargs.a_bp = bp;
		buf_strategy(devvp, &vsargs);
		error = bufwait(bp);
	}
	IXLOCK(ip);
	if (error) {
		buf_brelse(bp);
		*countp = 0;
		buf_brelse(tbp);
		return (error);
	}

	bap = (ext2_daddr_t *)buf_dataptr(bp);
	bcopy((caddr_t)bap, (caddr_t)copy, (u_int)fs->s_blocksize);
	bzero((caddr_t)&bap[last + 1],
	  (u_int)(NINDIR(fs) - (last + 1)) * sizeof (ext2_daddr_t));
	IULOCK(ip);
	if (last == -1)
		buf_markinvalid(bp);
	error = buf_bwrite(bp);
	IXLOCK(ip);
	if (error)
		allerror = error;
	bap = copy;

	/*
	 * Recursively free totally unused blocks.
	 */
	for (i = NINDIR(fs) - 1, nlbn = lbn + 1 - i * factor; i > last;
	    i--, nlbn += factor) {
		nb = le32_to_cpu(bap[i]);
		if (nb == 0)
			continue;
		if (level > SINGLE) {
			if ((error = ext2_indirtrunc(ip, nlbn,
			    fsbtodb(fs, nb), (int32_t)-1, level - 1, &blkcount)) != 0)
				allerror = error;
			blocksreleased += blkcount;
		}
		ext2_blkfree(ip, nb, fs->s_blocksize);
		blocksreleased += nblocks;
	}

	/*
	 * Recursively free last partial block.
	 */
	if (level > SINGLE && lastbn >= 0) {
		last = lastbn % factor;
		nb = le32_to_cpu(bap[i]);
		if (nb != 0) {
			if ((error = ext2_indirtrunc(ip, nlbn, fsbtodb(fs, nb),
			    last, level - 1, &blkcount)) != 0)
				allerror = error;
			blocksreleased += blkcount;
		}
	}
    buf_brelse(tbp);
	*countp = blocksreleased;
	return (allerror);
}

/*
 *	discard preallocated blocks
 */
int
ext2_inactive(ap)
        struct vnop_inactive_args /* {
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	proc_t p = vfs_context_proc(ap->a_context);
	int mode, error = 0;

	if (prtactive && vnode_isinuse(vp, 1) != 0)
		vprint("ext2_inactive: pushing active", vp);

	IXLOCK(ip);
	ext2_discard_prealloc(ip);

	/*
	 * Ignore inodes related to stale file handles.
	 */
	if (ip->i_mode == 0 && vnode_vfsisrdonly(vp) == 0)
		goto out;
	if (ip->i_nlink <= 0) {
		IULOCK(ip);
		(void) vn_write_suspend_wait(vp, NULL, V_WAIT);
		error = ext2_truncate(vp, (off_t)0, 0, NOCRED, p);
		IXLOCK(ip);
		ip->i_rdev = 0;
		mode = ip->i_mode;
		ip->i_mode = 0;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
		IULOCK(ip);
		ext2_vfree(vp, ip->i_number, mode);
		IXLOCK(ip);
	}
	if (ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) {
		int set = (ip->i_flag & (IN_CHANGE | IN_UPDATE | IN_MODIFIED)); 
		IULOCK(ip);
		if (0 == set && vn_write_suspend_wait(vp, NULL, V_NOWAIT)) {
			IXLOCK(ip);
			ip->i_flag &= ~IN_ACCESS;
		} else {
			(void) vn_write_suspend_wait(vp, NULL, V_WAIT);
			ext2_update(vp, 0);
			IXLOCK(ip);
		}
	}
out:
	/*
	 * If we are done with the inode, reclaim it
	 * so that it can be reused immediately.
	 */
	mode = ip->i_mode == 0;
	IULOCK(ip);
	if (mode)
		(void)vnode_recycle(vp);
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ext2_reclaim(ap)
	struct vnop_reclaim_args /* {
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	struct inode *ip;
	vnode_t vp = ap->a_vp;

	if (prtactive && vnode_isinuse(vp, 1) != 0)
		vprint("ext2_reclaim: pushing active", vp);
	ip = VTOI(vp);
	
	IXLOCK(ip);
	if (ip->i_flag & IN_LAZYMOD) {
		ip->i_flag |= IN_MODIFIED;
		IULOCK(ip);
		ext2_update(vp, 0);
		IXLOCK(ip);
	}
	/*
	 * Remove the inode from its hash chain.
	 */
	ext2_ihashrem(ip);
	
	while (ip->i_refct) {
		struct timespec ts;
		ts.tv_sec = 0;
		ts.tv_nsec = 100000000 /* 10ms == 1 sched quantum */;
		(void)ISLEEP(ip, flag, &ts);
	}
	
	assert(0 == (ip->i_flag & (IN_LOOK|IN_LOOKWAIT)));
	
	/*
	 * Purge old data structures associated with the inode.
	 */
	if (ip->i_devvp) {
		vnode_t tvp = ip->i_devvp;
        ip->i_devvp = NULLVP;
		vnode_rele(tvp);
	}
	if (ip->private_data_relse)
		ip->private_data_relse(NULL, ip);
	vnode_clearfsnode(vp);
	vnode_removefsref(vp);
	
	if (ip->i_dhashtbl) {
		__private_extern__ void ext2_dcacheremall(struct inode *);
		ext2_dcacheremall(ip);
		hashdestroy(ip->i_dhashtbl, M_TEMP, ip->i_dhashsz);
	}
	
	IULOCK(ip);
	lck_mtx_free(ip->i_lock, EXT2_LCK_GRP);
	FREE(ip, M_EXT2NODE);
	return (0);
}

static char *e2typename[] =
   { "VNON", "VREG", "VDIR", "VBLK", "VCHR", "VLNK", "VSOCK", "VFIFO", "VBAD" };
void
e2vprint(const char *label, struct vnode *vp)
{
	char sbuf[64];

	if (label != NULL)
		printf("%s: ", label);
	printf("type %s, usecount %d",
	       e2typename[vnode_vtype(vp)], vnode_isinuse(vp, 1));
	sbuf[0] = '\0';
	if (vnode_isvroot(vp))
		strcat(sbuf, "|VROOT");
	if (vnode_issystem(vp))
		strcat(sbuf, "|VSYSTEM");
	if (sbuf[0] != '\0')
		printf(" flags (%s)", &sbuf[1]);
}
