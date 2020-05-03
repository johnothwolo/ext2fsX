/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ufs_lookup.c	8.6 (Berkeley) 4/1/94
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_lookup.c,v 1.37 2002/10/18 21:41:41 bde Exp $
 */

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_lookup.c,v 1.36 2006/10/14 19:37:32 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/sysctl.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <libkern/OSByteOrder.h>

/* From kernel */
extern struct nchstats nchstats;

#include "ext2_apple.h"

#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/ext2_dir.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <ext2_byteorder.h>

#ifdef DIAGNOSTIC
__private_extern__ int dirchk = 1;
__private_extern__ u_int32_t lookcachehit = 0, lookwait = 0;
#define lookchit() OSIncrementAtomic((SInt32*)&lookcachehit)
#else
__private_extern__ int dirchk = 0;
#define lookchit()
#endif

__private_extern__ u_int32_t lookcacheinval = 0;

SYSCTL_NODE(_vfs, OID_AUTO, e2fs, CTLFLAG_RD, 0, "EXT2FS filesystem");
SYSCTL_INT(_vfs_e2fs, EXT2_SYSCTL_INT_DIRCHECK, dircheck, CTLFLAG_RW, &dirchk, 0, "");
SYSCTL_UINT(_vfs_e2fs, EXT2_SYSCTL_INT_LOOKCACHEINVAL, lookcacheinval, CTLFLAG_RD, &lookcacheinval, 0, "");

/* 
   DIRBLKSIZE in ffs is DEV_BSIZE (in most cases 512)
   while it is the native blocksize in ext2fs - thus, a #define
   is no longer appropriate
*/
#undef  DIRBLKSIZ

static const u_char ext2_ft_to_dt[] = {
	DT_UNKNOWN,		/* EXT2_FT_UNKNOWN */
	DT_REG,			/* EXT2_FT_REG_FILE */
	DT_DIR,			/* EXT2_FT_DIR */
	DT_CHR,			/* EXT2_FT_CHRDEV */
	DT_BLK,			/* EXT2_FT_BLKDEV */
	DT_FIFO,		/* EXT2_FT_FIFO */
	DT_SOCK,		/* EXT2_FT_SOCK */
	DT_LNK,			/* EXT2_FT_SYMLINK */
};
#define	FTTODT(ft)						\
    ((ft) > sizeof(ext2_ft_to_dt) / sizeof(ext2_ft_to_dt[0]) ?	\
    DT_UNKNOWN : ext2_ft_to_dt[(ft)])

static const u_char dt_to_ext2_ft[] = {
	EXT2_FT_UNKNOWN,	/* DT_UNKNOWN */
	EXT2_FT_FIFO,		/* DT_FIFO */
	EXT2_FT_CHRDEV,		/* DT_CHR */
	EXT2_FT_UNKNOWN,	/* unused */
	EXT2_FT_DIR,		/* DT_DIR */
	EXT2_FT_UNKNOWN,	/* unused */
	EXT2_FT_BLKDEV,		/* DT_BLK */
	EXT2_FT_UNKNOWN,	/* unused */
	EXT2_FT_REG_FILE,	/* DT_REG */
	EXT2_FT_UNKNOWN,	/* unused */
	EXT2_FT_SYMLINK,	/* DT_LNK */
	EXT2_FT_UNKNOWN,	/* unused */
	EXT2_FT_SOCK,		/* DT_SOCK */
	EXT2_FT_UNKNOWN,	/* unused */
	EXT2_FT_UNKNOWN,	/* DT_WHT */
};
#define	DTTOFT(dt)						\
    ((dt) > sizeof(dt_to_ext2_ft) / sizeof(dt_to_ext2_ft[0]) ?	\
    EXT2_FT_UNKNOWN : dt_to_ext2_ft[(dt)])

static int	ext2_dirbadentry_locked(vnode_t dp, struct ext2_dir_entry_2 *de,
		    int entryoffsetinblock);

/* Bring in dir index support. */
#include "kern/linux_namei_compat.c"
#include "linux/fs/ext3/namei.c"
#include "linux/fs/ext3/dir.c"

// Dir lookup cache utils
#define DCACHE_HASH(dp, cnp) (&dp->i_dhashtbl[cnp->cn_hash & E2DCACHE_MASK])
static struct dcache* ext2_dcacheget(const struct inode *dp, const struct componentname *cnp)
{
    struct dcache *dcp;
    LIST_FOREACH(dcp, DCACHE_HASH(dp, cnp), d_hash) {
        if (dcp->d_namelen == cnp->cn_namelen && 0 == strncmp(dcp->d_name, cnp->cn_nameptr, dcp->d_namelen))
            break;
    }
    return (dcp);
} 

static inline
void ext2_dcacheins(struct inode *dp, struct dcache *dcp, const struct componentname *cnp)
{
    struct idashhead *head = DCACHE_HASH(dp, cnp);
    LIST_INSERT_HEAD(head, dcp, d_hash);
    #ifdef DIAGNOSTIC
    dp->i_dhashct++;
    #endif
}

static void ext2_dcacherem(struct dcache *dcp, struct inode *dp E2_DGB_ONLY)
{
    dcp->d_refct--;
    if (dcp->d_refct <= 0) {
        LIST_REMOVE(dcp, d_hash);
        FREE(dcp, M_TEMP);
        #ifdef DIAGNOSTIC
        dp->i_dhashct--;
        #endif
    }
}

static int
ext2_is_dot_entry(struct componentname *cnp)
{
	if (cnp->cn_namelen <= 2 && cnp->cn_nameptr[0] == '.' &&
	    (cnp->cn_nameptr[1] == '.' || cnp->cn_nameptr[1] == '\0'))
		return (1);
	return (0);
}

__private_extern__
void ext2_dcacheremall(struct inode *dp)
{
    int i;
    for (i = 0; i < E2DCACHE_BUCKETS; ++i) {
        struct idashhead *head = &dp->i_dhashtbl[i];
        struct dcache *dcp;
        LIST_FOREACH(dcp, head, d_hash) {
            ext2_dcacherem(dcp, dp);
        }
    }
}

/*
 * Vnode op for reading directories.
 *
 * The routine below assumes that the on-disk format of a directory
 * is the same as that defined by <sys/dirent.h>. If the on-disk
 * format changes, then it will be necessary to do a conversion
 * from the on-disk format that read returns to the format defined
 * by <sys/dirent.h>.
 */
/*
 * this is exactly what we do here - the problem is that the conversion
 * will blow up some entries by four bytes, so it can't be done in place.
 * This is too bad. Right now the conversion is done entry by entry, the
 * converted entry is sent via uiomove. 
 *
 * XXX allocate a buffer, convert as many entries as possible, then send
 * the whole buffer to uiomove
 */
