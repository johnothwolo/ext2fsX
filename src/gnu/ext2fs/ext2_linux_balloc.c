/*
 *  modified for Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 *
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_linux_balloc.c,v 1.18 2002/12/30 21:18:08 schweikh Exp $
 */
/*
 *  linux/fs/ext2/balloc.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  Enhanced block allocation by Stephen Tweedie (sct@dcs.ed.ac.uk), 1993
 */

/*
 * The free blocks are managed by bitmaps.  A file system contains several
 * blocks groups.  Each group contains 1 bitmap block for blocks, 1 bitmap
 * block for inodes, N blocks for the inode table and data blocks.
 *
 * The file system contains group descriptors which are located after the
 * super block.  Each descriptor contains the number of the bitmap block and
 * the free blocks count in the block.  The descriptors are loaded in memory
 * when a file system is mounted (see ext2_read_super).
 */
 
static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_linux_balloc.c,v 1.18 2006/08/27 15:32:37 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
//#include <machine/spl.h>

#include "ext2_apple.h"

#include <gnu/ext2fs/ext2_fs.h>
#include <ext2_byteorder.h>
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/fs.h>

#if defined (__i386__) || defined (__x86_64__)
#include <gnu/ext2fs/i386-bitops.h>
#elif __alpha__
#include <gnu/ext2fs/alpha-bitops.h>
#elif __ppc__
#include <gnu/ext2fs/ppc-bitops.h>
#else
#error Provide a bitops.h file, please!
#endif

#define in_range(b, first, len)		((b) >= (first) && (b) <= (first) + (len) - 1)

/* got rid of get_group_desc since it can already be found in 
 * ext2_linux_ialloc.c
 */

static void read_block_bitmap (mount_t mp,
	unsigned int block_group,
	unsigned long bitmap_nr
#if !EXT2_SB_BITMAP_CACHE
	, buf_t *bpp
#endif
	)
{
	struct ext2mount *ump = VFSTOEXT2(mp);
	struct m_ext2fs *sb = ump->um_e2fs;
	struct ext2_group_desc * gdp;
	buf_t bp;
	int    error;
	
	gdp = get_group_desc(mp, block_group, NULL);
	
	ext2_daddr_t lbn = le32_to_cpu(gdp->bg_block_bitmap);
	unlock_super(sb);
	if ((error = buf_meta_bread (ump->um_devvp,
		(daddr64_t)fsbtodb(sb, lbn),
		sb->s_blocksize, NOCRED, &bp)) != 0)
		panic ( "ext2: read_block_bitmap: "
			    "Cannot read block bitmap - "
			    "block_group = %d, block_bitmap = %lu",
			    block_group, (unsigned long)lbn);
	lock_super(sb);
#if EXT2_SB_BITMAP_CACHE
	sb->s_block_bitmap_number[bitmap_nr] = block_group;
	sb->s_block_bitmap[bitmap_nr] = bp;
	LCK_BUF(bp)
#else
   *bpp = bp;
#endif
}

/*
 * load_block_bitmap loads the block bitmap for a blocks group
 *
 * It maintains a cache for the last bitmaps loaded.  This cache is managed
 * with a LRU algorithm.
 *
 * Notes:
 * 1/ There is one cache per mounted file system.
 * 2/ If the file system contains less than EXT2_MAX_GROUP_LOADED groups,
 *    this function reads the bitmap without maintaining a LRU cache.
 */
