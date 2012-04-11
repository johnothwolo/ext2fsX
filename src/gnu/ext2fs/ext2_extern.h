/*
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*-
 * Copyright (c) 1991, 1993, 1994
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
 *	@(#)ffs_extern.h	8.3 (Berkeley) 4/16/94
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_extern.h,v 1.29 2002/05/18 22:18:17 iedowse Exp $
 */

#ifndef _SYS_GNU_EXT2FS_EXT2_EXTERN_H_
#define	_SYS_GNU_EXT2FS_EXT2_EXTERN_H_

#include <sys/types.h>
#include <sys/vnode.h>

#ifndef __u32
#define __u32 u_int32_t
#endif

struct dx_hash_info;
struct dir_private_info;
struct ext2_inode;
struct ext2_sb_info;
struct ext2_dir_entry_2;
struct indir;
struct inode;

typedef struct ext2_valloc_args {
	ino_t va_ino;
	vnode_t va_parent;
	vfs_context_t va_vctx;
	struct componentname *va_cnp;
	u_int16_t va_flags;
	u_int16_t va_createmode;
} evalloc_args_t;
#define EVALLOC_CREATE 0x0001

//xxx this will break if kernel space ever goes 64bit
#define EVALLOC_ARGS_MAGIC(eap) (ino64_t)(0x5000000000000000LL | (ino64_t)((uintptr_t)(eap)))
#define IS_EVALLOC_ARGS(ino64) (((ino64_t)(ino64) & 0xffffffff00000000LL) == 0x5000000000000000LL ? 1 : 0)
#define EVALLOC_ARGS(ino64) (evalloc_args_t*)((uintptr_t)((ino64) & 0x00000000ffffffffULL))

__private_extern__ struct vfsops ext2fs_vfsops;
static __inline__
int EXT2_VGET(mount_t mp, evalloc_args_t *eap, vnode_t *vpp, vfs_context_t cp)
{
    ino64_t arg = EVALLOC_ARGS_MAGIC(eap);
    return (ext2fs_vfsops.vfs_vget(mp, arg, vpp, cp));
}

int	ext2_alloc(struct inode *,
	    ext2_daddr_t, ext2_daddr_t, int, struct ucred *, ext2_daddr_t *);
int	ext2_balloc2(struct inode *,
	    ext2_daddr_t, int, struct ucred *, buf_t  *, int, int *);
int	ext2_blkatoff(vnode_t , off_t, char **, buf_t  *);
void	ext2_blkfree(struct inode *, ext2_daddr_t, long);
ext2_daddr_t	ext2_blkpref(struct inode *, ext2_daddr_t, int, ext2_daddr_t *, ext2_daddr_t);
int	ext2_bmap(struct vnop_blockmap_args *);
int	ext2_bmaparray(vnode_t , ext2_daddr_t, ext2_daddr_t *, int *, int *);
void	ext2_dirbad(struct inode *ip, doff_t offset, char *how);
void	ext2_ei2i(struct ext2_inode *, struct inode *);
int	ext2_getlbns(vnode_t , ext2_daddr_t, struct indir *, int *);
void	ext2_i2ei(struct inode *, struct ext2_inode *);
int	ext2_ihashget(dev_t, ino_t, int, vnode_t *);
void	ext2_ihashinit(void);
void	ext2_ihashins(struct inode *);
vnode_t 
	ext2_ihashlookup(dev_t, ino_t);
void	ext2_ihashrem(struct inode *);
void	ext2_ihashuninit(void);
void	ext2_itimes(vnode_t);
#ifdef FANCY_REALLOC
int	ext2_reallocblks(struct vnop_reallocblks_args *);
#endif
int	ext2_reclaim(struct vnop_reclaim_args *);
/* void ext2_setblock(struct ext2_sb_info *, u_char *, int32_t); */
int	ext2_truncate(vnode_t, off_t, int, ucred_t, proc_t);
int	ext2_update(vnode_t, int);
int	ext2_valloc(vnode_t, int, evalloc_args_t *, vnode_t *);
int	ext2_vfree(vnode_t, ino_t, int);

