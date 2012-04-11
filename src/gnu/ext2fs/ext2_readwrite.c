/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
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
 *	@(#)ufs_readwrite.c	8.7 (Berkeley) 1/21/94
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_readwrite.c,v 1.25 2002/05/16 19:43:28 iedowse Exp $
 */
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

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_readwrite.c,v 1.27 2006/08/22 00:30:19 bbergstrand Exp $";

#define	BLKSIZE(a, b, c)	blksize(a, b, c)
#define	FS			struct ext2_sb_info
#define	I_FS			i_e2fs
#define	READ			ext2_read
#define	READ_S			"ext2_read"
#define	WRITE			ext2_write
#define	WRITE_S			"ext2_write"

/*
 * Vnode op for reading.
 */
/* ARGSUSED */
__private_extern__ int
READ(ap)
	struct vnop_read_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	vnode_t vp;
	struct inode *ip;
	struct uio *uio;
	FS *fs;
	buf_t  bp;
	ext2_daddr_t lbn, nextlbn;
    daddr64_t lbn64;
	off_t bytesinfile;
	long size, xfersize, blkoffset;
	int error, orig_resid;
	u_short mode;
    typeof(ip->i_lastr) ilastr;
   
    ext2_trace_enter();

	vp = ap->a_vp;
	ip = VTOI(vp);
    ISLOCK(ip);
	mode = ip->i_mode;
    typeof(ip->i_size) isize = ip->i_size;
    fs = ip->I_FS;
    IULOCK(ip);
	uio = ap->a_uio;

#ifdef DIAGNOSTIC
	if (uio_rw(uio) != UIO_READ)
		panic("%s: invalid uio_rw = %x", READ_S, uio_rw(uio));

	if (vnode_vtype(vp) == VLNK) {
		if ((int)isize < vfs_maxsymlen(vnode_mount(vp)))
			panic("%s: short symlink", READ_S);
	} else if (vnode_vtype(vp) != VREG && vnode_vtype(vp) != VDIR)
		panic("%s: invalid v_type %d", READ_S, vnode_vtype(vp));
#endif
    if (uio_offset(uio)  < 0)
		ext2_trace_return (EINVAL);

	if ((u_quad_t)uio_offset(uio) > fs->s_maxfilesize)
		ext2_trace_return (EFBIG);

	orig_resid = uio_resid(uio);
    if (UBCISVALID(vp)) {
		bp = NULL; /* So we don't try to free it later. */
        error = cluster_read(vp, uio, (off_t)isize, 0);
	} else {
	for (error = 0, bp = NULL; uio_resid(uio) > 0; bp = NULL) {
		if ((bytesinfile = isize - uio_offset(uio)) <= 0)
			break;
		lbn = lblkno(fs, uio_offset(uio));
		nextlbn = lbn + 1;
        // XXX If BLKSZIE actually used the inode we would need the inode lock
		size = BLKSIZE(fs, ip, lbn);
		blkoffset = blkoff(fs, uio_offset(uio));

		xfersize = EXT2_BLOCK_SIZE(fs) - blkoffset;
		if (uio_resid(uio) < xfersize)
			xfersize = uio_resid(uio);
		if (bytesinfile < xfersize)
			xfersize = bytesinfile;

        ISLOCK(ip);
        ilastr = ip->i_lastr;
        IULOCK(ip);
		
        if (lblktosize(fs, nextlbn) >= isize)
			error = buf_bread(vp, lbn, size, NOCRED, &bp);
		else if (lbn - 1 == ilastr && !vnode_isnoreadahead(vp)) {
			// XXX If BLKSZIE actually used the inode we would need the inode lock
            int nextsize = BLKSIZE(fs, ip, nextlbn);
			error = buf_breadn(vp, (daddr64_t)lbn,
			    size, &lbn64, &nextsize, 1, NOCRED, &bp);
            nextlbn = (ext2_daddr_t)lbn64;
		} else
			error = buf_bread(vp, lbn, size, NOCRED, &bp);
		if (error) {
			buf_brelse(bp);
			bp = NULL;
			break;
		}
        IXLOCK(ip);
        ip->i_lastr = lbn;

		/*
		 * We should only get non-zero b_resid when an I/O error
		 * has occurred, which should cause us to break above.
		 * However, if the short read did not cause an error,
		 * then we want to ensure that we do not uiomove bad
		 * or uninitialized data.
		 */
		size -= buf_resid(bp);
		if (size < xfersize) {
			if (size == 0) {
                IULOCK(ip);
				break;
            }
			xfersize = size;
		}
        
        caddr_t data = (caddr_t)(((char *)buf_dataptr(bp)) + blkoffset);
		error = uiomove(data, (int)xfersize, uio);
        
        isize = ip->i_size;
        IULOCK(ip);
        
        if (error)
			break;
      
        if (S_ISREG(mode) && (xfersize + blkoffset == EXT2_BLOCK_SIZE(fs) ||
		    uio_offset(uio) == isize)) {
			buf_markaged(bp);
        }

		bqrelse(bp);
	}
   } /* if (UBCISVALID(vp)) */
	if (bp != NULL)
		bqrelse(bp);
	if (orig_resid > 0 && (error == 0 || uio_resid(uio) != orig_resid) &&
	    (vfs_flags(vnode_mount(vp)) & MNT_NOATIME) == 0) {
		IXLOCK(ip);
        ip->i_flag |= IN_ACCESS;
        IULOCK(ip);
    }
	ext2_trace_return(error);
}