#define a_ncookies a_numdirent
int
ext2_readdir(
   struct vnop_readdir_args /* {
            vnode_t a_vp;
            struct uio *a_uio;
            int a_flags;
            int *a_eofflag;
            int *a_numdirent;
            vfs_context_t a_context;
   } */ *ap)
{
    vfs_context_t context = ap->a_context;
    uio_t uio = ap->a_uio, auio = NULL;
    int count, error;
    struct ext2_dir_entry_2 *edp, *dp;
    struct inode *ip;
    int ncookies;
    struct dirent dstdp;
    caddr_t dirbuf = 0;
    int DIRBLKSIZ;
    int readcnt, free_dirbuf = 1;
    off_t startoffset;
    user_ssize_t startresid;
    int *eof = ap->a_eofflag;

    // XXX need to add support for this
    if ((ap->a_flags & VNODE_READDIR_EXTENDED) || (ap->a_flags & VNODE_READDIR_REQSEEKOFF))
        ext2_trace_return(ENOTSUP);
    
    ip = VTOI(ap->a_vp);
    DIRBLKSIZ = ip->i_e2fs->s_blocksize;

    count = uio_resid(uio);
    startoffset = uio_offset(uio);
    startresid = uio_resid(uio);
    /*
     * Avoid complications for partial directory entries by adjusting
     * the i/o to end at a block boundary.  Don't give up (like ufs
     * does) if the initial adjustment gives a negative count, since
     * many callers don't supply a large enough buffer.  The correct
     * size is a little larger than DIRBLKSIZ to allow for expansion
     * of directory entries, but some callers just use 512.
     */
    count -= (startoffset + count) & (DIRBLKSIZ -1);
    if (count <= 0)
		count += DIRBLKSIZ;

#if defined(EXT2FS_DEBUG) && EXT2FS_DEBUG > 1
	ext2_debug("ext2_readdir: uio_offset = %lld, uio_resid = %d, count = %d\n", 
	    uio->uio_offset, uio->uio_resid, count);
#endif
   
   if (eof)
      *eof = 0;
   
   IXLOCK(ip);
   
   if (startoffset >= ip->i_size) {
      IULOCK(ip);
      if (eof)
        *eof = 1;
      return (0);
   }
   
    while (ip->i_flag & IN_LOOK) {
        ip->i_flag |= IN_LOOKWAIT;
        lookw();
        ISLEEP(ip, flag, NULL);
    }
    int havelook = 1;
    ip->i_flag |= IN_LOOK;
   
   /* Check for an indexed dir */
   if (EXT3_HAS_COMPAT_FEATURE(ip->i_e2fs, EXT3_FEATURE_COMPAT_DIR_INDEX) &&
      ((ip->i_e2flags & EXT3_INDEX_FL) /*||
      ((ip->i_size >> ip->i_e2fs->s_blocksize_bits) == 1)*/)) {
      /* -- BDB --
         The last condition allows ext3_dx_readdir() to build an
         in memory index of a non-indexed, single-block directory.
         This is disabled for now, because I'm not sure how to detect
         that this optimization has occured (EXT3_INDEX_FL is not set),
         or that it would even work. */
      
      struct filldir_args fda = {0, 0, 0};
      
      /* Make sure the caller is not trying to read more than
         they are allowed. */
      if (uio_offset(uio) && EXT3_HTREE_EOF == ip->f_pos) {
         ip->i_flag &= ~IN_LOOK;
         IWAKE(ip, flag, flag, IN_LOOKWAIT);
         IULOCK(ip);
         return (EIO);
      }
      
      /*ip->i_dir_start_lookup = lblkno(ip->i_e2fs, uio->uio_offset);*/
      if (0 == uio_isuserspace(uio)) {
         /* Setup dirbuf so the cookie calc works */
         user_addr_t iov = uio_curriovbase(uio);
         if (iov)
            dirbuf = (caddr_t)((uintptr_t)iov);
         else {
            error = EINVAL;
            goto io_done;
         }
      } else
         dirbuf = 0;
      free_dirbuf = 0;
      
      fda.uio = uio;
      // XXX ext3_dx* is not lock safe ATM, so don't unlock
      error = ext3_dx_readdir(ap->a_vp, &fda, bsd_filldir);
      if (error != ERR_BAD_DX_DIR) {
         if (EXT2_FILLDIR_ENOSPC == error)
            error = 0;
         error = -error; /* Linux uses -ve codes */
         /* we need to correct uio_offset */
         uio_setoffset(uio, startoffset + fda.count);
         ncookies = fda.cookies;
         
         /* Set EOF if needed */
         if (eof && EXT3_HTREE_EOF == ip->f_pos)
            *eof = 1;
         goto io_done;
      }
      
      /*
      * We don't set the inode dirty flag since it's not
      * critical that it get flushed back to the disk.
      */
      ip->i_e2flags &= ~EXT3_INDEX_FL;
      /* Fall back to normal dir entries. */
      ip->i_flag &= ~IN_LOOK;
      havelook = 0;
      IWAKE(ip, flag, flag, IN_LOOKWAIT);
   }
   
    // Verify we didn't get some partial dx entries
    if (startoffset != uio_offset(uio) || startresid != uio_resid(uio)) {
        error = EINVAL;
        goto io_done;
    }

	IULOCK(ip);
    auio = uio_create(1, startoffset, UIO_SYSSPACE32, UIO_READ);
    if (!auio) {
        error = ENOMEM;
        IXLOCK(ip);
        goto io_done;
    }
	MALLOC(dirbuf, caddr_t, count, M_TEMP, M_WAITOK);
    if ((error = uio_addiov(auio, CAST_USER_ADDR_T(dirbuf), (user_size_t)count))) {
        IXLOCK(ip);
        goto io_done;
    }
   
	struct vnop_read_args ra;
    ra.a_desc =  &vnop_read_desc;
    ra.a_vp = ap->a_vp;
    ra.a_uio = auio;
    ra.a_ioflag = 0;
    ra.a_context = context;
    error = ext2_read(&ra);
    IXLOCK(ip);
    
	if (error == 0) {
		readcnt = count - uio_resid(auio);
		edp = (struct ext2_dir_entry_2 *)&dirbuf[readcnt];
		ncookies = 0;
		bzero(&dstdp, offsetof(struct dirent, d_name));
		for (dp = (struct ext2_dir_entry_2 *)dirbuf; 
		    !error && uio_resid(uio) > 0 && dp < edp; ) {
			/*-
			 * "New" ext2fs directory entries differ in 3 ways
			 * from ufs on-disk ones:
			 * - the name is not necessarily NUL-terminated.
			 * - the file type field always exists and always
			 * follows the name length field.
			 * - the file type is encoded in a different way.
			 *
			 * "Old" ext2fs directory entries need no special
			 * conversions, since they are binary compatible with
			 * "new" entries having a file type of 0 (i.e.,
			 * EXT2_FT_UNKNOWN).  Splitting the old name length
			 * field didn't make a mess like it did in ufs,
			 * because ext2fs uses a machine-independent disk
			 * layout.
			 */
			dstdp.d_fileno = le32_to_cpu(dp->inode);
			dstdp.d_type = FTTODT(dp->file_type);
			dstdp.d_namlen = dp->name_len;
			dstdp.d_reclen = GENERIC_DIRSIZ(&dstdp);
			bcopy(dp->name, dstdp.d_name, dstdp.d_namlen);
			bzero(dstdp.d_name + dstdp.d_namlen,
			    dstdp.d_reclen - offsetof(struct dirent, d_name) -
			    dstdp.d_namlen);

			if (dp->rec_len > 0) {
				if(dstdp.d_reclen <= uio_resid(uio)) {
					/* advance dp */
					dp = (struct ext2_dir_entry_2 *)
					    ((char *)dp + le16_to_cpu(dp->rec_len)); 
					error = uiomove((caddr_t)&dstdp, dstdp.d_reclen, uio);
					if (!error)
						ncookies++;
				} else
					break;
			} else {
				error = EIO;
				break;
			}
		}
		/* we need to correct uio_offset */
        uio_setoffset(uio, startoffset + (caddr_t)dp - dirbuf);

io_done:
; // How the hell are cookies done with KPI?
#if 0
		if (!error && ap->a_ncookies != NULL) {
			u_long *cookiep, *cookies, *ecookies;
			off_t off;

			if (uio_isuserspace(uio) || uio_iovcnt(uio) != 1)
				panic("ext2_readdir: unexpected uio from NFS server");
			MALLOC(cookies, u_long *, ncookies * sizeof(u_long), M_TEMP,
			       M_WAITOK);
			off = startoffset;
			for (dp = (struct ext2_dir_entry_2 *)dirbuf,
			     cookiep = cookies, ecookies = cookies + ncookies;
			     cookiep < ecookies;
			     dp = (struct ext2_dir_entry_2 *)((caddr_t) dp + le16_to_cpu(dp->rec_len))) {
				off += le16_to_cpu(dp->rec_len);
				*cookiep++ = (u_long) off;
			}
			*ap->a_ncookies = ncookies;
			*ap->a_cookies = cookies;
		}
#endif
	}
    
    typeof(ip->i_size) isize = ip->i_size;
    if (havelook) {
        ip->i_flag &= ~IN_LOOK;
        IWAKE(ip, flag, flag, IN_LOOKWAIT);
    }
    IULOCK(ip);
    
    if (free_dirbuf && dirbuf)
      FREE(dirbuf, M_TEMP);
	if (eof && 0 == *eof)
		*eof = isize <= uio_offset(uio);
    if (auio)
        uio_free(auio);
   ext2_trace_return(error);
}

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The cnp->cn_nameiop argument is LOOKUP, CREATE, RENAME, or DELETE depending
 * on whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and vput
 * instead of two vputs.
 *
 * Overall outline of ext2_lookup:
 *
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 */
static
int ext2_lookupi(vnode_t vdp, vnode_t *vpp, struct componentname *cnp, vfs_context_t context)
{
    evalloc_args_t vallocargs = {0}; 
	struct inode *dp;		/* inode for directory being searched */
	buf_t  bp;			/* a buffer of directory entries */
	struct ext2_dir_entry_2 *ep;	/* the current directory entry */
	doff_t entryoffsetinblock;		/* offset of ep in bp's buffer */
	enum {NONE, COMPACT, FOUND}; //slotstatus;
	doff_t i_diroff;		/* cached i_diroff value */
//	doff_t i_offset;		/* cached i_offset value */
	struct ext2fs_searchslot ss;
	
//	doff_t slotoffset;		/* offset of area with free space */
//	int slotsize;			/* size of area at slotoffset */
//	int slotfreespace;		/* amount of space free in slot */
//	int slotneeded;			/* size of the entry we're seeking */
	int numdirpasses=0;		/* strategy for directory search */
	doff_t endsearch;		/* offset to end directory search */
	doff_t prevoff=0;			/* prev entry dp->i_offset */
	vnode_t pdp;		/* saved dp during symlink work */
	vnode_t tdp;		/* returned by VFS_VGET */
	doff_t enduseful=0;		/* pointer past last used dir slot */
	u_long bmask=0;			/* block offset mask */
	int wantparent;			/* 1 => wantparent or lockparent flag */
	int namlen, error;
	struct ucred *cred = vfs_context_ucred(context);
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	//struct proc *p = vfs_context_proc(ap->a_context);
    mount_t mp;
    int dx = 0;
   
    ext2_trace_enter();

	bp = NULL;
	ss.slotoffset = -1;
	if (vpp) *vpp = NULL;
	dp = VTOI(vdp);
    mp = vnode_mount(vdp);
	wantparent = flags & (LOCKPARENT|WANTPARENT);
    uint32_t	DIRBLKSIZ = dp->i_e2fs->s_blocksize;

	/*
	 * Suppress search for slots unless creating
	 * file and at end of pathname, in which case
	 * we watch for a place to put the new file in
	 * case it doesn't already exist.
	 */
	i_diroff =  dp->i_diroff;
	ss.slotstatus = FOUND;
	ss.slotfreespace = ss.slotsize = ss.slotneeded = 0;
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN)) {
		ss.slotstatus = NONE;
		ss.slotneeded = EXT2_DIR_REC_LEN(cnp->cn_namelen);
		/* was
		slotneeded = (sizeof(struct direct) - MAXNAMLEN +
			cnp->cn_namelen + 3) &~ 3; */
	}
    
    IXLOCK(dp);
    assert((dp->i_flag & IN_LOOK));
    
    dp->i_lastnamei = nameiop;

