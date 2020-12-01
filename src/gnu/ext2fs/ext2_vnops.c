/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
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
 *	@(#)ufs_vnops.c	8.7 (Berkeley) 2/3/94
 *	@(#)ufs_vnops.c 8.27 (Berkeley) 5/27/95
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_vnops.c,v 1.75 2003/01/04 08:47:19 phk Exp $
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

/* Use vwhatid so we don't conflict with whatid in ext2_readwrite.c. */
static const char vwhatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_vnops.c,v 1.45 2006/10/14 19:39:23 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
//#include <sys/resourcevar.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/kauth.h>
//#include <sys/signalvar.h>
//#include <ufs/ufs/dir.h>
#include <vfs/vfs_support.h>
#include <miscfs/fifofs/fifo.h>
#include <miscfs/specfs/specdev.h>

//#include <xnu/bsd/miscfs/fifofs/fifo.h>
#include "ext2_apple.h"

#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_dinode.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_dir.h>

#ifndef ISWHITEOUT
#define ISWHITEOUT 0x20000
#endif

#ifdef EXT_KNOTE
#define vnop_kqfilter_args vnop_kqfilt_add_args
// Unsupported KPI
#define vnop_kqfilter_desc vnop_kqfilt_add_desc
#define vnop_kqfilter VNOP_KQFILT_ADD
#endif

static int ext2_makeinode(int mode, vnode_t, vnode_t *, struct componentname *, vfs_context_t);
static int ext2_vnop_blockmap(struct vnop_blockmap_args *ap);

//static int ext2_advlock(struct vnop_advlock_args *);
static int ext2_chmod(vnode_t, int, kauth_cred_t, proc_t);
static int ext2_chown(struct inode *ip, uid_t, gid_t, kauth_cred_t, proc_t);
static int ext2_close(struct vnop_close_args *);
static int ext2_create(struct vnop_create_args *);
__private_extern__ int ext2_fsync(struct vnop_fsync_args *);
static int ext2_getattr(struct vnop_getattr_args *);
#ifdef EXT_KNOTE
static int ext2_kqfilter(struct vnop_kqfilter_args *ap);
#endif
static int ext2_link(struct vnop_link_args *);
static int ext2_mkdir(struct vnop_mkdir_args *);
static int ext2_mknod(struct vnop_mknod_args *);
static int ext2_open(struct vnop_open_args *);
static int ext2_pathconf(struct vnop_pathconf_args *);
__private_extern__ int ext2_read(struct vnop_read_args *);
static int ext2_readlink(struct vnop_readlink_args *);
static int ext2_remove(struct vnop_remove_args *);
static int ext2_rename(struct vnop_rename_args *);
static int ext2_rmdir(struct vnop_rmdir_args *);
static int ext2_setattr(struct vnop_setattr_args *);
static int ext2_strategy(struct vnop_strategy_args *);
static int ext2_symlink(struct vnop_symlink_args *);
__private_extern__ int ext2_write(struct vnop_write_args *);
static int ext2fifo_close(struct vnop_close_args *);
#ifdef EXT_KNOTE
static int ext2fifo_kqfilter(struct vnop_kqfilter_args *);
#endif
static int ext2fifo_read(struct vnop_read_args *);
static int ext2fifo_write(struct vnop_write_args *);
static int ext2spec_close(struct vnop_close_args *);
static int ext2spec_read(struct vnop_read_args *);
static int ext2spec_write(struct vnop_write_args *);
static int ext2_mnomap(struct vnop_mnomap_args*);
#define ext2_whiteout nop_whiteout
#define ext2_select err_select
#define ext2_exchange_files err_exchange
#define ext2_revoke err_revoke
#define ext2_readdirattr err_readdirattr
#define ext2_searchfs err_searchfs

#ifdef EXT_KNOTE
static int filt_ext2read(struct knote *kn, long hint);
static int filt_ext2write(struct knote *kn, long hint);
static int filt_ext2vnode(struct knote *kn, long hint);
static void filt_ext2detach(struct knote *kn);
#endif

#define vnop_defaultop vn_default_error
#define spec_vnoperate vn_default_error
#define fifo_vnoperate vn_default_error

#define vfs_cache_lookup cache_lookup

extern int (**fifo_vnodeop_p)(void *); /* From kernel */

/* Global vfs data structures for ext2. */
vnop_t **ext2_vnodeop_p;
static struct vnodeopv_entry_desc ext2_vnodeop_entries[] = {
    { &vnop_default_desc,		(vnop_t *) vn_default_error },
    // { &vnop_access_desc,		(vnop_t *) ext2_access }, // handled by kernel for us
    // should be handled by the kernel for us, but vfs_setlocklocal is not an exported KPI
    // and the locking code from panther is a huge mess and relies on access to the proc struct
    { &vnop_advlock_desc,		(vnop_t *) nop_advlock },
    { &vnop_blockmap_desc,		(vnop_t *) ext2_blockmap },
    { &vnop_close_desc,		(vnop_t *) ext2_close },
    { &vnop_create_desc,		(vnop_t *) ext2_create },
    { &vnop_fsync_desc,		(vnop_t *) ext2_fsync },
    { &vnop_getattr_desc,		(vnop_t *) ext2_getattr },
    { &vnop_inactive_desc,		(vnop_t *) ext2_inactive },
    { &vnop_link_desc,		(vnop_t *) ext2_link },
    { &vnop_mkdir_desc,		(vnop_t *) ext2_mkdir },
    { &vnop_mknod_desc,		(vnop_t *) ext2_mknod },
    { &vnop_open_desc,		(vnop_t *) ext2_open },
    { &vnop_pathconf_desc,		(vnop_t *) ext2_pathconf },
#ifdef EXT_KNOTE
    { &vnop_kqfilter_desc,		(vnop_t *) ext2_kqfilter },
#endif
    { &vnop_read_desc,		(vnop_t *) ext2_read },
    { &vnop_readdir_desc,		(vnop_t *) ext2_readdir },
    { &vnop_readlink_desc,		(vnop_t *) ext2_readlink },
    { &vnop_reclaim_desc,		(vnop_t *) ext2_reclaim },
    { &vnop_remove_desc,		(vnop_t *) ext2_remove },
    { &vnop_rename_desc,		(vnop_t *) ext2_rename },
    { &vnop_rmdir_desc,		(vnop_t *) ext2_rmdir },
    { &vnop_setattr_desc,		(vnop_t *) ext2_setattr },
    { &vnop_strategy_desc,		(vnop_t *) ext2_strategy },
    { &vnop_symlink_desc,		(vnop_t *) ext2_symlink },
    { &vnop_write_desc,		(vnop_t *) ext2_write },
    { &vnop_lookup_desc,	(vnop_t *) ext2_lookup },
    { &vnop_pagein_desc,			(vnop_t *) ext2_pagein },
    { &vnop_pageout_desc,		(vnop_t *) ext2_pageout },
    { &vnop_blktooff_desc,		(vnop_t *) ext2_blktooff },
    { &vnop_offtoblk_desc,		(vnop_t *) ext2_offtoblk },
    { &vnop_mmap_desc,       (vnop_t *) ext2_mmap },		/* mmap */
    { &vnop_copyfile_desc, (vnop_t *) err_copyfile },		/* copyfile */
    { &vnop_bwrite_desc, (vnop_t *)vn_bwrite },
    { &vnop_ioctl_desc, (vnop_t *)ext2_ioctl },
    { &vnop_whiteout_desc,	(vnop_t *) ext2_whiteout },
    { &vnop_select_desc,	(vnop_t *) ext2_select },
    { &vnop_exchange_desc,	(vnop_t *) ext2_exchange_files },
    { &vnop_revoke_desc,	(vnop_t *) ext2_revoke },
    { &vnop_mnomap_desc,	(vnop_t *) ext2_mnomap },
    { &vnop_readdirattr_desc,	(vnop_t *) ext2_readdirattr },
    { &vnop_searchfs_desc,	(vnop_t *) ext2_searchfs },
    { NULL, NULL }
};
__private_extern__ struct vnodeopv_desc ext2fs_vnodeop_opv_desc =
	{ &ext2_vnodeop_p, ext2_vnodeop_entries };

vnop_t **ext2_specop_p;
static struct vnodeopv_entry_desc ext2_specop_entries[] = {
    { &vnop_default_desc,		(vnop_t *) vn_default_error },
    //{ &vnop_access_desc,		(vnop_t *) ext2_access }, // handled by kernel for us
    { &vnop_close_desc,		(vnop_t *) ext2spec_close },
    { &vnop_fsync_desc,		(vnop_t *) ext2_fsync },
    { &vnop_getattr_desc,		(vnop_t *) ext2_getattr },
    { &vnop_inactive_desc,		(vnop_t *) ext2_inactive },
    { &vnop_read_desc,		(vnop_t *) ext2spec_read },
    { &vnop_reclaim_desc,		(vnop_t *) ext2_reclaim },
    { &vnop_setattr_desc,		(vnop_t *) ext2_setattr },
    { &vnop_write_desc,		(vnop_t *) ext2spec_write },

    { &vnop_advlock_desc,		(vnop_t *) err_advlock },
    { &vnop_blockmap_desc,		(vnop_t *) err_blockmap /*spec_bmap*/ },
    { &vnop_lookup_desc,	(vnop_t *) spec_lookup },
    { &vnop_create_desc,		(vnop_t *) spec_create },
    { &vnop_link_desc,		(vnop_t *) spec_link },
    { &vnop_mkdir_desc,		(vnop_t *) spec_mkdir },
    { &vnop_mknod_desc,		(vnop_t *) spec_mknod },
    { &vnop_open_desc,		(vnop_t *) spec_open },
    { &vnop_pathconf_desc,		(vnop_t *) spec_pathconf },
    { &vnop_readdir_desc,		(vnop_t *) spec_readdir },
    { &vnop_readlink_desc,		(vnop_t *) spec_readlink },
    { &vnop_remove_desc,		(vnop_t *) spec_remove },
    { &vnop_rename_desc,		(vnop_t *) spec_rename },
    { &vnop_rmdir_desc,		(vnop_t *) spec_rmdir },
    { &vnop_strategy_desc,		(vnop_t *) spec_strategy },
    { &vnop_symlink_desc,		(vnop_t *) spec_symlink },
    { &vnop_pagein_desc,			(vnop_t *) ext2_pagein },
    { &vnop_pageout_desc,		(vnop_t *) ext2_pageout },
    { &vnop_blktooff_desc,		(vnop_t *) ext2_blktooff },
    { &vnop_offtoblk_desc,		(vnop_t *) ext2_offtoblk },
    { &vnop_mmap_desc,       (vnop_t *) spec_mmap },		/* mmap */
    { &vnop_copyfile_desc, (vnop_t *) err_copyfile },		/* copyfile */
    { &vnop_bwrite_desc, (vnop_t *)vn_bwrite },
    { &vnop_ioctl_desc, (vnop_t *)spec_ioctl },
    { NULL, NULL }
};
__private_extern__ struct vnodeopv_desc ext2fs_specop_opv_desc =
	{ &ext2_specop_p, ext2_specop_entries };

vnop_t **ext2_fifoop_p;
static struct vnodeopv_entry_desc ext2_fifoop_entries[] = {
    { &vnop_default_desc,		(vnop_t *) vn_default_error },
    //{ &vnop_access_desc,		(vnop_t *) ext2_access }, // handled by kernel for us
    { &vnop_close_desc,		(vnop_t *) ext2fifo_close },
    { &vnop_fsync_desc,		(vnop_t *) ext2_fsync },
    { &vnop_getattr_desc,		(vnop_t *) ext2_getattr },
    { &vnop_inactive_desc,		(vnop_t *) ext2_inactive },
#ifdef EXT_KNOTE
    { &vnop_kqfilter_desc,		(vnop_t *) ext2fifo_kqfilter },
#endif
    { &vnop_read_desc,		(vnop_t *) ext2fifo_read },
    { &vnop_reclaim_desc,		(vnop_t *) ext2_reclaim },
    { &vnop_setattr_desc,		(vnop_t *) ext2_setattr },
    { &vnop_write_desc,		(vnop_t *) ext2fifo_write },

    { &vnop_advlock_desc,		(vnop_t *) err_advlock },
    { &vnop_blockmap_desc,		(vnop_t *) ext2_vnop_blockmap /*fifo_blockmap*/ },
    { &vnop_lookup_desc,	(vnop_t *) fifo_lookup },
    { &vnop_create_desc,		(vnop_t *) fifo_create },
    { &vnop_link_desc,		(vnop_t *) fifo_link },
    { &vnop_lookup_desc,		(vnop_t *) fifo_lookup },
    { &vnop_mkdir_desc,		(vnop_t *) fifo_mkdir },
    { &vnop_mknod_desc,		(vnop_t *) fifo_mknod },
    { &vnop_open_desc,		(vnop_t *) fifo_open },
    { &vnop_pathconf_desc,		(vnop_t *) fifo_pathconf },
    { &vnop_readdir_desc,		(vnop_t *) fifo_readdir },
    { &vnop_readlink_desc,		(vnop_t *) fifo_readlink },
    { &vnop_remove_desc,		(vnop_t *) fifo_remove },
    { &vnop_rename_desc,		(vnop_t *) fifo_rename },
    { &vnop_rmdir_desc,		(vnop_t *) fifo_rmdir },
    { &vnop_strategy_desc,		(vnop_t *) err_strategy },
    { &vnop_pagein_desc,			(vnop_t *) ext2_pagein },
    { &vnop_pageout_desc,		(vnop_t *) ext2_pageout },
    { &vnop_blktooff_desc,		(vnop_t *) ext2_blktooff },
    { &vnop_offtoblk_desc,		(vnop_t *) ext2_offtoblk },
    { &vnop_mmap_desc,       (vnop_t *) fifo_mmap },		/* mmap */
    { &vnop_copyfile_desc, (vnop_t *) err_copyfile },		/* copyfile */
    { &vnop_bwrite_desc, (vnop_t *)vn_bwrite },
    { &vnop_ioctl_desc, (vnop_t *)fifo_ioctl },
    { NULL, NULL }
};
__private_extern__ struct vnodeopv_desc ext2fs_fifoop_opv_desc =
	{ &ext2_fifoop_p, ext2_fifoop_entries };

#include <gnu/ext2fs/ext2_readwrite.c>

union _qcvt {
	int64_t qcvt;
	int32_t val[2];
};
#define SETHIGH(q, h) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_HIGHWORD] = (h); \
	(q) = tmp.qcvt; \
}
#define SETLOW(q, l) { \
	union _qcvt tmp; \
	tmp.qcvt = (q); \
	tmp.val[_QUAD_LOWWORD] = (l); \
	(q) = tmp.qcvt; \
}

