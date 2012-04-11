/*
 * Copyright (c) 1982, 1989, 1993
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
 *	@(#)inode.h	8.9 (Berkeley) 5/14/95
 * $FreeBSD: src/sys/gnu/ext2fs/inode.h,v 1.38 2002/05/18 19:12:38 iedowse Exp $
 */

#ifndef _SYS_GNU_EXT2FS_INODE_H_
#define	_SYS_GNU_EXT2FS_INODE_H_

#include <sys/queue.h>
#include <sys/event.h>
#include <kern/locks.h>

#include "ext2_apple.h"

// These are missing from sys/event.h, but knote is exported by the BSD kpi,
// so we will use it.
#if defined(EXT_KNOTE) && !defined(KNOTE)
struct knote {
	int		kn_inuse;	/* inuse count */
	struct kqtailq	*kn_tq;		/* pointer to tail queue */
	TAILQ_ENTRY(knote)	kn_tqe;		/* linkage for tail queue */
	struct kqueue	*kn_kq;	/* which kqueue we are on */
	SLIST_ENTRY(knote)	kn_link;	/* linkage for search list */
	SLIST_ENTRY(knote)	kn_selnext;	/* klist element chain */
	union {
		struct		fileproc *p_fp;	/* file data pointer */
		struct		proc *p_proc;	/* proc pointer */
	} kn_ptr;
	struct			filterops *kn_fop;
	int			kn_status;	/* status bits */
	int			kn_sfflags;	/* saved filter flags */
	struct 			kevent kn_kevent;
	caddr_t			kn_hook;
	int			kn_hookid;
	int64_t			kn_sdata;	/* saved data field */

#define KN_ACTIVE	0x01			/* event has been triggered */
#define KN_QUEUED	0x02			/* event is on queue */
#define KN_DISABLED	0x04			/* event is disabled */
#define KN_DROPPING	0x08			/* knote is being dropped */
#define KN_USEWAIT	0x10			/* wait for knote use */
#define KN_DROPWAIT	0x20			/* wait for knote drop */

#define kn_id		kn_kevent.ident
#define kn_filter	kn_kevent.filter
#define kn_flags	kn_kevent.flags
#define kn_fflags	kn_kevent.fflags
#define kn_data		kn_kevent.data
#define kn_fp		kn_ptr.p_fp
};

struct filterops {
	int	f_isfd;		/* true if ident == filedescriptor */
	int	(*f_attach)(struct knote *kn);
	void	(*f_detach)(struct knote *kn);
	int	(*f_event)(struct knote *kn, long hint);
};
SLIST_HEAD(klist, knote);

#define KNOTE(list, hint)	knote(list, hint)
#define KNOTE_ATTACH(list, kn)	knote_attach(list, kn)
#define KNOTE_DETACH(list, kn)	knote_detach(list, kn)
extern void	knote(struct klist *list, long hint);
extern int	knote_attach(struct klist *list, struct knote *kn);
extern int	knote_detach(struct klist *list, struct knote *kn);
#endif // KNOTE

#define	ROOTINO	((ino_t)2)

#define	NDADDR	12			/* Direct addresses in inode. */
#define	NIADDR	3			/* Indirect addresses in inode. */

/*
 * This must agree with the definition in <ufs/ufs/dir.h>.
 */
#define	doff_t		int32_t

typedef int32_t   ext2_daddr_t;

/* Function proto to release inode private data */
struct inode;
typedef int (*inode_prv_relse_t)(vnode_t, struct inode*);

/* Dir lookup cache entries */
struct dcache {
    LIST_ENTRY(dcache) d_hash; // chain
    doff_t d_offset;
    int32_t d_count;
    int32_t d_refct;
    u_int32_t d_reclen;
    #ifdef DIAGNOSTIC
    ino_t  d_ino;
    #endif
    unsigned char d_namelen;
    char d_name[0];
};
#define E2DCACHE_BUCKETS 8
#define E2DCACHE_MASK 7

