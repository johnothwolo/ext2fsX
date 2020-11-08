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
 *	@(#)ffs_balloc.c	8.4 (Berkeley) 9/23/93
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_balloc.c,v 1.17 2002/05/18 19:12:38 iedowse Exp $
 */

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_balloc.c,v 1.17 2006/08/19 21:33:52 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/lock.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include "ext2_apple.h"

#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>

/*
 * Balloc defines the structure of file system storage
 * by allocating the physical blocks on a device given
 * the inode and the logical block number in a file.
 */
int
ext2_balloc2(struct inode *ip, ext2_daddr_t bn,
	int size, struct ucred *cred, buf_t  *bpp, int flags, int *blk_alloc)
{
	struct m_ext2fs *fs;
	ext2_daddr_t nb;
	buf_t bp, nbp;
	vnode_t vp = ITOV(ip);
	struct indir indirs[NIADDR + 2];
	ext2_daddr_t newb, lbn, *bap, pref;
	int osize, nsize, num, i, error;
    int alloc_buf = 1;

    ext2_trace("inode=%d, lbn=%d, size=%d\n", 
        ip->i_number, (int)bn, size);

	*bpp = NULL;
	if (bn < 0)
		return (EFBIG);
	fs = ip->i_e2fs;
	lbn = bn;
   
    if (flags & B_NOBUFF)
      alloc_buf = 0;
   
    if (blk_alloc)
      *blk_alloc = 0;

	/*
	 * check if this is a sequential block allocation. 
	 * If so, increment next_alloc fields to allow ext2_blkpref 
	 * to make a good guess
	 */
    if (lbn == ip->i_next_alloc_block + 1) {
		ip->i_next_alloc_block++;
		ip->i_next_alloc_goal++;
	}

	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (bn < NDADDR) {
		nb = ip->i_db[bn];
		/* no new block is to be allocated, and no need to expand
		   the file */
		if (nb != 0 && ip->i_size >= (bn + 1) * fs->s_blocksize) {
			if (alloc_buf) {
                IULOCK(ip);
                error = buf_bread(vp, (daddr64_t)bn, fs->s_blocksize, NOCRED, &bp);
                IXLOCK(ip);
                if (error) {
                    buf_brelse(bp);
                    return (error);
                }
                buf_setblkno(bp, (daddr64_t)fsbtodb(fs, nb));
                *bpp = bp;
            } /* allocbuf */
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, ip->i_size));
			nsize = fragroundup(fs, size);
			if (nsize <= osize) {
                if (alloc_buf) {
                    IULOCK(ip);
                    error = buf_bread(vp, (daddr64_t)bn, osize, NOCRED, &bp);
                    IXLOCK(ip);
                    if (error) {
                        buf_brelse(bp);
                        return (error);
                    }
                    buf_setblkno(bp, (daddr64_t)fsbtodb(fs, nb));
                } else { /* alloc_buf */
                    ip->i_flag |= IN_CHANGE | IN_UPDATE;
                    return (0);
                }    
                newb = nb;
			} else {
			/* Godmar thinks: this shouldn't happen w/o fragments */
				ext2_debug("nsize %d(%d) > osize %d(%d) nb %d\n", 
					(int)nsize, (int)size, (int)osize, 
					(int)ip->i_size, (int)nb);
				
				panic("ext2_balloc: Something is terribly wrong");
/*
 * please note there haven't been any changes from here on -
 * FFS seems to work.
 */
			}
		} else { /* (nb != 0) */
			if (ip->i_size < (bn + 1) * fs->s_blocksize)
				nsize = fragroundup(fs, size);
			else
				nsize = fs->s_blocksize;
			error = ext2_alloc(ip, bn,
			    ext2_blkpref(ip, bn, (int)bn, &ip->i_db[0], 0),
			    nsize, cred, &newb);
			if (error)
				return (error);
            if (alloc_buf) {
                IULOCK(ip);
                bp = buf_getblk(vp, (daddr64_t)bn, nsize, 0, 0, BLK_WRITE);
                buf_setblkno(bp, (daddr64_t)fsbtodb(fs, newb));
                IXLOCK(ip);
                if (flags & B_CLRBUF)
                    buf_clear(bp);
            } /* alloc_buf */
		} /* (nb != 0) */
		ip->i_db[bn] = newb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
        if (blk_alloc)
           *blk_alloc = nsize;
        if (alloc_buf)
           *bpp = bp;
		return (0);
	} /* (bn < NDADDR) */
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ext2_getlbns(vp, bn, indirs, &num)) != 0)
		return(error);
#if DIAGNOSTIC
	if (num < 1)
		panic ("ext2_balloc: ext2_getlbns returned indirect block");
