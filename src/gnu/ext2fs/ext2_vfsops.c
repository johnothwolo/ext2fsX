/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*	
 * Copyright (c) 1989, 1991, 1993, 1994	
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
 *	@(#)ffs_vfsops.c	8.8 (Berkeley) 4/18/94
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_vfsops.c,v 1.101 2003/01/01 18:48:52 schweikh Exp $
 */
/*
* Copyright 2003-2006 Brian Bergstrand.
*
* Redistribution and use in source and binary forms, with or without modification, 
* are permitted provided that the following conditions are met:
*
* 1.	Redistributions of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer.
* 2.	Redistributions in binary form must reproduce the above copyright notice, this list of
*     conditions and the following disclaimer in the documentation and/or other materials provided
*     with the distribution.
* 3.	The name of the author may not be used to endorse or promote products derived from this
*     software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
* AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
* THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_vfsops.c,v 1.53 2006/08/27 17:29:27 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

#include <string.h>
//#include <machine/spl.h>

// Exported by BSD KPI, but not in headers
extern int spec_fsync(struct vnop_fsync_args*); // fsync

#include "ext2_apple.h"

#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/inode.h>

#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <ext2_byteorder.h>
#include <gnu/ext2fs/ext2_vnops.h>


__private_extern__ int ext2_fsync(struct vnop_fsync_args *);

/* Ext2 lock group */
__private_extern__ lck_grp_t *ext2_lck_grp = NULL;

/* VOPS */
static int ext2_fhtovp(mount_t, int, unsigned char *, vnode_t *, vfs_context_t);
static int ext2_flushfiles(mount_t mp, int flags, vfs_context_t);
static int ext2_init(struct vfsconf *);
static int ext2_mount(mount_t, vnode_t, user_addr_t, vfs_context_t);
static int ext2_mountfs(vnode_t, mount_t, vfs_context_t);
static int ext2_reload(mount_t mountp, vfs_context_t);
static int ext2_root(mount_t, vnode_t *vpp, vfs_context_t);
static int ext2_sbupdate(vfs_context_t, struct ext2mount *, int);
static int ext2_getattrfs(mount_t, struct vfs_attr *, vfs_context_t);
static int ext2_setattrfs(mount_t, struct vfs_attr *, vfs_context_t);
static int ext2_sync(mount_t, int, vfs_context_t);
static int ext2_uninit(struct vfsconf *);
static int ext2_unmount(mount_t, int, vfs_context_t);
static int ext2_vget(mount_t, ino64_t, vnode_t *, vfs_context_t);
static int ext2_vptofh(vnode_t, int*, unsigned char *, vfs_context_t);

static int ext2_sysctl(int *, u_int, user_addr_t, size_t *, user_addr_t, size_t, vfs_context_t);
static int ext2_quotactl(mount_t , int, uid_t, caddr_t, vfs_context_t);
static int vfs_stdstart(mount_t , int, vfs_context_t);

/* These assume SBSIZE == 1024 in fs.h */
#ifdef SBLOCK
#undef SBLOCK
#endif
#define SBLOCK ( 1024 / devBlockSize )
#ifdef SBSIZE
#undef SBSIZE
#endif
#define SBSIZE ( devBlockSize <= 1024 ? 1024 : devBlockSize )
#define SBOFF ( devBlockSize <= 1024 ? 0 : 1024 )

struct ext2_iter_cargs {
	vfs_context_t ca_vctx;
	int ca_wait;
	int ca_err;
};

__private_extern__ struct vfsops ext2fs_vfsops = {
	.vfs_mount 		= ext2_mount,
	.vfs_start 		= vfs_stdstart,
	.vfs_unmount 	= ext2_unmount,
	.vfs_root 		= ext2_root,		/* root inode via vget */
	.vfs_quotactl 	= ext2_quotactl,
	.vfs_getattr 	= ext2_getattrfs,
	.vfs_sync 		= ext2_sync,
	.vfs_vget 		= ext2_vget,
	.vfs_fhtovp 	= ext2_fhtovp,
	.vfs_vptofh 	= ext2_vptofh,
	.vfs_init 		= ext2_init,
	.vfs_sysctl 	= ext2_sysctl,
	.vfs_setattr 	= ext2_setattrfs,
};

#define bsd_malloc _MALLOC
#define bsd_free FREE

static int ext2fs_inode_hash_lock;

static int	ext2_check_sb_compat(struct ext2_super_block *es, dev_t dev,
		    int ronly);
static int	compute_sb_data(vnode_t devvp, vfs_context_t context,
		    struct ext2_super_block * es, struct ext2_sb_info * fs);

/* Ext2 supported attributes */
#define EXT2_ATTR_CMN_NATIVE ( \
   ATTR_CMN_DEVID | ATTR_CMN_FSID | ATTR_CMN_OBJTYPE | ATTR_CMN_OBJTAG | \
   ATTR_CMN_OBJID | ATTR_CMN_OBJPERMANENTID | ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | \
   ATTR_CMN_ACCTIME | ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK | \
   ATTR_CMN_USERACCESS )
#define EXT2_ATTR_CMN_SUPPORTED EXT2_ATTR_CMN_NATIVE
#define EXT2_ATTR_VOL_NATIVE ( \
   ATTR_VOL_FSTYPE | ATTR_VOL_SIGNATURE | ATTR_VOL_SIZE | \
   ATTR_VOL_SPACEFREE | ATTR_VOL_SPACEAVAIL | ATTR_VOL_IOBLOCKSIZE | \
   ATTR_VOL_OBJCOUNT |  ATTR_VOL_FILECOUNT | ATTR_VOL_DIRCOUNT | \
   ATTR_VOL_MAXOBJCOUNT | ATTR_VOL_MOUNTPOINT | ATTR_VOL_NAME | ATTR_VOL_MOUNTPOINT | \
   ATTR_VOL_MOUNTFLAGS | ATTR_VOL_MOUNTEDDEVICE | ATTR_VOL_CAPABILITIES | \
   ATTR_VOL_ATTRIBUTES | ATTR_VOL_INFO )
/*  ATTR_VOL_FILECOUNT | ATTR_VOL_DIRCOUNT aren't really native, but close enough */
#define EXT2_ATTR_VOL_SUPPORTED EXT2_ATTR_VOL_NATIVE
#define EXT2_ATTR_DIR_NATIVE ( \
   ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT | ATTR_DIR_MOUNTSTATUS )
#define EXT2_ATTR_DIR_SUPPORTED EXT2_ATTR_DIR_NATIVE
#define EXT2_ATTR_FILE_NATIVE ( \
   ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE | ATTR_FILE_ALLOCSIZE | \
   ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_CLUMPSIZE | ATTR_FILE_DEVTYPE )
#define EXT2_ATTR_FILE_SUPPORTED EXT2_ATTR_FILE_NATIVE
#define EXT2_ATTR_FORK_NATIVE 0
#define EXT2_ATTR_FORK_SUPPORTED 0

#define EXT2_ATTR_CMN_SETTABLE 0
#define EXT2_ATTR_VOL_SETTABLE ATTR_VOL_NAME
#define EXT2_ATTR_DIR_SETTABLE 0
#define EXT2_ATTR_FILE_SETTABLE 0
#define EXT2_ATTR_FORK_SETTABLE 0

#ifdef DEBUG
#define INDEXRO
#endif

#ifdef notyet
static int ext2_mountroot(void);

/*
 * Called by main() when ext2fs is going to be mounted as root.
 *
 * Name is updated by mount(8) after booting.
 */
#define ROOTNAME	"root_device"

static int
ext2_mountroot()
{
	struct ext2_sb_info *fs;
	mount_t mp;
	struct thread *td = curthread;
	struct ext2mount *ump;
	u_int size;
	int error;
	
	if ((error = bdevvp(rootdev, &rootvp))) {
		printf("EXT2-fs: ext2_mountroot: can't find rootvp\n");
		ext2_trace_return(error);
	}
	mp = bsd_malloc((u_long)sizeof(struct mount), M_MOUNT, M_WAITOK);
	bzero((char *)mp, (u_long)sizeof(struct mount));
	TAILQ_INIT(&mp->mnt_nvnodelist);
	TAILQ_INIT(&mp->mnt_reservedvnlist);
	mp->mnt_op = &ext2fs_vfsops;
	mp->mnt_flag = MNT_RDONLY;
	if (error = ext2_mountfs(rootvp, mp, td)) {
		bsd_free(mp, M_MOUNT);
		ext2_trace_return(error);
	}
	if (error = vfs_lock(mp)) {
		(void)ext2_unmount(mp, 0, td);
		bsd_free(mp, M_MOUNT);
		ext2_trace_return(error);
	}
	TAILQ_INSERT_HEAD(&mountlist, mp, mnt_list);
	mp->mnt_flag |= MNT_ROOTFS;
	mp->mnt_vnodecovered = NULLVP;
	ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
	bzero(fs->fs_fsmnt, sizeof(fs->fs_fsmnt));
	fs->fs_fsmnt[0] = '/';
	bcopy((caddr_t)fs->fs_fsmnt, (caddr_t)mp->mnt_stat.f_mntonname,
	    MNAMELEN);
	(void) copystr(ROOTNAME, mp->mnt_stat.f_mntfromname, MNAMELEN - 1,
	    &size);
	bzero(mp->mnt_stat.f_mntfromname + size, MNAMELEN - size);
	(void)ext2_statfs(mp, &mp->mnt_stat, td);
	vfs_unlock(mp);
	inittodr(fs->s_es->s_wtime);		/* this helps to set the time */
	return (0);
}
#endif