//   /*
//   * Try to lookup dir entry using htree directory index.
//   *
//   * If we got an error or we want to find '.' or '..' entry,
//   * we will fall back to linear search.
//   */
//	if (!ext2_is_dot_entry(cnp) && ext2_htree_has_idx(dp)) {
//		switch (ext2_htree_lookup(dp, cnp->cn_nameptr, cnp->cn_namelen,
//			&bp, &entryoffsetinblock, &dp->i_offset, &prevoff,
//			&enduseful, &ss)) {
//				case 0:
//					ep = (struct ext2_dir_entry_2 *)((char *)buf_dataptr(bp) +
//						(dp->i_offset & bmask));
//					goto found;
//				case ENOENT:
//					dp->i_offset = (doff_t)roundup(dp->i_size, DIRBLKSIZ); // must be power of 2
//					goto notfound;
//				default:
//					/*
//					 * Something failed; just fallback to do a linear
//					 * search.
//					 */
//					dxtrace(ext2_debug("ext2_lookup: dx failed, falling back\n"));
//					break;
//		}
//	}
//
//
//
   if (is_dx(dp)) {
      struct dentry dentry, dparent = {NULL, {NULL, 0}, dp};
      /* For some reason, ext3_dx_find_entry() doesn't handle
         '.' and '..', so fall back. */
      if ((1 == cnp->cn_namelen && !strncmp(cnp->cn_nameptr, ".", 1)) ||
         (2 == cnp->cn_namelen && !strncmp(cnp->cn_nameptr, "..", 2)))
         goto dx_fallback;
      
      dentry.d_parent = &dparent;
      dentry.d_name.name = cnp->cn_nameptr;
      dentry.d_name.len = cnp->cn_namelen;
      dentry.d_inode = NULL;
      // XXX ext3_dx* is not lock safe ATM, so don't unlock
      bp = ext3_dx_find_entry(&dentry, &ep, &error);
      /*
       * On success, or if the error was file not found,
       * return.  Otherwise, fall back to doing a search the
       * old fashioned way.
       */
      dx = 1;
      if (bp) {
         dp->i_ino = le32_to_cpu(ep->inode);
         dp->i_reclen = le16_to_cpu(ep->rec_len);
         entryoffsetinblock = (char*)ep - (char*)buf_dataptr(bp);
         error = 0;
         goto found;
      }
      if (error != ERR_BAD_DX_DIR) {
         error = -error; // Linux uses -ve errors
         entryoffsetinblock = 0;
         goto notfound;
      }
      dxtrace(ext2_debug("ext2_lookup: dx failed, falling back\n"));
      dx = 0;
   }
	   
dx_fallback:

    /*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	 */
	bmask = vfs_statfs(mp)->f_iosize - 1;
    if (nameiop != LOOKUP || dp->i_diroff == 0 ||
	    i_diroff > dp->i_size) {
		entryoffsetinblock = 0;
		dp->i_offset = 0;
		numdirpasses = 1;
	} else {
		dp->i_offset = dp->i_diroff;
		if ((entryoffsetinblock = dp->i_offset & bmask)) {
		    prevoff = dp->i_offset;
            IULOCK(dp);
            if (0 != (error = ext2_blkatoff(vdp, (off_t)prevoff, NULL, &bp)))
                ext2_trace_return(error);
            IXLOCK(dp);
        }
		numdirpasses = 2;
//		nchstats.ncs_2passes++;
	}
	prevoff = dp->i_offset;
	endsearch = (doff_t)roundup(dp->i_size, DIRBLKSIZ);
	enduseful = 0;

searchloop:
	while (dp->i_offset < endsearch) {
		/*
		 * If necessary, get the next directory block.
		 */
		if ((dp->i_offset & bmask) == 0) {
			if (bp != NULL)
				buf_brelse(bp);
			IULOCK(dp);
            if (0 != (error = ext2_blkatoff(vdp, (off_t)dp->i_offset, NULL, &bp)))
                ext2_trace_return(error);
            IXLOCK(dp);
			entryoffsetinblock = 0;
		}
		/*
		 * If still looking for a slot, and at a DIRBLKSIZE
		 * boundary, have to start looking for free space again.
		 */
		if (ss.slotstatus == NONE &&
		    (entryoffsetinblock & (DIRBLKSIZ - 1)) == 0) {
			ss.slotoffset = -1;
			ss.slotfreespace = 0;
		}
		/*
		 * Get pointer to next entry.
		 * Full validation checks are slow, so we only check
		 * enough to insure forward progress through the
		 * directory. Complete checks can be run by setting
		 * "vfs.e2fs.dirchk" to be true.
		 */
		ep = (struct ext2_dir_entry_2 *)
			((char *)buf_dataptr(bp) + entryoffsetinblock);
		if (ep->rec_len == 0 ||
		    (dirchk && ext2_dirbadentry_locked(vdp, ep, entryoffsetinblock))) {
			int i;
			ext2_dirbad(dp, dp->i_offset, "mangled entry");
			i = DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1));
			dp->i_offset += i;
			entryoffsetinblock += i;
			continue;
		}

		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		 */
		if (ss.slotstatus != FOUND) {
			int size = le16_to_cpu(ep->rec_len);

			if (ep->inode != 0)
				size -= EXT2_DIR_REC_LEN(ep->name_len);
			else if (ext2_is_dirent_tail(dp, ep))
				size -= sizeof(struct ext2fs_direct_tail);
			if (size > 0) {
				if (size >= ss.slotneeded) {
					ss.slotstatus = FOUND;
					ss.slotoffset = dp->i_offset;
					ss.slotsize = le16_to_cpu(ep->rec_len);
				} else if (ss.slotstatus == NONE) {
					ss.slotfreespace += size;
					if (ss.slotoffset == -1)
						ss.slotoffset = dp->i_offset;
					if (ss.slotfreespace >= ss.slotneeded) {
						ss.slotstatus = COMPACT;
						ss.slotsize = (dp->i_offset +
						      le16_to_cpu(ep->rec_len)) - ss.slotoffset;
					}
				}
			}
		}

		/*
		 * Check for a name match.
		 */
		if (ep->inode) {
			namlen = ep->name_len;
			if (namlen == cnp->cn_namelen &&
			    !bcmp(cnp->cn_nameptr, ep->name,
				(unsigned)namlen)) {
				/*
				 * Save directory entry's inode number and
				 * reclen in ndp->ni_ufs area, and release
				 * directory buffer.
				 */
                
				dp->i_ino = le32_to_cpu(ep->inode);
				dp->i_reclen = le16_to_cpu(ep->rec_len);
				goto found;
			}
		}
		prevoff = dp->i_offset;
		dp->i_offset += le16_to_cpu(ep->rec_len);
		entryoffsetinblock += le16_to_cpu(ep->rec_len);
		if (ep->inode)
			enduseful = dp->i_offset;
	}
