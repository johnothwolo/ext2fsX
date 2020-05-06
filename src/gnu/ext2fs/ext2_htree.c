/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2010, 2012 Zheng Liu <lz@freebsd.org>
 * Copyright (c) 2012, Vyacheslav Matyushin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <machine/endian.h>
#include <sys/systm.h>
#include <sys/namei.h>
////#include <sys/bio.h>
#include <sys/buf.h>
#include <libkern/OSByteOrder.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
//#include <sys/sdt.h>
#include <sys/sysctl.h>

//#include "ufs/ufs/dir.h"

#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_dir.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_dinode.h>
//#include <gnu/ext2fs/htree.h>

static void	ext2_append_entry(char *block, uint32_t blksize,
		    struct ext2fs_direct_2 *last_entry,
		    struct ext2fs_direct_2 *new_entry, int csum_size);
static int	ext2_htree_append_block(struct vnode *vp, char *data,
		    struct componentname *cnp, uint32_t blksize);
static int	ext2_htree_check_next(struct inode *ip, uint32_t hash,
		    const char *name, struct ext2fs_htree_lookup_info *info);
static int	ext2_htree_cmp_sort_entry(const void *e1, const void *e2);
static int	ext2_htree_find_leaf(struct inode *ip, const char *name,
		    int namelen, uint32_t *hash, uint8_t *hash_version,
		    struct ext2fs_htree_lookup_info *info);
static uint32_t ext2_htree_get_block(struct ext2fs_htree_entry *ep);
static uint16_t	ext2_htree_get_count(struct ext2fs_htree_entry *ep);
static uint32_t ext2_htree_get_hash(struct ext2fs_htree_entry *ep);
static uint16_t	ext2_htree_get_limit(struct ext2fs_htree_entry *ep);
static void	ext2_htree_insert_entry_to_level(struct ext2fs_htree_lookup_level *level,
		    uint32_t hash, uint32_t blk);
static void	ext2_htree_insert_entry(struct ext2fs_htree_lookup_info *info,
		    uint32_t hash, uint32_t blk);
static uint32_t	ext2_htree_node_limit(struct inode *ip);
static void	ext2_htree_set_block(struct ext2fs_htree_entry *ep,
		    uint32_t blk);
static void	ext2_htree_set_count(struct ext2fs_htree_entry *ep,
		    uint16_t cnt);
static void	ext2_htree_set_hash(struct ext2fs_htree_entry *ep,
		    uint32_t hash);
static void	ext2_htree_set_limit(struct ext2fs_htree_entry *ep,
		    uint16_t limit);
static int	ext2_htree_split_dirblock(struct inode *ip,
		    char *block1, char *block2, uint32_t blksize,
		    uint32_t *hash_seed, uint8_t hash_version,
		    uint32_t *split_hash, struct  ext2fs_direct_2 *entry);
static void	ext2_htree_release(struct ext2fs_htree_lookup_info *info);
static uint32_t	ext2_htree_root_limit(struct inode *ip, int len);
static int	ext2_htree_writebuf(struct inode *ip,
		    struct ext2fs_htree_lookup_info *info);

int
ext2_htree_has_idx(struct inode *ip)
{
	if (EXT2_HAS_COMPAT_FEATURE(ip->i_e2fs, EXT3_FEATURE_COMPAT_DIR_INDEX) &&
	    ip->i_flag & IN_E3INDEX)
		return (1);
	else
		return (0);
}

static int
ext2_htree_check_next(struct inode *ip, uint32_t hash, const char *name,
    struct ext2fs_htree_lookup_info *info)
{
	struct vnode *vp = ITOV(ip);
	struct ext2fs_htree_lookup_level *level;
	struct buf *bp;
	uint32_t next_hash;
	int idx = info->h_levels_num - 1;
	int levels = 0;

	do {
		level = &info->h_levels[idx];
		level->h_entry++;
		if (level->h_entry < level->h_entries +
		    ext2_htree_get_count(level->h_entries))
			break;
		if (idx == 0)
			return (0);
		idx--;
		levels++;
	} while (1);

	next_hash = ext2_htree_get_hash(level->h_entry);
	if ((hash & 1) == 0) {
		if (hash != (next_hash & ~1))
			return (0);
	}