/*
 * A virgin directory (no blushing please).
 * Note that the type and namlen fields are reversed relative to ext2.
 * Also, we don't use `struct odirtemplate', since it would just cause
 * endianness problems.
 */
#define DIRBLKSIZ 512
static const struct dirtemplate mastertemplate = {
	0, 12, 1, EXT2_FT_DIR, ".",
	0, DIRBLKSIZ - 12, 2, EXT2_FT_DIR, ".."
};
static const struct dirtemplate omastertemplate = {
	0, 12, 1, EXT2_FT_UNKNOWN, ".",
	0, DIRBLKSIZ - 12, 2, EXT2_FT_UNKNOWN, ".."
};
#undef DIRBLKSIZ

void
ext2_itimes(vnode_t vp)
{
	struct inode *ip;
	struct timespec ts;

	ip = VTOI(vp);
    IXLOCK(ip);
    
	if (ip->i_e2flags & EXT2_NOATIME_FL)
		ip->i_flag &= ~IN_ACCESS;
	if ((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_UPDATE)) == 0) {
        IULOCK(ip);
		return;
    }
	enum vtype type = vnode_vtype(vp);
    if ((type == VBLK || type == VCHR))
		ip->i_flag |= IN_LAZYMOD;
	else
		ip->i_flag |= IN_MODIFIED;
	
	if (!vfs_isrdonly(vnode_mount(vp))) {
		vfs_timestamp(&ts);
		if (ip->i_flag & IN_ACCESS) {
			ip->i_atime = ts.tv_sec;
			ip->i_atimensec = ts.tv_nsec;
		}
		if (ip->i_flag & IN_UPDATE) {
			ip->i_mtime = ts.tv_sec;
			ip->i_mtimensec = ts.tv_nsec;
			ip->i_modrev++;
		}
		if (ip->i_flag & IN_CHANGE) {
			ip->i_ctime = ts.tv_sec;
			ip->i_ctimensec = ts.tv_nsec;
		}
	}
	
	ip->i_flag &= ~(IN_ACCESS | IN_CHANGE | IN_UPDATE);
    IULOCK(ip);
}

/*
 * Create a regular file
 */