static int load__block_bitmap (mount_t mp,
	unsigned int block_group
#if !EXT2_SB_BITMAP_CACHE
	, buf_t *bpp
#endif
	)
{
	int i, j;
	struct m_ext2fs *sb = VFSTOEXT2(mp)->um_e2fs;
	unsigned long block_bitmap_number;
	buf_t block_bitmap;

	if (block_group >= sb->s_groups_count)
		panic ( "ext2: load_block_bitmap: "
			    "block_group >= groups_count - "
			    "block_group = %d, groups_count = %lu",
			    block_group, sb->s_groups_count);

	if (sb->s_groups_count <= EXT2_MAX_GROUP_LOADED) {
		if (sb->s_block_bitmap[block_group]) {
			if (sb->s_block_bitmap_number[block_group] !=
			    block_group)
				panic ( "ext2: load_block_bitmap: "
					    "block_group != block_bitmap_number");
			else
				return block_group;
		} else {
      #if EXT2_SB_BITMAP_CACHE
			read_block_bitmap (mp, block_group, block_group);
      #else
			read_block_bitmap (mp, block_group, block_group, bpp);
      #endif
			return (block_group);
		}
	}

	for (i = 0; i < sb->s_loaded_block_bitmaps &&
		    sb->s_block_bitmap_number[i] != block_group; i++)
		;
	if (i < sb->s_loaded_block_bitmaps &&
  	    sb->s_block_bitmap_number[i] == block_group) {
		block_bitmap_number = sb->s_block_bitmap_number[i];
		block_bitmap = sb->s_block_bitmap[i];
		for (j = i; j > 0; j--) {
			sb->s_block_bitmap_number[j] =
				sb->s_block_bitmap_number[j - 1];
			sb->s_block_bitmap[j] =
				sb->s_block_bitmap[j - 1];
		}
		sb->s_block_bitmap_number[0] = block_bitmap_number;
		sb->s_block_bitmap[0] = block_bitmap;
	} else {
#if EXT2_SB_BITMAP_CACHE
		if (sb->s_loaded_block_bitmaps < EXT2_MAX_GROUP_LOADED)
			sb->s_loaded_block_bitmaps++;
		else
			ULCK_BUF(sb->s_block_bitmap[EXT2_MAX_GROUP_LOADED - 1])

		for (j = sb->s_loaded_block_bitmaps - 1; j > 0;  j--) {
			sb->s_block_bitmap_number[j] =
				sb->s_block_bitmap_number[j - 1];
			sb->s_block_bitmap[j] =
				sb->s_block_bitmap[j - 1];
		}
		read_block_bitmap (mp, block_group, 0);
#else
      read_block_bitmap (mp, block_group, block_group, bpp);
#endif /* EXT2_SB_BITMAP_CACHE */
	}
	return (0);
}


// super block must be locked
static __inline int load_block_bitmap (mount_t   mp,
	unsigned int block_group
#if !EXT2_SB_BITMAP_CACHE
	, buf_t *bpp
#endif
	)
{
#if EXT2_SB_BITMAP_CACHE
	struct ext2_sb_info *sb = VFSTOEXT2(mp)->um_e2fs;
	if (sb->s_loaded_block_bitmaps > 0 &&
	    sb->s_block_bitmap_number[0] == block_group)
		return 0;
	
	if (sb->s_groups_count <= EXT2_MAX_GROUP_LOADED && 
	    sb->s_block_bitmap_number[block_group] == block_group &&
	    sb->s_block_bitmap[block_group]) 
		return block_group;

	return load__block_bitmap (mp, block_group);
#else
   return load__block_bitmap (mp, block_group, bpp);
#endif
}

void ext2_free_blocks (mount_t mp, unsigned long block,
	unsigned long count)
{
	struct m_ext2fs *sb = VFSTOEXT2(mp)->um_e2fs;
	buf_t bp;
	buf_t bp2;
	unsigned long block_group;
	unsigned long bit;
	unsigned long i;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2fs * es = sb->s_es;

	if (!sb) {
		ext2_debug ("ext2_free_blocks: nonexistent device");
		return;
	}
	lock_super (sb);
	if (block < le32_to_cpu(es->s_first_data_block) || 
	    (block + count) > le32_to_cpu(es->s_blocks_count)) {
		ext2_debug( "ext2_free_blocks: "
			    "Freeing blocks not in datazone - "
			    "block = %lu, count = %lu", block, count);
		unlock_super (sb);
		return;
	}

	ext2_debug("ext2_free_blocks: freeing blocks %lu to %lu\n", block, block+count-1);

	block_group = (block - le32_to_cpu(es->s_first_data_block)) /
		      EXT2_BLOCKS_PER_GROUP(sb);
	bit = (block - le32_to_cpu(es->s_first_data_block)) % EXT2_BLOCKS_PER_GROUP(sb);
	if (bit + count > EXT2_BLOCKS_PER_GROUP(sb))
		panic ( "ext2_free_blocks: "
			    "Freeing blocks across group boundary - "
			    "Block = %lu, count = %lu",
			    block, count);
#if EXT2_SB_BITMAP_CACHE
	bitmap_nr = load_block_bitmap (mp, block_group);
	bp = sb->s_block_bitmap[bitmap_nr];
#else
   bitmap_nr = load_block_bitmap (mp, block_group, &bp);
#endif
	gdp = get_group_desc (mp, block_group, &bp2);

	if (/* test_opt (sb, CHECK_STRICT) && 	assume always strict ! */
	    (in_range (le32_to_cpu(gdp->bg_block_bitmap), block, count) ||
	     in_range (le32_to_cpu(gdp->bg_inode_bitmap), block, count) ||
	     in_range (block, le32_to_cpu(gdp->bg_inode_table),
		       sb->e2fs_itpg) ||
	     in_range (block + count - 1, le32_to_cpu(gdp->bg_inode_table),
		       sb->e2fs_itpg)))
		panic ( "ext2_free_blocks: "
			    "Freeing blocks in system zones - "
			    "Block = %lu, count = %lu",
			    block, count);

	for (i = 0; i < count; i++) {
      if (!ext2_clear_bit (bit + i, (caddr_t)buf_dataptr(bp)))
			ext2_debug ("ext2_free_blocks: "
				      "bit already cleared for block %lu", 
				      block);
		else {
			gdp->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count) + 1);
			es->s_free_blocks_count = cpu_to_le32(le32_to_cpu(es->s_free_blocks_count) + 1);
		}
	}

	mark_buffer_dirty(bp2);