/*
 * VFS Operations.
 *
 * mount system call
 */
static int
ext2_mount(mount_t mp,
		   vnode_t devvp,
		   user_addr_t data,
		   vfs_context_t context)
{
	struct ext2mount *ump = 0;
	struct ext2_sb_info *fs;
    struct ext2_args args;
    char *fspec, *path;
	size_t size;
	int error, flags;
	//mode_t accessmode;
	//proc_t p = vfs_context_proc(context);
   
	// Advisory locking is handled at the VFS layer -- of course it's not a KPI -- WTF?!!
	#ifdef advlocks
	vfs_setlocklocal(mp);
	#endif
   
   if ((error = copyin(CAST_USER_ADDR_T(data), &args, sizeof(struct ext2_args))) != 0)
		ext2_trace_return(error);
   
   //export = &args.export;
   
   size = 0;
   fspec = (vfs_statfs(mp))->f_mntfromname;

	/*
	 * If updating, check whether changing from read-only to
	 * read/write; if there is no device name, that's all we do.
	 */
	if (vfs_isupdate(mp)) {
		ump = VFSTOEXT2(mp);
		fs = ump->um_e2fs;
		error = 0;
		if (fs->s_rd_only == 0 && vfs_isrdonly(mp)) {
			flags = WRITECLOSE;
			if (vfs_isforce(mp))
				flags |= FORCECLOSE;
			if (vfs_busy(mp, 0))
				ext2_trace_return(EBUSY);
			error = ext2_flushfiles(mp, flags, context);
			vfs_unbusy(mp);
			if (!error && fs->s_wasvalid) {
				fs->s_es->s_state =
               cpu_to_le16(le16_to_cpu(fs->s_es->s_state) | EXT2_VALID_FS);
				ext2_sbupdate(context, ump, MNT_WAIT);
			}
			if (error)
				ext2_trace_return(error);
			fs->s_rd_only = 1;
		}
		if (!error && vfs_isreload(mp))
			error = ext2_reload(mp, context);
		if (error)
			ext2_trace_return(error);
		if (devvp != ump->um_devvp)
			ext2_trace_return(EINVAL);
		if (ext2_check_sb_compat(fs->s_es, vnode_specrdev(devvp),
		    0 == vfs_iswriteupgrade(mp)) != 0)
			ext2_trace_return(EPERM);
		if (fs->s_rd_only && vfs_iswriteupgrade(mp)) {
			if ((le16_to_cpu(fs->s_es->s_state) & EXT2_VALID_FS) == 0 ||
			    (le16_to_cpu(fs->s_es->s_state) & EXT2_ERROR_FS)) {
				if (vfs_isforce(mp)) {
					printf(
"EXT2-fs WARNING: %s was not properly dismounted\n",
					    fs->fs_fsmnt);
				} else {
					printf(
"EXT2-fs WARNING: R/W mount of %s denied.  Filesystem is not clean - run fsck\n",
					    fs->fs_fsmnt);
					ext2_trace_return(EPERM);
				}
			}
			fs->s_es->s_state =
            cpu_to_le16(le16_to_cpu(fs->s_es->s_state) & ~EXT2_VALID_FS);
			ext2_sbupdate(context, ump, MNT_WAIT);
			fs->s_rd_only = 0;
		}
		if (fspec == NULL) {
         #if 0
         struct export_args apple_exp;
         struct netexport apple_netexp;
         
         ext2_trace_return(vfs_export(mp, &apple_netexp, export));
         #endif
         ext2_trace_return(EINVAL);
		}
		if (NULL == devvp)
			return (0);
	}
	if (NULL == devvp)
		return (ENODEV);

	path = (vfs_statfs(mp))->f_mntonname;

    if (0 == vfs_isupdate(mp)) {
		error = ext2_mountfs(devvp, mp, context);
	} else {
		if (devvp != ump->um_devvp)
			error = EINVAL;	/* needs translation */
	}
	if (error) {
		ext2_trace_return(error);
	}
    /* ump is setup by ext2_mountfs */
    ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
    
	strncpy(fs->fs_fsmnt, (vfs_statfs(mp))->f_mntonname, MAXMNTLEN-1);
	size = strlen(fs->fs_fsmnt);
	bzero(fs->fs_fsmnt + size, MAXMNTLEN - size);
	fs->s_mount_opt = args.e2_mnt_flags;
	#ifdef INDEXRO
	fs->s_mount_opt &= ~EXT2_MNT_INDEX;
	#endif
	
	if ((vfs_flags(mp) & MNT_IGNORE_OWNERSHIP)
		 && 0 == (vfs_flags(mp) & MNT_ROOTFS) && args.e2_uid > 0) {
		fs->s_uid_noperm = args.e2_uid;
		fs->s_gid_noperm = args.e2_gid;
	} else {
		vfs_clearflags(mp, MNT_IGNORE_OWNERSHIP);
	}
	
	// xxx - Unsupported KPI, but we are already linking against that anyway.
	// This should not be needed, but it seems there may be a kernel bug, as
	// I've seen a divide by zero panic on Intel because statfs was not initialized
	// before a lookup occurred in the FS.
	(void)vfs_update_vfsstat(mp, context, 0);
	assert(vfs_statfs(mp)->f_iosize > 0);
	return (0);
}

/*
 * checks that the data in the descriptor blocks make sense
 * this is taken from ext2/super.c
 */
static int ext2_check_descriptors (struct ext2_sb_info * sb)
{
	int i;
	int desc_block = 0;
	unsigned long block = le32_to_cpu(sb->s_es->s_first_data_block);
	struct ext2_group_desc * gdp = NULL;
	
	sb->s_dircount = 0;
	
	/* ext2_debug ("Checking group descriptors"); */
	for (i = 0; i < sb->s_groups_count; i++) {
		/* examine next descriptor block */
		if ((i % EXT2_DESC_PER_BLOCK(sb)) == 0)
		gdp = (struct ext2_group_desc *)buf_dataptr(sb->s_group_desc[desc_block++]);
		if (le32_to_cpu(gdp->bg_block_bitmap) < block ||
			le32_to_cpu(gdp->bg_block_bitmap) >= block + EXT2_BLOCKS_PER_GROUP(sb))
		{
			printf ("EXT2-fs: ext2_check_descriptors: "
				"Block bitmap for group %d"
				" not in group (block %lu)!\n",
				i, (unsigned long) le32_to_cpu(gdp->bg_block_bitmap));
			return 0;
		}
		if (le32_to_cpu(gdp->bg_inode_bitmap) < block ||
			le32_to_cpu(gdp->bg_inode_bitmap) >= block + EXT2_BLOCKS_PER_GROUP(sb))
		{
			printf ("EXT2-fs: ext2_check_descriptors: "
				"Inode bitmap for group %d"
				" not in group (block %lu)!\n",
				i, (unsigned long) le32_to_cpu(gdp->bg_inode_bitmap));
			return 0;
		}
		if (le32_to_cpu(gdp->bg_inode_table) < block ||
			le32_to_cpu(gdp->bg_inode_table) + sb->s_itb_per_group >=
			block + EXT2_BLOCKS_PER_GROUP(sb))
		{
			printf ("EXT2-fs: ext2_check_descriptors: "
				"Inode table for group %d"
				" not in group (block %lu)!\n",
				i, (unsigned long) le32_to_cpu(gdp->bg_inode_table));
			return 0;
		}
		block += EXT2_BLOCKS_PER_GROUP(sb);
		/* Compute initial directory count */
		sb->s_dircount += le16_to_cpu(gdp->bg_used_dirs_count);
		gdp++;
	}
	return 1;
}

static int
ext2_check_sb_compat(
	struct ext2_super_block *es,
	dev_t dev,
	int ronly)
{

	if (le16_to_cpu(es->s_magic) != EXT2_SUPER_MAGIC) {
		printf("EXT2-fs: %s: wrong magic number %#x (expected %#x)\n",
		    devtoname(dev), le16_to_cpu(es->s_magic), EXT2_SUPER_MAGIC);
		ext2_trace_return(1);
	}
	if (le32_to_cpu(es->s_rev_level) > EXT2_GOOD_OLD_REV) {
		if (le32_to_cpu(es->s_feature_incompat) & ~EXT2_FEATURE_INCOMPAT_SUPP) {
			printf(
"EXT2-fs WARNING: mount of %s denied due to unsupported optional features (%08X)\n",
			    devtoname(dev),
             (le32_to_cpu(es->s_feature_incompat) & ~EXT2_FEATURE_INCOMPAT_SUPP));
			ext2_trace_return(1);
		}
		if (!ronly &&
		    (le32_to_cpu(es->s_feature_ro_compat) & ~EXT2_FEATURE_RO_COMPAT_SUPP)) {
			printf(
"EXT2-fs WARNING: R/W mount of %s denied due to unsupported optional features (%08X)\n",
			    devtoname(dev),
             (le32_to_cpu(es->s_feature_ro_compat) & ~EXT2_FEATURE_RO_COMPAT_SUPP));
			ext2_trace_return(1);
		}
	}
	return (0);
}

/*
 * this computes the fields of the  ext2_sb_info structure from the
 * data in the ext2_super_block structure read in
 * ext2_sb_info is kept in cpu byte order except for group info
 * which is kept in LE (on disk) order.
 */