static int
ext2_create(struct vnop_create_args *ap)
	/* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */
{
	int error;
   
	ext2_trace_enter();
	if(ap->a_vap->va_uid != kauth_cred_getuid(vfs_context_ucred(ap->a_context)))
		panic("UID not equal to VFS_CTX UID");
	
	error =
	    ext2_makeinode(MAKEIMODE(ap->a_vap->va_type, ap->a_vap->va_mode),
	    ap->a_dvp, ap->a_vpp, ap->a_cnp, ap->a_context);
	if (error)
		ext2_trace_return(error);
   VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	return (0);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
static int
ext2_open(struct vnop_open_args  *ap)
	/* {
		vnode_t a_vp;
		int  a_mode;
		ucred_ta_cred;
		proc_ta_td;
	} */
{

	ext2_trace_enter();
   /*
	 * Files marked append-only must be opened for appending.
	 */
	int err = 0;
    struct inode *ip = VTOI(ap->a_vp);
    ISLOCK(ip);
    if ((ip->i_flags & APPEND) &&
	    (ap->a_mode & (FWRITE | O_APPEND)) == FWRITE)
		err = EPERM;
    IULOCK(ip);
	return (err);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
static int
ext2_close(struct vnop_close_args *ap)
/* {
		vnode_t a_vp;
		int  a_fflag;
		ucred_ta_cred;
		proc_ta_td;
 } */
{
   vnode_t vp = ap->a_vp;
   
   ext2_trace("inode=%d, vnode=%p\n", (VTOI(vp))->i_number, vp);
   
    if (vnode_isinuse(vp, 1))
      ext2_itimes(vp);
    
    cluster_push(vp, IO_CLOSE);
    return (0);
}

static int
ext2_getattr(
	struct vnop_getattr_args /* {
		vnode_t a_vp;
		struct vattr *a_vap;
		ucred_ta_cred;
		proc_ta_td;
	} */ *ap)
{
	vnode_t vp = ap->a_vp;
	struct vnode_attr *vap = ap->a_vap;
    enum vtype vtype = vnode_vtype(vp);
	ext2_itimes(vp);
	struct inode *ip = VTOI(vp);
    int devBlockSize = ip->i_e2fs->s_d_blocksize;
   
	ext2_trace_enter();
	/*
	 * Copy from inode table
	 */
    ISLOCK(ip);
    
	VATTR_RETURN(vap, va_mode, ip->i_mode & ~IFMT);
    if (0 != ip->i_uid && (vfs_flags(vnode_mount(vp)) & MNT_IGNORE_OWNERSHIP)) {
        /* The Finder needs to see that the perms are correct.
           Otherwise it won't allow access, even though we would. */
        vap->va_mode = DEFFILEMODE;
        if (ip->i_mode & S_IXUSR)
            vap->va_mode |= S_IXUSR|S_IXGRP|S_IXOTH;
    }
	VATTR_RETURN(vap, va_fsid, ip->i_dev);
 	VATTR_RETURN(vap, va_fileid, ip->i_number);
	vap->va_encoding = 0x0112 /* kTextEncodingUnicodeV8_0 */;
	VATTR_IS_SUPPORTED(vap, va_encoding);
 	VATTR_RETURN(vap, va_nlink, ip->i_nlink);
 	VATTR_RETURN(vap, va_uid, ip->i_uid);
 	VATTR_RETURN(vap, va_gid, ip->i_gid);
    if (VBLK == IFTOVT(ip->i_mode) || VCHR == IFTOVT(ip->i_mode))
        VATTR_RETURN(vap, va_rdev, (dev_t)ip->i_rdev);
 	VATTR_RETURN(vap, va_total_size, ip->i_size);
    VATTR_RETURN(vap, va_data_size, ip->i_size);

	vap->va_access_time.tv_sec = ip->i_atime;
	vap->va_access_time.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_atimensec : 0;
 	VATTR_SET_SUPPORTED(vap, va_access_time);
 	vap->va_create_time.tv_sec = ip->i_btime;
	vap->va_create_time.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_btimensec : 0;
 	VATTR_SET_SUPPORTED(vap, va_create_time);
 	vap->va_modify_time.tv_sec = ip->i_mtime;
	vap->va_modify_time.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_mtimensec : 0;
 	VATTR_SET_SUPPORTED(vap, va_modify_time);
 	vap->va_change_time.tv_sec = ip->i_ctime;
	vap->va_change_time.tv_nsec = E2DI_HAS_XTIME(ip) ? ip->i_ctimensec : 0;
 	VATTR_SET_SUPPORTED(vap, va_change_time);
	
	ext2_debug("last accessed %lld.%.9ld", (long long)ip->i_atime, ip->i_atimensec);
	ext2_debug("last modified %lld.%.9ld", (long long)ip->i_mtime, ip->i_mtimensec);
	ext2_debug("last changed %lld.%.9ld", (long long)ip->i_ctime, ip->i_ctimensec);
	ext2_debug("created %lld.%.9ld", (long long)ip->i_ctime, ip->i_ctimensec);
	
 	VATTR_RETURN(vap, va_flags, ip->i_flags);
 	VATTR_RETURN(vap, va_gen, ip->i_gen);
	if (vtype == VBLK)
 		VATTR_RETURN(vap, va_iosize, BLKDEV_IOSIZE);
	else if (vtype == VCHR)
 		VATTR_RETURN(vap, va_iosize, MAXPHYSIO);
	else
 		VATTR_RETURN(vap, va_iosize, vfs_statfs(vnode_mount(vp))->f_iosize);
 	VATTR_RETURN(vap, va_data_alloc, dbtob((u_quad_t)ip->i_blocks, devBlockSize));
 	VATTR_RETURN(vap, va_type, vtype);
 	VATTR_RETURN(vap, va_filerev, ip->i_modrev);
    
    IULOCK(ip);
	return (0);
}

/*
 * Set attribute vnode op. called from several syscalls
 */
static int
ext2_setattr(struct vnop_setattr_args *ap)
	/* {
		vnode_t a_vp;
		struct vattr *a_vap;
		ucred_ta_cred;
		proc_ta_td;
	} */
{
	struct vnode_attr *vap = ap->a_vap;
	vnode_t vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	kauth_cred_t cred = vfs_context_ucred(ap->a_context);
	proc_t p = vfs_context_proc(ap->a_context);
	int error = 0;
    uid_t nuid = VNOVAL;
	gid_t ngid = VNOVAL;
    
    ext2_trace_enter();
    
    int locked = 1;
    IXLOCK(ip);
    
    if (VATTR_IS_ACTIVE(vap, va_flags)) {
		ip->i_flags = vap->va_flags;
		ip->i_flag |= IN_CHANGE;
	}
	VATTR_SET_SUPPORTED(vap, va_flags);
    
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
        error = EPERM;
        goto setattr_exit_with_lock;
    }
	/*
	 * Go through the fields and update if not VNOVAL.
	 */
    
    if (VATTR_IS_ACTIVE(vap, va_uid))
        nuid = vap->va_uid;
    if (VATTR_IS_ACTIVE(vap, va_gid))
        ngid = vap->va_gid;
	if (nuid != (uid_t)VNOVAL || ngid != (gid_t)VNOVAL) {
        if ((error = ext2_chown(ip, nuid, ngid, cred, p)))
			goto setattr_exit_with_lock;
	}
	VATTR_SET_SUPPORTED(vap, va_uid);
	VATTR_SET_SUPPORTED(vap, va_gid);
    
	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		if (VDIR == vnode_vtype(vp)) {
            error = EISDIR;
            goto setattr_exit_with_lock;
        }
        
        IULOCK(ip);
        if ((error = ext2_truncate(vp, vap->va_data_size, vap->va_vaflags & 0xffff, cred, p)) != 0)
			ext2_trace_return(error);
        IXLOCK(ip);
	}
    VATTR_SET_SUPPORTED(vap, va_data_size);
    
	if (VATTR_IS_ACTIVE(vap, va_access_time) || VATTR_IS_ACTIVE(vap, va_modify_time)) {        
        if (VATTR_IS_ACTIVE(vap, va_access_time))
			ip->i_flag |= IN_ACCESS;
		if (VATTR_IS_ACTIVE(vap, va_modify_time))
			ip->i_flag |= IN_CHANGE | IN_UPDATE;
        
        IULOCK(ip);
        ext2_itimes(vp);
        IXLOCK(ip);
        
        if (VATTR_IS_ACTIVE(vap, va_access_time)) {
            ip->i_atime = vap->va_access_time.tv_sec;
            ip->i_atimensec = vap->va_access_time.tv_nsec;
        }
		if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
            ip->i_mtime = vap->va_modify_time.tv_sec;
            ip->i_mtimensec = vap->va_modify_time.tv_nsec;
        }
        
        IULOCK(ip);
		if ((error = ext2_update(vp, 0)))
			ext2_trace_return(error);
        IXLOCK(ip);
	}
	VATTR_SET_SUPPORTED(vap, va_access_time);
	VATTR_SET_SUPPORTED(vap, va_modify_time);
		
    
    if (VATTR_IS_ACTIVE(vap, va_mode)) {
        IULOCK(ip);
        locked = 0;
		error = ext2_chmod(vp, (int)vap->va_mode, cred, p);
	}
	VATTR_SET_SUPPORTED(vap, va_mode);

setattr_exit_with_lock:
    if (locked)
    	IULOCK(ip);
    
	VN_KNOTE(vp, NOTE_ATTRIB);
	ext2_trace_return(error);
}

/*
 * Change the mode on a file.
 * Inode must be locked before calling.
 */
static int
ext2_chmod(
	vnode_t vp,
	int mode,
	kauth_cred_t cred,
	proc_t p)
{
	struct inode *ip = VTOI(vp);
	int super;
   
   ext2_trace_enter();
   
    super = (0 != kauth_cred_getuid(cred)) ? 0 : 1;
    if ((vfs_flags(vnode_mount(vp)) & MNT_IGNORE_OWNERSHIP) && !super)
        return (0);
    
    IXLOCK(ip);
	ip->i_mode &= ~ALLPERMS;
	ip->i_mode |= (mode & ALLPERMS);
	ip->i_flag |= IN_CHANGE;
    IULOCK(ip);
	return (0);
}

/*
 * Perform chown operation on inode ip
 */
static int
ext2_chown(
	struct inode *ip,
	uid_t uid,
	gid_t gid,
	kauth_cred_t cred,
	proc_t p)
{
	uid_t ouid;
	gid_t ogid;
	int super;
   
   ext2_trace_enter();

	super = (0 != kauth_cred_getuid(cred)) ? 0 : 1;
    if ((vfs_flags(vnode_mount(ITOV(ip))) & MNT_IGNORE_OWNERSHIP) && !super)
        return (0);
    
    if (uid == (uid_t)VNOVAL)
		uid = ip->i_uid;
	if (gid == (gid_t)VNOVAL)
		gid = ip->i_gid;
	ogid = ip->i_gid;
	ouid = ip->i_uid;
	ip->i_gid = gid;
	ip->i_uid = uid;
    if (ouid != uid || ogid != gid) {
        ip->i_flag |= IN_CHANGE;
        if (super)
            ip->i_mode &= ~(ISUID | ISGID);
    }
	return (0);
}

/*
 * Synch an open file.
 */