	while (levels > 0) {
		levels--;
		if (ext2_blkatoff(vp, ext2_htree_get_block(level->h_entry) *
		    ip->i_e2fs->s_blocksize, NULL, &bp) != 0)
			return (0);
		level = &info->h_levels[idx + 1];
		buf_brelse(level->h_bp);
		level->h_bp = bp;
		level->h_entry = level->h_entries =
		    ((struct ext2fs_htree_node *)buf_dataptr(bp))->h_entries;
	}

	return (1);
}


static uint32_t
ext2_htree_get_block(struct ext2fs_htree_entry *ep)
{
	return (ep->h_blk & 0x00FFFFFF);
}

static void
ext2_htree_set_block(struct ext2fs_htree_entry *ep, uint32_t blk)
{
	ep->h_blk = blk;
}

static uint16_t
ext2_htree_get_count(struct ext2fs_htree_entry *ep)
{
	return (((struct ext2fs_htree_count *)(ep))->h_entries_num);
}

static void
ext2_htree_set_count(struct ext2fs_htree_entry *ep, uint16_t cnt)
{
	((struct ext2fs_htree_count *)(ep))->h_entries_num = cnt;
}

static uint32_t
ext2_htree_get_hash(struct ext2fs_htree_entry *ep)
{
	return (ep->h_hash);
}

static uint16_t
ext2_htree_get_limit(struct ext2fs_htree_entry *ep)
{
	return (((struct ext2fs_htree_count *)(ep))->h_entries_max);
}

static void
ext2_htree_set_hash(struct ext2fs_htree_entry *ep, uint32_t hash)
{
	ep->h_hash = hash;
}

static void
ext2_htree_set_limit(struct ext2fs_htree_entry *ep, uint16_t limit)
{
	((struct ext2fs_htree_count *)(ep))->h_entries_max = limit;
}


static void
ext2_htree_release(struct ext2fs_htree_lookup_info *info)
{
	u_int i;

	for (i = 0; i < info->h_levels_num; i++) {
		struct buf *bp = info->h_levels[i].h_bp;

		if (bp != NULL)
			buf_brelse(bp);
	}
}

static uint32_t
ext2_htree_root_limit(struct inode *ip, int len)
{
	struct ext2_sb_info *fs;
	uint32_t space;

	fs = ip->i_e2fs;
	space = ip->i_e2fs->s_blocksize - EXT2_DIR_REC_LEN(1) -
	    EXT2_DIR_REC_LEN(2) - len;

	if (EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT4_FEATURE_RO_COMPAT_METADATA_CSUM))
		space -= sizeof(struct ext2fs_htree_tail);

	return (space / sizeof(struct ext2fs_htree_entry));
}

static uint32_t
ext2_htree_node_limit(struct inode *ip)
{
	struct ext2_sb_info *fs;
	uint32_t space;

	fs = ip->i_e2fs;
	space = fs->s_blocksize - EXT2_DIR_REC_LEN(0);

	if (EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT4_FEATURE_RO_COMPAT_METADATA_CSUM))
		space -= sizeof(struct ext2fs_htree_tail);

	return (space / sizeof(struct ext2fs_htree_entry));
}

static int
ext2_htree_find_leaf(struct inode *ip, const char *name, int namelen,
    uint32_t *hash, uint8_t *hash_ver,
    struct ext2fs_htree_lookup_info *info)
{
	struct vnode *vp;
	struct ext2_super_block *fs;
	struct ext2_sb_info *m_fs;
	struct buf *bp = NULL;
	struct ext2fs_htree_root *rootp;
	struct ext2fs_htree_entry *entp, *start, *end, *middle, *found;
	struct ext2fs_htree_lookup_level *level_info;
	uint32_t hash_major = 0, hash_minor = 0;
	uint32_t levels, cnt;
	uint8_t hash_version;

	if (name == NULL || info == NULL)
		return (-1);

	vp = ITOV(ip);
	fs = ip->i_e2fs->s_es;
	m_fs = ip->i_e2fs;

	if (ext2_blkatoff(vp, 0, NULL, &bp) != 0)
		return (-1);

	info->h_levels_num = 1;
	info->h_levels[0].h_bp = bp;
	rootp = (struct ext2fs_htree_root *)buf_dataptr(bp);
	if (rootp->h_info.h_hash_version != EXT2_HTREE_LEGACY &&
	    rootp->h_info.h_hash_version != EXT2_HTREE_HALF_MD4 &&
	    rootp->h_info.h_hash_version != EXT2_HTREE_TEA)
		goto error;

