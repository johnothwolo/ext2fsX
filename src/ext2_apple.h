/*
* Copyright 2003-2005 Brian Bergstrand.
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

#ifndef EXT2_APPLE_H
#define EXT2_APPLE_H

#define EXT2FS_NAME "ext2"
#define EXT3FS_NAME "ext3"

#define EXT2_VOL_LABEL_LENGTH 16
#define EXT2_VOL_LABEL_INVAL_CHARS "\"*/:;<=>?[\\]|"

static __inline__
int ext2_vol_label_len(char *label) {
   int i;
   for (i = 0; i < EXT2_VOL_LABEL_LENGTH && 0 != label[i]; i++) ;
   return (i);
}

#include <sys/mount.h>
struct ext2_args {
	char *fspec;			/* block special device to mount */
    unsigned int e2_mnt_flags; /* Ext2 specific mount flags */
    uid_t   e2_uid; /* Only active when MNT_UNKNOWNPERMISSIONS is set */
    gid_t   e2_gid; /* ditto */
	//struct	export_args export;	/* network export information */
};

#define EXT2_MNT_INDEX 0x00000001
#define EXT2_MOPT_INDEX { "index", 0, EXT2_MNT_INDEX, 0 }

#define UNKNOWNUID ((uid_t)99)
#define UNKNOWNGID ((gid_t)99)

#ifdef __ppc__
#define E2_BAD_ADDRESS (void*)0xdeadbeef
#elif defined(__i386__)
#define E2_BAD_ADDRESS (void*)0xfeedface
#else
#error unknown architecture
#endif

#ifdef DIAGNOSTIC
#define E2_DGB_ONLY
#else
#define E2_DGB_ONLY __unused
#endif

#ifdef KERNEL

#ifdef DIAGNOSTIC
#define MACH_ASSERT 1
#endif
#include <kern/assert.h>

#include <sys/ubc.h>
#include <libkern/OSAtomic.h>
#include <sys/vnode_if.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/fcntl.h>

//#include <xnu/bsd/miscfs/specfs/specdev.h>

/* In BSD KPI, but not defined in headers */
extern int groupmember(gid_t gid, struct ucred *cred);
extern int vfs_init_io_attributes (struct vnode *, struct mount *);
extern int spec_ioctl(struct vnop_ioctl_args*); // EVOP_DEVBLOCKSIZE
//extern uid_t console_user; // in Unsupported KPI
//extern int prtactive; /* 1 => print out reclaim of active vnodes */
/**/

#ifndef LCK_GRP_NULL
#define LCK_GRP_NULL (lck_grp_t *)0
#endif
#ifndef LCK_GRP_ATTR_NULL
#define LCK_GRP_ATTR_NULL (lck_grp_attr_t *)0
#endif
#ifndef LCK_ATTR_NULL
#define LCK_ATTR_NULL (lck_attr_t *)0
#endif

/* Ext2 types */

__private_extern__ lck_grp_t *ext2_lck_grp;
#define EXT2_LCK_GRP ext2_lck_grp

#define M_EXT3DIRPRV M_TEMP
#define M_EXT2NODE M_TEMP
#define M_EXT2MNT M_TEMP
#define VT_EXT2 VT_OTHER

typedef int     vnop_t (void *);

// uprintf prints to the processes tty and the system log file -- not in KPI
#define uprintf printf

#ifdef _VNODE_H_
__private_extern__ int ext2_read(struct vnop_read_args*);
__private_extern__ int ext2_write(struct vnop_write_args*);
__private_extern__ int ext2_cache_lookup (struct vnop_lookup_args *);
__private_extern__ int ext2_blktooff (struct vnop_blktooff_args *);
__private_extern__ int ext2_offtoblk (struct vnop_offtoblk_args *);
__private_extern__ int ext2_pagein (struct vnop_pagein_args *);
__private_extern__ int ext2_pageout (struct vnop_pageout_args *);
__private_extern__ int ext2_mmap (struct vnop_mmap_args *);
__private_extern__ int ext2_ioctl (struct vnop_ioctl_args *);
__private_extern__ int ext2_blockmap (struct vnop_blockmap_args *);

static __inline__
int EXT2_READ(vnode_t vp, uio_t uio, int flags, vfs_context_t context)
{
    struct vnop_read_args args;
    args.a_desc = &vnop_write_desc;
    args.a_vp = vp;
    args.a_uio = uio;
    args.a_ioflag = flags;
    args.a_context = context;
    return (ext2_read(&args));
}
static __inline__
int EXT2_WRITE(vnode_t vp, uio_t uio, int flags, vfs_context_t context)
{
    struct vnop_write_args args;
    args.a_desc = &vnop_write_desc;
    args.a_vp = vp;
    args.a_uio = uio;
    args.a_ioflag = flags;
    args.a_context = context;
    return (ext2_write(&args));
}
#endif

#if DIAGNOSTIC
__private_extern__ void ext2_checkdir_locked(struct vnode *dvp);
#else
#define ext2_checkdir_locked(dvp)
#endif

/* Set to 1 to enable inode/block bitmap caching -- currently, this will
cause a panic situation when the filesystem is stressed. */
#define EXT2_SB_BITMAP_CACHE 0

/* Pre-Tiger emmulation support */
#ifdef _SYS_BUF_H_

static __inline__
void clrbuf(buf_t bp)
{
    buf_clear(bp);
}

#endif

#ifdef _VNODE_H_
// This is exported by the BSD KPI, but is not in the headers
int vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base,
	    		int len, off_t offset, enum uio_seg segflg, int ioflg,
	    		struct ucred *cred, int *aresid, struct proc *p);
#endif