/*
 * The inode is used to describe each active (or recently active) file in the
 * EXT2FS filesystem. It is composed of two types of information. The first
 * part is the information that is needed only while the file is active (such
 * as the identity of the file and linkage to speed its lookup). The second
 * part is the permanent meta-data associated with the file which is read in
 * from the permanent dinode from long term storage when the file becomes
 * active, and is put back when the file is no longer being used.
 */
struct inode {
	lck_mtx_t *i_lock;
	LIST_ENTRY(inode) i_hash;/* Hash chain. */
	vnode_t   i_vnode;/* Vnode associated with this inode. */
	vnode_t   i_devvp;/* Vnode for block I/O. */
	u_int32_t i_flag;	/* flags, see below */
	dev_t	  i_dev;	/* Device associated with the inode. */
	ino_t	  i_number;	/* The identity of the inode. */
	u_int32_t i_refct;
	ext2_daddr_t i_lastr; /* last read... read-ahead */
#ifdef DIAGNOSTIC
    thread_t  i_lockowner;
    char      i_lockfile[64];
    int32_t   i_lockline;
    int32_t   i_dhashct;
#endif
	
	/*
	 * Various accounting
	 */
#ifdef EXT_KNOTE
	struct klist i_knotes; /* Attached knotes. */ 
#endif
	void	*private_data; /* Private storge, used by dir index (others?). */
	inode_prv_relse_t private_data_relse; /* Function to release private_data storage. */
	struct	ext2_sb_info *i_e2fs;	/* EXT2FS */
	u_quad_t i_modrev;	/* Revision level for NFS lease. */
#ifdef notyet
	struct	 ext2lockf *i_lockf;/* Head of byte-level lock list. */
#endif
	/*
	 * Side effects; used during directory lookup - protected by IN_LOOK.
	 */
	int32_t	  i_count;	/* Size of free slot in directory. */
	doff_t	  i_endoff;	/* End of useful stuff in directory. */
	doff_t	  i_diroff;	/* Offset in dir, where we found last entry. */
	doff_t	  i_offset;	/* Offset of free space in directory. */
	ino_t	  i_ino;	/* Inode number of found directory. */
	u_int32_t i_reclen;	/* Size of found directory entry. */
	//u_int32_t i_dir_start_lookup; /* Index dir lookup */
	#define f_pos i_diroff /* Index dir lookups */
    LIST_HEAD(idashhead, dcache) *i_dhashtbl; // protected by I_LOOK
    int32_t i_lastnamei;
	
	u_int32_t i_block_group;
	u_int32_t i_next_alloc_block;
	u_int32_t i_next_alloc_goal;
	u_int32_t i_prealloc_block;
	u_int32_t i_prealloc_count;
	
	/* Fields from struct dinode in UFS. */
	u_int16_t	i_mode;		/* IFMT, permissions; see below. */
	int16_t		i_nlink;	/* File link count. */
	u_int64_t	i_size;		/* File byte count. */
	int32_t		i_atime;	/* Last access time. */
	int32_t		i_atimensec;	/* Last access time. */
	int32_t		i_mtime;	/* Last modified time. */
	int32_t		i_mtimensec;	/* Last modified time. */
	int32_t		i_ctime;	/* Last inode change time. */
	int32_t		i_ctimensec;	/* Last inode change time. */
	ext2_daddr_t	i_db[NDADDR];	/* Direct disk blocks. */
	ext2_daddr_t	i_ib[NIADDR];	/* Indirect disk blocks. */
	u_int32_t	i_flags;	/* Status flags (chflags). */
	ext2_daddr_t	i_blocks;	/* Blocks actually held. */
	int32_t		i_gen;		/* Generation number. */
	u_int32_t	i_uid;		/* File owner. */
	u_int32_t	i_gid;		/* File group. */
	
	u_int32_t   i_e2flags; /* copy of on disk ext2 inode flags */
};

/*
 * The di_db fields may be overlaid with other information for
 * file types that do not have associated disk storage. Block
 * and character devices overlay the first data block with their
 * dev_t value. Short symbolic links place their path in the
 * di_db area.
 */
#define	i_shortlink	i_db
#define	i_rdev		i_db[0]
#define	MAXSYMLINKLEN	((NDADDR + NIADDR) * sizeof(int32_t))

