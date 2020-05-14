/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*-
 * Copyright (c) 1993
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
 *	@(#)ufs_readwrite.c	8.11 (Berkeley) 5/8/95
 */

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_io_apple.c,v 1.22 2006/08/22 00:30:19 bbergstrand Exp $";

#include <sys/param.h>

//#include <sys/resourcevar.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/mount.h>
//#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include "ext2_apple.h"

#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include "ext2_apple.h"

#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct ext2_sb_info
#define	I_FS			i_e2fs

static int ext2_blkalloc(register struct inode *, int32_t, int, struct ucred *, int);

/*
 * Vnode op for pagein.
 * Similar to ext2_read()
 */
/* ARGSUSED */
int
ext2_pagein(struct vnop_pagein_args *ap)
	/* {
	   	vnode_t a_vp,
	   	upl_t 	a_pl,
		vm_offset_t   a_pl_offset,
		off_t         a_f_offset,
		size_t        a_size,
		struct ucred *a_cred,
		int           a_flags
	} */
{
	register vnode_t vp = ap->a_vp;
	upl_t pl = ap->a_pl;
	size_t size= ap->a_size;
	off_t f_offset = ap->a_f_offset;
	vm_offset_t pl_offset = ap->a_pl_offset;
	int flags  = ap->a_flags;
	register struct inode *ip;
	int error;

    ext2_trace_enter();

	ip = VTOI(vp);

#if DIAGNOSTIC
	if (VLNK == vnode_vtype(vp)) {
		if ((int)ip->i_size < vfs_maxsymlen(vnode_mount(vp)))
			panic("ext2_pagein: short symlink");
	} else if (VREG != vnode_vtype(vp) && VDIR != vnode_vtype(vp))
		panic("ext2_pagein: type %d", vnode_vtype(vp));
#endif

  	error = cluster_pagein(vp, pl, pl_offset, f_offset, size,
			    (off_t)ip->i_size, flags);
	/* ip->i_flag |= IN_ACCESS; */
	ext2_trace_return(error);
}

/*
 * Vnode op for pageout.
 * Similar to ext2_write()
 * make sure the buf is not in hash queue when you return
 */
int
ext2_pageout(struct vnop_pageout_args *ap)
	/* {
	   vnode_t a_vp,
	   upl_t        a_pl,
	   vm_offset_t   a_pl_offset,
	   off_t         a_f_offset,
	   size_t        a_size,
	   struct ucred *a_cred,
	   int           a_flags
	} */
{
	register vnode_t vp = ap->a_vp;
	upl_t pl = ap->a_pl;
	size_t size= ap->a_size;
	off_t f_offset = ap->a_f_offset;
	vm_offset_t pl_offset = ap->a_pl_offset;
	int flags  = ap->a_flags;
	register struct inode *ip;
	register FS *fs;
	int error ;
	int devBlockSize;
	size_t xfer_size = 0;
	int local_flags=0;
	off_t local_offset;
	int resid, blkoffset;
	size_t xsize/*, lsize*/;
	ext2_daddr_t lbn;
	int save_error =0, save_size=0;
	vm_offset_t lupl_offset;
	int nocommit = flags & UPL_NOCOMMIT;

    ext2_trace_enter();

	ip = VTOI(vp);
    if (vfs_flags(vnode_mount(vp)) & MNT_RDONLY) {
		if (!nocommit)
  			ubc_upl_abort_range(pl, pl_offset, size, 
				UPL_ABORT_FREE_ON_EMPTY);
		ext2_trace_return(EROFS);
	}
	fs = ip->I_FS;

	ISLOCK(ip);
	typeof(ip->i_size) isize = ip->i_size;
    IULOCK(ip);
    
    if (f_offset < 0 || f_offset >= isize) {
        if (!nocommit)
            ubc_upl_abort_range(pl, pl_offset, size, 
				UPL_ABORT_FREE_ON_EMPTY);
		ext2_trace_return(EINVAL);
	}

	/*
	 * once we enable multi-page pageouts we will
	 * need to make sure we abort any pages in the upl
	 * that we don't issue an I/O for
	 */
	if (f_offset + size > isize)
        xfer_size = isize - f_offset;
	else
        xfer_size = size;

	devBlockSize = fs->s_d_blocksize;

	if (xfer_size & (PAGE_SIZE - 1)) {
        /* if not a multiple of page size
		 * then round up to be a multiple
		 * the physical disk block size
		 */
		xfer_size = (xfer_size + (devBlockSize - 1)) & ~(devBlockSize - 1);
	}

	/*
	 * once the block allocation is moved to ext2_blockmap
	 * we can remove all the size and offset checks above
	 * cluster_pageout does all of this now
	 * we need to continue to do it here so as not to
	 * allocate blocks that aren't going to be used because
	 * of a bogus parameter being passed in
	 */
	local_flags = 0;
	resid = xfer_size;
	local_offset = f_offset;
	for (error = 0; resid > 0;) {
		lbn = lblkno(fs, local_offset);
		blkoffset = blkoff(fs, local_offset);
		xsize = EXT2_BLOCK_SIZE(fs) - blkoffset;
		if (resid < xsize)
			xsize = resid;
		/* Allocate block without reading into a buf */
		error = ext2_blkalloc(ip,
			lbn, blkoffset + xsize, vfs_context_ucred(ap->a_context), 
			local_flags);
		if (error)
			break;
		resid -= xsize;
		local_offset += (off_t)xsize;
	}

	if (error) {
		save_size = resid;
		save_error = error;
		xfer_size -= save_size;
	}

	error = cluster_pageout(vp, pl, pl_offset, f_offset, round_page_32(xfer_size), isize, flags);

	if(save_error) {
		lupl_offset = size - save_size;
		resid = round_page_32(save_size);
		if (!nocommit)
			ubc_upl_abort_range(pl, lupl_offset, resid,
				UPL_ABORT_FREE_ON_EMPTY);
		if(!error)
			error= save_error;
	}
	ext2_trace_return(error);
}

/*
 * ext2_blkalloc allocates a disk block for ext2_pageout(), as a consequence
 * it does no breads (that could lead to deadblock as the page may be already
 * marked busy as it is being paged out. Also it's important to note that we are not
 * growing the file in pageouts. So ip->i_size  cannot increase by this call
 * due to the way UBC works.  
 * This code is derived from ext2_balloc and many cases of that are dealt with
 * in ext2_balloc are not applicable here.
 * Do not call with B_CLRBUF flags as this should only be called only 
 * from pageouts.
 */
static int
ext2_blkalloc(register struct inode *ip, int32_t lbn, int size,
			  struct ucred *cred, int flags)
{
	register FS *fs;
	register int32_t nb;
	buf_t  bp, nbp;
	vnode_t vp = ITOV(ip);
	struct indir indirs[NIADDR + 2];
	int32_t newb, *bap, pref;
	uint32_t deallocated, osize, nsize;
	int num, i, error;
	int32_t *allocib, *blkp, *allocblk, allociblk[NIADDR + 1];

	fs = ip->i_e2fs;
   
    ext2_trace_enter();

	if(size > EXT2_BLOCK_SIZE(fs))
		panic("ext2_blkalloc: too large for allocation\n");

	/*
	 * If the next write will extend the file into a new block,
	 * and the file is currently composed of a fragment
	 * this fragment has to be extended to be a full block.
	 */
    ISLOCK(ip);
	typeof(ip->i_size) isize = ip->i_size;
    IULOCK(ip);
    nb = lblkno(fs, isize);
	if (nb < NDADDR && nb < lbn) {
		panic("ext2_blkalloc(): cannot extend file: i_size %llu, lbn %d\n", isize, lbn);
	}
	/*
	 * The first NDADDR blocks are direct blocks
	 */
	if (lbn < NDADDR) {
		ISLOCK(ip);
        nb = ip->i_db[lbn];
        IULOCK(ip);
		if (nb != 0 && isize >= (lbn + 1) * EXT2_BLOCK_SIZE(fs)) {
            /* TBD: trivial case; the block  is already allocated */
			return (0);
		}
		if (nb != 0) {
			/*
			 * Consider need to reallocate a fragment.
			 */
			osize = fragroundup(fs, blkoff(fs, isize));
			nsize = fragroundup(fs, size);
			if (nsize > osize) {
				panic("ext2_allocblk: Something is terribly wrong\n");
			}
			return(0);
		} else {
			if (isize < (lbn + 1) * EXT2_BLOCK_SIZE(fs))
				nsize = fragroundup(fs, size);
			else
				nsize = EXT2_BLOCK_SIZE(fs);
			
            IXLOCK(ip);
            error = ext2_alloc(ip, lbn,
			    ext2_blkpref(ip, lbn, (int)lbn, &ip->i_db[0], 0),
			    nsize, cred, &newb);
			if (error) {
                IULOCK(ip);
				ext2_trace_return(error);
            }
			ip->i_db[lbn] = newb;
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
            IULOCK(ip);
			return (0);
		}
	}
	/*
	 * Determine the number of levels of indirection.
	 */
	pref = 0;
	if ((error = ext2_getlbns(vp, lbn, indirs, &num)))
		ext2_trace_return(error);

	if(num < 1) {
		panic("ext2_blkalloc: file with direct blocks only\n"); 
	}

	/*
	 * Fetch the first indirect block allocating if necessary.
	 */
	--num;
    ISLOCK(ip);
	nb = ip->i_ib[indirs[0].in_off];
    IULOCK(ip);
	allocib = NULL;
	allocblk = allociblk;
	if (nb == 0) {
        IXLOCK(ip);
#if 0
      pref = ext2_blkpref(ip, lbn, 0, (int32_t *)0, 0);
#else
      pref = ext2_blkpref(ip, lbn, indirs[0].in_off +
         EXT2_NDIR_BLOCKS, &ip->i_db[0], 0);
#endif
        error = ext2_alloc(ip, lbn, pref, (int)EXT2_BLOCK_SIZE(fs), cred, &newb);
        IULOCK(ip);
        if (error)
			ext2_trace_return(error);
		nb = newb;
		*allocblk++ = nb;
		bp = buf_getblk(vp, (daddr64_t)((unsigned)indirs[1].in_lbn), EXT2_BLOCK_SIZE(fs), 0, 0, BLK_META);
		buf_setblkno(bp, (daddr64_t)((unsigned)fsbtodb(fs, nb)));
		clrbuf(bp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = buf_bwrite(bp)))
			goto fail;
        IXLOCK(ip);
		allocib = &ip->i_ib[indirs[0].in_off];
		*allocib = nb;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
        IULOCK(ip);
	}
	/*
	 * Fetch through the indirect blocks, allocating as necessary.
	 */
	for (i = 1;;) {
		error = buf_meta_bread(vp,
		    (daddr64_t)((unsigned)indirs[i].in_lbn), (int)EXT2_BLOCK_SIZE(fs), NOCRED, &bp);
		if (error) {
			buf_brelse(bp);
			goto fail;
		}
        
        IXLOCK(ip);
        bap = (ext2_daddr_t *)buf_dataptr(bp);
		nb = le32_to_cpu(bap[indirs[i].in_off]);

		if (i == num) {
            IULOCK(ip);
			break;
        }
        i += 1;
        if (nb != 0) {
            IULOCK(ip);
			buf_brelse(bp);
			continue;
		}
		if (pref == 0)
#if 0
			pref = ext2_blkpref(ip, lbn, 0, (int32_t *)0, 0);
#else
            pref = ext2_blkpref(ip, lbn, indirs[i].in_off, bap, buf_lblkno(bp));
#endif
		error = ext2_alloc(ip, lbn, pref, (int)EXT2_BLOCK_SIZE(fs), cred, &newb);
        IULOCK(ip);
        if (error) {
            buf_brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;
		nbp = buf_getblk(vp, (daddr64_t)((unsigned)indirs[i].in_lbn), EXT2_BLOCK_SIZE(fs), 0, 0, BLK_META);
		buf_setblkno(nbp, (daddr64_t)((unsigned)fsbtodb(fs, nb)));
		clrbuf(nbp);
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		if ((error = buf_bwrite(nbp))) {
			buf_brelse(bp);
			goto fail;
		}

		IXLOCK(ip);
        bap[indirs[i - 1].in_off] = cpu_to_le32(nb);
        IULOCK(ip);

		/*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			buf_bwrite(bp);
		} else {
			buf_bdwrite(bp);
		}
	}
	/*
	 * Get the data block, allocating if necessary.
	 */
	if (nb == 0) {
		IXLOCK(ip);
        pref = ext2_blkpref(ip, lbn, indirs[i].in_off, &bap[0], buf_lblkno(bp));
		if ((error = ext2_alloc(ip,
		    lbn, pref, (int)EXT2_BLOCK_SIZE(fs), cred, &newb))) {
			IULOCK(ip);
            buf_brelse(bp);
			goto fail;
		}
		nb = newb;
		*allocblk++ = nb;

		bap[indirs[i].in_off] = cpu_to_le32(nb);
		IULOCK(ip);
        
        /*
		 * If required, write synchronously, otherwise use
		 * delayed write.
		 */
		if (flags & B_SYNC) {
			buf_bwrite(bp);
		} else {
			buf_bdwrite(bp);
		}
		return (0);
	}
	buf_brelse(bp);
	return (0);
fail:
	/*
	 * If we have failed part way through block allocation, we
	 * have to deallocate any indirect blocks that we have allocated.
	 */
	IXLOCK(ip);
    for (deallocated = 0, blkp = allociblk; blkp < allocblk; blkp++) {
		ext2_blkfree(ip, *blkp, EXT2_BLOCK_SIZE(fs));
		deallocated += EXT2_BLOCK_SIZE(fs);
	}
	if (allocib != NULL)
		*allocib = 0;
	if (deallocated) {
		ip->i_blocks -= btodb(deallocated, fs->s_d_blocksize);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
	}
    IULOCK(ip);
	ext2_trace_return(error);
}

/*
 * Notfication that a file is being mapped
 */
/* ARGSUSED */
int
ext2_mmap(struct vnop_mmap_args *ap)
	/* {
		vnode_t a_vp;
		int  a_fflags;
		vfs_context_t context;
	} */
{

	return (0);
}

__private_extern__ int
ext2_blktooff (struct vnop_blktooff_args *ap)
{
   
   struct inode *ip;
   struct ext2_sb_info *fs;
   /*typeof(ap->a_lblkno)*/ daddr64_t bn = ap->a_lblkno;
   
   ext2_trace_enter();
   
   if (bn < 0) {
		bn = -bn;
        panic("negative blkno in ext2_blktooff");
	}
   
	ip = VTOI(ap->a_vp);
	fs = ip->i_e2fs;
	*ap->a_offset = (typeof(*ap->a_offset))lblktosize(fs, bn);
   
   return (0);
}

__private_extern__ int
ext2_offtoblk (struct vnop_offtoblk_args *ap)
{
   struct inode *ip;
   struct ext2_sb_info *fs;
   
   ext2_trace_enter();
   
   if (ap->a_vp == NULL)
		ext2_trace_return(EINVAL);
   
   ip = VTOI(ap->a_vp);
   fs = ip->i_e2fs;
   *ap->a_lblkno = (daddr_t)lblkno(fs, ap->a_offset);
   
   return (0);
}

/*
 * blockmap converts the file offset of a file to its physical block
 * number on the disk And returns a contiguous size for transfer.
 */
int
ext2_blockmap(struct vnop_blockmap_args *ap)
	/* {
		vnode_t a_vp;
		off_t a_foffset;    
		size_t a_size;
		daddr_t *a_bpn;
		size_t *a_run;
		void *a_poff;
        int a_flags;
	} */
{
	vnode_t  vp = ap->a_vp;
	daddr64_t *bnp = ap->a_bpn;
	size_t *runp = ap->a_run;
	size_t size = ap->a_size;
	ext2_daddr_t bn, daddr = 0;
	int nblks;
	register struct inode *ip;
	int devBlockSize;
	FS *fs;
	int retsize=0;
	int error;

	ip = VTOI(vp);
	fs = ip->i_e2fs;

    if ((error = blkoff(fs, ap->a_foffset))) {
		panic("ext2_blockmap: allocation requested inside a block (possible filesystem corruption): "
         "qbmask=%qd, inode=%u, offset=%qd, blkoff=%d",
         fs->s_qbmask, ip->i_number, ap->a_foffset, error);
	}
    error = 0;

	bn = (ext2_daddr_t)lblkno(fs, ap->a_foffset);
    devBlockSize = fs->s_d_blocksize;

	//ext2_trace("inode=%u, lbn=%d, off=%qu, size=%u\n", ip->i_number, bn, ap->a_foffset, size);
    
    if (size % devBlockSize) {
		panic("ext2_blockmap: size is not multiple of device block size\n");
	}

//cmap_bmap:
    if ((error = ext2_bmaparray(vp, bn, &daddr, &nblks, NULL))) {
        ext2_trace_return(error);
    }
    
	if (bnp)
		*bnp = (daddr64_t)daddr;

	if (ap->a_poff) 
		*(int *)ap->a_poff = 0;

#ifdef cluster_io_bug_fix
    if (daddr == -1) {
        /* XXX - BDB - Workaround for bug #965119
           There is a bug in cluster_io() in all pre 10.4.x kernels that causes nasty problems 
           when there is a hole in the inode that is not aligned on a page boundry.
           To fix this, we allocate a new block. No more sparse hole, but also no
           more panics, or system lock-ups.
         */
        if (EXT2_BLOCK_SIZE(fs) < PAGE_SIZE) { 
            IXLOCK(ip);
            error = ext2_blkalloc(ip, bn, EXT2_BLOCK_SIZE(fs), curproc->p_ucred, 0);
            IULOCK(ip);
            if (error) {
                ext2_trace_return(error);
            }
            goto cmap_bmap;
        }
        
        if (size < EXT2_BLOCK_SIZE(fs)) {
			retsize = fragroundup(fs, size);
			if(size >= retsize)
				*runp = retsize;
			else
				*runp = size;
		} else {
			*runp = EXT2_BLOCK_SIZE(fs);
		}
		return(0);
	}
#endif

	if (runp) {
		ISLOCK(ip);
        if (bn < 0) {
            // indirect block
            retsize = (nblks + 1) * EXT2_BLOCK_SIZE(fs);
        } else if (-1 == daddr || 0 == nblks) {
            // sparse hole or no contiguous blocks
            retsize = blksize(fs, ip, lbn);
        } else {
            // 1 or more contiguous blocks
            retsize = nblks * EXT2_BLOCK_SIZE(fs);
            retsize += blksize(fs, ip, (bn + nblks));
        }
        IULOCK(ip);
        if (retsize < size)
            *runp = retsize;
        else
            *runp = size;
	}
	return (0);
}