static int compute_sb_data(devvp, context, es, fs)
	vnode_t devvp;
	vfs_context_t context;
	struct ext2_super_block * es;
	struct ext2_sb_info * fs;
{
    int db_count, error;
    int i, j;
    int logic_sb_block = 1;	/* XXX for now */
    /* fs->s_d_blocksize has not been set yet */
    u_int32_t devBlockSize=0;
    
    EVOP_DEVBLOCKSIZE(devvp, &devBlockSize, context);

#if 1
#define V(v)  
#else
#define V(v)  printf(#v"= %d\n", fs->v);
#endif

    fs->s_blocksize = EXT2_MIN_BLOCK_SIZE << le32_to_cpu(es->s_log_block_size); 
    V(s_blocksize)
    fs->s_bshift = EXT2_MIN_BLOCK_LOG_SIZE + le32_to_cpu(es->s_log_block_size);
    V(s_bshift)
    fs->s_fsbtodb = le32_to_cpu(es->s_log_block_size) + 1;
    V(s_fsbtodb)
    fs->s_qbmask = fs->s_blocksize - 1;
    V(s_bmask)
    fs->s_blocksize_bits = EXT2_BLOCK_SIZE_BITS(es);
    V(s_blocksize_bits)
    fs->s_frag_size = EXT2_MIN_FRAG_SIZE << le32_to_cpu(es->s_log_frag_size);
    V(s_frag_size)
    if (fs->s_frag_size)
	fs->s_frags_per_block = fs->s_blocksize / fs->s_frag_size;
    V(s_frags_per_block)
    fs->s_blocks_per_group = le32_to_cpu(es->s_blocks_per_group);
    V(s_blocks_per_group)
    fs->s_frags_per_group = le32_to_cpu(es->s_frags_per_group);
    V(s_frags_per_group)
    fs->s_inodes_per_group = le32_to_cpu(es->s_inodes_per_group);
    V(s_inodes_per_group)
    fs->s_inodes_per_block = fs->s_blocksize / EXT2_INODE_SIZE;
    V(s_inodes_per_block)
    fs->s_itb_per_group = fs->s_inodes_per_group /fs->s_inodes_per_block;
    V(s_itb_per_group)
    fs->s_desc_per_block = fs->s_blocksize / sizeof (struct ext2_group_desc);
    V(s_desc_per_block)
    /* s_resuid / s_resgid ? */
    fs->s_groups_count = (le32_to_cpu(es->s_blocks_count) -
			  le32_to_cpu(es->s_first_data_block) +
			  EXT2_BLOCKS_PER_GROUP(fs) - 1) /
			 EXT2_BLOCKS_PER_GROUP(fs);
    V(s_groups_count)
    db_count = (fs->s_groups_count + EXT2_DESC_PER_BLOCK(fs) - 1) /
	EXT2_DESC_PER_BLOCK(fs);
    fs->s_db_per_group = db_count;
    V(s_db_per_group)
   #if (EXT2_FEATURE_RO_COMPAT_SUPP & EXT2_FEATURE_RO_COMPAT_LARGE_FILE)
      u_int64_t addrs_per_block = fs->s_blocksize / sizeof(u32);
	  fs->s_maxfilesize = (EXT2_NDIR_BLOCKS*(u_int64_t)fs->s_blocksize) +
		(addrs_per_block*(u_int64_t)fs->s_blocksize) +
		((addrs_per_block*addrs_per_block)*(u_int64_t)fs->s_blocksize) +
		((addrs_per_block*addrs_per_block*addrs_per_block)*(u_int64_t)fs->s_blocksize);
   #else
      fs->s_maxfilesize = 0x7FFFFFFFLL;
   #endif
	for (i = 0; i < 4; ++i)
		fs->s_hash_seed[i] = le32_to_cpu(es->s_hash_seed[i]);
	fs->s_def_hash_version = es->s_def_hash_version;

    fs->s_group_desc = bsd_malloc(db_count * sizeof (buf_t ),
		M_EXT2MNT, M_WAITOK);

    /* adjust logic_sb_block */
    if(fs->s_blocksize > SBSIZE)
	/* Godmar thinks: if the blocksize is greater than 1024, then
	   the superblock is logically part of block zero. 
	 */
        logic_sb_block = 0;
    
    for (i = 0; i < db_count; i++) {
      error = buf_meta_bread(devvp , (daddr64_t)fsbtodb(fs, logic_sb_block + i + 1), 
         fs->s_blocksize, NOCRED, &fs->s_group_desc[i]);
      if(error) {
            for (j = 0; j < i; j++)
               ULCK_BUF(fs->s_group_desc[j]);
            bsd_free(fs->s_group_desc, M_EXT2MNT);
            printf("EXT2-fs: unable to read group descriptors (%d)\n", error);
            return EIO;
      }
      LCK_BUF(fs->s_group_desc[i]);
    }
    if(!ext2_check_descriptors(fs)) {
	    for (j = 0; j < db_count; j++)
		    ULCK_BUF(fs->s_group_desc[j]);
	    bsd_free(fs->s_group_desc, M_EXT2MNT);
	    printf("EXT2-fs: (ext2_check_descriptors failure) "
		   "unable to read group descriptors\n");
	    return EIO;
    }

    for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
	    fs->s_inode_bitmap_number[i] = 0;
	    fs->s_inode_bitmap[i] = NULL;
	    fs->s_block_bitmap_number[i] = 0;
	    fs->s_block_bitmap[i] = NULL;
    }
    fs->s_loaded_inode_bitmaps = 0;
    fs->s_loaded_block_bitmaps = 0;
    return 0;
}

/*
 * Reload all incore data for a filesystem (used after running fsck on
 * the root filesystem and finding things to fix). The filesystem must
 * be mounted read-only.
 *
 * Things to do to update the mount:
 *	1) invalidate all cached meta-data.
 *	2) re-read superblock from disk.
 *	3) re-read summary information from disk.
 *	4) invalidate all inactive vnodes.
 *	5) invalidate all cached file data.
 *	6) re-read inode data for all active vnodes.
 */
static int
ext2_reload_callback(vnode_t vp, void *cargs)
{
	struct ext2_iter_cargs *args = (struct ext2_iter_cargs*)cargs;
	struct inode *ip;
	struct ext2_sb_info *fs;
	struct ext2mount *ump;
	buf_t bp;
	int err;
	
	/*
	 * Step 4: invalidate all inactive vnodes.
	 */
	if (vnode_recycle(vp))
		return (VNODE_RETURNED);
	/*
	 * Step 5: invalidate all cached file data.
	 */
	//vnode_lock(vp);
	if (buf_invalidateblks(vp, BUF_WRITE_DATA, 0, 0))
		panic("ext2_reload: dirty2");
	/*
	 * Step 6: re-read inode data for all active vnodes.
	 */
	ip = VTOI(vp);
	ump = VFSTOEXT2(vnode_mount(vp));
	fs = ump->um_e2fs;
	err =
		buf_bread (ump->um_devvp, (daddr64_t)fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
			(int)fs->s_blocksize, NOCRED, &bp);
	if (err) {
		//vnode_unlock(vp);
		args->ca_err = err;
		return (VNODE_RETURNED_DONE);
	}
	ext2_ei2i((struct ext2_inode *) ((char *)buf_dataptr +
		EXT2_INODE_SIZE * ino_to_fsbo(fs, ip->i_number)), ip);
	buf_brelse(bp);
	//vnode_unlock(vp);
	return (VNODE_RETURNED);
}

static int
ext2_reload(mount_t mountp,
			vfs_context_t context)
{
	struct ext2_iter_cargs cargs;
	vnode_t devvp;
	buf_t bp;
	struct ext2_super_block * es;
	struct ext2_sb_info *fs;
	int error;
    u_int32_t devBlockSize=0;

	if (vfs_isrdonly(mountp))
		ext2_trace_return(EINVAL);
	/*
	 * Step 1: invalidate all cached meta-data.
	 */
	devvp = VFSTOEXT2(mountp)->um_devvp;
	if (buf_invalidateblks(devvp, BUF_WRITE_DATA, 0, 0))
		panic("ext2_reload: dirty1");
	/*
	 * Step 2: re-read superblock from disk.
	 * constants have been adjusted for ext2
	 */
   /* Get the current block size */
   EVOP_DEVBLOCKSIZE(devvp, &devBlockSize, context);
   
	if ((error = buf_meta_bread(devvp, (daddr64_t)SBLOCK, SBSIZE, NOCRED, &bp)) != 0)
		ext2_trace_return(error);
	es = (struct ext2_super_block *)(buf_dataptr(bp)+SBOFF);
	if (ext2_check_sb_compat(es, vnode_specrdev(devvp), 0) != 0) {
		buf_brelse(bp);
		ext2_trace_return(EIO);		/* XXX needs translation */
	}
	fs = VFSTOEXT2(mountp)->um_e2fs;
	bcopy(es, fs->s_es, sizeof(struct ext2_super_block));

	if((error = compute_sb_data(devvp, context, es, fs)) != 0) {
		buf_brelse(bp);
		return error;
	}
#ifdef UNKLAR
	if (fs->fs_sbsize < SBSIZE)
		buf_markinvalid(bp);
#endif
	buf_brelse(bp);
	
	cargs.ca_vctx = context;
	cargs.ca_wait = cargs.ca_err = 0;
	if ((error = vnode_iterate(mountp, VNODE_RELOAD|VNODE_NOLOCK_INTERNAL,
		ext2_reload_callback, &cargs)))
		ext2_trace_return(error);
	if (cargs.ca_err)
		ext2_trace_return(cargs.ca_err);
	return (0);
}