#endif
	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
	nb = ip->i_ib[indirs[0].in_off];
	if (nb == 0) {
#if 0
		pref = ext2_blkpref(ip, lbn, 0, (int32_t *)0, 0);
#else
		/* see the comment by ext2_blkpref. What we do here is
		   to pretend that it'd be good for a block holding indirect
		   pointers to be allocated near its predecessor in terms 
		   of indirection, or the last direct block. 
		   We shamelessly exploit the fact that i_ib immediately
		   follows i_db. 
		   Godmar thinks it make sense to allocate i_ib[0] immediately
		   after i_db[11], but it's not utterly clear whether this also
		   applies to i_ib[1] and i_ib[0]
		*/

		pref = ext2_blkpref(ip, lbn, indirs[0].in_off + 
					     EXT2_NDIR_BLOCKS, &ip->i_db[0], 0);
#endif
        if ((error = ext2_alloc(ip, lbn, pref, (int)fs->s_blocksize,
		    cred, &newb)) != 0)
			return (error);
		nb = newb;
        
		IULOCK(ip);
        bp = buf_getblk(vp, (daddr64_t)indirs[1].in_lbn, fs->s_blocksize, 0, 0, BLK_META);
		buf_setblkno(bp, (daddr64_t)fsbtodb(fs, nb));
		buf_clear(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
        error = buf_bwrite(bp);
        IXLOCK(ip);
        
		if (error != 0) {
			ext2_blkfree(ip, nb, fs->s_blocksize);
			return (error);
		}
		ip->i_ib[indirs[0].in_off] = nb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		IULOCK(ip);
        error = buf_meta_bread(vp,
		    (daddr64_t)indirs[i].in_lbn, (int)fs->s_blocksize, NOCRED, &bp);
		IXLOCK(ip);
        if (error) {
			buf_brelse(bp);
			return (error);
		}
		bap = (ext2_daddr_t *)buf_dataptr(bp);
		nb = le32_to_cpu(bap[indirs[i].in_off]);
		if (i == num)
			break;
		i += 1;
		if (nb != 0) {
			buf_brelse(bp);
			continue;
		}
		if (pref == 0) 
#if 1
			/* see the comment above and by ext2_blkpref
			 * I think this implements Linux policy, but
			 * does it really make sense to allocate to
			 * block containing pointers together ?
			 * Also, will it ever succeed ?
			 */
			pref = ext2_blkpref(ip, lbn, indirs[i].in_off, bap,
						/* XXX */(ext2_daddr_t)buf_lblkno(bp));
#else
			pref = ext2_blkpref(ip, lbn, 0, (int32_t *)0, 0);
#endif
		if ((error =
		    ext2_alloc(ip, lbn, pref, (int)fs->s_blocksize, cred, &newb)) != 0) {
			buf_brelse(bp);
			return (error);
		}
		nb = newb;
        
        IULOCK(ip);
		nbp = buf_getblk(vp, (daddr64_t)indirs[i].in_lbn, fs->s_blocksize, 0, 0, BLK_META);
		buf_setblkno(nbp, (daddr64_t)fsbtodb(fs, nb));
		buf_clear(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
        error = buf_bwrite(nbp);
        IXLOCK(ip);
        
        if (error != 0) {
			ext2_blkfree(ip, nb, fs->s_blocksize);
			buf_brelse(bp);
			return (error);
		}
		bap[indirs[i - 1].in_off] = cpu_to_le32(nb);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		IULOCK(ip);
        if (flags & B_SYNC) {
			buf_bwrite(bp);
		} else {
			buf_bdwrite(bp);
		}
        IXLOCK(ip);
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		pref = ext2_blkpref(ip, lbn, indirs[i].in_off, &bap[0], 
				/* XXX */(ext2_daddr_t)buf_lblkno(bp));
		if ((error = ext2_alloc(ip,
		    lbn, pref, (int)fs->s_blocksize, cred, &newb)) != 0) {
			buf_brelse(bp);
			return (error);
		}
		nb = newb;
		bap[indirs[i].in_off] = cpu_to_le32(nb);
		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		IULOCK(ip);
        if (flags & B_SYNC) {
			buf_bwrite(bp);
		} else {
			buf_bdwrite(bp);
		}
        
        if (alloc_buf) {
            nbp = buf_getblk(vp, (daddr64_t)lbn, (int)fs->s_blocksize, 0, 0, BLK_WRITE);
            buf_setblkno(nbp, (daddr64_t)fsbtodb(fs, nb));
            if (flags & B_CLRBUF)
                buf_clear(nbp);
        } /* alloc_buf */
        if (blk_alloc) {
            *blk_alloc = (int)fs->s_blocksize;
        }
        if (alloc_buf)
            *bpp = nbp;
        IXLOCK(ip);
		return (0);
	}
	
    buf_brelse(bp);
    if (alloc_buf) {
        IULOCK(ip);
        if (flags & B_CLRBUF) {
            error = buf_bread(vp, (daddr64_t)lbn, (int)fs->s_blocksize, NOCRED, &nbp);
            if (error) {
                buf_brelse(nbp);
                IXLOCK(ip);
                return (error);
            }
        } else {
            nbp = buf_getblk(vp, (daddr64_t)lbn, (int)fs->s_blocksize, 0, 0, BLK_WRITE);
            buf_setblkno(nbp, (daddr64_t)fsbtodb(fs, nb));
        }
        *bpp = nbp;
        IXLOCK(ip);
    } /* alloc_buf */
    
	return (0);
}