/* File permissions. */
#define	IEXEC		0000100		/* Executable. */
#define	IWRITE		0000200		/* Writeable. */
#define	IREAD		0000400		/* Readable. */
#define	ISVTX		0001000		/* Sticky bit. */
#define	ISGID		0002000		/* Set-gid. */
#define	ISUID		0004000		/* Set-uid. */

/* File types. */
#define	IFMT		0170000		/* Mask of file type. */
#define	IFIFO		0010000		/* Named pipe (fifo). */
#define	IFCHR		0020000		/* Character device. */
#define	IFDIR		0040000		/* Directory file. */
#define	IFBLK		0060000		/* Block device. */
#define	IFREG		0100000		/* Regular file. */
#define	IFLNK		0120000		/* Symbolic link. */
#define	IFSOCK		0140000		/* UNIX domain socket. */
#define	IFWHT		0160000		/* Whiteout. */

/* These flags are kept in i_flag. */
#define	IN_ACCESS	0x00000001		/* Access time update request. */
#define	IN_CHANGE	0x00000002		/* Inode change time update request. */
#define	IN_UPDATE	0x00000004		/* Modification time update request. */
#define	IN_MODIFIED	0x00000008		/* Inode has been modified. */
#define	IN_RENAME	0x00000010		/* Inode is being renamed. */
#define	IN_HASHED	0x00000020		/* Inode is on hash list */
#define	IN_LAZYMOD	0x00000040		/* Modified, but don't write yet. */
#define	IN_SPACECOUNTED	0x00000080	/* Blocks to be freed in free count. */
#define	IN_DX_UPDATE	0x00000100	/* In-core dir index needs to be sync'd with disk */
#define IN_INIT		0x00000200		/* inode is being created */
#define IN_INITWAIT 0x00000400		/* waiting for creation */
/* These are used to protect inode lookup cache members w/o having to hold the lock */
#define IN_LOOK     0x00000800      /* dir lookup in progress */
#define IN_LOOKWAIT 0x00001000      /* waiting for lookup completion */

#ifdef _KERNEL
/*
 * Structure used to pass around logical block paths generated by
 * ext2_getlbns and used by truncate and bmap code.
 */
struct indir {
	ext2_daddr_t in_lbn;	/* Logical block number. */
	int	in_off;			/* Offset in buffer. */
	int	in_exists;		/* Flag if the block exists. */
};

/* Convert between inode pointers and vnode pointers. */
#define VTOI(vp)	((struct inode *)vnode_fsnode(vp))
#define ITOV(ip)	((ip)->i_vnode)
#define ITOVFS(ip)  (vnode_mount((ip)->i_vnode))
#ifdef EXT_KNOTE
#define VN_KNOTE(vp,hint) KNOTE(&(VTOI(vp))->i_knotes, (hint))
#else
#define VN_KNOTE(vp,hint)
#endif

/* Locking contract:
	1) If a function takes an inode, the inode is expected locked
	2) If a function takes a vnode, the inode is expected unlocked
	3) If locking 2 (or more) inodes, always lock from smallest to largest based on the inode #
	4) If locking an inode and the superblock, the inode must be locked before the sb
	5) Exceptions to the above are marked.
*/
#define EXT2_RW_ILOCK 0
#ifndef DIAGNOSTIC
/* Exclusive lock */
#define IXLOCK(ip) lck_mtx_lock((ip)->i_lock)
/* Shared lock - In case we actually switch to a r/w lock */
#define ISLOCK(ip) lck_mtx_lock((ip)->i_lock)
/* Unlock */
#define IULOCK(ip) lck_mtx_unlock((ip)->i_lock)
#define IASSERTLOCK(ip)
#else

static __inline__
void IXLOCK2(struct inode *ip, const char *file, int32_t line)
{
    assert(ip->i_lockowner != current_thread()); // lock recursion
    const char *f = e_strrchr(file, '/');
    if (f)
        f++;
    else
        f = file;
    size_t sz = strlen(f);
    if (sz >= sizeof(ip->i_lockfile))
        sz = sizeof(ip->i_lockfile)-1;
    lck_mtx_lock(ip->i_lock);
    bcopy(f, ip->i_lockfile, sz);
    ip->i_lockfile[sz] = 0;
    ip->i_lockline = line;
    ip->i_lockowner = current_thread();
}