/*
 * Common code for mount and mountroot
 */
static int
ext2_mountfs(vnode_t devvp,
			 mount_t mp,
			 vfs_context_t context)
{
	struct timeval tv;
    struct ext2mount *ump;
	buf_t bp;
	struct ext2_sb_info *fs;
	struct ext2_super_block * es;
	dev_t dev = vnode_specrdev(devvp);
	int error;
	int ronly;
    u_int32_t devBlockSize=0;
   
    getmicrotime(&tv); /* Curent time */

	/*
	 * Disallow multiple mounts of the same device.
	 * Disallow mounting of a device that is currently in use
	 * (except for root, which might share swap device for miniroot).
	 * Flush out any old buffers remaining from a previous use.
	 */
	if ((error = buf_invalidateblks(devvp, BUF_WRITE_DATA, 0, 0)) != 0)
		ext2_trace_return(error);
#ifdef READONLY
/* turn on this to force it to be read-only */
	vfs_setflags(mp, MNT_RDONLY);
#endif

	ronly = vfs_isrdonly(mp);
   /* Set the block size to 512. Things just seem to royally screw 
      up otherwise.
    */
   devBlockSize = 512;
   
   struct vnop_ioctl_args ioctlargs;
   ioctlargs.a_desc = &vnop_ioctl_desc;
   ioctlargs.a_vp = devvp;
   ioctlargs.a_command = DKIOCSETBLOCKSIZE;
   ioctlargs.a_data = (caddr_t)&devBlockSize;
   ioctlargs.a_fflag = FWRITE;
   ioctlargs.a_context = context;
   if (spec_ioctl(&ioctlargs)) {
      ext2_trace_return(ENXIO);
   }
   /* force specfs to re-read the new size */
   set_fsblocksize(devvp);
   
   /* cache the IO attributes */
	if ((error = vfs_init_io_attributes(devvp, mp))) {
		printf("EXT2-fs: ext2_mountfs: vfs_init_io_attributes returned %d\n",
			error);
		ext2_trace_return(error);
	}
   
   /* EVOP_DEVBLOCKSIZE(devvp, &devBlockSize, context); */
   
	bp = NULL;
	ump = NULL;
	#if defined(DIAGNOSTIC)
	printf("EXT2-fs: reading superblock from block %u, with size %u and offset %u\n",
		SBLOCK, SBSIZE, SBOFF);
	#endif
	if ((error = buf_meta_bread(devvp, (daddr64_t)SBLOCK, SBSIZE, NOCRED, &bp)) != 0)
		goto out;
	es = (struct ext2_super_block *)(buf_dataptr(bp)+SBOFF);
#ifdef INDEXRO
#warning dx readonly active
if (le16_to_cpu(es->s_magic) == EXT2_SUPER_MAGIC && (es->s_feature_compat & cpu_to_le32(EXT3_FEATURE_COMPAT_DIR_INDEX))) {
	// haven't tested dx write support yet, and with the inode locking there's likely problems
	vfs_setflags(mp, MNT_RDONLY);
	ronly = 1;
	printf("EXT2-fs: dir_index feature detected - write support disabled\n");
}
#endif
	if (ext2_check_sb_compat(es, dev, ronly) != 0) {
		error = EINVAL;		/* XXX needs translation */
		goto out;
	}
	if ((le16_to_cpu(es->s_state) & EXT2_VALID_FS) == 0 ||
	    (le16_to_cpu(es->s_state) & EXT2_ERROR_FS)) {
		if (ronly || vfs_isforce(mp)) {
			printf(
"EXT2-fs WARNING: Filesystem was not properly dismounted\n");
		} else {
			printf(
"EXT2-fs WARNING: R/W mount denied.  Filesystem is not clean - run fsck\n");
			error = EPERM;
			goto out;
		}
	}
   if ((int16_t)le16_to_cpu(es->s_max_mnt_count) >= 0 &&
		 le16_to_cpu(es->s_mnt_count) >= (u_int16_t)le16_to_cpu(es->s_max_mnt_count))
		printf ("EXT2-fs:EXT2-fs WARNING: maximal mount count reached, "
			"running fsck is recommended\n");
	else if (le32_to_cpu(es->s_checkinterval) &&
		(le32_to_cpu(es->s_lastcheck) + le32_to_cpu(es->s_checkinterval) <= tv.tv_sec))
		printf ("EXT2-fs WARNING: checktime reached, "
			"running fsck is recommended\n");
   
   /* UFS does this, so I assume we have the same shortcoming. */
   /*
	 * Buffer cache does not handle multiple pages in a buf when
	 * invalidating incore buffer in pageout. There are no locks 
	 * in the pageout path.  So there is a danger of loosing data when
	 * block allocation happens at the same time a pageout of buddy
	 * page occurs. incore() returns buf with both
	 * pages, this leads vnode-pageout to incorrectly flush of entire. 
	 * buf. Till the low level ffs code is modified to deal with these
	 * do not mount any FS more than 4K size.
	 */
   if ((EXT2_MIN_BLOCK_SIZE << le32_to_cpu(es->s_log_block_size)) > PAGE_SIZE) {
      error = ENOTSUP;
      goto out;
   }
   
	ump = bsd_malloc(sizeof *ump, M_EXT2MNT, M_WAITOK);
	bzero((caddr_t)ump, sizeof *ump);
	/* I don't know whether this is the right strategy. Note that
	   we dynamically allocate both an ext2_sb_info and an ext2_super_block
	   while Linux keeps the super block in a locked buffer
	 */
	ump->um_e2fs = bsd_malloc(sizeof(struct ext2_sb_info), 
		M_EXT2MNT, M_WAITOK);
	ump->um_e2fs->s_es = bsd_malloc(sizeof(struct ext2_super_block), 
		M_EXT2MNT, M_WAITOK);
	bcopy(es, ump->um_e2fs->s_es, (u_int)sizeof(struct ext2_super_block));
	if ((error = compute_sb_data(devvp, context, ump->um_e2fs->s_es, ump->um_e2fs)))
		goto out;
	/*
	 * We don't free the group descriptors allocated by compute_sb_data()
	 * until ext2_unmount().  This is OK since the mount will succeed.
	 */
	buf_brelse(bp);
	bp = NULL;
	fs = ump->um_e2fs;
    /* Init the lock */
	fs->s_lock = lck_mtx_alloc_init(EXT2_LCK_GRP, LCK_ATTR_NULL);
    assert(fs->s_lock != NULL);
   
	fs->s_rd_only = ronly;	/* ronly is set according to mnt_flags */
	/* if the fs is not mounted read-only, make sure the super block is 
	   always written back on a sync()
	 */
	fs->s_wasvalid = le16_to_cpu(fs->s_es->s_state) & EXT2_VALID_FS ? 1 : 0;
	if (ronly == 0) {
		fs->s_dirt = 1;		/* mark it modified */
		fs->s_es->s_state =
         cpu_to_le16(le16_to_cpu(fs->s_es->s_state) & ~EXT2_VALID_FS);	/* set fs invalid */
	}
	vfs_setfsprivate(mp, ump);
    vfs_getnewfsid(mp);
	vfs_setmaxsymlen(mp, EXT2_MAXSYMLINKLEN);
	vfs_setflags(mp, MNT_LOCAL); /* XXX Is this already set by vfs_fsadd? */
	ump->um_mountp = mp;
	ump->um_dev = dev;
	ump->um_devvp = devvp;
	/* setting those two parameters allowed us to use
	   ufs_bmap w/o changes !
	*/
	ump->um_nindir = EXT2_ADDR_PER_BLOCK(fs);
	ump->um_bptrtodb = le32_to_cpu(fs->s_es->s_log_block_size) + 1;
	ump->um_seqinc = EXT2_FRAGS_PER_BLOCK(fs);
    vnode_setmountedon(devvp);
   
   /* set device block size */
   fs->s_d_blocksize = devBlockSize;
   
   fs->s_es->s_mtime = cpu_to_le32(tv.tv_sec);
   if (!(int16_t)fs->s_es->s_max_mnt_count)
		fs->s_es->s_max_mnt_count = (int16_t)cpu_to_le16(EXT2_DFL_MAX_MNT_COUNT);
   fs->s_es->s_mnt_count = cpu_to_le16(le16_to_cpu(fs->s_es->s_mnt_count) + 1);
   /* last mount point */
   bzero(&fs->s_es->s_last_mounted[0], sizeof(fs->s_es->s_last_mounted));
   bcopy((caddr_t)(vfs_statfs(mp))->f_mntonname,
		(caddr_t)&fs->s_es->s_last_mounted[0],
		min(sizeof(fs->s_es->s_last_mounted), MNAMELEN));
	if (ronly == 0) 
		ext2_sbupdate(context, ump, MNT_WAIT);
	return (0);
out:
	if (bp)
		buf_brelse(bp);
	if (ump) {
		vfs_setfsprivate(mp, NULL);
		bsd_free(ump->um_e2fs->s_es, M_EXT2MNT);
		bsd_free(ump->um_e2fs, M_EXT2MNT);
		bsd_free(ump, M_EXT2MNT);
	}
	ext2_trace_return(error);
}

/*
 * unmount system call
 */