static __inline__
int EVOP_DEVBLOCKSIZE(vnode_t vp, u_int32_t *size, vfs_context_t ctx) {
    struct vnop_ioctl_args a;
    a.a_desc = &vnop_ioctl_desc;
    a.a_vp = (vp);
    a.a_command = DKIOCGETBLOCKSIZE;
    a.a_data = (caddr_t)size;
    a.a_fflag = FREAD;
    a.a_context = ctx;
    return (spec_ioctl(&a));
}

/* vnode_t */
/* XXX Is this really a correct assumption? */
#define UBCINFOEXISTS(vp) (VREG == vnode_vtype((vp)))
#define UBCISVALID(vp) (VREG == vnode_vtype((vp)))

/* FreeBSD emulation support */

/* Process stuff */
#define curproc (current_proc())
#define curthread curproc
#define thread proc
#define td_ucred p_ucred
#define a_td a_p
#define cn_thread cn_proc
#define uio_td uio_procp
#define PROC_LOCK(p)
#define PROC_UNLOCK(p)

/*
 * Test the active securelevel against a given level.  securelevel_gt()
 * implements (securelevel > level).  securelevel_ge() implements
 * (securelevel >= level).  Note that the logic is inverted -- these
 * functions return EPERM on "success" and 0 on "failure".
 *
 * Must call while holding kernel funnel for SMP safeness.
 */
__private_extern__ int e2securelevel();
#define securelevel_gt(cr,level) ( e2securelevel > (level) ? EPERM : 0 )
#define securelevel_ge(cr,level) ( e2securelevel >= (level) ? EPERM : 0 )

/* dev_t */
#define devtoname(d) "unknown"

#define mnt_iosize_max mnt_maxreadcnt

/* vnode_t */
#define VI_MTX(vp) NULL
/* #define VI_MTX(vp) (&(vp)->v_interlock) */

#define VI_LOCK(vp)
#define VI_UNLOCK(vp)

/* FreeBSD kern calls */

/* vnode_t, u_int32_t */
#define vnode_pager_setsize(vp,sz) \
do { \
   if (UBCISVALID((vp))) {ubc_setsize((vp), (sz));} \
} while(0)

#define vfs_timestamp nanotime
/* XXX Is 0 always right for thread use cnt? */
#define vrefcnt(vp) vnode_isinuse((vp), 0)

#if defined(_SYS_BUF_H_) && defined(_VNODE_H_)
static __inline__
int vop_stdfsync(struct vnop_fsync_args *ap)
{
    buf_flushdirtyblks(ap->a_vp, (MNT_WAIT == ap->a_waitfor),
        BUF_SKIP_LOCKED, "ext2_fsync");
    return (0);
}
#endif

/* Vnode flags */
#define VV_ROOT VROOT
#define VI_BWAIT VBWAIT

/* FreeBSD flag for vn_rdwr() */
#define IO_NOMACCHECK 0

/* FreeBSD Mount flags */
#define MNT_NOCLUSTERR 0
#define MNT_NOCLUSTERW 0
#define MNT_NOATIME 0

/* Soft Updates */
#define SF_SNAPSHOT 0
#define SF_NOUNLINK 0
/* No delete */
#define NOUNLINK 0

// This is a FreeBSD fn used to prevent vfs deadlocks.
// It waits for the filesystem to exit write suspension mode.
// I don't believe this is relevant on OS X.
#ifdef _VNODE_H_
#define V_WAIT 1 // XXX
#define V_NOWAIT 0 // XXX
static __inline__
int vn_write_suspend_wait(vnode_t vp, mount_t mp, int flag)
{
    return (0);
}
#endif

#define vfs_bio_clrbuf clrbuf

#define BIO_ERROR B_ERROR

/* FreeBSD getblk flag */
#define GB_LOCK_NOWAIT 0

#define BUF_WRITE buf_bwrite
#define BUF_STRATEGY VNOP_STRATEGY
/* Buffer, Lock, InterLock */
#define BUF_LOCK(b,l,il)

#define bqrelse buf_brelse
#define bufwait buf_biowait

#define hashdestroy(tbl,type,cnt) FREE((tbl), (type))

#ifndef mtx_destroy
#define mtx_destroy(mp) lck_mtx_free((mp))
#endif
#ifndef mtx_lock
#define mtx_lock(mp) lck_mtx_lock((mp))
#endif
#ifndef mtx_unlock
#define mtx_unlock(mp) lck_mtx_unlock((mp))
#endif

#ifndef __i386__
// i386-bitops.h contains a memscan implementation
static __inline void * memscan(void * addr, int c, size_t size)
{
	unsigned char * p = (unsigned char *) addr;

	while (size) {
		if (*p == c)
			return (void *) p;
		p++;
		size--;
	}
  	return (void *) p;
}
#endif // _SYS_GNU_EXT2FS_I386_BITOPS_H_

/* Cribbed from FBSD sys/dirent.h */
/*
 * The _GENERIC_DIRSIZ macro gives the minimum record length which will hold
 * the directory entry.  This requires the amount of space in struct direct
 * without the d_name field, plus enough space for the name with a terminating
 * null byte (dp->d_namlen+1), rounded up to a 4 byte boundary.
 */
#define	GENERIC_DIRSIZ(dp) \
    ((sizeof (struct dirent) - (MAXNAMLEN+1)) + (((dp)->d_namlen+1 + 3) &~ 3))
    
/* Linux compat */

#define printk printf
#define memcmp bcmp

#define ERR_PTR(err) (void*)(err)

#define EXT3_I(inode) (inode)

#define KERN_CRIT "CRITICAL"

/* Missing clib functions (as of 10.4.x) */
__private_extern__ char* e_strrchr(const char *, int);

/* Debug */

#endif /*KERNEL*/

#endif /* EXT2_APPLE_H */