#define IXLOCK(ip) IXLOCK2((ip), __FILE__, __LINE__)

static __inline__
void IULOCK(struct inode *ip)
{
    assert(ip->i_lockowner == current_thread());
    ip->i_lockowner = NULL;
    lck_mtx_unlock(ip->i_lock);
}

#define ISLOCK(ip) IXLOCK((ip))

#define IASSERTLOCK(ip) assert((ip)->i_lockowner == current_thread())

#endif // DIAGNOSTIC

static __inline__
#ifndef DIAGNOSTIC
void IXLOCK_WITH_LOCKED_INODE(struct inode *ip, struct inode *lip)
#else
void IXLOCK_WITH_LOCKED_INODE2(struct inode *ip, struct inode *lip, const char *file, int32_t line)
#define IXLOCK_WITH_LOCKED_INODE(ip, lip) IXLOCK_WITH_LOCKED_INODE2(ip, lip, __FILE__, __LINE__)
#endif
{
#ifdef DIAGNOSTIC
	IASSERTLOCK(lip);
    if (ip == lip || lip->i_number == ip->i_number)
		panic("ext2: attempt to lock duplicate nodes!");
#endif
	if (lip->i_number < ip->i_number) {
		#ifndef DIAGNOSTIC
		IXLOCK(ip);
		#else
		IXLOCK2(ip, file, line);
		#endif
		return;
	}
	
	IULOCK(lip);
	#ifndef DIAGNOSTIC
	IXLOCK(ip);
	IXLOCK(lip);
	#else
	IXLOCK2(ip, file, line);
	IXLOCK2(lip, file, line);
	#endif
}

#define IREF(ip) (void)OSIncrementAtomic((SInt32*)&ip->i_refct)
#define IRELSE(ip) (void)OSDecrementAtomic((SInt32*)&ip->i_refct)

static __inline__
int inosleep(struct inode *ip, void *chan, const char *wmsg, struct timespec *ts)
{
	int error;
	IREF(ip);
#ifdef DIAGNOSTIC
    assert(ip->i_lockowner == current_thread());
    ip->i_lockowner = NULL;
#endif
	error = msleep(chan, ip->i_lock, PINOD, wmsg, ts);
#ifdef DIAGNOSTIC
    ip->i_lockowner = current_thread();
#endif
	IRELSE(ip);
	return (error);
}
#define ISLEEP(ip, field, ts) inosleep((ip), &(ip)->i_ ## field, __FUNCTION__, (ts))

static __inline__
int inosleep_nolck(struct inode *ip, void *chan, const char *wmsg, struct timespec *ts)
{
	assert(ip->i_lockowner != current_thread());
    IREF(ip);
	int err = msleep(chan, NULL, PINOD, wmsg, ts);
    IRELSE(ip);
	return (err);
}
#define ISLEEP_NOLCK(ip, field, ts) inosleep_nolck((ip), &(ip)->i_ ## field, __FUNCTION__, (ts))

/* chan is the sleep/wake field, wakefield is the bitfield to test/clear */
#define IWAKE(ip, chan, wakefield, wakebit) do { \
	IASSERTLOCK(ip); \
	if ((ip)->i_ ## wakefield & (wakebit)) { \
		(ip)->i_ ## wakefield &= ~(wakebit); \
		wakeup(&(ip)->i_ ## chan); \
	} \
} while(0)

#ifdef DIAGNOSTIC
__private_extern__ u_int32_t lookwait;
#define lookw() OSIncrementAtomic((SInt32*)&lookwait)
#else
#define lookw()
#endif

/* This overlays the fid structure (see mount.h). */
struct ufid {
	u_int16_t ufid_len;	/* Length of structure. */
	u_int16_t ufid_pad;	/* Force 32-bit alignment. */
	ino_t	  ufid_ino;	/* File number (ino). */
	int32_t	  ufid_gen;	/* Generation number. */
};
#endif /* _KERNEL */

#endif /* !_SYS_GNU_EXT2FS_INODE_H_ */