/* ARGSUSED */
__private_extern__ int
ext2_fsync(
	struct vnop_fsync_args /* {
		vnode_t a_vp;
		int a_waitfor;
        vfs_context_t a_context;
	} */ *ap)
{
    ext2_trace_enter();
   
    /*
	 * Write out any clusters.
	 */
	cluster_push(ap->a_vp, 0);

	/*
	 * Flush all dirty buffers associated with a vnode.
	 */
    struct inode *ip = VTOI(ap->a_vp);
    IXLOCK(ip);
	ext2_discard_prealloc(ip);
    IULOCK(ip);
    
    vop_stdfsync(ap);

	ext2_trace_return(ext2_update(ap->a_vp, ap->a_waitfor == MNT_WAIT));
}

/*
 * Mknod vnode call
 */
/* ARGSUSED */
static int
ext2_mknod(
	struct vnop_mknod_args /* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap)
{
	struct vnode_attr *vap = ap->a_vap;
	vnode_t *vpp = ap->a_vpp;
    struct vnode *dvp = ap->a_dvp;
    struct inode *ip;
	int error;
    struct componentname *cnp = ap->a_cnp;
    
    ext2_trace_enter();

	error = ext2_makeinode(MAKEIMODE(vap->va_type, vap->va_mode),
	    dvp, vpp, cnp, ap->a_context);
	if (error)
		ext2_trace_return(error);
    VN_KNOTE(dvp, NOTE_WRITE);
	ip = VTOI(*vpp);
    
    IXLOCK(ip);
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	if (vap->va_rdev != VNOVAL) {
		/*
		 * Want to be able to use this to make badblock
		 * inodes, so don't truncate the dev number.
		 */
		ip->i_rdev = vap->va_rdev;
	}
    IULOCK(ip);
#ifdef notsure // ???
	/*
	 * Remove inode, then reload it through VFS_VGET so it is
	 * checked to see if it is an alias of an existing entry in
	 * the inode cache.
	 */
	vput(*vpp);
	(*vpp)->v_type = VNON;
	ino = ip->i_number;	/* Save this before vgone() invalidates ip. */
	vgone(*vpp);
   #if 0
	error = VFS_VGET(ap->a_dvp->v_mount, ino, LK_EXCLUSIVE, vpp);
	if (error) {
		*vpp = NULL;
		ext2_trace_return(error);
	}
   #else
    /* lookup will reload the inode for us */
    *vpp = NULL;
   #endif
#endif
	return (0);
}

static int
ext2_remove(
	struct vnop_remove_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
		int a_flags;
		vfs_context_t a_context;
	} */ *ap)
{
	struct inode *ip, *dip;
	vnode_t vp = ap->a_vp;
	vnode_t dvp = ap->a_dvp;
	int error = 0;
   
   ext2_trace_enter();

	ip = VTOI(vp);
    ISLOCK(ip);
    error = (ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND));
    IULOCK(ip);
    
    dip = VTOI(dvp);
    if (!error) {
        ISLOCK(dip);
        error = (dip->i_flags & APPEND);
        IULOCK(dip);
    }
    
	if (error) {
		error = EPERM;
		goto remove_out;
	}
   
   if (ap->a_flags & VNODE_REMOVE_NODELETEBUSY) {
		/* Caller requested Carbon delete semantics */
		if (vnode_isinuse(vp, 1)) {
			error = EBUSY;
			goto remove_out;
		}
	}
   
	cache_purge(vp);
    error = ext2_dirremove(dvp, ap->a_cnp, ap->a_context);
	if (error == 0) {
		IXLOCK(ip);
        ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
        IULOCK(ip);
        
        VN_KNOTE(vp, NOTE_DELETE);
        VN_KNOTE(dvp, NOTE_WRITE);
	}

remove_out:
	ext2_trace_return(error);
}

/*
 * link vnode call
 */
static int
ext2_link(
	struct vnop_link_args /* {
		vnode_t a_tdvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
	} */ *ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t tdvp = ap->a_tdvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip;
   //struct proc *p = curproc;
	int error;
   
   ext2_trace_enter();

	ip = VTOI(vp);
    IXLOCK(ip);
	if ((nlink_t)ip->i_nlink >= LINK_MAX) {
      //VOP_ABORTOP(tdvp, cnp);
		IULOCK(ip);
        error = EMLINK;
		goto link_out;
	}
	if (ip->i_flags & (IMMUTABLE | APPEND)) {
      //VOP_ABORTOP(tdvp, cnp);
		IULOCK(ip);
        error = EPERM;
		goto link_out;
	}
	ip->i_nlink++;
	ip->i_flag |= IN_CHANGE;
    IULOCK(ip);
	error = ext2_update(vp, 1);
    IXLOCK(ip);
	if (!error)
		error = ext2_direnter(ip, tdvp, cnp, ap->a_context);
	if (error) {
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
	}
    IULOCK(ip);
   VN_KNOTE(vp, NOTE_LINK);
   VN_KNOTE(tdvp, NOTE_WRITE);

link_out:
	ext2_trace_return(error);
}

/*
 * Rename system call.
 *   See comments in sys/ufs/ufs/ufs_vnops.c
 */
