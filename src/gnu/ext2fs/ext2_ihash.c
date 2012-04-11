/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993, 1995
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
 *	@(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_ihash.c,v 1.36 2002/10/14 03:20:34 mckusick Exp $
 */
 
static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_ihash.c,v 1.14 2006/08/22 00:30:19 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include "ext2_apple.h"

#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_extern.h>

#ifndef APPLE
static MALLOC_DEFINE(M_EXT2IHASH, "EXT2 ihash", "EXT2 Inode hash tables");
#else
#define M_EXT2IHASH M_TEMP
#endif
/*
 * Structures associated with inode caching.
 */
static LIST_HEAD(ihashhead, inode) *ihashtbl;
static u_long	ihash;		/* size of hash table - 1 */
#define	INOHASH(device, inum)	(&ihashtbl[(minor(device) + (inum)) & ihash])
static lck_mtx_t *ext2_ihash_lock;

#define hlock() do {lck_mtx_lock(ext2_ihash_lock);} while(0)
#define hulock() do {lck_mtx_unlock(ext2_ihash_lock);} while(0)

/*
 * Initialize inode hash table.
 */
void
ext2_ihashinit()
{

	KASSERT(ihashtbl == NULL, ("ext2_ihashinit called twice"));
	ext2_ihash_lock = lck_mtx_alloc_init(EXT2_LCK_GRP, LCK_ATTR_NULL);
	ihashtbl = hashinit(desiredvnodes, M_EXT2IHASH, &ihash);
}

/*
 * Destroy the inode hash table.
 */
void
ext2_ihashuninit()
{
	hlock();
	hashdestroy(ihashtbl, M_EXT2IHASH, ihash);
	ihashtbl = NULL;
	hulock();
	lck_mtx_free(ext2_ihash_lock, EXT2_LCK_GRP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
vnode_t 
ext2_ihashlookup(dev, inum)
	dev_t dev;
	ino_t inum;
{
	struct inode *ip;

	hlock();
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash)
	if (inum == ip->i_number && dev == ip->i_dev)
		break;
	hulock();

	if (ip)
		return (ITOV(ip));
	return (NULLVP);
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */
int
ext2_ihashget(dev, inum, flags, vpp)
	dev_t dev;
	ino_t inum;
	int flags;
	vnode_t *vpp;
{
	struct inode *ip;
	vnode_t vp;
	uint32_t vid;
	int error;

	*vpp = NULL;
loop:
	hlock();
	LIST_FOREACH(ip, INOHASH(dev, inum), i_hash) {
		if (inum == ip->i_number && dev == ip->i_dev) {
			hulock();
			
			error = 0;
			IXLOCK(ip);
			while (ip->i_flag & IN_INIT) {
				ip->i_flag |= IN_INITWAIT;
				error = ISLEEP(ip, flag, NULL);
			}
			// If we are no longer on the hash chain, init failed
			if (!error && 0 == (ip->i_flag & IN_HASHED))
				error = EIO;
				
			vp = ITOV(ip);
			vid = vnode_vid(vp);
			IULOCK(ip);
			
			if (error)
				return (error);
			
			error = vnode_getwithvid(vp, vid);
			if (error == ENOENT)
				goto loop; // vnode has changed identity
			if (error)
				return (error);
			if (ip != VTOI(vp))
				goto loop;
			
			*vpp = vp;
			return (0);
		}
	}
	hulock();
	return (0);
}

/*
 * Insert the inode into the hash table, and return it locked.
 */
void
ext2_ihashins(ip)
	struct inode *ip;
{
	struct ihashhead *ipp;

	hlock();
	ipp = INOHASH(ip->i_dev, ip->i_number);
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
	hulock();
}

/*
 * Remove the inode from the hash table.
 */
void
ext2_ihashrem(ip)
	struct inode *ip;
{
	hlock();
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
	}
	hulock();
}