static int
ext2_unmount(mp, mntflags, context)
	mount_t mp;
	int mntflags;
	vfs_context_t context;
{
	struct ext2mount *ump;
	struct ext2_sb_info *fs;
	int error, flags, ronly, i;

	flags = 0;
	if (mntflags & MNT_FORCE) {
		if (vfs_flags(mp) & MNT_ROOTFS)
			ext2_trace_return(EINVAL);
		flags |= FORCECLOSE;
	}
	if ((error = ext2_flushfiles(mp, flags, context)) != 0)
		ext2_trace_return(error);
	ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
	ronly = fs->s_rd_only;
	if (ronly == 0) {
		if (fs->s_wasvalid)
			fs->s_es->s_state =
            cpu_to_le16(le16_to_cpu(fs->s_es->s_state) | EXT2_VALID_FS);
		ext2_sbupdate(context, ump, MNT_WAIT);
	}

	/* release buffers containing group descriptors */
	for(i = 0; i < fs->s_db_per_group; i++) 
		ULCK_BUF(fs->s_group_desc[i]);
	bsd_free(fs->s_group_desc, M_EXT2MNT);

#if EXT2_SB_BITMAP_CACHE
	/* release cached inode/block bitmaps */
   for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
      if (fs->s_inode_bitmap[i])
         ULCK_BUF(fs->s_inode_bitmap[i]);
   
   for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++)
      if (fs->s_block_bitmap[i])
         ULCK_BUF(fs->s_block_bitmap[i]);
#endif

    /* Free the lock alloc'd in mountfs */
    lck_mtx_free(fs->s_lock, EXT2_LCK_GRP);
   
	vfs_setfsprivate(mp, NULL);
	bsd_free(fs->s_es, M_EXT2MNT);
	bsd_free(fs, M_EXT2MNT);
	bsd_free(ump, M_EXT2MNT);
	ext2_trace_return(error);
}

/*
 * Flush out all the files in a filesystem.
 */
static int
ext2_flushfiles(mp, flags, context)
	mount_t mp;
	int flags;
	vfs_context_t context;
{
	int error;
	
    error = vflush(mp, NULLVP, SKIPSWAP|flags);
	error = vflush(mp, NULLVP, flags);
	ext2_trace_return(error);
}

/*
 * Get file system statistics.
 * taken from ext2/super.c ext2_statfs
 */
static int
ext2_getattrfs(mount_t mp,
			   struct vfs_attr *attrs,
			   vfs_context_t context)
{
	unsigned long overhead;
	struct ext2mount *ump;
	struct ext2_sb_info *fs;
	struct ext2_super_block *es;
	uint64_t nsb;
	int i;

	ump = VFSTOEXT2(mp);
	fs = ump->um_e2fs;
	es = fs->s_es;
	if (le16_to_cpu(es->s_magic) != EXT2_SUPER_MAGIC)
		panic("ext2_getattrfs - magic number spoiled");

	/*
	 * Compute the overhead (FS structures)
	 */
	if (le32_to_cpu(es->s_feature_ro_compat) & EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) {
		nsb = 0;
		for (i = 0 ; i < fs->s_groups_count; i++)
			if (ext2_group_sparse(i))
				nsb++;
	} else
		nsb = fs->s_groups_count;
	overhead = le32_to_cpu(es->s_first_data_block) + 
   /* Superblocks and block group descriptors: */
   nsb * (1 + fs->s_db_per_group) +
   /* Inode bitmap, block bitmap, and inode table: */
   fs->s_groups_count * (1 + 1 + fs->s_itb_per_group);

	VFSATTR_RETURN(attrs, f_bsize, EXT2_FRAG_SIZE(fs));	
	VFSATTR_RETURN(attrs, f_iosize, EXT2_BLOCK_SIZE(fs));
	VFSATTR_RETURN(attrs, f_blocks, le32_to_cpu(es->s_blocks_count) - overhead);
	VFSATTR_RETURN(attrs, f_bfree, le32_to_cpu(es->s_free_blocks_count)); 
	VFSATTR_RETURN(attrs, f_bavail, attrs->f_bfree - le32_to_cpu(es->s_r_blocks_count)); 
	VFSATTR_RETURN(attrs, f_files, le32_to_cpu(es->s_inodes_count)); 
	VFSATTR_RETURN(attrs, f_ffree, le32_to_cpu(es->s_free_inodes_count));
	
	struct vfsstatfs *sbp = vfs_statfs(mp);
	uint64_t numdirs = fs->s_dircount;
	if (VFSATTR_IS_ACTIVE(attrs, f_signature))
		VFSATTR_RETURN(attrs, f_signature, (typeof(attrs->f_signature))EXT2_SUPER_MAGIC);
	if (VFSATTR_IS_ACTIVE(attrs, f_objcount))
		VFSATTR_RETURN(attrs, f_objcount, (typeof(attrs->f_objcount))(sbp->f_files - sbp->f_ffree));
	if (VFSATTR_IS_ACTIVE(attrs, f_filecount))
		VFSATTR_RETURN(attrs, f_filecount, (typeof(attrs->f_filecount))(sbp->f_files - sbp->f_ffree) - numdirs);
	if (VFSATTR_IS_ACTIVE(attrs, f_dircount))
		VFSATTR_RETURN(attrs, f_dircount, (typeof(attrs->f_dircount))numdirs);
	if (VFSATTR_IS_ACTIVE(attrs, f_maxobjcount))
		VFSATTR_RETURN(attrs, f_maxobjcount, (typeof(attrs->f_maxobjcount))sbp->f_files);
	
	if (VFSATTR_IS_ACTIVE(attrs, f_capabilities)) {
		int extracaps = 0;
		if (EXT3_HAS_COMPAT_FEATURE(fs, EXT3_FEATURE_COMPAT_HAS_JOURNAL))
			extracaps = VOL_CAP_FMT_JOURNAL /* | VOL_CAP_FMT_JOURNAL_ACTIVE */;

		/* Capabilities we support */
		attrs->f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] =
		VOL_CAP_FMT_SYMBOLICLINKS|
		VOL_CAP_FMT_HARDLINKS|
		VOL_CAP_FMT_SPARSE_FILES|
		VOL_CAP_FMT_CASE_SENSITIVE|
		VOL_CAP_FMT_CASE_PRESERVING|
		VOL_CAP_FMT_FAST_STATFS|
		VOL_CAP_FMT_2TB_FILESIZE|
		extracaps;
		attrs->f_capabilities.capabilities[VOL_CAPABILITIES_INTERFACES] =
		/* VOL_CAP_INT_NFSEXPORT| */
		VOL_CAP_INT_VOL_RENAME|
		#ifdef advlocks
		VOL_CAP_INT_ADVLOCK|
		#endif
		VOL_CAP_INT_FLOCK;
		attrs->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		attrs->f_capabilities.capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

		/* Capabilities we know about */
		attrs->f_capabilities.valid[VOL_CAPABILITIES_FORMAT] =
		VOL_CAP_FMT_PERSISTENTOBJECTIDS |
		VOL_CAP_FMT_SYMBOLICLINKS |
		VOL_CAP_FMT_HARDLINKS |
		VOL_CAP_FMT_JOURNAL |
		VOL_CAP_FMT_JOURNAL_ACTIVE |
		VOL_CAP_FMT_NO_ROOT_TIMES |
		VOL_CAP_FMT_SPARSE_FILES |
		VOL_CAP_FMT_ZERO_RUNS |
		VOL_CAP_FMT_CASE_SENSITIVE |
		VOL_CAP_FMT_CASE_PRESERVING |
		VOL_CAP_FMT_FAST_STATFS|
		VOL_CAP_FMT_2TB_FILESIZE;
		attrs->f_capabilities.valid[VOL_CAPABILITIES_INTERFACES] =
		VOL_CAP_INT_SEARCHFS |
		VOL_CAP_INT_ATTRLIST |
		VOL_CAP_INT_NFSEXPORT |
		VOL_CAP_INT_READDIRATTR |
		VOL_CAP_INT_EXCHANGEDATA |
		VOL_CAP_INT_COPYFILE |
		VOL_CAP_INT_ALLOCATE |
		VOL_CAP_INT_VOL_RENAME |
		VOL_CAP_INT_ADVLOCK |
		VOL_CAP_INT_FLOCK ;
		attrs->f_capabilities.valid[VOL_CAPABILITIES_RESERVED1] = 0;
		attrs->f_capabilities.valid[VOL_CAPABILITIES_RESERVED2] = 0;
        VFSATTR_SET_SUPPORTED(attrs, f_capabilities); 
    }
	
    if (VFSATTR_IS_ACTIVE(attrs, f_attributes)) {
        attrs->f_attributes.validattr.commonattr = EXT2_ATTR_CMN_NATIVE;
		attrs->f_attributes.validattr.volattr = EXT2_ATTR_VOL_SUPPORTED;
		attrs->f_attributes.validattr.dirattr = EXT2_ATTR_DIR_SUPPORTED;
		attrs->f_attributes.validattr.fileattr = EXT2_ATTR_FILE_SUPPORTED;
		attrs->f_attributes.validattr.forkattr = EXT2_ATTR_FORK_SUPPORTED;

		attrs->f_attributes.nativeattr.commonattr = EXT2_ATTR_CMN_NATIVE;
		attrs->f_attributes.nativeattr.volattr = EXT2_ATTR_VOL_NATIVE;
	    attrs->f_attributes.nativeattr.dirattr = EXT2_ATTR_DIR_NATIVE;
		attrs->f_attributes.nativeattr.fileattr = EXT2_ATTR_FILE_NATIVE;
		attrs->f_attributes.nativeattr.forkattr = EXT2_ATTR_FORK_NATIVE;
        VFSATTR_SET_SUPPORTED(attrs, f_attributes);
    }
	
    if (VFSATTR_IS_ACTIVE(attrs, f_vol_name)) {
		int len = ext2_vol_label_len(fs->s_es->s_volume_name);
		(void)strncpy(attrs->f_vol_name, fs->s_es->s_volume_name, len);
		attrs->f_vol_name[len] = 0;
        VFSATTR_SET_SUPPORTED(attrs, f_vol_name);
    }
	
	if (VFSATTR_IS_ACTIVE(attrs, f_create_time)) {
		attrs->f_create_time.tv_sec = le32_to_cpu(fs->s_es->s_mkfs_time);
		attrs->f_create_time.tv_nsec = 0;
		VFSATTR_SET_SUPPORTED(attrs, f_create_time);
	}
	
    if (VFSATTR_IS_ACTIVE(attrs, f_owner) ||
		VFSATTR_IS_ACTIVE(attrs, f_modify_time) ||
        VFSATTR_IS_ACTIVE(attrs, f_access_time) /* ||
        VFSATTR_IS_ACTIVE(attrs, f_backup_time) */) {
		
		vnode_t vp;
		if (0 == ext2_root(mp, &vp, context)) {
			struct inode *ip = VTOI(vp);
			
			VFSATTR_RETURN(attrs, f_owner, ip->i_uid);
			
			attrs->f_modify_time.tv_sec = ip->i_mtime;
			attrs->f_modify_time.tv_nsec = ip->i_mtimensec;
			VFSATTR_SET_SUPPORTED(attrs, f_modify_time);
			
			attrs->f_access_time.tv_sec = ip->i_atime;
			attrs->f_access_time.tv_nsec = ip->i_atimensec;
			VFSATTR_SET_SUPPORTED(attrs, f_access_time);
			
			vnode_put(vp);
		}
    }

	return (0);
}