typedef struct ext2_vinit_args {
	evalloc_args_t *vi_vallocargs;
	struct inode *vi_ip;
	vnop_t **vi_vnops, **vi_specops, **vi_fifoops;
	u_int32_t vi_flags;
} evinit_args_t;
#define EXT2_VINIT_INO_LCKD 0x00000001

int	ext2_vinit(mount_t, evinit_args_t *, vnode_t *vpp);
int ext2_lookup(struct vnop_lookup_args *);
int ext2_readdir(struct vnop_readdir_args *);
void	ext2_print_inode(struct inode *);
int	ext2_direnter(struct inode *, 
		vnode_t , struct componentname *, vfs_context_t);
int	ext2_dirremove(vnode_t , struct componentname *, vfs_context_t);
int	ext2_dirrewrite_nolock(struct inode *,
		struct inode *, struct componentname *, vfs_context_t);
int	ext2_dirempty(struct inode *, ino_t, struct ucred *);
int	ext2_checkpath_nolock(struct inode *, struct inode *, evalloc_args_t *);
struct  ext2_group_desc * get_group_desc(mount_t   , 
		unsigned int , buf_t  * );
int	ext2_group_sparse(int group);
void	ext2_discard_prealloc(struct inode *);
int	ext2_inactive(struct vnop_inactive_args *);
int	ext2_new_block(mount_t   mp, unsigned long goal,
	    u_int32_t *prealloc_count, u_int32_t *prealloc_block);
ino_t	ext2_new_inode(const struct inode * dir, int mode);
unsigned long ext2_count_free(buf_t  map, unsigned int numchars);
void	ext2_free_blocks(mount_t  mp, unsigned long block,
	    unsigned long count);
void	ext2_free_inode(struct inode * inode);

/* ext3_super.c */
extern void __ext3_std_error (struct ext2_sb_info *, const char *, int);
#define ext3_std_error(sb, errno) \
do { \
	if ((errno)) \
		__ext3_std_error((sb), __FUNCTION__, (errno)); \
} while (0)
extern void ext3_warning (struct ext2_sb_info *, const char *, const char *, ...);
extern void ext3_update_dynamic_rev(struct ext2_sb_info *);

/* ext3_dx.c */
extern int ext3_htree_store_dirent(vnode_t dir_file, __u32 hash,
				    __u32 minor_hash,
				    struct ext2_dir_entry_2 *dirent);
extern void ext3_htree_free_dir_info(struct dir_private_info *p);

/* hash.c */
extern int ext3fs_dirhash(const char *name, int len, struct
			  dx_hash_info *hinfo);

/* Flags to low-level allocation routines. */
#define B_CLRBUF	0x01	/* Request allocated buffer be cleared. */
#define B_SYNC		0x02	/* Do all allocations synchronously. */
#define B_METAONLY	0x04	/* Return indirect block buffer. */
#define B_NOWAIT	0x08	/* do not sleep to await lock */
#define B_NOBUFF    0x10    /* do not allocate buffer */

extern vnop_t **ext2_vnodeop_p;
extern vnop_t **ext2_specop_p;
extern vnop_t **ext2_fifoop_p;

/* Compatibility wrapper for ext2_balloc2 */
static __inline__ int ext2_balloc(struct inode *ip,
	    ext2_daddr_t bn, int size, struct ucred *cred, buf_t  *bpp, int flags)
{
   return (ext2_balloc2(ip, bn, size, cred, bpp, flags, NULL));
}

/* Sysctl OID numbers. */
#define EXT2_SYSCTL_INT_DIRCHECK 1
#define EXT2_SYSCTL_INT_LOOKCACHEINVAL 2

#endif /* !_SYS_GNU_EXT2FS_EXT2_EXTERN_H_ */