static int
ext2_rename(
	struct vnop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
		vfs_context_t a_context;
	} */ *ap)
{
	vnode_t tvp = ap->a_tvp;
	vnode_t tdvp = ap->a_tdvp;
	vnode_t fvp = ap->a_fvp;
	vnode_t fdvp = ap->a_fdvp;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	struct inode *ip, *xp, *dp;
	struct dirtemplate dirbuf;
	int doingdirectory = 0, oldparent = 0, newparent = 0;
	int error = 0;
	u_char namlen;
    kauth_cred_t cred = vfs_context_ucred(ap->a_context);
   
   ext2_trace_enter();
    
    if (tvp) {
        ip = VTOI(tvp);
        IXLOCK(ip);
        error = (ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND));
        IULOCK(ip);
        
        if (!error) {
            ip = VTOI(tdvp);
            IXLOCK(ip);
            error = (ip->i_flags & APPEND);
            IULOCK(ip);
        }
        if (error) {
            error = EPERM;
            goto abortit;
        }
	}

	/*
	 * Renaming a file to itself has no effect.  The upper layers should
	 * not call us in that case.  Temporarily just warn if they do.
	 */
	if (fvp == tvp) {
		ext2_debug("ext2_rename: fvp == tvp (can't happen)\n");
		error = 0;
		goto abortit;
	}

	dp = VTOI(fdvp);
	ip = VTOI(fvp);
    IXLOCK(ip);
 	if (ip->i_nlink >= LINK_MAX) {
 		IULOCK(ip);
 		error = EMLINK;
 		goto abortit;
 	}
    IXLOCK_WITH_LOCKED_INODE(dp, ip);
	if ((ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))
	    || (dp->i_flags & APPEND)) {
		IULOCK(ip);
        IULOCK(dp);
		error = EPERM;
		goto abortit;
	}
	if ((ip->i_mode & IFMT) == IFDIR) {
		/*
		 * Avoid ".", "..", and aliases of "." for obvious reasons.
		 */
		if ((fcnp->cn_namelen == 1 && fcnp->cn_nameptr[0] == '.') ||
		    dp == ip || ((fcnp->cn_flags | tcnp->cn_flags) & ISDOTDOT) ||
		    (ip->i_flag & IN_RENAME)) {
			IULOCK(ip);
            IULOCK(dp);
			error = EINVAL;
			goto abortit;
		}
		ip->i_flag |= IN_RENAME;
		oldparent = dp->i_number;
		doingdirectory++;
	}
    VN_KNOTE(fdvp, NOTE_WRITE); /* XXX right place? */
	//vrele(fdvp);
    
    IULOCK(dp);
    // ip is still locked

	/*
	 * When the target exists, both the directory
	 * and target vnodes are returned locked.
	 */
	dp = VTOI(tdvp);
	xp = NULL;
	if (tvp)
		xp = VTOI(tvp);

	/*
	 * 1) Bump link count while we're moving stuff
	 *    around.  If we crash somewhere before
	 *    completing our work, the link count
	 *    may be wrong, but correctable.
	 */
	ip->i_nlink++;
	ip->i_flag |= IN_CHANGE;
    IULOCK(ip);
	if ((error = ext2_update(fvp, 1)) != 0) {
		goto bad;
	}
    
    // ip and dp are unlocked

	/*
	 * If ".." must be changed (ie the directory gets a new
	 * parent) then the source directory must not be in the
	 * directory heirarchy above the target, as this would
	 * orphan everything below the source directory. Also
	 * the user must have write permission in the source so
	 * as to be able to change "..". We must repeat the call
	 * to namei, as the parent directory is unlocked by the
	 * call to checkpath().
	 */
	
    // i_number is constant, don't worry about lock
    if (oldparent != dp->i_number)
		newparent = dp->i_number;
	if (doingdirectory && newparent) {
		if (error)	/* write access check above */
			goto bad;
		
		evalloc_args_t vargs;
        vargs.va_vctx = ap->a_context;
        vargs.va_parent = ITOV(dp);
        vargs.va_cnp = tcnp;
        error = ext2_checkpath_nolock(ip, dp, &vargs);
		if (error)
			goto bad;
	}
	/*
	 * 2) If target doesn't exist, link the target
	 *    to the source and unlink the source.
	 *    Otherwise, rewrite the target directory
	 *    entry to reference the source inode and
	 *    expunge the original entry's existence.
	 */
	if (tvp)
        cache_purge(tvp);
    if (xp == NULL) {
		IXLOCK(dp);
        IXLOCK_WITH_LOCKED_INODE(ip, dp);
        if (dp->i_dev != ip->i_dev)
			panic("ext2_rename: EXDEV");
		IULOCK(ip);
        /*
		 * Account for ".." in new directory.
		 * When source and destination have the same
		 * parent we don't fool with the link count.
		 */
		if (doingdirectory && newparent) {
			if ((nlink_t)dp->i_nlink >= LINK_MAX) {
				IULOCK(dp);
                error = EMLINK;
				goto bad;
			}
			dp->i_nlink++;
			dp->i_flag |= IN_CHANGE;
            IULOCK(dp);
			error = ext2_update(tdvp, 1);
			if (error)
				goto bad;
		} else
            IULOCK(dp);
        
        IXLOCK(ip);
		error = ext2_direnter(ip, tdvp, tcnp, ap->a_context);
        IULOCK(ip);
		if (error) {
			if (doingdirectory && newparent) {
				IXLOCK(dp);
                dp->i_nlink--;
				dp->i_flag |= IN_CHANGE;
                IULOCK(dp);
				(void)ext2_update(tdvp, 1);
			}
			goto bad;
		}
        VN_KNOTE(tdvp, NOTE_WRITE);
		//vput(tdvp);
	} else {
		IXLOCK(dp);
        IXLOCK_WITH_LOCKED_INODE(xp, dp);
        if (xp->i_dev != dp->i_dev || xp->i_dev != ip->i_dev)
			panic("ext2_rename: EXDEV");
		/*
		 * Short circuit rename(foo, foo).
		 */
		if (xp->i_number == ip->i_number)
			panic("ext2_rename: same file");
		/*
		 * Target must be empty if a directory and have no links
		 * to it. Also, ensure source and target are compatible
		 * (both directories, or both not directories).
		 */
        IULOCK(dp);
		if ((xp->i_mode & IFMT) == IFDIR) {
            if (! ext2_dirempty(xp, dp->i_number, cred) || 
			    xp->i_nlink > 2) {
				IULOCK(xp);
                error = ENOTEMPTY;
				goto bad;
			}
			if (!doingdirectory) {
				IULOCK(xp);
                error = ENOTDIR;
				goto bad;
			}
			cache_purge(tdvp);
		} else if (doingdirectory) {
            IULOCK(xp);
            error = EISDIR;
			goto bad;
		}
        IULOCK(xp);
		error = ext2_dirrewrite_nolock(dp, ip, tcnp, ap->a_context);
		if (error)
			goto bad;
		/*
		 * If the target directory is in the same
		 * directory as the source directory,
		 * decrement the link count on the parent
		 * of the target directory.
		 */
		 if (doingdirectory && !newparent) {
			IXLOCK(dp);
            dp->i_nlink--;
			dp->i_flag |= IN_CHANGE;
            IULOCK(dp);
		}
        VN_KNOTE(tdvp, NOTE_WRITE);
		//vput(tdvp);
		/*
		 * Adjust the link count of the target to
		 * reflect the dirrewrite above.  If this is
		 * a directory it is empty and there are
		 * no links to it, so we can squash the inode and
		 * any space associated with it.  We disallowed
		 * renaming over top of a directory with links to
		 * it above, as the remaining link would point to
		 * a directory without "." or ".." entries.
		 */
		IXLOCK(xp);
        xp->i_nlink--;
		if (doingdirectory) {
			if (--xp->i_nlink != 0)
				panic("ext2_rename: linked directory");
			IULOCK(xp);
            error = ext2_truncate(tvp, (off_t)0, IO_SYNC,
			    cred, vfs_context_proc(ap->a_context));
            IXLOCK(xp);
		}
		xp->i_flag |= IN_CHANGE;
        IULOCK(xp);
        VN_KNOTE(tvp, NOTE_DELETE);
		//vput(tvp);
		xp = NULL;
	}
    
    /*
     * 3) Unlink the source
     */
    
    xp = VTOI(fvp);
    dp = VTOI(fdvp);
	/*
	 * Ensure that the directory entry still exists and has not
	 * changed while the new name has been entered. If the source is
	 * a file then the entry may have been unlinked or renamed. In
	 * either case there is no further work to be done. If the source
	 * is a directory then it cannot have been rmdir'ed; its link
	 * count of three would cause a rmdir to fail with ENOTEMPTY.
	 * The IN_RENAME flag ensures that it cannot be moved by another
	 * rename.
	 */
	if (xp != ip) {
		if (doingdirectory)
			panic("ext2_rename: lost dir entry");
	} else {
		/*
		 * If the source is a directory with a
		 * new parent, the link count of the old
		 * parent directory must be decremented
		 * and ".." set to point to the new parent.
		 */
		if (doingdirectory && newparent) {
			IXLOCK(dp);
            dp->i_nlink--;
			dp->i_flag |= IN_CHANGE;
            IULOCK(dp);
			error = vn_rdwr(UIO_READ, fvp, (caddr_t)&dirbuf,
				sizeof (struct dirtemplate), (off_t)0,
				UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK,
				cred, (int *)0, (struct proc *)0);
			if (error == 0) {
				/* Like ufs little-endian: */
				namlen = dirbuf.dotdot_type;
				if (namlen != 2 ||
				    dirbuf.dotdot_name[0] != '.' ||
				    dirbuf.dotdot_name[1] != '.') {
					ext2_dirbad(xp, (doff_t)12,
					    "ext2_rename: mangled dir");
				} else {
					dirbuf.dotdot_ino = cpu_to_le32(newparent);
					(void) vn_rdwr(UIO_WRITE, fvp,
					    (caddr_t)&dirbuf,
					    sizeof (struct dirtemplate),
					    (off_t)0, UIO_SYSSPACE,
					    IO_NODELOCKED | IO_SYNC |
					    IO_NOMACCHECK, cred,
                   (int *)0, (struct proc *)0);
					cache_purge(fdvp);
				}
			}
		}
		cache_purge(fvp);
        error = ext2_dirremove(fdvp, fcnp, ap->a_context);
		IXLOCK(xp);
        if (!error) {
			xp->i_nlink--;
			xp->i_flag |= IN_CHANGE;
		}
		xp->i_flag &= ~IN_RENAME;
        IULOCK(xp);
	}
    VN_KNOTE(ap->a_fvp, NOTE_RENAME); 
	ext2_trace_return(error);

bad:
	IXLOCK(ip);
    if (doingdirectory)
		ip->i_flag &= ~IN_RENAME;
	//if (vn_lock(fvp, LK_EXCLUSIVE, p) == 0) {
		ip->i_nlink--;
		ip->i_flag |= IN_CHANGE;
		ip->i_flag &= ~IN_RENAME;
		//vput(fvp);
	//} else
		//vrele(fvp);
    IULOCK(ip);
    
abortit:
	ext2_trace_return(error);
}

/*
 * Mkdir system call
 */