static int
ext2_check_label(const char *label)
{
   int c, i;

   for (i = 0, c = 0; i < EXT2_VOL_LABEL_LENGTH; i++) {
      c = (u_char)*label++;
      if (c < ' ' + !i || strchr(EXT2_VOL_LABEL_INVAL_CHARS, c))
         break;
   }
   return ((i && !c) ? 0 : EINVAL);
}

static int ext2_setattrfs(mount_t mp, struct vfs_attr *attrs, vfs_context_t context)
{
	int error = 0;
	if (VFSATTR_IS_ACTIVE(attrs, f_vol_name)) {
		struct ext2_sb_info *fsp = (VFSTOEXT2(mp))->um_e2fs;
		const char *name = attrs->f_vol_name;
		size_t name_length = strlen(name);
      
		if (name_length > EXT2_VOL_LABEL_LENGTH) {
			printf("EXT2-fs: %s: Warning volume label too long, truncating.\n", __FUNCTION__);
			name_length = EXT2_VOL_LABEL_LENGTH;
		}
      
		error = ext2_check_label(name);
		if (name_length && !error) {
			lock_super(fsp);
			bzero(fsp->s_es->s_volume_name, EXT2_VOL_LABEL_LENGTH);
			bcopy (name, fsp->s_es->s_volume_name, name_length);
			fsp->s_dirt = 1;
			unlock_super(fsp);
			VFSATTR_SET_SUPPORTED(attrs, f_vol_name);
		} else
			error = EINVAL;
	}
	
	return (error);
}

/*
 * Go through the disk queues to initiate sandbagged IO;
 * go through the inodes to write those that have been modified;
 * initiate the writing of the super block if it has been modified.
 *
 * Note: we are always called with the filesystem marked `MPBUSY'.
 */
static int
ext2_sync_callback(vnode_t vp, void *cargs)
{
	struct ext2_iter_cargs *args = (struct ext2_iter_cargs*)cargs;
	struct inode *ip;
	int error;
	
	ip = VTOI(vp);
	/* The inode can be NULL when ext2_vget encounters an error from bread()
	and a sync() gets in before the vnode is invalidated.
	*/
	if (NULL == ip ||
		((ip->i_flag & (IN_ACCESS | IN_CHANGE | IN_MODIFIED | IN_UPDATE)) == 0 &&
		0 == vnode_hasdirtyblks(vp))) {
		return (VNODE_RETURNED);
	}
	
	struct vnop_fsync_args sargs;
	sargs.a_vp = vp;
	sargs.a_waitfor = (MNT_WAIT == args->ca_wait);
	sargs.a_context = args->ca_vctx;
	if ((error = ext2_fsync(&sargs)) != 0)
		args->ca_err = error;
	return (VNODE_RETURNED);
}

static int
ext2_sync(mount_t mp,
		  int waitfor,
		  vfs_context_t context)
{
	struct ext2_iter_cargs args;
	struct ext2mount *ump = VFSTOEXT2(mp);
	struct ext2_sb_info *fs;
	int flags, error, allerror = 0;
	
	fs = ump->um_e2fs;
	if (fs->s_dirt != 0 && fs->s_rd_only != 0) {		/* XXX */
		printf("EXT2-fs: fs = %s\n", fs->fs_fsmnt);
		panic("ext2_sync: rofs mod");
	}
	// Called for each node attached to mount point.
	args.ca_vctx = context;
	args.ca_wait = waitfor;
	args.ca_err = 0;
	flags = VNODE_NOLOCK_INTERNAL|VNODE_NODEAD;
#ifdef notyet
	if (waitfor)
		flags |= VNODE_WAIT;
#endif
	/*
	 * Write back each (modified) inode.
	 */
	if ((error = vnode_iterate(mp, flags, ext2_sync_callback, (void *)&args)))
		allerror = error;
	if (args.ca_err)
		allerror = args.ca_err;

	/*
	 * Force stale file system control information to be flushed.
	 */
   #if 0
    if (waitfor != MNT_LAZY)
   #endif
    {
		struct vnop_fsync_args sargs;
		sargs.a_vp = ump->um_devvp;
		sargs.a_waitfor = waitfor;
		sargs.a_context = context;
	
		//vnode_lock(ump->um_devvp);
		if ((error = spec_fsync(&sargs)) != 0)
			allerror = error;
		//vnode_unlock(ump->um_devvp);
	}
	/*
	 * Write back modified superblock.
	 */
	if (fs->s_dirt != 0) {
		struct timespec ts;
		nanotime(&ts);
		fs->s_dirt = 0;
		fs->s_es->s_wtime = cpu_to_le32(ts.tv_sec);
		if ((error = ext2_sbupdate(context, ump, waitfor)) != 0)
			allerror = error;
	}
	ext2_trace_return(allerror);
}


static // inode must be locked
void ext2_vget_irelse (struct inode *ip)
{
	IASSERTLOCK(ip);
	
	ext2_ihashrem(ip);
	do {
		if (ip->i_flag & IN_INITWAIT) {
			ip->i_flag &= ~IN_INITWAIT;
			IULOCK(ip);
			wakeup(&ip->i_flag);
			IXLOCK(ip);
		} else if (ip->i_refct > 0) {
			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 100000000 /* 10ms == 1 sched quantum */;
			(void)ISLEEP(ip, flag, &ts);
		} else
			break;
	} while (1);
	
	assert(NULLVP == ITOV(ip));
	assert(0 == ip->i_refct);
	IULOCK(ip);
	lck_mtx_free(ip->i_lock, EXT2_LCK_GRP);
	FREE(ip, M_EXT2NODE);
}

/*
 * For greater versatility everything was moved below
 
 */
int
ext2_vget(mount_t mp, ino64_t ino,
		  vnode_t *vpp, vfs_context_t context)
{
	evalloc_args_t ea_local = {0};
	ea_local.va_ino = (ino_t)ino;
	ea_local.va_vctx = context;
	
	return ext2_vget_internal(mp, &ea_local, vpp, context);
}