/*
 * Vnode op for writing.
 */
__private_extern__ int
WRITE(ap)
	struct vnop_write_args /* {
		vnode_t a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	vnode_t vp;
	uio_t uio;
	struct inode *ip;
	FS *fs;
	buf_t  bp;
	ext2_daddr_t lbn;
	off_t osize;
	int blkoffset, error, flags, ioflag, resid, size, xfersize;
    int rsd, blkalloc=0, save_error=0, save_size=0;
    int file_extended = 0, dodir=0;
    vfs_context_t context = ap->a_context;
    typeof(ip->i_size) isize;

	ioflag = ap->a_ioflag;
	uio = ap->a_uio;
	vp = ap->a_vp;
	ip = VTOI(vp);

#ifdef DIAGNOSTIC
	if (uio_rw(uio) != UIO_WRITE)
		panic("%s: uio_rw = %x\n", WRITE_S, uio_rw(uio));
#endif

	switch (vnode_vtype(vp)) {
	case VREG:
		ISLOCK(ip);
        if (ioflag & IO_APPEND)
			uio_setoffset(uio, ip->i_size);
		if ((ip->i_flags & APPEND) && uio_offset(uio) != ip->i_size) {
			IULOCK(ip);
            ext2_trace_return(EPERM);
        }
		/* FALLTHROUGH */
        IULOCK(ip);
	case VLNK:
		break;
	case VDIR:
		dodir = 1;
        if ((ioflag & IO_SYNC) == 0)
			panic("%s: nonsync dir write", WRITE_S);
		break;
	default:
		panic("%s: type", WRITE_S);
	}

	fs = ip->I_FS;

	ext2_trace(" vn=%p,inode=%d,isize=%qu,woff=%qu,wsize=%d\n",
        vp, ip->i_number, ip->i_size, uio_offset(uio), uio_resid(uio));
    
    if (uio_offset(uio) < 0 ||
	    ((u_quad_t)(uio_offset(uio) + uio_resid(uio))) > fs->s_maxfilesize)
		ext2_trace_return(EFBIG);
    if (uio_resid(uio) == 0)
        return (0);

	resid = uio_resid(uio);
    IXLOCK(ip);
	osize = ip->i_size;
	flags = ioflag & IO_SYNC ? B_SYNC : 0;
   
   if (UBCISVALID(vp)) {
      off_t filesize, endofwrite, local_offset, head_offset;
      int local_flags, first_block, fboff, fblk, loopcount;
   
      endofwrite = uio_offset(uio) + uio_resid(uio);
   
      if (endofwrite > ip->i_size) {
         filesize = endofwrite;
         file_extended = 1;
      } else 
         filesize = ip->i_size;
   
      head_offset = ip->i_size;
   
      /* Go ahead and allocate the blocks that are going to be written */
      rsd = uio_resid(uio);
      local_offset = uio_offset(uio);
      local_flags = ioflag & IO_SYNC ? B_SYNC : 0;
      local_flags |= B_NOBUFF;

      first_block = 1;
      fboff = 0;
      fblk = 0;
      loopcount = 0;
      /* There's no need for this, since we can't get here with rsd == 0, but the compiler
        is not smart enough to figure this out and warns of possible badness. */
      xfersize = 0;
   
      for (error = 0; rsd > 0;) {
         blkalloc = 0;
         lbn = lblkno(fs, local_offset);
         blkoffset = blkoff(fs, local_offset);
         xfersize = EXT2_BLOCK_SIZE(fs) - blkoffset;
         if (first_block)
            fboff = blkoffset;
         if (rsd < xfersize)
            xfersize = rsd;
         if (EXT2_BLOCK_SIZE(fs) > xfersize)
            local_flags |= B_CLRBUF;
         else
            local_flags &= ~B_CLRBUF;
         
         /* Allocate block without reading into a buf (B_NOBUFF) */
         error = ext2_balloc2(ip,
            lbn, blkoffset + xfersize, vfs_context_ucred(context), 
            &bp, local_flags, &blkalloc);
         assert(NULL == bp);
         if (error)
            break;
         if (first_block) {
            fblk = blkalloc;
            first_block = 0;
         }
         loopcount++;
   
         rsd -= xfersize;
         local_offset += (off_t)xfersize;
         if (local_offset > ip->i_size)
            ip->i_size = local_offset;
      }
   
      if(error) {
         save_error = error;
         save_size = rsd;
         uio_setresid(uio, uio_resid(uio) - rsd);
         if (file_extended)
            filesize -= rsd;
      }
   
      flags = ioflag & IO_SYNC ? IO_SYNC : 0;
      /* UFS does not set this flag, but if we don't then 
         previously written data is zero filled and the file is corrupt.
         Perhaps a difference in balloc()?
      */
      flags |= IO_NOZEROVALID;
   
      if((error == 0) && fblk && fboff) {
         if(fblk > EXT2_BLOCK_SIZE(fs)) 
            panic("ext2_write : ext2_balloc allocated more than bsize(head)");
         /* We need to zero out the head */
         head_offset = uio_offset(uio) - (off_t)fboff ;
         flags |= IO_HEADZEROFILL;
         flags &= ~IO_NOZEROVALID;
      }
   
      if((error == 0) && blkalloc && ((blkalloc - xfersize) > 0)) {
         /* We need to zero out the tail */
         if( blkalloc > EXT2_BLOCK_SIZE(fs)) 
            panic("ext2_write : ext2_balloc allocated more than bsize(tail)");
         local_offset += (blkalloc - xfersize);
         if (loopcount == 1) {
            /* blkalloc is same as fblk; so no need to check again*/
            local_offset -= fboff;
         }
         flags |= IO_TAILZEROFILL;
         /*  Freshly allocated block; bzero even if find a page */
         flags &= ~IO_NOZEROVALID;
      }
      /*
      * if the write starts beyond the current EOF then
      * we we'll zero fill from the current EOF to where the write begins
      */
      
      IULOCK(ip);
      
      error = cluster_write(vp, uio, osize, filesize, head_offset, local_offset, flags);
      
      if (uio_offset(uio) > osize) {
         if (error && 0 == (ioflag & IO_UNIT))
            (void)ext2_truncate(vp, uio_offset(uio),
               ioflag & IO_SYNC, vfs_context_ucred(context), vfs_context_proc(context));
         
         IXLOCK(ip);
         isize = ip->i_size = uio_offset(uio);
         IULOCK(ip);
         
         ubc_setsize(vp, (off_t)isize);
      }
      if(save_error) {
         uio_setresid(uio, uio_resid(uio) + save_size);
         if(!error)
            error = save_error;	
      }
      
      IXLOCK(ip);
      
      ip->i_flag |= IN_CHANGE | IN_UPDATE;
   } else {
	for (error = 0; uio_resid(uio) > 0;) {
		lbn = lblkno(fs, uio_offset(uio));
		blkoffset = blkoff(fs, uio_offset(uio));
		xfersize = EXT2_BLOCK_SIZE(fs) - blkoffset;
		if (uio_resid(uio) < xfersize)
			xfersize = uio_resid(uio);

		if (EXT2_BLOCK_SIZE(fs) > xfersize)
			flags |= B_CLRBUF;
		else
			flags &= ~B_CLRBUF;

		error = ext2_balloc(ip,
		    lbn, blkoffset + xfersize, vfs_context_ucred(context), &bp, flags);
		if (error)
			break;

		if (uio_offset(uio) + xfersize > ip->i_size) {
			isize = ip->i_size = uio_offset(uio) + xfersize;
            if (UBCISVALID(vp)) {
                IULOCK(ip);
                ubc_setsize(vp, (off_t)isize); /* XXX check errors */
                IXLOCK(ip);
            }
		}

		size = BLKSIZE(fs, ip, lbn) - buf_resid(bp);
		if (size < xfersize)
			xfersize = size;
        
        if (xfersize > 0) {
            caddr_t data = (caddr_t)(((char *)buf_dataptr(bp)) + blkoffset);
            error = uiomove(data, (int)xfersize, uio);
        }
        if (error || xfersize == 0) {
            buf_brelse(bp);
			break;
        }
        
        ip->i_flag |= IN_CHANGE | IN_UPDATE;
        IULOCK(ip);

		if (0 == dodir && (ioflag & IO_SYNC)) {
            (void)buf_bwrite(bp);
		} else if (xfersize + blkoffset == EXT2_BLOCK_SIZE(fs)) {
            buf_markaged(bp);
            buf_bawrite(bp);
		} else {
			buf_bdwrite(bp);
		}
        
        IXLOCK(ip);
	}
   } /* if (UBCISVALID(vp)) */
   
    /*
	 * If we successfully wrote any data, and we are not the superuser
	 * we clear the setuid and setgid bits as a precaution against
	 * tampering.
	 */
    if (resid > uio_resid(uio) && !vfs_context_suser(context))
		ip->i_mode &= ~(ISUID | ISGID);
    if (resid > uio_resid(uio))
       VN_KNOTE(vp, NOTE_WRITE | (file_extended ? NOTE_EXTEND : 0));
    
    IULOCK(ip);
    
	if (error) {
		if (ioflag & IO_UNIT) {
			(void)ext2_truncate(vp, osize,
			    ioflag & IO_SYNC, vfs_context_ucred(context), vfs_context_proc(context));
			uio_setoffset(uio, uio_offset(uio) - (resid - uio_resid(uio)));
			uio_setresid(uio, resid);
		}
	} else if (resid > uio_resid(uio) && (ioflag & IO_SYNC))
		error = ext2_update(vp, 1);
      
    ext2_trace_return(error);
}