static int
ext2_mkdir(struct vnop_mkdir_args *ap)
	/* {
		struct vnodeop_desc *a_desc;
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vnode_attr *a_vap;
		vfs_context_t a_context;
	} */
{
	vnode_t dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	vnode_t tvp;
	struct dirtemplate dirtemplate;
    const struct dirtemplate *dtp;
	int error, dmode;
    gid_t dgid;
    uid_t duid;
    mode_t imode;
    vfs_context_t context = ap->a_context;
    kauth_cred_t cred = vfs_context_ucred(context);
    
	ext2_trace_enter();
   
	dp = VTOI(dvp);
    ISLOCK(dp);
	if ((nlink_t)dp->i_nlink >= LINK_MAX) {
		IULOCK(dp);
        error = EMLINK;
		goto out;
	}
    
    duid = dp->i_uid;
    dgid = dp->i_gid;
    imode = dp->i_mode;
    IULOCK(dp);
	
    dmode = vap->va_mode & 0777;
	dmode |= IFDIR;
	/*
	 * Must simulate part of ext2_makeinode here to acquire the inode,
	 * but not have it entered in the parent directory. The entry is
	 * made later after writing "." and ".." entries.
	 */
	evalloc_args_t vargs;
    vargs.va_vctx = context;
    vargs.va_parent = dvp;
    vargs.va_cnp = ap->a_cnp;
    error = ext2_valloc(dvp, dmode, &vargs, &tvp);
	if (error)
		goto out;
	
    ip = VTOI(tvp);
    IXLOCK(ip);
	ip->i_gid = dgid;
#ifdef SUIDDIR
	{
		/*
		 * if we are hacking owners here, (only do this where told to)
		 * and we are not giving it TOO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * The new directory also inherits the SUID bit. 
		 * If user's UID and dir UID are the same,
		 * 'give it away' so that the SUID is still forced on.
		 */
		if ( (vfs_flags(vnode_mount(dvp)) & MNT_SUIDDIR) &&
		   (imode & ISUID) && duid) {
			dmode |= ISUID;
			ip->i_uid = duid;
		} else {
			ip->i_uid = cred->cr_uid;
		}
	}
#else
	ip->i_uid = cred->cr_posix.cr_uid;
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = dmode;
	ip->i_nlink = 2;
	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;
    IULOCK(ip);
	if ((error = ext2_update(tvp, 1)))
        goto bad;

	/*
	 * Bump link count in parent directory
	 * to reflect work done below.  Should
	 * be done before reference is created
	 * so reparation is possible if we crash.
	 */
	IXLOCK(dp);
    dp->i_nlink++;
	dp->i_flag |= IN_CHANGE;
    IULOCK(dp);
	if ((error = ext2_update(dvp, 1)))
		goto bad;

    /* Initialize directory with "." and ".." from static template. */
    // i_e2fs && i_number are static, don't worry about lock
	if (EXT2_HAS_INCOMPAT_FEATURE(ip->i_e2fs,
	    EXT2_FEATURE_INCOMPAT_FILETYPE))
		dtp = &mastertemplate;
	else
		dtp = &omastertemplate;
	dirtemplate = *dtp;
    dirtemplate.dot_reclen = cpu_to_le16(dirtemplate.dot_reclen);
	dirtemplate.dot_ino = cpu_to_le32(ip->i_number);
	dirtemplate.dotdot_ino = cpu_to_le32(dp->i_number);
	/* note that in ext2 DIRBLKSIZ == blocksize, not DEV_BSIZE 
	 * so let's just redefine it - for this function only
	 */
#undef  DIRBLKSIZ 
#define DIRBLKSIZ  VTOI(dvp)->i_e2fs->s_blocksize
	dirtemplate.dotdot_reclen = cpu_to_le16(DIRBLKSIZ - 12);
	error = vn_rdwr(UIO_WRITE, tvp, (caddr_t)&dirtemplate,
	    sizeof (dirtemplate), (off_t)0, UIO_SYSSPACE,
	    IO_NODELOCKED | IO_SYNC | IO_NOMACCHECK, cred,
       (int *)0, (struct proc *)0);
	if (error) {
		IXLOCK(dp);
        dp->i_nlink--;
		dp->i_flag |= IN_CHANGE;
        IULOCK(dp);
		goto bad;
	}
    IXLOCK(ip);
	if (DIRBLKSIZ > vfs_statfs(VFSTOEXT2(vnode_mount(dvp))->um_mountp)->f_bsize)
		/* XXX should grow with balloc() */
		panic("ext2_mkdir: blksize");
	else {
		ip->i_size = DIRBLKSIZ;
		ip->i_flag |= IN_CHANGE;
	}

	/* Directory set up, now install its entry in the parent directory. */
	error = ext2_direnter(ip, dvp, cnp, context);
    IULOCK(ip);
	if (error) {
		IXLOCK(dp);
        dp->i_nlink--;
		dp->i_flag |= IN_CHANGE;
        IULOCK(dp);
	}
bad:
	/*
	 * No need to do an explicit VOP_TRUNCATE here, vrele will do this
	 * for us because we set the link count to 0.
	 */
	if (error) {
		IXLOCK(ip);
        ip->i_nlink = 0;
		ip->i_flag |= IN_CHANGE;
        IULOCK(ip);
	} else {
      VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
      *ap->a_vpp = tvp;
   }
out:
	ext2_trace_return(error);
#undef  DIRBLKSIZ
#define DIRBLKSIZ  DEV_BSIZE
}

/*
 * Rmdir system call.
 */
static int
ext2_rmdir(struct vnop_rmdir_args *ap)
	/* {
		vnode_t a_dvp;
		vnode_t a_vp;
		struct componentname *a_cnp;
	} */
{
	vnode_t vp = ap->a_vp;
	vnode_t dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct inode *ip, *dp;
	int error;
    kauth_cred_t cred = vfs_context_ucred(ap->a_context);

	ip = VTOI(vp);
	dp = VTOI(dvp);
   
   ext2_trace_enter();
   
   /*
	 * No rmdir "." please.
	 */
	if (dp == ip) {
		ext2_trace_return(EINVAL);
	}

    // Make sure no one else is in the doomed dir
    IXLOCK(ip);
    while (ip->i_flag & IN_LOOK) {
        ip->i_flag |= IN_LOOKWAIT;
        lookw();
        ISLEEP(ip, flag, NULL);
    }
    ip->i_flag |= IN_LOOK;

	/*
	 * Verify the directory is empty (and valid).
	 * (Rmdir ".." won't be valid since
	 *  ".." will contain a reference to
	 *  the current directory and thus be
	 *  non-empty.)
	 */
	error = 0;
    
	if (ip->i_nlink != 2 || !ext2_dirempty(ip, dp->i_number, cred)) {
		IULOCK(ip);
        error = ENOTEMPTY;
		goto out;
	}
    IXLOCK_WITH_LOCKED_INODE(dp, ip);
	if ((dp->i_flags & APPEND)
	    || (ip->i_flags & (NOUNLINK | IMMUTABLE | APPEND))) {
		IULOCK(ip);
        IULOCK(dp);
        error = EPERM;
		goto out;
	}
    IULOCK(ip);
    IULOCK(dp);
	/*
	 * Delete reference to directory before purging
	 * inode.  If we crash in between, the directory
	 * will be reattached to lost+found,
	 */
	error = ext2_dirremove(dvp, cnp, ap->a_context);
	if (error)
		goto out;
    VN_KNOTE(dvp, NOTE_WRITE | NOTE_LINK);
	IXLOCK(dp);
    dp->i_nlink--;
	dp->i_flag |= IN_CHANGE;
    IULOCK(dp);
	cache_purge(dvp);
    //vput(dvp);
	dvp = NULL;
	/*
	 * Truncate inode.  The only stuff left
	 * in the directory is "." and "..".  The
	 * "." reference is inconsequential since
	 * we're quashing it.  The ".." reference
	 * has already been adjusted above.  We've
	 * removed the "." reference and the reference
	 * in the parent directory, but there may be
	 * other hard links so decrement by 2 and
	 * worry about them later.
	 */
    IXLOCK(ip);
	ip->i_nlink -= 2;
    IULOCK(ip);
	error = ext2_truncate(vp, (off_t)0, IO_SYNC, cred, vfs_context_proc(ap->a_context));
	cache_purge(ITOV(ip));

out:
    IXLOCK(ip);
    ip->i_flag &= ~IN_LOOK;
    IWAKE(ip, flag, flag, IN_LOOKWAIT);
    IULOCK(ip);
    
    VN_KNOTE(vp, NOTE_DELETE);
	//vput(vp);
	ext2_trace_return(error);
}

/*
 * symlink -- make a symbolic link
 */
static int
ext2_symlink(struct vnop_symlink_args *ap)
	/* {
		vnode_t a_dvp;
		vnode_t *a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */
{
	vnode_t vp;
    vnode_t *vpp = ap->a_vpp;
	struct inode *ip;
	int len, error;
   
   ext2_trace_enter();

	error = ext2_makeinode(IFLNK | ap->a_vap->va_mode, ap->a_dvp,
	    vpp, ap->a_cnp, ap->a_context);
	if (error)
		ext2_trace_return(error);
    VN_KNOTE(ap->a_dvp, NOTE_WRITE);
	vp = *vpp;
	len = strlen(ap->a_target);
	if (len < vfs_maxsymlen(vnode_mount(vp))) {
		ip = VTOI(vp);
        IXLOCK(ip);
		bcopy(ap->a_target, (char *)ip->i_shortlink, len);
		ip->i_size = len;
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
        IULOCK(ip);
	} else
		error = vn_rdwr(UIO_WRITE, vp, ap->a_target, len, (off_t)0,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_NOMACCHECK,
		    vfs_context_ucred(ap->a_context), (int *)0, (struct proc *)0);
	ext2_trace_return(error);
}

/*
 * Return target name of a symbolic link
 */
static int
ext2_readlink(struct vnop_readlink_args *ap)
	/* {
		vnode_t a_vp;
		struct uio *a_uio;
		ucred_ta_cred;
	} */
{
	vnode_t vp = ap->a_vp;
	struct inode *ip = VTOI(vp);
	int isize;
   
   ext2_trace_enter();

	ISLOCK(ip);
    isize = ip->i_size;
	if (isize < vfs_maxsymlen(vnode_mount(vp))) {
		uiomove((char *)ip->i_shortlink, isize, ap->a_uio);
		IULOCK(ip);
        return (0);
	}
    IULOCK(ip);
	ext2_trace_return(EXT2_READ(vp, ap->a_uio, 0, ap->a_context));
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 *
 * In order to be able to swap to a file, the ext2_bmaparray() operation may not
 * deadlock on memory.  See ext2_bmap() for details.
 */
static int
ext2_strategy(struct vnop_strategy_args *ap)
	/* {
		buf_t  a_bp;
	} */
{
	struct inode *ip = VTOI(buf_vnode(ap->a_bp));
    ext2_trace_return (buf_strategy(ip->i_devvp, ap));
}

#if 0
/*
 * Print out the contents of an inode.
 */
static int
ext2_print(struct vnop_print_args *ap)
	/* {
		vnode_t a_vp;
	} */
{
	vnode_t vp = ap->a_vp;
	struct inode *ip = VTOI(vp);

	ext2_debug("\text2 ino %lu, on dev %s (%d, %d)", (u_long)ip->i_number,
	    devtoname(ip->i_dev), major(ip->i_dev), minor(ip->i_dev));
	return (0);
}
#endif

#pragma mark SPECIAL DEVICE OPS

/*
 * Read wrapper for special devices.
 */
static int
ext2spec_read(struct vnop_read_args *ap)
	/* {
		vnode_t a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		ucred_ta_cred;
	} */
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;
   
   ext2_trace_enter();

	uio = ap->a_uio;
	resid = uio_resid(uio);
    error = spec_read(ap);
	/*
	 * The inode may have been revoked during the call, so it must not
	 * be accessed blindly here or in the other wrapper functions.
	 */
	ip = VTOI(ap->a_vp);
	if (ip != NULL && (uio_resid(uio) != resid || (error == 0 && resid != 0))) {
        IXLOCK(ip);
		ip->i_flag |= IN_ACCESS;
        IULOCK(ip);
    }
	ext2_trace_return(error);
}