/*
* Look up an EXT2FS dinode number to find its incore vnode, otherwise read it
* in from disk.  If it is in core, wait for the lock bit to clear, then
* return the inode locked.  Detection and handling of mount points must be
* done by the calling routine.
*/
int
ext2_vget_internal(mount_t mp, evalloc_args_t *valloc_args,
				   vnode_t *vpp, vfs_context_t context)
{
	evinit_args_t viargs;
	struct ext2_sb_info *fs;
	struct inode *ip;
	struct ext2mount *ump;
	buf_t bp;
	vnode_t vp;
	dev_t dev;
	int i, error;
	int used_blocks;
//	evalloc_args_t ea_local = {0};
	evalloc_args_t *eap = NULL;
	ino64_t ino;
	
//	if (IS_EVALLOC_ARGS(valloc_args)) {
	eap = valloc_args;
	ino = (ino64_t)eap->va_ino;
//	} else {
//		ea_local.va_ino = (ino_t)ino;
//		ea_local.va_vctx = context;
//		eap = &ea_local;
//	}

	ump = VFSTOEXT2(mp);
	dev = ump->um_dev;
restart:
	if ((error = ext2_ihashget(dev, (ino_t)ino, 0, vpp)) != 0)
		ext2_trace_return(error);
	if (*vpp != NULL)
		return (0);

	/*
	 * Lock out the creation of new entries in the hash table in
	 * case getnewvnode() or MALLOC() blocks, otherwise a duplicate
	 * may occur!
	 */
	if (ext2fs_inode_hash_lock) {
		while (ext2fs_inode_hash_lock) {
			(void)OSCompareAndSwap(ext2fs_inode_hash_lock, -1, (UInt32*)&ext2fs_inode_hash_lock);
			msleep(&ext2fs_inode_hash_lock, NULL, PVM, "e2vget", 0);
		}
		goto restart;
	}
	if (!OSCompareAndSwap(ext2fs_inode_hash_lock, 1, (UInt32*)&ext2fs_inode_hash_lock))
		goto restart;

	/*
	 * If this MALLOC() is performed after the getnewvnode()
	 * it might block, leaving a vnode with a NULL v_data to be
	 * found by ext2_sync() if a sync happens to fire right then,
	 * which will cause a panic because ext2_sync() blindly
	 * dereferences vp->v_data (as well it should).
	 */
	MALLOC(ip, struct inode *, sizeof(struct inode), M_EXT2NODE, M_WAITOK);
    assert(NULL != ip);
	bzero((caddr_t)ip, sizeof(struct inode));
	ip->i_e2fs = fs = ump->um_e2fs;
	ip->i_dev = dev;
	ip->i_number = (ino_t)ino;
    /* Init our private lock */
	ip->i_lock = lck_mtx_alloc_init(EXT2_LCK_GRP, LCK_ATTR_NULL);
    assert(fs->s_lock != NULL);
   
	/*
	 * Put it onto its hash chain and lock it so that other requests for
	 * this inode will block if they arrive while we are sleeping waiting
	 * for old data structures to be purged or for the contents of the
	 * disk portion of this inode to be read.
	 */
	IXLOCK(ip);
	ip->i_flag |= IN_INIT;
	ext2_ihashins(ip);

	if (ext2fs_inode_hash_lock < 0)
		wakeup(&ext2fs_inode_hash_lock);
	(void)OSCompareAndSwap(ext2fs_inode_hash_lock, 0, (UInt32*)&ext2fs_inode_hash_lock);

	/* Read in the disk contents for the inode, copy into the inode. */
//	ext2_debug("ext2_vget(%llu) dbn= %llu ", ino, fsbtodb(fs, ino_to_fsba(fs, (ino_t)ino)));

	if ((error = buf_bread(ump->um_devvp, (daddr64_t)fsbtodb(fs, ino_to_fsba(fs, ino)),
	    (int)fs->s_blocksize, NOCRED, &bp)) != 0) {
		ext2_vget_irelse(ip);
		buf_brelse(bp);
		*vpp = NULL;
		ext2_trace_return(error);
	}
	/* convert ext2 inode to dinode */
	ext2_ei2i((struct ext2_inode *) ((char *)buf_dataptr(bp) + EXT2_INODE_SIZE *
			ino_to_fsbo(fs, ino)), ip);
	ip->i_block_group = ino_to_cg(fs, ino);
	ip->i_next_alloc_block = 0;
	ip->i_next_alloc_goal = 0;
	ip->i_prealloc_count = 0;
	ip->i_prealloc_block = 0;
	
	/* Now we want to make sure that block pointers for unused
	   blocks are zeroed out - ext2_balloc depends on this 
	   although for regular files and directories only
	*/
	if(/* !(ip->i_flag & IN_E4EXTENTS) && */
	   (S_ISDIR(ip->i_mode) || S_ISREG(ip->i_mode))) {
		used_blocks = (ip->i_size+fs->s_blocksize-1) / fs->s_blocksize;
		for(i = used_blocks; i < EXT2_NDIR_BLOCKS; i++)
			ip->i_db[i] = 0;
	} else if ((eap->va_flags & EVALLOC_CREATE) && eap->va_createmode) {
		ip->i_mode = eap->va_createmode; // required by ext2_vinit
		kauth_cred_t cred = vfs_context_ucred(context);
		ip->i_uid = cred->cr_posix.cr_uid;
		ip->i_gid = cred->cr_posix.cr_rgid;
	}
/*
	ext2_print_inode(ip);
*/
	buf_brelse(bp);

	/*
	 * Create the vnode from the inode.
	 */
	if (!eap->va_vctx)
		eap->va_vctx = context;
	viargs.vi_vallocargs = eap;
	viargs.vi_ip = ip;
	viargs.vi_vnops = ext2_vnodeop_p;
	viargs.vi_specops = ext2_specop_p;
	viargs.vi_fifoops = ext2_fifoop_p;
	viargs.vi_flags = EXT2_VINIT_INO_LCKD;
	if ((error = ext2_vinit(mp, &viargs, &vp)) != 0) {
		ext2_vget_irelse(ip);
		*vpp = NULL;
		ext2_trace_return(error);
	}
	/*
	 * Finish inode initialization now that aliasing has been resolved.
	 */
	ip->i_vnode = vp;
	ip->i_devvp = ump->um_devvp;
	vnode_ref(ip->i_devvp);
	/*
	 * Set up a generation number for this inode if it does not
	 * already have one. This should only happen on old filesystems.
	 */
	if (ip->i_gen == 0) {
		ip->i_gen = random() / 2 + 1;
		if (0 == vfs_isrdonly(mp))
			ip->i_flag |= IN_MODIFIED;
	}
	
	ip->i_flag &= ~IN_INIT;
	IWAKE(ip, flag, flag, IN_INITWAIT);
	IULOCK(ip);
	
	*vpp = vp;
	return (0);
}

/*
 * File handle to vnode
 *
 * Have to be really careful about stale file handles:
 * - check that the inode number is valid
 * - call ext2_vget() to get the locked inode
 * - check for an unallocated inode (i_mode == 0)
 * - check that the given client host has export rights and return
 *   those rights via. exflagsp and credanonp
 */
static int
ext2_fhtovp(mp, fhlen, fhp, vpp, context)
	mount_t mp;
	int fhlen;
	unsigned char *fhp;
	vnode_t *vpp;
	vfs_context_t context;
{
	struct inode *ip;
	struct ufid *ufhp;
	vnode_t nvp;
	struct ext2_sb_info *fs;
	int error;

	ufhp = (struct ufid *)fhp;
	fs = VFSTOEXT2(mp)->um_e2fs;
	if (ufhp->ufid_ino < ROOTINO ||
	    ufhp->ufid_ino > fs->s_groups_count * le32_to_cpu(fs->s_es->s_inodes_per_group))
		ext2_trace_return(ESTALE);
	
   error = ext2_vget(mp, (ino64_t)ufhp->ufid_ino, &nvp, context);
	if (error) {
		*vpp = NULLVP;
		ext2_trace_return(error);
	}
	ip = VTOI(nvp);
	if (ip->i_mode == 0 ||
	    ip->i_gen != ufhp->ufid_gen || ip->i_nlink <= 0) {
		vnode_put(nvp);
		*vpp = NULLVP;
		ext2_trace_return(ESTALE);
	}
	*vpp = nvp;
	return (0);
}

/*
 * Vnode pointer to File handle
 */
/* ARGSUSED */
static int
ext2_vptofh(vp, fhlen, fhp, context)
	vnode_t vp;
	int *fhlen;
	unsigned char *fhp;
	vfs_context_t context;
{
	struct inode *ip;
	struct ufid *ufhp;

	ip = VTOI(vp);
	ufhp = (struct ufid *)fhp;
	*fhlen = ufhp->ufid_len = sizeof(*ufhp);
	ufhp->ufid_ino = ip->i_number;
	ufhp->ufid_gen = ip->i_gen;
	return (0);
}

/*
 * Write a superblock and associated information back to disk.
 */
static int
ext2_sbupdate(vfs_context_t context,
			  struct ext2mount *mp,
			  int waitfor)
{
	struct ext2_sb_info *fs = mp->um_e2fs;
	struct ext2_super_block *es = fs->s_es;
	buf_t bp;
	int error = 0;
    u_int32_t devBlockSize=0, i;
   
    EVOP_DEVBLOCKSIZE(mp->um_devvp, &devBlockSize, context);
/*
printf("EXT2-fs: \nupdating superblock, waitfor=%s\n", waitfor == MNT_WAIT ? "yes":"no");
*/	
	/* group descriptors */
	lock_super(fs);
	for(i = 0; i < fs->s_db_per_group; i++) {
		bp = fs->s_group_desc[i];
		if (!BMETA_ISDIRTY(bp)) {
			continue;
		}
		//xxx More nastiness because of KPI limitations. We have to get the buf marked as busy to write/relse it.
		daddr64_t blk = buf_blkno(bp);
		vnode_t vp = buf_vnode(bp);
		int bytes = (int)buf_count(bp);
		buf_clearflags(bp, B_LOCKED);
		if (buf_getblk(vp, blk, bytes, 0, 0, BLK_META|BLK_ONLYVALID) != bp)
			panic("ext2: cached group buffer not found!");
		buf_setflags(bp, B_LOCKED);  // re-lock so bwrite puts it back on the lock queue when done
		BMETA_CLEAN(bp);
		if (MNT_WAIT == waitfor)
			buf_bwrite(bp);
		else
			buf_bawrite(bp);
	}
	unlock_super(fs);
	
#if EXT2_SB_BITMAP_CACHE
	/* cached inode/block bitmaps */
	lock_super(fs);
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
		bp = fs->s_inode_bitmap[i];
		if (bp) {
			if (!(bp->b_flags & B_DIRTY)) {
				continue;
			} 
			bp->b_flags |= (B_NORELSE|B_BUSY);
			bp->b_flags &= ~B_DIRTY;
			buf_bwrite(bp);
			bp->b_flags &= ~B_BUSY;
		}
	}
	unlock_super(fs);
	
	lock_super(fs);
	for (i = 0; i < EXT2_MAX_GROUP_LOADED; i++) {
		bp = fs->s_block_bitmap[i];
		if (bp) {
			if (!(bp->b_flags & B_DIRTY)) {
				continue;
			} 
			bp->b_flags |= (B_NORELSE|B_BUSY);
			bp->b_flags &= ~B_DIRTY;
			buf_bwrite(bp);
			bp->b_flags &= ~B_BUSY;
		}
	}
	unlock_super(fs);