notfound:
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		dp->i_offset = 0;
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		buf_brelse(bp);
	/*
	 * If creating, and at end of pathname and current
	 * directory has not been removed, then can consider
	 * allowing file to be created.
	 */
	if ((nameiop == CREATE || nameiop == RENAME) &&
	    (flags & ISLASTCN) && dp->i_nlink != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		IULOCK(dp);
        if (0 != (error = vnode_authorize(vdp, NULL, KAUTH_VNODE_WRITE_DATA, context)))
            ext2_trace_return(error);
        IXLOCK(dp);
        
		/*
		 * Return an indication of where the new directory
		 * entry should be put.  If we didn't find a slot,
		 * then set dp->i_count to 0 indicating
		 * that the new slot belongs at the end of the
		 * directory. If we found a slot, then the new entry
		 * can be put in the range from dp->i_offset to
		 * dp->i_offset + dp->i_count.
		 */
		if (ss.slotstatus == NONE) {
			dp->i_offset = (doff_t)roundup(dp->i_size, DIRBLKSIZ);
			dp->i_count = 0;
			enduseful = dp->i_offset;
		} else {
			dp->i_offset = ss.slotoffset;
			dp->i_count = ss.slotsize;
			if (enduseful < ss.slotoffset + ss.slotsize)
				enduseful = ss.slotoffset + ss.slotsize;
		}
		dp->i_endoff = roundup(enduseful, DIRBLKSIZ);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
//        #if 0
		ext2_debug("dirent: count:%d, endoff:%d, diroff:%d, off:%d, ino:%u, reclen:%u name:%s\n",
            dp->i_count, dp->i_endoff, dp->i_diroff, dp->i_offset, dp->i_ino, dp->i_reclen, cnp->cn_nameptr);
//        #endif
		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
        IULOCK(dp);
        ext2_trace_return(EJUSTRETURN);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if (vpp && (cnp->cn_flags & MAKEENTRY) && nameiop != CREATE)
		cache_enter(vdp, *vpp, cnp);
	
    IULOCK(dp);
    ext2_trace_return(ENOENT);

found:
	/*
	 * Check that directory length properly reflects presence
	 * of this entry.
	 */
	if (entryoffsetinblock + EXT2_DIR_REC_LEN(ep->name_len) > dp->i_size) {
		ext2_dirbad(dp, dp->i_offset, "i_size too small");
		dp->i_size = entryoffsetinblock+EXT2_DIR_REC_LEN(ep->name_len);
		dp->i_flag |= IN_CHANGE | IN_UPDATE;
	}
	buf_brelse(bp);

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
    * -- BDB -- Don't alter diroff for dx dirs.
	 */
	if ((flags & ISLASTCN) && nameiop == LOOKUP && !dx)
		dp->i_diroff = dp->i_offset &~ (DIRBLKSIZ - 1);

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in ndp->ni_dvp), otherwise we go
	 * on and lock the inode, being careful with ".".
	 */
	if (nameiop == DELETE && (flags & ISLASTCN)) {
		/*
		 * Write access to directory required to delete files.
		 */
		if (vpp) {
				IULOCK(dp);
				if (0 != (error = vnode_authorize(vdp, NULL, KAUTH_VNODE_WRITE_DATA, context)))
					ext2_trace_return(error);
				IXLOCK(dp);
			}
			/*
			 * Return pointer to current entry in dp->i_offset,
			 * and distance past previous entry (if there
			 * is a previous entry in this block) in dp->i_count.
			 * Save directory inode pointer in ndp->ni_dvp for dirremove().
			 */
			if ((dp->i_offset & (DIRBLKSIZ - 1)) == 0)
				dp->i_count = 0;
			else
				dp->i_count = dp->i_offset - prevoff;
			if (dp->i_number == dp->i_ino) {
				IULOCK(dp);
				
				if (vpp) {
					vnode_get(vdp);
					*vpp = vdp;
				}
				return (0);
			}
			
			if (vpp) {
			vallocargs.va_ino = dp->i_ino;
			vallocargs.va_parent = vdp;
			vallocargs.va_vctx = context;
			vallocargs.va_cnp = cnp;
			IULOCK(dp);
			if (0 != (error =  ext2fs_vfsops.vfs_vget(vnode_mount(vdp), dp->i_ino, &tdp, context)))
				//EXT2_VGET(mp, &vallocargs, &tdp, context)))
			{
				ext2_trace_return(error);
			}
			IXLOCK(dp);
			
			/*
			 * If directory is "sticky", then user must own
			 * the directory, or the file in it, else she
			 * may not delete it (unless she's root). This
			 * implements append-only directories.
			 */
			if ((dp->i_mode & ISVTX) &&
				cred->cr_posix.cr_uid != 0 &&
				cred->cr_posix.cr_uid != dp->i_uid &&
				VTOI(tdp)->i_uid /* XXX Lock tdp? */ != cred->cr_posix.cr_uid) {
				
				IULOCK(dp);
				
				vnode_put(tdp);
				ext2_trace_return(EPERM);
			}
			*vpp = tdp;
		} // (vpp)
        
        IULOCK(dp);
        return (0);
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (nameiop == RENAME && wantparent &&
	    (flags & ISLASTCN)) {
        IULOCK(dp);
        if (vpp) {
		if (0 != (error = vnode_authorize(vdp, NULL, KAUTH_VNODE_WRITE_DATA, context)))
            ext2_trace_return(error);
        }
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
        // XXX: Locking - i_number doesn't change, and i_ino is protected by IN_LOOK
		if (dp->i_number == dp->i_ino) {
			ext2_trace_return(EISDIR);
        }
        
        if (vpp) {
        vallocargs.va_ino = dp->i_ino;
        vallocargs.va_parent = vdp;
        vallocargs.va_vctx = context;
        vallocargs.va_cnp = cnp;
		if (0 != (error = ext2fs_vfsops.vfs_vget(vnode_mount(vdp), dp->i_ino, &tdp, context)))
				//EXT2_VGET(mp, &vallocargs, &tdp, context)))
			ext2_trace_return(error);
		*vpp = tdp;
        } // (vpp)
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `vput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the VFS_VGET for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
    IULOCK(dp); // XXX: Locking - i_ino is protected by IN_LOOK
    
    if (vpp) {
	pdp = vdp;
	if (flags & ISDOTDOT) {
        vallocargs.va_ino = dp->i_ino;
        vallocargs.va_parent = pdp;
        vallocargs.va_vctx = context;
        vallocargs.va_cnp = cnp;
		if (0 != (error = ext2fs_vfsops.vfs_vget(vnode_mount(vdp), dp->i_ino, &tdp, context))){
			//EXT2_VGET(mp, &vallocargs, &tdp, context)))
            ext2_trace_return(error);
		}
        
		*vpp = tdp;
	} else if (dp->i_number == dp->i_ino) {
        vnode_get(vdp);	/* we want ourself, ie "." */
		*vpp = vdp;
	} else {
      vallocargs.va_ino = dp->i_ino;
      vallocargs.va_parent = pdp;
      vallocargs.va_vctx = context;
      vallocargs.va_cnp = cnp;
		if ((error = ext2fs_vfsops.vfs_vget(vnode_mount(vdp), dp->i_ino, &tdp, context)) != 0){
		  //EXT2_VGET(mp, &vallocargs, &tdp, context)) != 0)
			ext2_trace_return(error);
		}
        
		*vpp = tdp;
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vdp, *vpp, cnp);
    } // (vpp)
    
    return (0);
}


int
ext2_lookup(
	struct vnop_lookup_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
        vfs_context_t a_context;
	} */ *ap)
{
	*ap->a_vpp = NULL;
    
    /*
	 * Lookup an entry in the cache
	 * If the lookup succeeds, the vnode is returned in *vpp, and a status of -1 is
	 * returned. If the lookup determines that the name does not exist
	 * (negative cacheing), a status of ENOENT is returned. If the lookup
	 * fails, a status of zero is returned.
     * If found, an extra ref is already taken, so there's no need for vnode_get.
	 */
	int error = cache_lookup(ap->a_dvp, ap->a_vpp, ap->a_cnp);
	if (error == 0) /* Unsuccessful */
        goto dolookup;
	if (error == ENOENT)
		return (error);
	
	/* We have a name that matched */
    return (0);
    
dolookup:
    ;
    /*
	 * We now have a segment name to search for, and a directory to search.
	 */
    
    struct inode *dp = VTOI(ap->a_dvp);
    struct componentname *cnp = ap->a_cnp;
    
    IXLOCK(dp);
    while (dp->i_flag & IN_LOOK) {
        dp->i_flag |= IN_LOOKWAIT;
        lookw();
        ISLEEP(dp, flag, NULL);
    }
    dp->i_flag |= IN_LOOK;
    IULOCK(dp);
    
    int nameiop = cnp->cn_nameiop;
    error = ext2_lookupi(ap->a_dvp, ap->a_vpp, cnp, ap->a_context);
    
    if ((DELETE == nameiop || RENAME == nameiop) && (0 == error || EJUSTRETURN == error)) {
        if (!dp->i_dhashtbl) {
            u_long dummy;
            dp->i_dhashtbl = hashinit(E2DCACHE_BUCKETS, M_TEMP, &dummy);
        }
        if (dp->i_dhashtbl) {
            struct dcache *dcp = ext2_dcacheget(dp, cnp);
            if (!dcp) {
                MALLOC(dcp, typeof(dcp), sizeof(*dcp) + cnp->cn_namelen + 1, M_TEMP, M_WAITOK);
                bzero(dcp, sizeof(*dcp)); // don't care about name
                dcp->d_namelen = (typeof(dcp->d_namelen))cnp->cn_namelen;
                bcopy(cnp->cn_nameptr, dcp->d_name, dcp->d_namelen);
                dcp->d_name[dcp->d_namelen] = 0;
                ext2_dcacheins(dp, dcp, cnp);
            }
            
            dcp->d_offset = dp->i_offset;
            dcp->d_count = dp->i_count;
            dcp->d_reclen = dp->i_reclen;
            #ifdef DIAGNOSTIC
            dcp->d_ino = dp->i_ino;
            #endif
            dcp->d_refct++;
        }
    }
    
    IXLOCK(dp);
    dp->i_flag &= ~IN_LOOK;
    IWAKE(dp, flag, flag, IN_LOOKWAIT);
    IULOCK(dp);
    
    return (error);
}

void
ext2_dirbad(ip, offset, how)
	struct inode *ip;
	doff_t offset;
	char *how;
{
	mount_t  mp;

	mp = ITOVFS(ip);
	(void)ext2_debug("%s: bad dir ino %lu at offset %ld: %s\n",
	    vfs_statfs(mp)->f_mntonname, (u_long)ip->i_number, (long)offset, how);
	if (0 == vfs_isrdonly(mp))
		panic("ext2_dirbad: bad dir (%s)", how);
}

/*
 * Do consistency checking on a directory entry:
 *	record length must be multiple of 4
 *	entry must fit in rest of its DIRBLKSIZ block
 *	record must be large enough to contain entry
 *	name is not longer than MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 */
/*
 *	changed so that it confirms to ext2_check_dir_entry
 */
static int
ext2_dirbadentry_locked(
	vnode_t dp,
	struct ext2_dir_entry_2 *de,
	int entryoffsetinblock)
{
	const uint64_t DIRBLKSIZ = VTOI(dp)->i_e2fs->s_blocksize;
   const int rlen = le16_to_cpu(de->rec_len);

   char * error_msg = NULL;

   if (rlen < EXT2_DIR_REC_LEN(1))
            error_msg = "rec_len is smaller than minimal";
   else if (rlen % 4 != 0)
            error_msg = "rec_len % 4 != 0";
   else if (rlen < EXT2_DIR_REC_LEN(de->name_len))
            error_msg = "reclen is too small for name_len";
   else if (entryoffsetinblock + rlen > DIRBLKSIZ)
            error_msg = "directory entry across blocks";
   else if (le32_to_cpu(de->inode) >
      le32_to_cpu(VTOI(dp)->i_e2fs->s_es->s_inodes_count))
            error_msg = "inode out of bounds";

   if (error_msg != NULL) {
            ext2_debug("ext2 bad directory entry: %s\n", error_msg);
            ext2_debug("offset=%d, inode=%lu, rec_len=%u, name_len=%u\n",
   entryoffsetinblock, (unsigned long)le32_to_cpu(de->inode),
   le16_to_cpu(de->rec_len), de->name_len);
   }
   return error_msg == NULL ? 0 : 1;
}

/*
 * Write a directory entry after a call to namei, using the parameters
 * that it left in nameidata.  The argument ip is the inode which the new
 * directory entry will refer to.  Dvp is a pointer to the directory to
 * be written, which was left locked by namei. Remaining parameters
 * (dp->i_offset, dp->i_count) indicate how the space for the new
 * entry is to be obtained.
 */
int
ext2_direnter(struct inode *ip,
			  vnode_t dvp,
			  struct componentname *cnp,
			  vfs_context_t context)
{
	off_t offset;
    struct ext2_dir_entry_2 *ep, *nep;
	struct inode *dp;
	buf_t  bp;
	struct ext2_dir_entry_2 newdir;
	uio_t auio;
	u_int dsize;
	int error, loc, newentrysize;
	off_t spacefree;
	char *dirbuf;
	uint64_t     DIRBLKSIZ = ip->i_e2fs->s_blocksize;
    struct ext2_sb_info *fs;
    int dx_fallback = 0;
    struct dentry dentry, dparent = {NULL, {NULL, 0}, NULL};
    handle_t h = {cnp, context};
    
	dp = VTOI(dvp);
    fs = dp->i_e2fs;
	newdir.inode = cpu_to_le32(ip->i_number);
	newdir.name_len = cnp->cn_namelen;
	if (EXT2_HAS_INCOMPAT_FEATURE(ip->i_e2fs,
	    EXT2_FEATURE_INCOMPAT_FILETYPE))
		newdir.file_type = DTTOFT(IFTODT(ip->i_mode));
	else
		newdir.file_type = EXT2_FT_UNKNOWN;
	bcopy(cnp->cn_nameptr, newdir.name, (unsigned)cnp->cn_namelen + 1);
	newentrysize = EXT2_DIR_REC_LEN(newdir.name_len);
   
   /* Check for indexed dir */
   dparent.d_inode = dp;
   dentry.d_parent = &dparent;
   dentry.d_name.name = cnp->cn_nameptr;
   dentry.d_name.len = cnp->cn_namelen;
   dentry.d_inode = ip;
   
   IXLOCK_WITH_LOCKED_INODE(dp, ip);
    // We use/modify dir lookup cache, so we need IN_LOOK
    while (dp->i_flag & IN_LOOK) {
        // XXX Can't use ISLEEP here, because msleep knows nothing of our inode locking order
        dp->i_flag |= IN_LOOKWAIT;
        lookw();
        IULOCK(dp);
        ISLEEP_NOLCK(dp, flag, NULL);
        IXLOCK_WITH_LOCKED_INODE(dp, ip);
    }
    dp->i_flag |= IN_LOOK;
    
    dsize = EXT2_DIR_REC_LEN(cnp->cn_namelen);
    if (CREATE == dp->i_lastnamei && dsize <= dp->i_count) {
        lookchit();
    } else {
        IULOCK(dp);
        
        OSIncrementAtomic((SInt32*)&lookcacheinval);
        error = ext2_lookupi(dvp, NULL, cnp, context);
        
        IXLOCK_WITH_LOCKED_INODE(dp, ip);
        if (0 == error || EJUSTRETURN == error) {
            error = 0;
        } else {
            dp->i_flag &= ~IN_LOOK;
            IWAKE(dp, flag, flag, IN_LOOKWAIT);
            IULOCK(dp);
            ext2_trace_return(error);
        }
    }
   
   if (is_dx(dp)) {
        #ifdef notyet
        IULOCK(ip);
        IULOCK(dp);
        #endif
		// XXX ext3_dx* is not lock safe ATM, so don't unlock
        error = ext3_dx_add_entry(&h, &dentry, ip);
        #ifdef notyet
        IXLOCK(ip);
        IXLOCK_WITH_LOCKED_INODE(dp, ip);
        #endif
        
		if (!error || (error != ERR_BAD_DX_DIR)) {
            dp->i_flag |= IN_CHANGE | IN_UPDATE;
			dp->i_flag &= ~IN_LOOK;
            IWAKE(dp, flag, flag, IN_LOOKWAIT);
            IULOCK(dp);
            return -(error); /* Linux uses -ve errors */
        }
		dp->i_flags &= ~EXT3_INDEX_FL;
		dx_fallback++;
		ext3_mark_inode_dirty(&h, dp);
	}
   
	if (dp->i_count == 0) {
		/*
		 * If dp->i_count is 0, then namei could find no
		 * space in the directory. Here, dp->i_offset will
		 * be on a directory block boundary and we will write the
		 * new entry into a fresh block.
		 */
		if (dp->i_offset & (DIRBLKSIZ - 1))
			panic("ext2_direnter: newblk");
      
      /* Create an indexed dir if possible */
      spacefree = dp->i_size >> fs->s_blocksize_bits;
      if (spacefree == 1 && (fs->s_mount_opt & EXT2_MNT_INDEX) &&
         !EXT3_HAS_COMPAT_FEATURE(fs, EXT3_FEATURE_COMPAT_DIR_INDEX)) {
         /* First index dir, add flag to superblock. */
         lock_super(fs);
         ext3_update_dynamic_rev(fs);
         EXT2_SET_COMPAT_FEATURE(fs, EXT3_FEATURE_COMPAT_DIR_INDEX);
         fs->s_dirt = 1;
         unlock_super(fs);
      }
      if (spacefree == 1 && !dx_fallback &&
         EXT3_HAS_COMPAT_FEATURE(fs, EXT3_FEATURE_COMPAT_DIR_INDEX)) {
         IULOCK(dp);
         error = ext2_blkatoff(dvp, (off_t)0, &dirbuf, &bp);
         IXLOCK_WITH_LOCKED_INODE(dp, ip);
         if (!error) {
             dp->f_pos = 0;
             dp->i_flag |= IN_CHANGE;
             // XXX ext3_dx* is not lock safe ATM, so don't unlock
             error = make_indexed_dir(&h, &dentry, ip, bp); // bp is released by this routine
         }
         dp->i_flag &= ~IN_LOOK;
         IWAKE(dp, flag, flag, IN_LOOKWAIT);
         IULOCK(dp);
         ext2_trace_return (error);
      }
      
		offset = dp->i_offset;
        IULOCK(dp);
        
        if ((auio = uio_create(1, offset, UIO_SYSSPACE32, UIO_WRITE)))
            error = uio_addiov(auio, CAST_USER_ADDR_T(&newdir), (user_size_t)newentrysize);
        else
            error = ENOMEM;
            
        if (!error) {
            uio_setresid(auio, newentrysize);
            newdir.rec_len = cpu_to_le16(DIRBLKSIZ);
        
            error = EXT2_WRITE(dvp, auio, IO_SYNC, context);
            if (DIRBLKSIZ > vfs_statfs(vnode_mount(dvp))->f_bsize) {
                /* XXX should grow with balloc() */
                panic("ext2_direnter: frag size");
            }
        }
        
        IXLOCK_WITH_LOCKED_INODE(dp, ip);
        if (!error) {
            dp->i_size = roundup(dp->i_size, DIRBLKSIZ);
            dp->i_flag |= IN_CHANGE | IN_DX_UPDATE;
        }
        dp->i_flag &= ~IN_LOOK;
        IWAKE(dp, flag, flag, IN_LOOKWAIT);
        IULOCK(dp);
        
        if (auio)
            uio_free(auio);
		ext2_trace_return (error);
	}

	/*
	 * If dp->i_count is non-zero, then namei found space
	 * for the new entry in the range dp->i_offset to
	 * dp->i_offset + dp->i_count in the directory.
	 * To use this space, we may have to compact the entries located
	 * there, by copying them together towards the beginning of the
	 * block, leaving the free space in one usable chunk at the end.
	 */

	/*
	 * Increase size of directory if entry eats into new space.
	 * This should never push the size past a new multiple of
	 * DIRBLKSIZE.
	 *
	 * N.B. - THIS IS AN ARTIFACT OF 4.2 AND SHOULD NEVER HAPPEN.
	 */
	if (dp->i_offset + dp->i_count > dp->i_size)
		dp->i_size = dp->i_offset + dp->i_count;
	/*
	 * Get the block containing the space for the new directory entry.
	 */
	offset = dp->i_offset;
    IULOCK(dp);
    
    error = ext2_blkatoff(dvp, offset, &dirbuf, &bp);
	
    IXLOCK_WITH_LOCKED_INODE(dp, ip);
    
    if (error) {
        dp->i_flag &= ~IN_LOOK;
        IWAKE(dp, flag, flag, IN_LOOKWAIT);
        IULOCK(dp);
        ext2_trace_return (error);
    }
    
    /*
	 * Find space for the new entry. In the simple case, the entry at
	 * offset base will have the space. If it does not, then namei
	 * arranged that compacting the region dp->i_offset to
	 * dp->i_offset + dp->i_count would yield the
	 * space.
	 */
	ep = (struct ext2_dir_entry_2 *)dirbuf;
	dsize = EXT2_DIR_REC_LEN(ep->name_len);
	spacefree = le16_to_cpu(ep->rec_len) - dsize;
	for (loc = le16_to_cpu(ep->rec_len); loc < dp->i_count; ) {
		nep = (struct ext2_dir_entry_2 *)(dirbuf + loc);
		if (ep->inode) {
			/* trim the existing slot */
			ep->rec_len = cpu_to_le16(dsize);
			ep = (struct ext2_dir_entry_2 *)((char *)ep + dsize);
		} else {
			/* overwrite; nothing there; header is ours */
			spacefree += dsize;
		}
		dsize = EXT2_DIR_REC_LEN(nep->name_len);
		spacefree += le16_to_cpu(nep->rec_len) - dsize;
		loc += le16_to_cpu(nep->rec_len);
		bcopy((caddr_t)nep, (caddr_t)ep, dsize);
	}
	/*
	 * Update the pointer fields in the previous entry (if any),
	 * copy in the new entry, and write out the block.
	 */
	if (ep->inode == 0) {
		if (spacefree + dsize < newentrysize)
			panic("ext2_direnter: compact1");
		newdir.rec_len = cpu_to_le16(spacefree + dsize);
	} else {
		if (spacefree < newentrysize)
			panic("ext2_direnter: compact2");
		newdir.rec_len = cpu_to_le16(spacefree);
		ep->rec_len = cpu_to_le16(dsize);
		ep = (struct ext2_dir_entry_2 *)((char *)ep + dsize);
	}
	bcopy((caddr_t)&newdir, (caddr_t)ep, (u_int)newentrysize);
    #if 0
    ext2_debug("dirent {%u,%u,%u,%s} @ %u to %u\n",
        le32_to_cpu(newdir.inode), le16_to_cpu(newdir.rec_len), newdir.name_len, cnp->cn_nameptr,
        (char*)ep - (char*)buf_dataptr(bp), dp->i_number);
    #endif
	dp->i_flag |= IN_CHANGE | IN_UPDATE | IN_DX_UPDATE;
    
    offset = (off_t)dp->i_endoff;
    off_t isize = dp->i_size;
    dp->i_flag &= ~IN_LOOK;
    IWAKE(dp, flag, flag, IN_LOOKWAIT);
    IULOCK(dp);
    
    error = BUF_WRITE(bp);
	if (!error && offset && offset < isize)
		error = ext2_truncate(dvp, offset, IO_SYNC,
		    vfs_context_ucred(context), vfs_context_proc(context));
	ext2_trace_return(error);
}

static inline
void e2copydcache(const struct dcache *dcp, struct inode *dp)
{
    dp->i_offset = dcp->d_offset;
    dp->i_count = dcp->d_count;
    dp->i_reclen = dcp->d_reclen;
    #ifdef DIAGNOSTIC
    dp->i_ino = dcp->d_ino;
    #if 0
    bcopy(dcp->d_name, dp->i_cachename, dcp->d_namelen);
    dp->i_cachenamelen = dcp->d_namelen;
    dp->i_cachename[dp->i_cachenamelen] = 0;
    #endif
    #endif
}

/*
 * Remove a directory entry after a call to namei, using
 * the parameters which it left in nameidata. The entry
 * dp->i_offset contains the offset into the directory of the
 * entry to be eliminated.  The dp->i_count field contains the
 * size of the previous record in the directory.  If this
 * is 0, the first entry is being deleted, so we need only
 * zero the inode number to mark the entry as free.  If the
 * entry is not the first in the directory, we must reclaim
 * the space of the now empty record by adding the record size
 * to the size of the previous entry.
 */
int
ext2_dirremove(vnode_t dvp,
			   struct componentname *cnp,
			   vfs_context_t context)
{
	struct inode *dp;
	struct ext2_dir_entry_2 *ep;
	buf_t  bp;
	int error, cacheinval E2_DGB_ONLY = 0;
	 
	dp = VTOI(dvp);
   
   bp = NULL;
   IXLOCK(dp);
   
    // We use/modify dir lookup cache, so we need IN_LOOK
    while (dp->i_flag & IN_LOOK) {
        dp->i_flag |= IN_LOOKWAIT;
        lookw();
        ISLEEP(dp, flag, NULL);
    }
    dp->i_flag |= IN_LOOK;
   
    struct dcache *dcp = dp->i_dhashtbl ? ext2_dcacheget(dp, cnp) : NULL;
    if (dcp) {
        lookchit();
        e2copydcache(dcp, dp);
        dcp->d_namelen = 0;
        ext2_dcacherem(dcp, dp);
    } else {
        // dir cache is invalid, need to lookup again
        IULOCK(dp);
        
        #ifdef DIAGNOSTIC
        cacheinval = 1;
        #endif
        OSIncrementAtomic((SInt32*)&lookcacheinval);
        error = ext2_lookupi(dvp, NULL, cnp, context);
        
        IXLOCK(dp);
        
        if (0 == error) {
            ;
        } else {
            dp->i_flag &= ~IN_LOOK;
            IWAKE(dp, flag, flag, IN_LOOKWAIT);
            IULOCK(dp);
            ext2_trace_return(error);
        }
    }
   
   /* Check for indexed dir */
   if (is_dx(dp)) {
      struct dentry dentry, dparent = {NULL, {NULL, 0}, dp};
      handle_t h = {cnp, context};
      
      dentry.d_parent = &dparent;
      dentry.d_name.name = cnp->cn_nameptr;
      dentry.d_name.len = cnp->cn_namelen;
      dentry.d_inode = NULL;
      
      // XXX ext3_dx* is not lock safe ATM, so don't unlock
      // IULOCK(dp);
      
      if (NULL == (bp = ext3_dx_find_entry(&dentry, &ep, &error))) {
         dp->i_flag &= ~IN_LOOK;
         IWAKE(dp, flag, flag, IN_LOOKWAIT);
         IULOCK(dp); // XXX
         return -(error); /* Linux uses -ve errors */
      }
      error = ext3_delete_entry(&h, dp, ep, bp);
      buf_brelse(bp);
      
      if (0 == error) {
         // IXLOCK(dp);
         dp->i_flag |= IN_CHANGE | IN_UPDATE | IN_DX_UPDATE;
         // IULOCK(dp);
      }
      
      dp->i_flag &= ~IN_LOOK;
      IWAKE(dp, flag, flag, IN_LOOKWAIT);
      IULOCK(dp); // XXX
      return -(error); /* Linux uses -ve errors */
   }
   
	if (dp->i_count == 0) {
		/*
		 * First entry in block: set d_ino to zero.
		 */
		IULOCK(dp);
        if (!(error = ext2_blkatoff(dvp, (off_t)dp->i_offset, (char **)&ep, &bp))) {
            IXLOCK(dp);
            ep->inode = 0;
            IULOCK(dp);
            error = BUF_WRITE(bp);
        }        
        IXLOCK(dp);
        if (!error) {
            dp->i_flag |= IN_CHANGE | IN_UPDATE | IN_DX_UPDATE;
            ext2_checkdir_locked(dvp);
        }
        
        dp->i_flag &= ~IN_LOOK;
        IWAKE(dp, flag, flag, IN_LOOKWAIT);
        IULOCK(dp);
        
		return (error);
	}
	/*
	 * Collapse new free space into previous entry.
	 */
	off_t offset = (off_t)(dp->i_offset - dp->i_count);
    IULOCK(dp);
    error = ext2_blkatoff(dvp, offset, (char **)&ep, &bp);
    if (!error) {
        #ifdef DIAGNOSTIC
        buf_t bp2;
        struct ext2_dir_entry_2 *ep2;
        if (lblkno(dp->i_e2fs, dp->i_offset) == buf_lblkno(bp)) {
            ep2 = (typeof(ep))((char*)buf_dataptr(bp) + blkoff(dp->i_e2fs, dp->i_offset));
            bp2 = NULL;
        } else if (0 != ext2_blkatoff(dvp, dp->i_offset, (char **)&ep2, &bp2)) {
            bp2 = NULL;
            ep2 = NULL;
        }
        #endif
        IXLOCK(dp);
        #if DIAGNOSTIC
        if (ep2) {
            assert(0 == (ep2->file_type & 0xF0));
            assert(dp->i_ino == le32_to_cpu(ep2->inode));
            ep2->file_type |= 0xF0;
            int i = cnp->cn_namelen - 1;
            for (; i >= 0; --i) {
                if (ep2->name[i] != cnp->cn_nameptr[i])
                    panic("ext2: cn name != entry name");
            }
        }
        #endif
        ep->rec_len = cpu_to_le16(le16_to_cpu(ep->rec_len) + dp->i_reclen);
        IULOCK(dp);
        error = BUF_WRITE(bp);
        #ifdef DIAGNOSTIC
        if (bp2 && bp2 != bp)
            BUF_WRITE(bp2);
        else if (bp2)
            buf_brelse(bp2);
        #endif
    }
	
    IXLOCK(dp);
    if (!error) {
        dp->i_flag |= IN_CHANGE | IN_UPDATE | IN_DX_UPDATE;
        ext2_checkdir_locked(dvp);
    }
    dp->i_flag &= ~IN_LOOK;
    IWAKE(dp, flag, flag, IN_LOOKWAIT);
    IULOCK(dp);
    
    ext2_trace_return (error);
}

/*
 * Rewrite an existing directory entry to point at the inode
 * supplied.  The parameters describing the directory entry are
 * set up by a call to namei.
 */
int
ext2_dirrewrite_nolock(struct inode *dp,
					   struct inode *ip,
					   struct componentname *cnp,
					   vfs_context_t context)
{
	buf_t  bp;
	struct ext2_dir_entry_2 *ep;
	vnode_t vdp = ITOV(dp);
	int error;
    off_t offset;

   assert(ip->i_lockowner != current_thread());
   
   IXLOCK(dp);
   
    // We use/modify dir lookup cache, so we need IN_LOOK
    while (dp->i_flag & IN_LOOK) {
        dp->i_flag |= IN_LOOKWAIT;
        lookw();
        ISLEEP(dp, flag, NULL);
    }
    dp->i_flag |= IN_LOOK;
   
    struct dcache *dcp = dp->i_dhashtbl ? ext2_dcacheget(dp, cnp) : NULL;
    if (dcp) {
        lookchit();
        e2copydcache(dcp, dp);
        dcp->d_namelen = 0;
        ext2_dcacherem(dcp, dp);
    } else {
        // dir cache is invalid, need to lookup again
        IULOCK(dp);
        
        OSIncrementAtomic((SInt32*)&lookcacheinval);
        error = ext2_lookupi(ITOV(dp), NULL, cnp, context);
        
        IXLOCK(dp);
        
        if (0 == error || EJUSTRETURN == error) {
            error = 0;
        } else {
            dp->i_flag &= ~IN_LOOK;
            IWAKE(dp, flag, flag, IN_LOOKWAIT);
            IULOCK(dp);
            ext2_trace_return(error);
        }
    }
   
   int isdx = is_dx(dp);
   offset = (off_t)dp->i_offset;
   IULOCK(dp);
   
   /* Check for indexed dir */
   if (isdx) {
      struct dentry dentry, dparent = {NULL, {NULL, 0}, dp};
      
      dentry.d_parent = &dparent;
      dentry.d_name.name = cnp->cn_nameptr;
      dentry.d_name.len = cnp->cn_namelen;
      dentry.d_inode = NULL;
      
      // XXX ext3_dx* is not lock safe ATM
      IXLOCK(dp); // XXX
      bp = ext3_dx_find_entry(&dentry, &ep, &error);
      IULOCK(dp); // XXX
      
      if (!bp) {
         dp->i_flag &= ~IN_LOOK;
         IWAKE(dp, flag, flag, IN_LOOKWAIT);
         return -(error); /* Linux uses -ve errors */
      }
      error = 0;
   } else {
      error = ext2_blkatoff(vdp, offset, (char **)&ep, &bp);
   }
	IXLOCK(dp);
    if (!error) {
        ep->inode = cpu_to_le32(ip->i_number);
        if (EXT2_HAS_INCOMPAT_FEATURE(ip->i_e2fs,
            EXT2_FEATURE_INCOMPAT_FILETYPE))
            ep->file_type = DTTOFT(IFTODT(ip->i_mode));
        else
            ep->file_type = EXT2_FT_UNKNOWN;
        dp->i_flag |= IN_CHANGE | IN_UPDATE | IN_DX_UPDATE;
        ext2_checkdir_locked(vdp);
    }
    
    dp->i_flag &= ~IN_LOOK;
    IWAKE(dp, flag, flag, IN_LOOKWAIT);
	IULOCK(dp);
    if (!error)
        error = BUF_WRITE(bp);
	ext2_trace_return (error);
}

/*
 * Check if a directory is empty or not.
 * Inode supplied must be locked.
 *
 * Using a struct dirtemplate here is not precisely
 * what we want, but better than using a struct direct.
 *
 * NB: does not handle corrupted directories.
 */
int
ext2_dirempty(struct inode *ip,
			  ino_t parentino,
			  struct ucred *cred)
{
	off_t off;
	struct dirtemplate dbuf;
	struct ext2_dir_entry_2 *dp = (struct ext2_dir_entry_2 *)&dbuf;
	int error, count, namlen;
		 
#define	MINDIRSIZ (sizeof (struct dirtemplate) / 2)

	for (off = 0; off < ip->i_size; off += le16_to_cpu(dp->rec_len)) {
		IULOCK(ip);
        error = vn_rdwr(UIO_READ, ITOV(ip), (caddr_t)dp, MINDIRSIZ,
		    off, UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK, cred,
          &count, (struct proc *)0);
        IXLOCK(ip);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (error || count != 0)
			return (0);
		/* avoid infinite loops */
		if (dp->rec_len == 0)
			return (0);
		/* skip empty entries */
		if (dp->inode == 0)
			continue;
		/* accept only "." and ".." */
		namlen = dp->name_len;
		if (namlen > 2)
			return (0);
		if (dp->name[0] != '.')
			return (0);
		/*
		 * At this point namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (namlen == 1)
			continue;
		if (dp->name[1] == '.' && le32_to_cpu(dp->inode) == parentino)
			continue;
		return (0);
	}
	return (1);
}

/*
 * Check if source directory is in the path of the target directory.
 * Target is supplied locked, source is unlocked.
 * The target is always vput before returning.
 */
int
ext2_checkpath_nolock(struct inode *source,
					  struct inode *target,
					  evalloc_args_t *vallocp)
{
	vnode_t vp;
    vfs_context_t context = vallocp->va_vctx;
	int error, rootino, namlen;
	struct dirtemplate dirbuf;
    u_int32_t dotdot_ino;
    int put = 0;

	assert(NULL != context);
    
    vp = ITOV(target);
	if (target->i_number == source->i_number) {
		error = EEXIST;
		goto out;
	}
	rootino = ROOTINO;
	error = 0;
	if (target->i_number == rootino)
		goto out;

	for (;;) {
		if (vnode_vtype(vp) != VDIR) {
			error = ENOTDIR;
			break;
		}
        error = vn_rdwr(UIO_READ, vp, (caddr_t)&dirbuf,
			sizeof (struct dirtemplate), (off_t)0, UIO_SYSSPACE,
			IO_NODELOCKED | IO_NOMACCHECK, vfs_context_ucred(context), 
            (int *)0, (proc_t)0);
		if (error != 0)
			break;
		namlen = dirbuf.dotdot_type;	/* like ufs little-endian */
		if (namlen != 2 ||
		    dirbuf.dotdot_name[0] != '.' ||
		    dirbuf.dotdot_name[1] != '.') {
			error = ENOTDIR;
			break;
		}
        dotdot_ino = le32_to_cpu(dirbuf.dotdot_ino);
		if (dotdot_ino == source->i_number) {
			error = EINVAL;
			break;
		}
		if (dotdot_ino == rootino)
			break;
        if (put)
            vnode_put(vp);
        vallocp->va_ino = dotdot_ino;
        if ((error = ext2fs_vfsops.vfs_vget(vnode_mount(vp), dotdot_ino, &vp, context)) != 0)
		{
			vp = NULL;
			break;
		}
        put = 1;
	}

out:
	if (error == ENOTDIR)
		ext2_debug("ext2_checkpath: .. not a directory\n");
	if (vp != NULL && put)
		vnode_put(vp);
	return (error);
}

int
ext2_search_dirblock(struct inode *ip, void *data, int *foundp,
    const char *name, int namelen, int *entryoffsetinblockp,
    doff_t *offp, doff_t *prevoffp, doff_t *endusefulp,
    struct ext2fs_searchslot *ssp)
{
	struct vnode *vdp;
	struct ext2_dir_entry_2 *ep, *top;
	uint32_t bsize = ip->i_e2fs->s_blocksize;
	int offset = *entryoffsetinblockp;
	int namlen;

	vdp = ITOV(ip);

	ep = (struct ext2_dir_entry_2 *)((char *)data + offset);
	top = (struct ext2_dir_entry_2 *)((char *)data + bsize);
	while (ep < top) {
		/*
		 * Full validation checks are slow, so we only check
		 * enough to insure forward progress through the
		 * directory. Complete checks can be run by setting
		 * "vfs.e2fs.dirchk" to be true.
		 */
		if (ep->rec_len == 0 ||
		    (dirchk && ext2_dirbadentry_locked(vdp, ep, offset))) {
			int i;

			ext2_dirbad(ip, *offp, "mangled entry");
			i = bsize - (offset & (bsize - 1));
			*offp += i;
			offset += i;
			continue;
		}

		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		 */
		if (ssp->slotstatus != FOUND) {
			int size = ep->rec_len;

			if (ep->inode != 0)
				size -= EXT2_DIR_REC_LEN(ep->name_len);
			else if (ext2_is_dirent_tail(ip, ep))
				size -= sizeof(struct ext2fs_direct_tail);
			if (size > 0) {
				if (size >= ssp->slotneeded) {
					ssp->slotstatus = FOUND;
					ssp->slotoffset = *offp;
					ssp->slotsize = ep->rec_len;
				} else if (ssp->slotstatus == NONE) {
					ssp->slotfreespace += size;
					if (ssp->slotoffset == -1)
						ssp->slotoffset = *offp;
					if (ssp->slotfreespace >= ssp->slotneeded) {
						ssp->slotstatus = COMPACT;
						ssp->slotsize = *offp +
						    ep->rec_len -
						    ssp->slotoffset;
					}
				}
			}
		}
		/*
		 * Check for a name match.
		 */
		if (ep->inode) {
			namlen = ep->name_len;
			if (namlen == namelen &&
			    !bcmp(name, ep->name, (unsigned)namlen)) {
				/*
				 * Save directory entry's inode number and
				 * reclen in ndp->ni_ufs area, and release
				 * directory buffer.
				 */
				*foundp = 1;
				return (0);
			}
		}
		*prevoffp = *offp;
		*offp += ep->rec_len;
		offset += ep->rec_len;
		*entryoffsetinblockp = offset;
		if (ep->rec_len)
			*endusefulp = *offp;
		/*
		 * Get pointer to the next entry.
		 */
		ep = (struct ext2_dir_entry_2 *)((char *)data + offset);
	}

	return (0);
}