/*
 * Write wrapper for special devices.
 */
static int
ext2spec_write(struct vnop_write_args *ap)
	/* {
		vnode_t a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		ucred_ta_cred;
	} */
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;
   
   ext2_trace_enter();

	uio = ap->a_uio;
	resid = uio_resid(uio);
	error = spec_write(ap);
	ip = VTOI(ap->a_vp);
	if (ip != NULL && (uio_resid(uio) != resid || (error == 0 && resid != 0))) {
        IXLOCK(ip);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
        IULOCK(ip);
    }
	ext2_trace_return(error);
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the inode then do device close.
 */
static int
ext2spec_close(struct vnop_close_args *ap)
	/* {
		vnode_t a_vp;
		int  a_fflag;
		ucred_ta_cred;
		proc_ta_td;
	} */
{
	vnode_t vp = ap->a_vp;
   
   ext2_trace_enter();

	if (vnode_isinuse(vp, 1))
		ext2_itimes(vp);
    ext2_trace_return(spec_close(ap));
}


#pragma mark FIFO OPERATIONS

/*
 * Read wrapper for fifos.
 */
static int
ext2fifo_read(struct vnop_read_args *ap)
	/* {
		vnode_t a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		ucred_ta_cred;
	} */
{
	int error, resid;
    vnode_t vp;
	struct inode *ip;
	struct uio *uio;
   
   ext2_trace_enter();

	uio = ap->a_uio;
	resid = uio_resid(uio);
	error = fifo_read(ap);
    vp = ap->a_vp;
	ip = VTOI(vp);
	if ((vfs_flags(vnode_mount(vp)) & MNT_NOATIME) == 0 && ip != NULL &&
	    (uio_resid(uio) != resid || (error == 0 && resid != 0))) {
		IXLOCK(ip);
        ip->i_flag |= IN_ACCESS;
        IULOCK(ip);
    }
	ext2_trace_return(error);
}

/*
 * Write wrapper for fifos.
 */
static int ext2fifo_write(struct vnop_write_args *ap)
	/* {
		vnode_t a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		ucred_ta_cred;
	} */
{
	int error, resid;
	struct inode *ip;
	struct uio *uio;
   
   ext2_trace_enter();

	uio = ap->a_uio;
	resid = uio_resid(uio);
	error = fifo_write(ap);
	ip = VTOI(ap->a_vp);
	if (ip != NULL && (uio_resid(uio) != resid || (error == 0 && resid != 0))) {
        IXLOCK(ip);
		ip->i_flag |= IN_CHANGE | IN_UPDATE;
        IULOCK(ip);
    }
	ext2_trace_return(error);
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the inode then do device close.
 */
static int
ext2fifo_close(struct vnop_close_args *ap)
	/* {
		vnode_t a_vp;
		int  a_fflag;
		ucred_ta_cred;
		proc_ta_td;
	} */
{
	vnode_t vp = ap->a_vp;
   
   ext2_trace_enter();

	if (vnode_isinuse(vp, 1))
		ext2_itimes(vp);
	ext2_trace_return(fifo_close(ap));
}

#pragma mark EXT NOTE

#ifdef EXT_KNOTE
/*
 * Kqfilter wrapper for fifos.
 *
 * Fall through to ext2 kqfilter routines if needed 
 */
static int
ext2fifo_kqfilter(struct vnop_kqfilter_args *ap)
{
	int error;

#ifdef XXX // ???
	error = VOCALL(fifo_vnodeop_p, VOFFSET(vnop_kqfilter), ap);
	if (error)
#endif
		error = ext2_kqfilter(ap);
	ext2_trace_return(error);
}
#endif

/*
 * Return POSIX pathconf information applicable to ext2 filesystems.
 */
static int
ext2_pathconf(struct vnop_pathconf_args *ap)
	/* {
		vnode_t a_vp;
		int a_name;
		int *a_retval;
	} */
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		if(EXT2_HAS_COMPAT_FEATURE(VTOI(ap->a_vp)->i_e2fs, EXT4_FEATURE_RO_COMPAT_DIR_NLINK))
			*ap->a_retval = LINK_MAX;
		else
			*ap->a_retval = EXT4_LINK_MAX;
		return (0);
	case _PC_NAME_MAX:
		*ap->a_retval = NAME_MAX;
		return (0);
	case _PC_PATH_MAX:
		*ap->a_retval = PATH_MAX;
		return (0);
	case _PC_PIPE_BUF:
		if (vnode_vtype(ap->a_vp) == VDIR || vnode_vtype(ap->a_vp) == VFIFO)
			*ap->a_retval = PIPE_BUF;
		else return EINVAL;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_NO_TRUNC:
		*ap->a_retval = 1;
		return (0);
   case _PC_CASE_SENSITIVE:
      *ap->a_retval = 1;
      return(0);
   case _PC_SYNC_IO:
	   *ap->a_retval = 0;
	   return (0);
   case _PC_FILESIZEBITS:
	   *ap->a_retval = 42;
	   return (0);
   case _PC_SYMLINK_MAX:
	   *ap->a_retval = MAXPATHLEN;
	   return (0);
   case _PC_2_SYMLINKS:
	   *ap->a_retval = 1;
	   return (0);
	default:
		ext2_trace_return(EINVAL);
	}
	/* NOTREACHED */
}

#ifdef notyet
/*
 * Advisory record locking support
 */
static int
ext2_advlock(
	struct vnop_advlock_args /* {
		vnode_t a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap)
{
   /* Cribbed straigt from ufs */
   register struct inode *ip = VTOI(ap->a_vp);
	register struct flock *fl = ap->a_fl;
	register struct ext2lockf *lock;
	off_t start, end;
	int error;
   
   ext2_trace_enter();

	/*
	 * Avoid the common case of unlocking when inode has no locks.
	 */
	if (ip->i_lockf == (struct ext2lockf *)0) {
		if (ap->a_op != F_SETLK) {
			fl->l_type = F_UNLCK;
			return (0);
		}
	}
	/*
	 * Convert the flock structure into a start and end.
	 */
	switch (fl->l_whence) {

	case SEEK_SET:
	case SEEK_CUR:
		/*
		 * Caller is responsible for adding any necessary offset
		 * when SEEK_CUR is used.
		 */
		start = fl->l_start;
		break;

	case SEEK_END:
		start = ip->i_size + fl->l_start;
		break;

	default:
		ext2_trace_return(EINVAL);
	}
	if (start < 0)
		ext2_trace_return(EINVAL);
	if (fl->l_len == 0)
		end = -1;
	else
		end = start + fl->l_len - 1;
	/*
	 * Create the ext2lockf structure
	 */
	MALLOC(lock, struct ext2lockf *, sizeof *lock, M_LOCKF, M_WAITOK);
	lock->lf_start = start;
	lock->lf_end = end;
	lock->lf_id = ap->a_id;
	lock->lf_inode = ip;
	lock->lf_type = fl->l_type;
	lock->lf_next = (struct ext2lockf *)0;
	TAILQ_INIT(&lock->lf_blkhd);
	lock->lf_flags = ap->a_flags;
	/*
	 * Do the requested operation.
	 */
	switch(ap->a_op) {
	case F_SETLK:
		ext2_trace_return(ext2_lf_setlock(lock));

	case F_UNLCK:
		error = ext2_lf_clearlock(lock);
		FREE(lock, M_LOCKF);
		ext2_trace_return(error);

	case F_GETLK:
		error = ext2_lf_getlock(lock, fl);
		FREE(lock, M_LOCKF);
		ext2_trace_return(error);
	
	default:
		_FREE(lock, M_LOCKF);
		ext2_trace_return(EINVAL);
	}
	/* NOTREACHED */
}
#endif

int
ext2_mnomap(struct vnop_mnomap_args *ap)
{
    ext2_trace_enter();
    return (ENOTSUP);
}

/*
 * Initialize the vnode associated with a new inode, handle aliased
 * vnodes. Returns vnode/inode referenced and locked.
 */