#if EXT2_SB_BITMAP_CACHE
	mark_buffer_dirty(bp);
#endif
/****
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bp);
		wait_on_buffer (bp);
	}
****/
	sb->s_dirt = 1;
	unlock_super (sb);
#if !EXT2_SB_BITMAP_CACHE
	if (0 == vfs_issynchronous(mp))
		buf_bdwrite(bp);
	else
		buf_bwrite(bp);
#endif
	return;
}

/*
 * ext2_new_block uses a goal block to assist allocation.  If the goal is
 * free, or there is a free block within 32 blocks of the goal, that block
 * is allocated.  Otherwise a forward search is made for a free block; within 
 * each block group the search first looks for an entire free byte in the block
 * bitmap, and then for any free bit if that fails.
 */
int ext2_new_block (mount_t mp, unsigned long goal,
		    u_int32_t * prealloc_count,
		    u_int32_t * prealloc_block)
{
	struct m_ext2fs *sb = VFSTOEXT2(mp)->um_e2fs;
	buf_t bp = NULL;
	buf_t bp2;
	char *p, *r, *data;
	int i, j, k, tmp, sync;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	struct ext2fs * es = sb->s_es;

#if defined(EXT2FS_DEBUG) && EXT2FS_DEBUG > 1
	static int goal_hits = 0, goal_attempts = 0;
#endif
	if (!sb) {
		ext2_debug ("ext2_new_block: nonexistent device");
		return 0;
	}
	lock_super (sb);

   ext2_debug ("ext2_new_block: goal=%lu.\n", goal);

repeat:
	/*
	 * First, test whether the goal block is free.
	 */
	if (goal < le32_to_cpu(es->s_first_data_block) || goal >= le32_to_cpu(es->s_blocks_count))
		goal = le32_to_cpu(es->s_first_data_block);
	i = (goal - le32_to_cpu(es->s_first_data_block)) / EXT2_BLOCKS_PER_GROUP(sb);
	gdp = get_group_desc (mp, i, &bp2);
	if (gdp->bg_free_blocks_count > 0) {
		j = ((goal - le32_to_cpu(es->s_first_data_block)) % EXT2_BLOCKS_PER_GROUP(sb));
#if defined(EXT2FS_DEBUG) && EXT2FS_DEBUG > 1
		if (j)
			goal_attempts++;
#endif
#if EXT2_SB_BITMAP_CACHE
		bitmap_nr = load_block_bitmap (mp, i);
		bp = sb->s_block_bitmap[bitmap_nr];
#else
		bitmap_nr = load_block_bitmap (mp, i, &bp);
#endif

		ext2_debug ("ext2_new_block: goal is at %d:%d.\n", i, j); 

		if (!ext2_test_bit(j, (u_long*)buf_dataptr(bp))) {
#if defined(EXT2FS_DEBUG) && EXT2FS_DEBUG > 1
			goal_hits++;
			ext2_debug ("ext2_new_block: goal bit allocated.\n");
#endif
			goto got_block;
		}
		if (j) {
			/*
			 * The goal was occupied; search forward for a free 
			 * block within the next XX blocks.
			 *
			 * end_goal is more or less random, but it has to be
			 * less than EXT2_BLOCKS_PER_GROUP. Aligning up to the
			 * next 64-bit boundary is simple..
			 */
			int end_goal = (j + 63) & ~63;
			j = ext2_find_next_zero_bit((void*)buf_dataptr(bp), end_goal, j);
			if (j < end_goal)
				goto got_block;
		}
      
		ext2_debug ("ext2_new_block: Bit not found near goal\n");

		/*
		 * There has been no free block found in the near vicinity
		 * of the goal: do a search forward through the block groups,
		 * searching in each group first for an entire free byte in
		 * the bitmap and then for any free bit.
		 * 
		 * Search first in the remainder of the current group; then,
		 * cyclicly search through the rest of the groups.
		 */
		data = (char *)buf_dataptr(bp);
		p = data + (j >> 3);
		r = memscan(p, 0, (EXT2_BLOCKS_PER_GROUP(sb) - j + 7) >> 3);
		k = (r - data) << 3;
		if (k < EXT2_BLOCKS_PER_GROUP(sb)) {
			j = k;
			goto search_back;
		}
		k = ext2_find_next_zero_bit ((unsigned long *)data, 
					EXT2_BLOCKS_PER_GROUP(sb),
					j);
		data = NULL;
		if (k < EXT2_BLOCKS_PER_GROUP(sb)) {
			j = k;
			goto got_block;
		}
	}

#if !EXT2_SB_BITMAP_CACHE
	if (bp)
		buf_brelse(bp);
#endif
	ext2_debug ("ext2_new_block: Bit not found in block group %d.\n", i); 

	/*
	 * Now search the rest of the groups.  We assume that 
	 * i and gdp correctly point to the last group visited.
	 */
	for (k = 0; k < sb->s_groups_count; k++) {
		i++;
		if (i >= sb->s_groups_count)
			i = 0;
		gdp = get_group_desc (mp, i, &bp2);
		if (gdp->bg_free_blocks_count > 0)
			break;
	}
	if (k >= sb->s_groups_count) {
		unlock_super (sb);
		return 0;
	}
#if EXT2_SB_BITMAP_CACHE
	bitmap_nr = load_block_bitmap (mp, i);
	bp = sb->s_block_bitmap[bitmap_nr];
#else
   bitmap_nr = load_block_bitmap (mp, i, &bp);
#endif
	data = (char*)buf_dataptr(bp);
	r = memscan(data, 0, EXT2_BLOCKS_PER_GROUP(sb) >> 3);
	j = (r - data) << 3;

	if (j < EXT2_BLOCKS_PER_GROUP(sb))
		goto search_back;
	else
		j = ext2_find_first_zero_bit ((unsigned long *)data,
					 EXT2_BLOCKS_PER_GROUP(sb));
	data = NULL;
	if (j >= EXT2_BLOCKS_PER_GROUP(sb)) {
		ext2_debug ( "ext2_new_block: "
			 "Free blocks count corrupted for block group %d", i);
		unlock_super (sb);
#if !EXT2_SB_BITMAP_CACHE
		buf_brelse(bp);
#endif
		return (0);
	}

search_back:
	/* 
	 * We have succeeded in finding a free byte in the block
	 * bitmap.  Now search backwards up to 7 bits to find the
	 * start of this group of free blocks.
	 */
	data = (char*)buf_dataptr(bp);
	for (k = 0; k < 7 && j > 0 && !ext2_test_bit (j - 1, (u_long*)data); k++, j--);
	
got_block:

	ext2_debug ("using block group %d(%d)\n", i, le16_to_cpu(gdp->bg_free_blocks_count));

	tmp = j + i * EXT2_BLOCKS_PER_GROUP(sb) + le32_to_cpu(es->s_first_data_block);

	if (/* test_opt (sb, CHECK_STRICT) && we are always strict. */
	    (tmp == le32_to_cpu(gdp->bg_block_bitmap) ||
	     tmp == le32_to_cpu(gdp->bg_inode_bitmap) ||
	     in_range (tmp, le32_to_cpu(gdp->bg_inode_table), sb->e2fs_itpg))) {
		panic ( "ext2_new_block: "
			    "Allocating block in system zone - "
			    "%dth block = %u in group %u", j, tmp, i);
      /* Instead of panicing, Linux does the following (after logging the panic msg) */
      /*
      ext2_set_bit(j, bp->b_data);
#if !EXT2_SB_BITMAP_CACHE
      brelse(bp);
		bp = NULL;
#endif
		goto repeat;
      */
   }
   
   if (ext2_set_bit (j, (char*)buf_dataptr(bp))) {
		ext2_debug ( "ext2_new_block: "
			 "bit already set for block %d", j);
#if !EXT2_SB_BITMAP_CACHE
		buf_brelse(bp);
		bp = NULL;
#endif
		goto repeat;
	}

	ext2_debug ("ext2_new_block: found bit %d\n", j);

	/*
	 * Do block preallocation now if required.
	 */
#ifdef EXT2_PREALLOCATE
	if (prealloc_block) {
		int prealloc_goal;
		*prealloc_count = 0;
		*prealloc_block = tmp + 1;
      
		prealloc_goal = es->s_prealloc_blocks ?
			es->s_prealloc_blocks : EXT2_DEFAULT_PREALLOC_BLOCKS;
      
		for (k = 1;
		     k < prealloc_goal && (j + k) < EXT2_BLOCKS_PER_GROUP(sb); k++) {
         if (ext2_set_bit (j + k, (char*)buf_dataptr(bp)))
				break;
			(*prealloc_count)++;
		}
		assert(*prealloc_count < 0x10000);
      	
		gdp->bg_free_blocks_count =
		cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count) - *prealloc_count);
		es->s_free_blocks_count =
		cpu_to_le32(le32_to_cpu(es->s_free_blocks_count) - *prealloc_count);
		ext2_debug ("ext2_new_block: Preallocated a further %lu bits.\n",
			    *((unsigned long*)prealloc_count)); 
	}