#endif
	
	/* superblock */
	bp = buf_getblk(mp->um_devvp, (daddr64_t)SBLOCK, SBSIZE, 0, 0, BLK_META);
	lock_super(fs);
	bcopy((caddr_t)es, ((caddr_t)buf_dataptr(bp)+SBOFF), (u_int)sizeof(struct ext2_super_block));
	unlock_super(fs);
	if (waitfor == MNT_WAIT)
		error = buf_bwrite(bp);
	else
		buf_bawrite(bp);

	ext2_trace_return(error);
}

/*
 * Return the root of a filesystem.
 */
static int
ext2_root(mount_t mp,
		  vnode_t *vpp,
		  vfs_context_t context)
{
	vnode_t nvp;
	struct inode *ip;
	int error;
   
	*vpp = NULL;
    error = ext2_vget(mp, (ino64_t)ROOTINO, &nvp, context);
	if (error)
		ext2_trace_return(error);
	ip = VTOI(nvp);
	if (!S_ISDIR(ip->i_mode) || !ip->i_blocks || !ip->i_size) {
		printf("EXT2-fs: root inode is corrupt, please run fsck.\n");
		vnode_put(nvp);
		return (EINVAL);
	}
	*vpp = nvp;
	return (0);
}

static int
ext2_init(struct vfsconf *vfsp)
{

	ext2_ihashinit();
	return (0);
}

static int
ext2_uninit(struct vfsconf *vfsp)
{

	ext2_ihashuninit();
	return (0);
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
static int
vfs_stdstart(mp, flags, context)
	mount_t mp;
	int flags;
	vfs_context_t context;
{
	return (0);
}

/*
 * Do operations associated with quotas, not supported
 */
/* ARGSUSED */
static int
ext2_quotactl(mp, cmd, uid, arg, context)
	mount_t mp;
	int cmd;
	uid_t uid;
	caddr_t arg;
	vfs_context_t context;
{
	return (EOPNOTSUPP);
}

__private_extern__ int dirchk;
__private_extern__ u_int32_t lookcacheinval;

static int
ext2_sysctl(int *name, u_int namelen, user_addr_t oldp, size_t *oldlenp, user_addr_t newp,
	   size_t newlen, vfs_context_t context)
{
	int error = 0, intval;
	
#ifdef DARWIN7
	struct sysctl_req *req;
	struct vfsidctl vc;
	mount_t mp;
	struct nfsmount *nmp;
	struct vfsquery vq;
	
	/*
		* All names at this level are terminal.
		*/
	if(namelen > 1)
		return ENOTDIR;	/* overloaded */
	
	/* common code for "new style" VFS_CTL sysctl, get the mount. */
	switch (name[0]) {
	case VFS_CTL_TIMEO:
	case VFS_CTL_QUERY:
		req = oldp;
		error = SYSCTL_IN(req, &vc, sizeof(vc));
		if (error)
			return (error);
		mp = vfs_getvfs(&vc.vc_fsid);
		if (mp == NULL)
			return (ENOENT);
		nmp = VFSTONFS(mp);
		if (nmp == NULL)
			return (ENOENT);
		bzero(&vq, sizeof(vq));
		VCTLTOREQ(&vc, req);
	}
#endif /* DARWIN7 */

	switch (name[0]) {
		case EXT2_SYSCTL_INT_DIRCHECK:
			if (!oldp && !newp) {
				*oldlenp = sizeof(dirchk);
				return (0);
			}
			if (oldp && *oldlenp < sizeof(dirchk)) {
				*oldlenp = sizeof(dirchk);
				return (ENOMEM);
			}
			if (oldp) {
				*oldlenp = sizeof(dirchk);
				error = copyout(&dirchk, CAST_USER_ADDR_T(oldp), sizeof(dirchk));
				if (error)
					return (error);
			}
			
			if (newp && newlen != sizeof(int))
				return (EINVAL);
			if (newp) {
				error = copyin(CAST_USER_ADDR_T(newp), &intval, sizeof(dirchk));
				if (!error) {
					if (1 == intval || 0 == intval)
						dirchk = intval;
					else
						error = EINVAL;
				}
			}
			break;
		
		case EXT2_SYSCTL_INT_LOOKCACHEINVAL:
			if (!oldp && !newp) {
				*oldlenp = sizeof(lookcacheinval);
				return (0);
			}
			if (oldp && *oldlenp < sizeof(lookcacheinval)) {
				*oldlenp = sizeof(lookcacheinval);
				return (ENOMEM);
			}
			if (oldp) {
				*oldlenp = sizeof(lookcacheinval);
				error = copyout(&lookcacheinval, CAST_USER_ADDR_T(oldp), sizeof(lookcacheinval));
				if (error)
					return (error);
			}
			
			if (newp)
				return (ENOTSUP);
			break;
		
		default:
			error = ENOTSUP;
			break;
	}

	return (error);
}

/* Kernel entry/exit points */

extern struct sysctl_oid sysctl__vfs_e2fs;
extern struct sysctl_oid sysctl__vfs_e2fs_dircheck;
extern struct sysctl_oid sysctl__vfs_e2fs_lookcacheinval;
static struct sysctl_oid* const e2sysctl_list[] = {
	&sysctl__vfs_e2fs,
	&sysctl__vfs_e2fs_dircheck,
	&sysctl__vfs_e2fs_lookcacheinval,
	(struct sysctl_oid *)0
};
static vfstable_t ext2_tableid;

kern_return_t ext2fs_start (kmod_info_t * ki, void * d) {
	lck_grp_attr_t *lgattr;
#ifndef DIAGNOSTIC
	lgattr = LCK_GRP_ATTR_NULL;
#else
	lgattr = lck_grp_attr_alloc_init();
	if (lgattr)
		lck_grp_attr_setstat(lgattr);
#endif
	if (NULL == (ext2_lck_grp = lck_grp_alloc_init("Ext2 Filesystem", lgattr)))
		return (KERN_RESOURCE_SHORTAGE);
	if (lgattr)
		lck_grp_attr_free(lgattr);
	
	struct vfs_fsentry fsc;
	struct vnodeopv_desc* vnops[3] = {
		&ext2fs_vnodeop_opv_desc,
		&ext2fs_specop_opv_desc,
		&ext2fs_fifoop_opv_desc
	};
	int kret, i;
	
	bzero(&fsc, sizeof(struct vfs_fsentry));
   
	fsc.vfe_vfsops = &ext2fs_vfsops;
	fsc.vfe_vopcnt = 3;
	fsc.vfe_opvdescs = vnops;
	strncpy(&fsc.vfe_fsname[0], EXT2FS_NAME, MFSNAMELEN);
	// If we let the kernel assign our typenum, there is no way to access it
	// until a vol is mounted. So, we have to use a static # so we can register
	// our sysctl's.
	fsc.vfe_fstypenum = (int)EXT2_SUPER_MAGIC;
	fsc.vfe_flags = VFS_TBLTHREADSAFE/*|VFS_TBLNOTYPENUM*/|VFS_TBLLOCALVOL;
	kret = vfs_fsadd(&fsc, &ext2_tableid);
	if (kret) {
		printf ("EXT2-fs: ext2fs_start: Failed to register with kernel, error = %d\n", kret);
		lck_grp_free(ext2_lck_grp);
		return (KERN_FAILURE);
	}
	
	/* This is required for vfs_sysctl() to call our handler. */
	sysctl__vfs_e2fs.oid_number = fsc.vfe_fstypenum;
	/* Register our sysctl's */
	for (i=0; e2sysctl_list[i]; ++i) {
		#ifdef DIAGNOSTIC
		printf ("EXT2-fs: ext2fs_start: registering sysctl '%s' with OID %d\n", (e2sysctl_list[i])->oid_name, (e2sysctl_list[i])->oid_number);
		#endif
		sysctl_register_oid(e2sysctl_list[i]);
	};
	
   return (KERN_SUCCESS);
}

kern_return_t ext2fs_stop (kmod_info_t * ki, void * d) {
	int error, i;
	
	/* Unregister with the kernel */

	if ((error = vfs_fsremove(ext2_tableid)))
		return (KERN_FAILURE);
	
	for (i=0; e2sysctl_list[i]; ++i) ; /* Get Count */
	for (--i; i >= 0; --i) { /* Work backwords */
		assert(NULL != e2sysctl_list[i]);
		sysctl_unregister_oid(e2sysctl_list[i]);
	};

   ext2_uninit(NULL);
   lck_grp_free(ext2_lck_grp);
   
   ext2_trace_return(KERN_SUCCESS);
}