int
ext2_vinit(mount_t  mntp, evinit_args_t *args, vnode_t *vpp)
{
	struct vnode_fsparam vfsargs;
    struct inode *ip = args->vi_ip;
    evalloc_args_t *vaargs = args->vi_vallocargs;
	vnode_t vp;
	struct timeval tv;

	*vpp = NULL;
    bzero(&vfsargs, sizeof(vfsargs));
    if (0 == (args->vi_flags & EXT2_VINIT_INO_LCKD))
        ISLOCK(ip);
    vfsargs.vnfs_mp = mntp;
    vfsargs.vnfs_vtype = IFTOVT(ip->i_mode);
    vfsargs.vnfs_str = "ext2 vnode";
    vfsargs.vnfs_dvp = vaargs->va_parent;
    vfsargs.vnfs_fsnode = ip;
    vfsargs.vnfs_vops = args->vi_vnops;
    vfsargs.vnfs_markroot = (ROOTINO == ip->i_number);
#if 0
    // file inodes used for FS meta-data - don't believe this is ever the case in ext2/3
    vfsargs.vnfs_marksystem = VREG == vfsargs.vnfs_vtype;
#endif
    vfsargs.vnfs_filesize = ip->i_size;
    vfsargs.vnfs_cnp = vaargs->va_cnp;
    if (!vfsargs.vnfs_dvp || (vfsargs.vnfs_cnp && !(vfsargs.vnfs_cnp->cn_flags & MAKEENTRY)))
        vfsargs.vnfs_flags = VNFS_NOCACHE;
    if (0 == (args->vi_flags & EXT2_VINIT_INO_LCKD))
        IULOCK(ip);
    
	if (VNON == vfsargs.vnfs_vtype){
		ext2_debug("got mode %d", ip->i_mode);
        return (ENOENT);
	}
    switch(vfsargs.vnfs_vtype) {
	case VCHR:
	case VBLK:
		vfsargs.vnfs_vops = args->vi_specops;
        vfsargs.vnfs_rdev = ip->i_rdev;
		break;
	case VFIFO:
		vfsargs.vnfs_vops = args->vi_fifoops;
		break;
	default:
		break;

	}
    /*
	 * Initialize modrev times
	 */
	getmicrouptime(&tv);
    if (0 == (args->vi_flags & EXT2_VINIT_INO_LCKD))
        IXLOCK(ip);
	SETHIGH(ip->i_modrev, tv.tv_sec);
	SETLOW(ip->i_modrev, tv.tv_usec * 4294);
    IULOCK(ip); // We have to unlock no matter the state of EXT2_VINIT_INO_LCKD
    
    int error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsargs, &vp);
    
    if (args->vi_flags & EXT2_VINIT_INO_LCKD)
        IXLOCK(ip);
    
    if (error)
        return (error);
    
    vnode_addfsref(vp);
    vnode_settag(vp, VT_OTHER);
    *vpp = vp;
	return (0);
}

/*
 * Allocate a new inode.
 */
static int
ext2_makeinode(int mode, vnode_t dvp, vnode_t *vpp,
			   struct componentname *cnp, vfs_context_t context)
{
	struct inode *ip, *pdir;
	vnode_t tvp;
	int error;
    kauth_cred_t cred = vfs_context_ucred(context);

//#ifdef DIAGNOSTIC
//	if ((cnp->cn_flags & 0x00000400/*HASBUF*/) == 0)
//		panic("ext2fs_makeinode: no name");
//#endif
	
	pdir = VTOI(dvp);
	*vpp = NULL;
	if ((mode & IFMT) == 0)
		mode |= IFREG;

	evalloc_args_t vargs;
    vargs.va_vctx = context;
    vargs.va_parent = dvp;
    vargs.va_cnp = cnp;
    error = ext2_valloc(dvp, mode, &vargs, &tvp);
	if (error) {
		ext2_trace_return(error);
	}
    
    uid_t nuid;
    gid_t ngid;
    mode_t dirmode;
    
    ISLOCK(pdir);
    nuid = pdir->i_uid;
    ngid = pdir->i_gid;
    dirmode = pdir->i_mode;
    IULOCK(pdir);
    
	ip = VTOI(tvp);
    IXLOCK(ip);
	ip->i_gid = ngid;
#ifdef SUIDDIR
	{
		/*
		 * if we are
		 * not the owner of the directory,
		 * and we are hacking owners here, (only do this where told to)
		 * and we are not giving it TOO root, (would subvert quotas)
		 * then go ahead and give it to the other user.
		 * Note that this drops off the execute bits for security.
		 */
		if ( (vfs_flags(vnode_mount(dvp)) & MNT_SUIDDIR) &&
		     (dirmode & ISUID) &&
		     (nuid != cred->cr_uid) && nuid) {
			ip->i_uid = nuid;
			mode &= ~07111;
		} else {
         if ((mode & IFMT) == IFLNK)
            ip->i_uid = nuid;
         else
            ip->i_uid = cred->cr_uid;
		}
	}
#else
   if ((mode & IFMT) == IFLNK)
		ip->i_uid = nuid;
	else
      ip->i_uid = cred->cr_posix.cr_uid;
#endif
	ip->i_flag |= IN_ACCESS | IN_CHANGE | IN_UPDATE;
	ip->i_mode = mode;
	ip->i_nlink = 1;
    int isgroupmember = 0;
    (void)kauth_cred_ismember_gid(cred, ip->i_gid, &isgroupmember);
	if ((ip->i_mode & ISGID) && !isgroupmember && vfs_context_suser(context))
		ip->i_mode &= ~ISGID;

	if (cnp->cn_flags & ISWHITEOUT)
		ip->i_flags |= UF_OPAQUE;
    
    IULOCK(ip);

	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	error = ext2_update(tvp, 1);
	if (error)
		goto bad;
	IXLOCK(ip);
    error = ext2_direnter(ip, dvp, cnp, context);
    IULOCK(ip);
	if (error)
		goto bad;

	*vpp = tvp;
	return (0);

bad:
	/*
	 * Write error occurred trying to update the inode
	 * or the directory so must deallocate the inode.
	 */
    IXLOCK(ip);
	ip->i_nlink = 0;
	ip->i_flag |= IN_CHANGE;
    IULOCK(ip);
	//vput(tvp);
	ext2_trace_return(error);
}

static int ext2_vnop_blockmap(struct vnop_blockmap_args *ap)
/*
	struct vnop_blockmap_args {
		struct vnode	*a_vp;
		off_t		a_foffset;
		size_t		a_size;
		daddr64_t	*a_bpn;
		size_t		*a_run;
		void		*a_poff;
		int		a_flags;
*/
{
	ext2_debug("+vnop_blockmap\n");
	return (ENOTSUP);
}


#ifdef EXT_KNOTE
static struct filterops ext2read_filtops = 
	{ 1, NULL, filt_ext2detach, filt_ext2read };
static struct filterops ext2write_filtops = 
	{ 1, NULL, filt_ext2detach, filt_ext2write };
static struct filterops ext2vnode_filtops = 
	{ 1, NULL, filt_ext2detach, filt_ext2vnode };

static int
ext2_kqfilter(
	struct vnop_kqfilter_args /* {
		vnode_t a_vp;
		struct knote *a_kn;
	} */ *ap)
{
	vnode_t vp = ap->a_vp;
	struct knote *kn = ap->a_kn;

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &ext2read_filtops;
		break;
	case EVFILT_WRITE:
		kn->kn_fop = &ext2write_filtops;
		break;
	case EVFILT_VNODE:
		kn->kn_fop = &ext2vnode_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = (caddr_t)vp;
    kn->kn_hookid = vnode_vid(vp);

	struct inode *ip = VTOI(vp);
    IXLOCK(ip);
    KNOTE_ATTACH(&ip->i_knotes, kn);
    IULOCK(ip);
	return (0);
}

static void
filt_ext2detach(struct knote *kn)
{
   vnode_t vp = (vnode_t )kn->kn_hook;
   
    if (vnode_getwithvid(vp, kn->kn_hookid))
        return;

   struct inode *ip = VTOI(vp);
   IXLOCK(ip);
   (void)KNOTE_DETACH(&ip->i_knotes, kn);
   IULOCK(ip);
   vnode_put(vp);
}

/*ARGSUSED*/
static int
filt_ext2read(struct knote *kn, long hint)
{
	vnode_t vp = (vnode_t )kn->kn_hook;
	struct inode *ip;
    int dropvp = 0;

	if (hint == 0)  {
		if ((vnode_getwithvid(vp, kn->kn_hookid) != 0)) {
			hint = NOTE_REVOKE;
		} else 
			dropvp = 1;
	}
    
	if (hint == NOTE_REVOKE) {
        /*
         * filesystem is gone, so set the EOF flag and schedule 
         * the knote for deletion.
         */
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}
    
    ip = VTOI(vp);
    IXLOCK(ip);
    if (kn->kn_flags & EV_POLL) {
		kn->kn_data = 1;
	} else {
#ifdef notyet // file struct is private, with no (useful) accessors
        kn->kn_data = ip->i_size - kn->kn_fp->f_fglob->fg_offset;
#endif
        kn->kn_data = 1;
	}
    
    int result = (kn->kn_data != 0);
    IULOCK(ip);

	if  (dropvp)
		vnode_put(vp);
    
    return (result);
}

/*ARGSUSED*/
static int
filt_ext2write(struct knote *kn, long hint)
{
	vnode_t vp = (vnode_t )kn->kn_hook;
    struct inode *ip;
	
	if (hint == 0)  {
		if ((vnode_getwithvid(vp, kn->kn_hookid) != 0)) {
			hint = NOTE_REVOKE;
		} else 
			vnode_put(vp);
	}
    
	if (hint == NOTE_REVOKE) {
		/*
		 * filesystem is gone, so set the EOF flag and schedule 
		 * the knote for deletion.
		 */
		kn->kn_data = 0;
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		return (1);
	}
    ip = VTOI(vp);
    IXLOCK(ip);
	kn->kn_data = 0;
    IULOCK(ip);
	return (1);
}

static int
filt_ext2vnode(struct knote *kn, long hint)
{
    vnode_t vp = (vnode_t )kn->kn_hook;
    struct inode *ip;
    
	if (hint == 0)  {
		if ((vnode_getwithvid(vp, kn->kn_hookid) != 0)) {
			hint = NOTE_REVOKE;
		} else
			vnode_put(vp);
	}
    
    ip = VTOI(vp);
    if (ip) IXLOCK(ip);
	if (kn->kn_sfflags & hint)
		kn->kn_fflags |= hint;
	if (hint == NOTE_REVOKE) {
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
        if (ip) IULOCK(ip);
		return (1);
	}
	
    int result = (kn->kn_fflags != 0);
    if (ip) IULOCK(ip);
    
	return (result);
}
#endif //EXT_KNOTE