#endif

	j = tmp;

#if EXT2_SB_BITMAP_CACHE
	mark_buffer_dirty(bp);
#endif
/****
	if (sb->s_flags & MS_SYNCHRONOUS) {
		ll_rw_block (WRITE, 1, &bp);
		wait_on_buffer (bp);
	}
****/
	sync = vfs_issynchronous(mp);
	if (j >= le32_to_cpu(es->s_blocks_count)) {
		ext2_debug ( "ext2_new_block: "
			    "block(%d) >= blocks count(%d) - "
			    "block_group = %d", j, le32_to_cpu(es->s_blocks_count), i);
		unlock_super (sb);

#if !EXT2_SB_BITMAP_CACHE
		if (0 == sync)
			buf_bdwrite(bp);
		else
			buf_bwrite(bp);
#endif
		return (0);
	}

#if defined(EXT2FS_DEBUG) && EXT2FS_DEBUG > 1
	ext2_debug ("ext2_new_block: allocating block %d. "
		    "Goal hits %d of %d.\n", j, goal_hits, goal_attempts);
#endif

	gdp->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(gdp->bg_free_blocks_count) - 1);
	mark_buffer_dirty(bp2);
	es->s_free_blocks_count = cpu_to_le32(le32_to_cpu(es->s_free_blocks_count) - 1);
	sb->s_dirt = 1;
	unlock_super (sb);