	hash_version = rootp->h_info.h_hash_version;
	if (hash_version <= EXT2_HTREE_TEA)
		hash_version += m_fs->s_def_hash_version;
	*hash_ver = hash_version;

	ext2_htree_hash(name, namelen, fs->s_hash_seed,
	    hash_version, &hash_major, &hash_minor);
	*hash = hash_major;

	if ((levels = rootp->h_info.h_ind_levels) > 1)
		goto error;

	entp = (struct ext2fs_htree_entry *)(((char *)&rootp->h_info) +
	    rootp->h_info.h_info_len);

	if (ext2_htree_get_limit(entp) !=
	    ext2_htree_root_limit(ip, rootp->h_info.h_info_len))
		goto error;

	while (1) {
		cnt = ext2_htree_get_count(entp);
		if (cnt == 0 || cnt > ext2_htree_get_limit(entp))
			goto error;

		start = entp + 1;
		end = entp + cnt - 1;
		while (start <= end) {
			middle = start + (end - start) / 2;
			if (ext2_htree_get_hash(middle) > hash_major)
				end = middle - 1;
			else
				start = middle + 1;
		}
		found = start - 1;

		level_info = &(info->h_levels[info->h_levels_num - 1]);
		level_info->h_bp = bp;
		level_info->h_entries = entp;
		level_info->h_entry = found;
		if (levels == 0)
			return (0);
		levels--;
		if (ext2_blkatoff(vp,
		    ext2_htree_get_block(found) * m_fs->s_blocksize,
		    NULL, &bp) != 0)
			goto error;
		entp = ((struct ext2fs_htree_node *)buf_dataptr(bp))->h_entries;
		info->h_levels_num++;
		info->h_levels[info->h_levels_num - 1].h_bp = bp;
	}

error:
	ext2_htree_release(info);
	return (-1);
}

/*
 * Try to lookup a directory entry in HTree index
 */
int
ext2_htree_lookup(struct inode *ip, const char *name, int namelen,
    struct buf **bpp, int *entryoffp, doff_t *offp,
    doff_t *prevoffp, doff_t *endusefulp,
    struct ext2fs_searchslot *ss)
{
	struct vnode *vp;
	struct ext2fs_htree_lookup_info info;
	struct ext2fs_htree_entry *leaf_node;
	struct ext2_sb_info *m_fs;
	struct buf *bp;
	uint32_t blk;
	uint32_t dirhash;
	uint32_t bsize;
	uint8_t hash_version;
	int search_next;
	int found = 0;

	m_fs = ip->i_e2fs;
	bsize = m_fs->s_blocksize;
	vp = ITOV(ip);

	/* TODO: print error msg because we don't lookup '.' and '..' */

	memset(&info, 0, sizeof(info));
	if (ext2_htree_find_leaf(ip, name, namelen, &dirhash,
	    &hash_version, &info))
		return (-1);

	do {
		leaf_node = info.h_levels[info.h_levels_num - 1].h_entry;
		blk = ext2_htree_get_block(leaf_node);
		if (ext2_blkatoff(vp, blk * bsize, NULL, &bp) != 0) {
			ext2_htree_release(&info);
			return (-1);
		}

		*offp = blk * bsize;
		*entryoffp = 0;
		*prevoffp = blk * bsize;
		*endusefulp = blk * bsize;

		if (ss->slotstatus == NONE) {
			ss->slotoffset = -1;
			ss->slotfreespace = 0;
		}
#pragma mark FIXME
		if (ext2_search_dirblock(ip, (ext2_daddr_t *)buf_dataptr(bp), &found,
		    name, namelen, entryoffp, offp, prevoffp,
		    endusefulp, ss) != 0) {
			buf_brelse(bp);
			ext2_htree_release(&info);
			return (-1);
		}

		if (found) {
			*bpp = bp;
			ext2_htree_release(&info);
			return (0);
		}

		buf_brelse(bp);
		search_next = ext2_htree_check_next(ip, dirhash, name, &info);
	} while (search_next);

	ext2_htree_release(&info);
	return (ENOENT);
}