#if !EXT2_SB_BITMAP_CACHE
	if (0 == sync)
		buf_bdwrite(bp);
	else
		buf_bwrite(bp);
#endif
	return j;
}

#ifdef unused
static unsigned long ext2_count_free_blocks (mount_t   mp)
{
	struct ext2_sb_info *sb = VFSTOEXT2(mp)->um_e2fs;
#if defined(EXT2FS_DEBUG) && EXT2FS_DEBUG > 1
	struct ext2_super_block * es;
	unsigned long desc_count, bitmap_count, x;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	int i;
	
	lock_super (sb);
	es = sb->s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	for (i = 0; i < sb->s_groups_count; i++) {
		gdp = get_group_desc (mp, i, NULL);
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		bitmap_nr = load_block_bitmap (mp, i);
		x = ext2_count_free (sb->s_block_bitmap[bitmap_nr],
				     sb->s_blocksize);
		ext2_debug ("group %d: stored = %d, counted = %lu\n",
			i, le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	ext2_debug( "stored = %lu, computed = %lu, %lu\n",
	       le32_to_cpu(es->s_free_blocks_count), desc_count, bitmap_count);
	unlock_super (sb);
	return bitmap_count;
#else
	lock_super(sb);
	typeof(sb->s_es->s_free_blocks_count) ct = sb->s_es->s_free_blocks_count;
	unlock_super(sb);
	return le32_to_cpu(ct);
#endif
}

static __inline int block_in_use (unsigned long block,
				  struct ext2_sb_info * sb,
				  unsigned char * map)
{
	return ext2_test_bit ((block - le32_to_cpu(sb->s_es->s_first_data_block)) %
			 EXT2_BLOCKS_PER_GROUP(sb), (u_long*)map);
}
#endif /* unused */

static int test_root(int a, int b)
{
	if (a == 0)
		return 1;
	while (1) {
		if (a == 1)
			return 1;
		if (a % b)
			return 0;
		a = a / b;
	}
}

int ext2_group_sparse(int group)
{
	return (test_root(group, 3) || test_root(group, 5) ||
		test_root(group, 7));
}

#ifdef unused
static void ext2_check_blocks_bitmap (mount_t   mp)
{
	struct ext2_sb_info *sb = VFSTOEXT2(mp)->um_e2fs;
	buf_t * bp;
	struct ext2_super_block * es;
	unsigned long desc_count, bitmap_count, x;
	unsigned long desc_blocks;
	int bitmap_nr;
	struct ext2_group_desc * gdp;
	int i, j;

	lock_super (sb);
	es = sb->s_es;
	desc_count = 0;
	bitmap_count = 0;
	gdp = NULL;
	desc_blocks = (sb->s_groups_count + EXT2_DESC_PER_BLOCK(sb) - 1) /
		      EXT2_DESC_PER_BLOCK(sb);
	for (i = 0; i < sb->s_groups_count; i++) {
		gdp = get_group_desc (mp, i, NULL);
		desc_count += le16_to_cpu(gdp->bg_free_blocks_count);
		bitmap_nr = load_block_bitmap (mp, i);
		bp = sb->s_block_bitmap[bitmap_nr];

		if (!(le32_to_cpu(es->s_feature_ro_compat) &
		     EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER) ||
		    ext2_group_sparse(i)) {
			if (!ext2_test_bit (0, bp->b_data))
				ext2_debug ("ext2_check_blocks_bitmap: "
					    "Superblock in group %d "
					    "is marked free", i);

			for (j = 0; j < desc_blocks; j++)
				if (!ext2_test_bit (j + 1, bp->b_data))
					ext2_debug ("ext2_check_blocks_bitmap: "
					    "Descriptor block #%d in group "
					    "%d is marked free", j, i);
		}

		if (!block_in_use (le32_to_cpu(gdp->bg_block_bitmap), sb, bp->b_data))
			ext2_debug ("ext2_check_blocks_bitmap: "
				    "Block bitmap for group %d is marked free",
				    i);

		if (!block_in_use (le32_to_cpu(gdp->bg_inode_bitmap), sb, bp->b_data))
			ext2_debug ("ext2_check_blocks_bitmap: "
				    "Inode bitmap for group %d is marked free",
				    i);

		for (j = 0; j < sb->s_itb_per_group; j++)
			if (!block_in_use (le32_to_cpu(gdp->bg_inode_table) + j, sb, bp->b_data))
				ext2_debug ("ext2_check_blocks_bitmap: "
					    "Block #%d of the inode table in "
					    "group %d is marked free", j, i);

		x = ext2_count_free (bp, sb->s_blocksize);
		if (le16_to_cpu(gdp->bg_free_blocks_count) != x)
			ext2_debug ("ext2_check_blocks_bitmap: "
				    "Wrong free blocks count for group %d, "
				    "stored = %d, counted = %lu", i,
				    le16_to_cpu(gdp->bg_free_blocks_count), x);
		bitmap_count += x;
	}
	if (le32_to_cpu(es->s_free_blocks_count) != bitmap_count)
		ext2_debug ("ext2_check_blocks_bitmap: "
			    "Wrong free blocks count in super block, "
			    "stored = %lu, counted = %lu",
			    (unsigned long) le32_to_cpu(es->s_free_blocks_count), bitmap_count);
	unlock_super (sb);
}

/*
 *  this function is taken from 
 *  linux/fs/ext2/bitmap.c
 */

static int nibblemap[] = {4, 3, 3, 2, 3, 2, 2, 1, 3, 2, 2, 1, 2, 1, 1, 0};

unsigned long ext2_count_free (buf_t map, unsigned int numchars)
{
        unsigned int i;
        unsigned long sum = 0;
		char *data = (char*)buf_dataptr(map);

        if (!map)
                return (0);
        for (i = 0; i < numchars; i++)
                sum += nibblemap[data[i] & 0xf] +
                        nibblemap[(data[i] >> 4) & 0xf];
        return (sum);
}
#endif /* unused */
