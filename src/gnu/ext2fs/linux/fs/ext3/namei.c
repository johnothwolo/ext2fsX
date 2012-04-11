/*
 *  linux/fs/ext3/namei.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/namei.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *  Directory entry file type support and forward compatibility hooks
 *  	for B-tree directories by Theodore Ts'o (tytso@mit.edu), 1998
 *  Hash Tree Directory indexing (c)
 *  	Daniel Phillips, 2001
 *  Hash Tree Directory indexing porting
 *  	Christopher Li, 2002
 *  Hash Tree Directory indexing cleanup
 * 	Theodore Ts'o, 2002
 */
/*
 * Darwin/BSD support (c) 2003 Brian Bergstrand
 */ 
static const char dxwhatid[] __attribute__ ((unused)) =
"@(#) $Id: namei.c,v 1.5 2006/08/19 21:31:30 bbergstrand Exp $";

#ifdef linux
typedef struct buffer_head *buf_t;
#define buf_dataptr(bh) (bh)->b_data
#define buf_size(bh) (bh)->b_size
#else
/* XXX - Define i_flags to i_e2flags -- this overrides access to
the real i_flags field in an inode.*/
#define i_flags i_e2flags
/* XXX - Same thing for i_ino */
#define i_ino i_number
#endif

static buf_t ext3_append(handle_t *handle,
					struct inode *inode,
					u32 *block, int *err)
{
	buf_t bh;

	*block = inode->i_size >> inode->i_sb->s_blocksize_bits;

	if ((bh = ext3_bread(handle, inode, *block, 1, err))) {
		inode->i_size += inode->i_sb->s_blocksize;
   #ifdef linux
		EXT3_I(inode)->i_disksize = inode->i_size;
   #endif
		ext3_journal_get_write_access(handle,bh);
	}
	return bh;
}

#ifndef swap
#define swap(x, y) do { typeof(x) z = x; x = y; y = z; } while (0)
#endif

typedef struct { u32 v; } le_u32;
typedef struct { u16 v; } le_u16;

#ifdef DX_DEBUG
#define dxtrace(command) command
#else
#define dxtrace(command) 
#endif

struct fake_dirent
{
	/*le*/u32 inode;
	/*le*/u16 rec_len;
	u8 name_len;
	u8 file_type;
};

struct dx_countlimit
{
	le_u16 limit;
	le_u16 count;
};

struct dx_entry
{
	le_u32 hash;
	le_u32 block;
};

/*
 * dx_root_info is laid out so that if it should somehow get overlaid by a
 * dirent the two low bits of the hash version will be zero.  Therefore, the
 * hash version mod 4 should never be 0.  Sincerely, the paranoia department.
 */

struct dx_root
{
	struct fake_dirent dot;
	char dot_name[4];
	struct fake_dirent dotdot;
	char dotdot_name[4];
	struct dx_root_info
	{
		le_u32 reserved_zero;
		u8 hash_version;
		u8 info_length; /* 8 */
		u8 indirect_levels;
		u8 unused_flags;
	}
	info;
	struct dx_entry	entries[0];
};

struct dx_node
{
	struct fake_dirent fake;
	struct dx_entry	entries[0];
};


struct dx_frame
{
	buf_t bh;
	struct dx_entry *entries;
	struct dx_entry *at;
};

struct dx_map_entry
{
	u32 hash;
	u32 offs;
};

static inline unsigned dx_get_block (struct dx_entry *entry);
static void dx_set_block (struct dx_entry *entry, unsigned value);
static inline unsigned dx_get_hash (struct dx_entry *entry);
static void dx_set_hash (struct dx_entry *entry, unsigned value);
static unsigned dx_get_count (struct dx_entry *entries);
static unsigned dx_get_limit (struct dx_entry *entries);
static void dx_set_count (struct dx_entry *entries, unsigned value);
static void dx_set_limit (struct dx_entry *entries, unsigned value);
static unsigned dx_root_limit (struct inode *dir, unsigned infosize);
static unsigned dx_node_limit (struct inode *dir);
static struct dx_frame *dx_probe(struct dentry *dentry,
				 struct inode *dir,
				 struct dx_hash_info *hinfo,
				 struct dx_frame *frame,
				 int *err);
static void dx_release (struct dx_frame *frames);
static int dx_make_map (struct ext3_dir_entry_2 *de, int size,
			struct dx_hash_info *hinfo, struct dx_map_entry map[]);
static void dx_sort_map(struct dx_map_entry *map, unsigned count);
static struct ext3_dir_entry_2 *dx_move_dirents (char *from, char *to,
		struct dx_map_entry *offsets, int count);
static struct ext3_dir_entry_2* dx_pack_dirents (char *base, int size);
static void dx_insert_block (struct dx_frame *frame, u32 hash, u32 block);
static int ext3_htree_next_block(struct inode *dir, __u32 hash,
				 struct dx_frame *frame,
				 struct dx_frame *frames, 
				 __u32 *start_hash);
static buf_t  ext3_dx_find_entry(struct dentry *dentry,
		       struct ext3_dir_entry_2 **res_dir, int *err);
static int ext3_dx_add_entry(handle_t *handle, struct dentry *dentry,
			     struct inode *inode);

/*
 * Future: use high four bits of block for coalesce-on-delete flags
 * Mask them off for now.
 */

static inline unsigned dx_get_block (struct dx_entry *entry)
{
	return le32_to_cpu(entry->block.v) & 0x00ffffff;
}

static inline void dx_set_block (struct dx_entry *entry, unsigned value)
{
	entry->block.v = cpu_to_le32(value);
}

static inline unsigned dx_get_hash (struct dx_entry *entry)
{
	return le32_to_cpu(entry->hash.v);
}

static inline void dx_set_hash (struct dx_entry *entry, unsigned value)
{
	entry->hash.v = cpu_to_le32(value);
}

static inline unsigned dx_get_count (struct dx_entry *entries)
{
	return le16_to_cpu(((struct dx_countlimit *) entries)->count.v);
}

static inline unsigned dx_get_limit (struct dx_entry *entries)
{
	return le16_to_cpu(((struct dx_countlimit *) entries)->limit.v);
}

static inline void dx_set_count (struct dx_entry *entries, unsigned value)
{
	((struct dx_countlimit *) entries)->count.v = cpu_to_le16(value);
}

static inline void dx_set_limit (struct dx_entry *entries, unsigned value)
{
	((struct dx_countlimit *) entries)->limit.v = cpu_to_le16(value);
}

static inline unsigned dx_root_limit (struct inode *dir, unsigned infosize)
{
	unsigned entry_space = dir->i_sb->s_blocksize - EXT3_DIR_REC_LEN(1) -
		EXT3_DIR_REC_LEN(2) - infosize;
	return 0? 20: entry_space / sizeof(struct dx_entry);
}

static inline unsigned dx_node_limit (struct inode *dir)
{
	unsigned entry_space = dir->i_sb->s_blocksize - EXT3_DIR_REC_LEN(0);
	return 0? 22: entry_space / sizeof(struct dx_entry);
}

/*
 * Debug
 */
#ifdef DX_DEBUG
static void dx_show_index (char * label, struct dx_entry *entries)
{
        int i, n = dx_get_count (entries);
        printk("%s index ", label);
        for (i = 0; i < n; i++)
        {
                printk("%x->%u ", i? dx_get_hash(entries + i): 0, dx_get_block(entries + i));
        }
        printk("\n");
}

struct stats
{ 
	unsigned names;
	unsigned space;
	unsigned bcount;
};

static struct stats dx_show_leaf(struct dx_hash_info *hinfo, struct ext3_dir_entry_2 *de,
				 int size, int show_names)
{
	unsigned names = 0, space = 0;
	char *base = (char *) de;
	struct dx_hash_info h = *hinfo;

	printk("names: ");
	while ((char *) de < base + size)
	{
		if (de->inode)
		{
			if (show_names)
			{
				int len = de->name_len;
				char *name = de->name;
				while (len--) printk("%c", *name++);
				ext3fs_dirhash(de->name, de->name_len, &h);
				printk(":%x.%u ", h.hash,
				       ((char *) de - base));
			}
			space += EXT3_DIR_REC_LEN(de->name_len);
	 		names++;
		}
		de = (struct ext3_dir_entry_2 *) ((char *) de + le16_to_cpu(de->rec_len));
	}
	printk("(%i)\n", names);
	return (struct stats) { names, space, 1 };
}

struct stats dx_show_entries(struct dx_hash_info *hinfo, struct inode *dir,
			     struct dx_entry *entries, int levels)
{
	unsigned blocksize = dir->i_sb->s_blocksize;
	unsigned count = dx_get_count (entries), names = 0, space = 0, i;
	unsigned bcount = 0;
	buf_t bh;
	int err;
	printk("%i indexed blocks...\n", count);
	for (i = 0; i < count; i++, entries++)
	{
		u32 block = dx_get_block(entries), hash = i? dx_get_hash(entries): 0;
		u32 range = i < count - 1? (dx_get_hash(entries + 1) - hash): ~hash;
		struct stats stats;
		printk("%s%3u:%03u hash %8x/%8x ",levels?"":"   ", i, block, hash, range);
		if (!(bh = ext3_bread (NULL,dir, block, 0,&err))) continue;
        stats = levels?
		   dx_show_entries(hinfo, dir, ((struct dx_node *) buf_dataptr(bh))->entries, levels - 1):
		   dx_show_leaf(hinfo, (struct ext3_dir_entry_2 *) buf_dataptr(bh), blocksize, 0);
		names += stats.names;
		space += stats.space;
		bcount += stats.bcount;
		brelse (bh);
	}
	if (bcount)
		printk("%snames %u, fullness %u (%u%%)\n", levels?"":"   ",
			names, space/bcount,(space/bcount)*100/blocksize);
	return (struct stats) { names, space, bcount};
}
#endif /* DX_DEBUG */

/*
 * Probe for a directory leaf block to search.
 *
 * dx_probe can return ERR_BAD_DX_DIR, which means there was a format
 * error in the directory index, and the caller should fall back to
 * searching the directory normally.  The callers of dx_probe **MUST**
 * check for this error code, and make sure it never gets reflected
 * back to userspace.
 */
static struct dx_frame *
dx_probe(struct dentry *dentry, struct inode *dir,
	 struct dx_hash_info *hinfo, struct dx_frame *frame_in, int *err)
{
	unsigned count, indirect;
	struct dx_entry *at, *entries, *p, *q, *m;
	struct dx_root *root;
	buf_t bh;
	struct dx_frame *frame = frame_in;
	u32 hash;

	frame->bh = NULL;
	if (dentry)
		dir = dentry->d_parent->d_inode;
	if (!(bh = ext3_bread (NULL,dir, 0, 0, err)))
		goto fail;
	root = (struct dx_root *) buf_dataptr(bh);
	if (root->info.hash_version != DX_HASH_TEA &&
	    root->info.hash_version != DX_HASH_HALF_MD4 &&
	    root->info.hash_version != DX_HASH_LEGACY) {
		ext3_warning(dir->i_sb, __FUNCTION__,
			     "Unrecognised inode hash code %d",
			     root->info.hash_version);
		brelse(bh);
		*err = ERR_BAD_DX_DIR;
		goto fail;
	}
	hinfo->hash_version = root->info.hash_version;
	hinfo->seed = EXT3_SB(dir->i_sb)->s_hash_seed;
	if (dentry)
		ext3fs_dirhash(dentry->d_name.name, dentry->d_name.len, hinfo);
	hash = hinfo->hash;

	if (root->info.unused_flags & 1) {
		ext3_warning(dir->i_sb, __FUNCTION__,
			     "Unimplemented inode hash flags: %#06x",
			     root->info.unused_flags);
		brelse(bh);
		*err = ERR_BAD_DX_DIR;
		goto fail;
	}

	if ((indirect = root->info.indirect_levels) > 1) {
		ext3_warning(dir->i_sb, __FUNCTION__,
			     "Unimplemented inode hash depth: %#06x",
			     root->info.indirect_levels);
		brelse(bh);
		*err = ERR_BAD_DX_DIR;
		goto fail;
	}

	entries = (struct dx_entry *) (((char *)&root->info) +
				       root->info.info_length);
	assert(dx_get_limit(entries) == dx_root_limit(dir,
						      root->info.info_length));
	dxtrace (printk("Look up %x", hash));
	while (1)
	{
		count = dx_get_count(entries);
		assert (count && count <= dx_get_limit(entries));
		p = entries + 1;
		q = entries + count - 1;
		while (p <= q)
		{
			m = p + (q - p)/2;
			dxtrace(printk("."));
			if (dx_get_hash(m) > hash)
				q = m - 1;
			else
				p = m + 1;
		}

		#if 0
        if (0) // linear search cross check
		{
			unsigned n = count - 1;
			at = entries;
			while (n--)
			{
				dxtrace(printk(","));
				if (dx_get_hash(++at) > hash)
				{
					at--;
					break;
				}
			}
			assert (at == p - 1);
		}
        #endif

		at = p - 1;
		dxtrace(printk(" %x->%u\n", at == entries? 0: dx_get_hash(at), dx_get_block(at)));
		frame->bh = bh;
		frame->entries = entries;
		frame->at = at;
		if (!indirect--) return frame;
		if (!(bh = ext3_bread (NULL,dir, dx_get_block(at), 0, err)))
			goto fail2;
		at = entries = ((struct dx_node *) buf_dataptr(bh))->entries;
		assert (dx_get_limit(entries) == dx_node_limit (dir));
		frame++;
	}
fail2:
	while (frame >= frame_in) {
		brelse(frame->bh);
		frame--;
	}
fail:
	return NULL;
}

static void dx_release (struct dx_frame *frames)
{
	if (frames[0].bh == NULL)
		return;

	if (((struct dx_root *) buf_dataptr(frames[0].bh))->info.indirect_levels)
		brelse(frames[1].bh);
	brelse(frames[0].bh);
}

/*
 * This function increments the frame pointer to search the next leaf
 * block, and reads in the necessary intervening nodes if the search
 * should be necessary.  Whether or not the search is necessary is
 * controlled by the hash parameter.  If the hash value is even, then
 * the search is only continued if the next block starts with that
 * hash value.  This is used if we are searching for a specific file.
 *
 * If the hash value is HASH_NB_ALWAYS, then always go to the next block.
 *
 * This function returns 1 if the caller should continue to search,
 * or 0 if it should not.  If there is an error reading one of the
 * index blocks, it will a negative error code.
 *
 * If start_hash is non-null, it will be filled in with the starting
 * hash of the next page.
 */
static int ext3_htree_next_block(struct inode *dir, __u32 hash,
				 struct dx_frame *frame,
				 struct dx_frame *frames, 
				 __u32 *start_hash)
{
	struct dx_frame *p;
	buf_t bh;
	int err, num_frames = 0;
	__u32 bhash;

	p = frame;
	/*
	 * Find the next leaf page by incrementing the frame pointer.
	 * If we run out of entries in the interior node, loop around and
	 * increment pointer in the parent node.  When we break out of
	 * this loop, num_frames indicates the number of interior
	 * nodes need to be read.
	 */
	while (1) {
		if (++(p->at) < p->entries + dx_get_count(p->entries))
			break;
		if (p == frames)
			return 0;
		num_frames++;
		p--;
	}

	/*
	 * If the hash is 1, then continue only if the next page has a
	 * continuation hash of any value.  This is used for readdir
	 * handling.  Otherwise, check to see if the hash matches the
	 * desired contiuation hash.  If it doesn't, return since
	 * there's no point to read in the successive index pages.
	 */
	bhash = dx_get_hash(p->at);
	if (start_hash)
		*start_hash = bhash;
	if ((hash & 1) == 0) {
		if ((bhash & ~1) != hash)
			return 0;
	}
	/*
	 * If the hash is HASH_NB_ALWAYS, we always go to the next
	 * block so no check is necessary
	 */
	while (num_frames--) {
		if (!(bh = ext3_bread(NULL, dir, dx_get_block(p->at),
				      0, &err)))
			return err; /* Failure */
		p++;
		brelse (p->bh);
		p->bh = bh;
		p->at = p->entries = ((struct dx_node *) buf_dataptr(bh))->entries;
	}
	return 1;
}


/*
 * p is at least 6 bytes before the end of page
 */
static inline struct ext3_dir_entry_2 *ext3_next_entry(struct ext3_dir_entry_2 *p)
{
	return (struct ext3_dir_entry_2 *)((char*)p + le16_to_cpu(p->rec_len));
}

/*
 * This function fills a red-black tree with information from a
 * directory block.  It returns the number directory entries loaded
 * into the tree.  If there is an error it is returned in err.
 */
static int htree_dirblock_to_tree(vnode_t dir_file,
				  struct inode *dir, int block,
				  struct dx_hash_info *hinfo,
				  __u32 start_hash, __u32 start_minor_hash)
{
	buf_t bh;
	struct ext3_dir_entry_2 *de, *top;
	int err, count = 0;

	dxtrace(printk("In htree dirblock_to_tree: block %d\n", block));
	if (!(bh = ext3_bread (NULL, dir, block, 0, &err)))
		return err;

	de = (struct ext3_dir_entry_2 *) buf_dataptr(bh);
	top = (struct ext3_dir_entry_2 *) ((char *) de +
					   dir->i_sb->s_blocksize -
					   EXT3_DIR_REC_LEN(0));
	for (; de < top; de = ext3_next_entry(de)) {
		ext3fs_dirhash(de->name, de->name_len, hinfo);
		if ((hinfo->hash < start_hash) ||
		    ((hinfo->hash == start_hash) &&
		     (hinfo->minor_hash < start_minor_hash)))
			continue;
		if ((err = ext3_htree_store_dirent(dir_file,
				   hinfo->hash, hinfo->minor_hash, de)) != 0) {
			brelse(bh);
			return err;
		}
		count++;
	}
	brelse(bh);
	return count;
}


/*
 * This function fills a red-black tree with information from a
 * directory.  We start scanning the directory in hash order, starting
 * at start_hash and start_minor_hash.
 *
 * This function returns the number of entries inserted into the tree,
 * or a negative error code.
 */
int ext3_htree_fill_tree(vnode_t dir_file, __u32 start_hash,
			 __u32 start_minor_hash, __u32 *next_hash)
{
	struct dx_hash_info hinfo;
	struct ext3_dir_entry_2 *de;
	struct dx_frame frames[2], *frame;
	struct inode *dir;
	int block, err;
	int count = 0;
	int ret;
	__u32 hashval;

	dxtrace(printk("In htree_fill_tree, start hash: %x:%x\n", start_hash,
		       start_minor_hash));
	dir = VTOI(dir_file);
	if (!(EXT3_I(dir)->i_flags & EXT3_INDEX_FL)) {
		hinfo.hash_version = EXT3_SB(dir->i_sb)->s_def_hash_version;
		hinfo.seed = EXT3_SB(dir->i_sb)->s_hash_seed;
		count = htree_dirblock_to_tree(dir_file, dir, 0, &hinfo,
					       start_hash, start_minor_hash);
		*next_hash = ~0;
		return count;
	}
	hinfo.hash = start_hash;
	hinfo.minor_hash = 0;
	frame = dx_probe(0, dir, &hinfo, frames, &err);
	if (!frame)
		return err;

	/* Add '.' and '..' from the htree header */
	if (!start_hash && !start_minor_hash) {
		de = (struct ext3_dir_entry_2 *) buf_dataptr(frames[0].bh);
		if ((err = ext3_htree_store_dirent(dir_file, 0, 0, de)) != 0)
			goto errout;
		de = ext3_next_entry(de);
		if ((err = ext3_htree_store_dirent(dir_file, 0, 0, de)) != 0)
			goto errout;
		count += 2;
	}

	while (1) {
		block = dx_get_block(frame->at);
		ret = htree_dirblock_to_tree(dir_file, dir, block, &hinfo,
					     start_hash, start_minor_hash);
		if (ret < 0) {
			err = ret;
			goto errout;
		}
		count += ret;
		hashval = ~0;
		ret = ext3_htree_next_block(dir, HASH_NB_ALWAYS, 
					    frame, frames, &hashval);
		*next_hash = hashval;
		if (ret < 0) {
			err = ret;
			goto errout;
		}
		/*
		 * Stop if:  (a) there are no more entries, or
		 * (b) we have inserted at least one entry and the
		 * next hash value is not a continuation
		 */
		if ((ret == 0) ||
		    (count && ((hashval & 1) == 0)))
			break;
	}
	dx_release(frames);
	dxtrace(printk("Fill tree: returned %d entries, next hash: %x\n", 
		       count, *next_hash));
	return count;
errout:
	dx_release(frames);
	return (err);
}


/*
 * Directory block splitting, compacting
 */

static int dx_make_map (struct ext3_dir_entry_2 *de, int size,
			struct dx_hash_info *hinfo, struct dx_map_entry *map_tail)
{
	int count = 0;
	char *base = (char *) de;
	struct dx_hash_info h = *hinfo;

	while ((char *) de < base + size)
	{
		if (de->name_len && de->inode) {
			ext3fs_dirhash(de->name, de->name_len, &h);
			map_tail--;
			map_tail->hash = h.hash;
			map_tail->offs = (u32) ((char *) de - base);
			count++;
		}
		/* XXX: do we need to check rec_len == 0 case? -Chris */
		de = (struct ext3_dir_entry_2 *) ((char *) de + le16_to_cpu(de->rec_len));
	}
	return count;
}

static void dx_sort_map (struct dx_map_entry *map, unsigned count)
{
        struct dx_map_entry *p, *q, *top = map + count - 1;
        int more;
        /* Combsort until bubble sort doesn't suck */
        while (count > 2)
	{
                count = count*10/13;
                if (count - 9 < 2) /* 9, 10 -> 11 */
                        count = 11;
                for (p = top, q = p - count; q >= map; p--, q--)
                        if (p->hash < q->hash)
                                swap(*p, *q);
        }
        /* Garden variety bubble sort */
        do {
                more = 0;
                q = top;
                while (q-- > map)
		{
                        if (q[1].hash >= q[0].hash)
				continue;
                        swap(*(q+1), *q);
                        more = 1;
		}
	} while(more);
}

static void dx_insert_block(struct dx_frame *frame, u32 hash, u32 block)
{
	struct dx_entry *entries = frame->entries;
	struct dx_entry *old = frame->at, *new = old + 1;
	int count = dx_get_count(entries);

	assert(count < dx_get_limit(entries));
	assert(old < entries + count);
	memmove(new + 1, new, (char *)(entries + count) - (char *)(new));
	dx_set_hash(new, hash);
	dx_set_block(new, block);
	dx_set_count(entries, count + 1);
}

static void ext3_update_dx_flag(struct inode *inode)
{
	if (!EXT3_HAS_COMPAT_FEATURE(inode->i_sb,
				     EXT3_FEATURE_COMPAT_DIR_INDEX))
		EXT3_I(inode)->i_flags &= ~EXT3_INDEX_FL;
}

/*
 * NOTE! unlike strncmp, ext3_match returns 1 for success, 0 for failure.
 *
 * `len <= EXT3_NAME_LEN' is guaranteed by caller.
 * `de != NULL' is guaranteed by caller.
 */
static inline int ext3_match (int len, const char * const name,
			      struct ext3_dir_entry_2 * de)
{
	if (len != de->name_len)
		return 0;
	if (!de->inode)
		return 0;
	return !memcmp(name, de->name, len);
}

static buf_t  ext3_dx_find_entry(struct dentry *dentry,
		       struct ext3_dir_entry_2 **res_dir, int *err)
{
	struct super_block * sb;
	struct dx_hash_info	hinfo;
	u32 hash;
	struct dx_frame frames[2], *frame;
	struct ext3_dir_entry_2 *de, *top;
	buf_t bh;
	unsigned long block;
	int retval;
	int namelen = dentry->d_name.len;
	const char *name = dentry->d_name.name;
	struct inode *dir = dentry->d_parent->d_inode;

	sb = dir->i_sb;
	if (!(frame = dx_probe (dentry, 0, &hinfo, frames, err)))
		return NULL;
	hash = hinfo.hash;
	do {
		block = dx_get_block(frame->at);
		if (!(bh = ext3_bread (NULL,dir, block, 0, err)))
			goto errout;
		de = (struct ext3_dir_entry_2 *) buf_dataptr(bh);
		top = (struct ext3_dir_entry_2 *) ((char *) de + sb->s_blocksize -
				       EXT3_DIR_REC_LEN(0));
		for (; de < top; de = ext3_next_entry(de))
		if (ext3_match (namelen, name, de)) {
			if (!ext3_check_dir_entry("ext3_find_entry",
						  dir, de, bh,
         #ifdef linux
				  (block<<EXT3_BLOCK_SIZE_BITS(sb))
         #else
            /* Darwin check routine expects offsets from the block,
               not the dir. */
               0
         #endif
					  +((char *)de - (char*)buf_dataptr(bh)))) {
				brelse (bh);
				goto errout;
			}
			*res_dir = de;
			dx_release (frames);
			return bh;
		}
		brelse (bh);
		/* Check to see if we should continue to search */
		retval = ext3_htree_next_block(dir, hash, frame,
					       frames, 0);
		if (retval < 0) {
			ext3_warning(sb, __FUNCTION__,
			     "error reading index page in directory #%lu",
			     dir->i_ino);
			*err = retval;
			goto errout;
		}
	} while (retval == 1);

	*err = -ENOENT;
errout:
	dxtrace(printk("%s not found\n", name));
	dx_release (frames);
	return NULL;
}

#define S_SHIFT 12
static unsigned char ext3_type_by_mode[S_IFMT >> S_SHIFT] = {
	[S_IFREG >> S_SHIFT]	= EXT2_FT_REG_FILE,
	[S_IFDIR >> S_SHIFT]	= EXT2_FT_DIR,
	[S_IFCHR >> S_SHIFT]	= EXT2_FT_CHRDEV,
	[S_IFBLK >> S_SHIFT]	= EXT2_FT_BLKDEV,
	[S_IFIFO >> S_SHIFT]	= EXT2_FT_FIFO,
	[S_IFSOCK >> S_SHIFT]	= EXT2_FT_SOCK,
	[S_IFLNK >> S_SHIFT]	= EXT2_FT_SYMLINK,
};

static inline void ext3_set_de_type(struct super_block *sb,
				struct ext3_dir_entry_2 *de,
				umode_t mode) {
	if (EXT3_HAS_INCOMPAT_FEATURE(sb, EXT2_FEATURE_INCOMPAT_FILETYPE))
		de->file_type = ext3_type_by_mode[(mode & S_IFMT)>>S_SHIFT];
}

static struct ext3_dir_entry_2 *
dx_move_dirents(char *from, char *to, struct dx_map_entry *map, int count)
{
	unsigned rec_len = 0;

	while (count--) {
		struct ext3_dir_entry_2 *de = (struct ext3_dir_entry_2 *) (from + map->offs);
		rec_len = EXT3_DIR_REC_LEN(de->name_len);
		memcpy (to, de, rec_len);
		((struct ext3_dir_entry_2 *) to)->rec_len =
				cpu_to_le16(rec_len);
		de->inode = 0;
		map++;
		to += rec_len;
	}
	return (struct ext3_dir_entry_2 *) (to - rec_len);
}

static struct ext3_dir_entry_2* dx_pack_dirents(char *base, int size)
{
	struct ext3_dir_entry_2 *next, *to, *prev, *de = (struct ext3_dir_entry_2 *) base;
	unsigned rec_len = 0;

	prev = to = de;
	while ((char*)de < base + size) {
		next = (struct ext3_dir_entry_2 *) ((char *) de +
						    le16_to_cpu(de->rec_len));
		if (de->inode && de->name_len) {
			rec_len = EXT3_DIR_REC_LEN(de->name_len);
			if (de > to)
				memmove(to, de, rec_len);
			to->rec_len = cpu_to_le16(rec_len);
			prev = to;
			to = (struct ext3_dir_entry_2 *) (((char *) to) + rec_len);
		}
		de = next;
	}
	return prev;
}

static struct ext3_dir_entry_2 *do_split(handle_t *handle, struct inode *dir,
			buf_t *bh,struct dx_frame *frame,
			struct dx_hash_info *hinfo, int *error)
{
	unsigned blocksize = dir->i_sb->s_blocksize;
	unsigned count, continued;
	buf_t bh2;
	u32 newblock;
	u32 hash2;
	struct dx_map_entry *map;
	char *data1 = (char*)buf_dataptr(*bh), *data2;
	unsigned split;
	struct ext3_dir_entry_2 *de = NULL, *de2;
	int	err;

	bh2 = ext3_append (handle, dir, &newblock, error);
	if (!(bh2)) {
		brelse(*bh);
		*bh = NULL;
		goto errout;
	}

	BUFFER_TRACE(*bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, *bh);
	if (err) {
	journal_error:
		brelse(*bh);
		brelse(bh2);
		*bh = NULL;
		ext3_std_error(dir->i_sb, err);
		goto errout;
	}
	BUFFER_TRACE(frame->bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, frame->bh);
	if (err)
		goto journal_error;

	data2 = (char*)buf_dataptr(bh2);

	/* create map in the end of data2 block */
	map = (struct dx_map_entry *) (data2 + blocksize);
	count = dx_make_map ((struct ext3_dir_entry_2 *) data1,
			     blocksize, hinfo, map);
	map -= count;
	split = count/2; // need to adjust to actual middle
	dx_sort_map (map, count);
	hash2 = map[split].hash;
	continued = hash2 == map[split - 1].hash;
	dxtrace(printk("Split block %i at %x, %i/%i\n",
		dx_get_block(frame->at), hash2, split, count-split));

	/* Fancy dance to stay within two buffers */
	de2 = dx_move_dirents(data1, data2, map + split, count - split);
	de = dx_pack_dirents(data1,blocksize);
	de->rec_len = cpu_to_le16(data1 + blocksize - (char *) de);
	de2->rec_len = cpu_to_le16(data2 + blocksize - (char *) de2);
	dxtrace(dx_show_leaf (hinfo, (struct ext3_dir_entry_2 *) data1, blocksize, 1));
	dxtrace(dx_show_leaf (hinfo, (struct ext3_dir_entry_2 *) data2, blocksize, 1));

	/* Which block gets the new entry? */
	if (hinfo->hash >= hash2)
	{
		swap(*bh, bh2);
		de = de2;
	}
	dx_insert_block (frame, hash2 + continued, newblock);
	err = ext3_journal_dirty_metadata (handle, bh2);
	if (err)
		goto journal_error;
	err = ext3_journal_dirty_metadata (handle, frame->bh);
	if (err)
		goto journal_error;
	brelse (bh2);
	dxtrace(dx_show_index ("frame", frame->entries));
errout:
	return de;
}

/*
 * Add a new entry into a directory (leaf) block.  If de is non-NULL,
 * it points to a directory entry which is guaranteed to be large
 * enough for new directory entry.  If de is NULL, then
 * add_dirent_to_buf will attempt search the directory block for
 * space.  It will return -ENOSPC if no space is available, and -EIO
 * and -EEXIST if directory entry already exists.
 * 
 * NOTE!  bh is NOT released in the case where ENOSPC is returned.  In
 * all other cases bh is released.
 */
static int add_dirent_to_buf(handle_t *handle, struct dentry *dentry,
			     struct inode *inode, struct ext3_dir_entry_2 *de,
			     buf_t  bh)
{
	struct inode	*dir = dentry->d_parent->d_inode;
	const char	*name = dentry->d_name.name;
	int		namelen = dentry->d_name.len;
	unsigned long	offset = 0;
	unsigned short	reclen;
	int		nlen, rlen, err;
	char		*top;

	reclen = EXT3_DIR_REC_LEN(namelen);
	if (!de) {
		de = (struct ext3_dir_entry_2 *)buf_dataptr(bh);
		top = (char*)buf_dataptr(bh) + dir->i_sb->s_blocksize - reclen;
		while ((char *) de <= top) {
			if (!ext3_check_dir_entry("ext3_add_entry", dir, de,
						  bh, offset)) {
				brelse (bh);
				return -EIO;
			}
			if (ext3_match (namelen, name, de)) {
				brelse (bh);
				return -EEXIST;
			}
			nlen = EXT3_DIR_REC_LEN(de->name_len);
			rlen = le16_to_cpu(de->rec_len);
			if ((de->inode? rlen - nlen: rlen) >= reclen)
				break;
			de = (struct ext3_dir_entry_2 *)((char *)de + rlen);
			offset += rlen;
		}
		if ((char *) de > top)
			return -ENOSPC;
	}
	BUFFER_TRACE(bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, bh);
	if (err) {
		ext3_std_error(dir->i_sb, err);
		brelse(bh);
		return err;
	}

	/* By now the buffer is marked for journaling */
	nlen = EXT3_DIR_REC_LEN(de->name_len);
	rlen = le16_to_cpu(de->rec_len);
	if (de->inode) {
		struct ext3_dir_entry_2 *de1 = (struct ext3_dir_entry_2 *)((char *)de + nlen);
		de1->rec_len = cpu_to_le16(rlen - nlen);
		de->rec_len = cpu_to_le16(nlen);
		de = de1;
	}
	de->file_type = EXT3_FT_UNKNOWN;
	if (inode) {
		de->inode = cpu_to_le32(inode->i_ino);
		ext3_set_de_type(dir->i_sb, de, inode->i_mode);
	} else
		de->inode = 0;
	de->name_len = namelen;
	memcpy (de->name, name, namelen);
	/*
	 * XXX shouldn't update any times until successful
	 * completion of syscall, but too many callers depend
	 * on this.
	 *
	 * XXX similarly, too many callers depend on
	 * ext3_new_inode() setting the times, but error
	 * recovery deletes the inode, so the worst that can
	 * happen is that the times are slightly out of date
	 * and/or different from the directory change time.
	 */
#ifdef linux
	dir->i_mtime = dir->i_ctime = CURRENT_TIME;
#else
   {
   struct timespec ts;
   vfs_timestamp(&ts);
   dir->i_mtime = dir->i_ctime = ts.tv_sec;
   dir->i_mtimensec = dir->i_ctimensec = ts.tv_nsec;
   }
#endif
	ext3_update_dx_flag(dir);
#ifdef linux
	dir->i_version++;
#else
   dir->i_flag |= IN_DX_UPDATE;
#endif
	ext3_mark_inode_dirty(handle, dir);
	BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
	err = ext3_journal_dirty_metadata(handle, bh);
	if (err)
		ext3_std_error(dir->i_sb, err);
	brelse(bh);
	return 0;
}

/*
 * This converts a one block unindexed directory to a 3 block indexed
 * directory, and adds the dentry to the indexed directory.
 */
static int make_indexed_dir(handle_t *handle, struct dentry *dentry,
			    struct inode *inode, buf_t bh)
{
	struct inode	*dir = dentry->d_parent->d_inode;
	const char	*name = dentry->d_name.name;
	int		namelen = dentry->d_name.len;
	buf_t bh2;
	struct dx_root	*root;
	struct dx_frame	frames[2], *frame;
	struct dx_entry *entries;
	struct ext3_dir_entry_2	*de, *de2;
	char		*data1, *top;
	unsigned	len;
	int		retval;
	unsigned	blocksize;
	struct dx_hash_info hinfo;
	u32		block;
	struct fake_dirent *fde;

	blocksize =  dir->i_sb->s_blocksize;
	dxtrace(printk("Creating index\n"));
	retval = ext3_journal_get_write_access(handle, bh);
	if (retval) {
		ext3_std_error(dir->i_sb, retval);
		brelse(bh);
		return retval;
	}
	root = (struct dx_root *) buf_dataptr(bh);

	bh2 = ext3_append (handle, dir, &block, &retval);
	if (!(bh2)) {
		brelse(bh);
		return retval;
	}
	EXT3_I(dir)->i_flags |= EXT3_INDEX_FL;
	data1 = (char*)buf_dataptr(bh2);

	/* The 0th block becomes the root, move the dirents out */
	fde = &root->dotdot;
	de = (struct ext3_dir_entry_2 *)((char *)fde + le16_to_cpu(fde->rec_len));
	len = ((char *) root) + blocksize - (char *) de;
	memcpy (data1, de, len);
	de = (struct ext3_dir_entry_2 *) data1;
	top = data1 + len;
	while ((char *)(de2=(struct ext3_dir_entry_2*)((char*)de+le16_to_cpu(de->rec_len))) < top)
		de = de2;
	de->rec_len = cpu_to_le16(data1 + blocksize - (char *) de);
	/* Initialize the root; the dot dirents already exist */
	de = (struct ext3_dir_entry_2 *) (&root->dotdot);
	de->rec_len = cpu_to_le16(blocksize - EXT3_DIR_REC_LEN(2));
	memset (&root->info, 0, sizeof(root->info));
	root->info.info_length = sizeof(root->info);
	root->info.hash_version = EXT3_SB(dir->i_sb)->s_def_hash_version;
	entries = root->entries;
	dx_set_block (entries, 1);
	dx_set_count (entries, 1);
	dx_set_limit (entries, dx_root_limit(dir, sizeof(root->info)));

	/* Initialize as for dx_probe */
	hinfo.hash_version = root->info.hash_version;
	hinfo.seed = EXT3_SB(dir->i_sb)->s_hash_seed;
	ext3fs_dirhash(name, namelen, &hinfo);
	frame = frames;
	frame->entries = entries;
	frame->at = entries;
	frame->bh = bh;
	bh = bh2;
	de = do_split(handle,dir, &bh, frame, &hinfo, &retval);
	dx_release (frames);
	if (!(de))
		return retval;

	return add_dirent_to_buf(handle, dentry, inode, de, bh);
}

/*
 * Returns 0 for success, or a negative error value
 */
static int ext3_dx_add_entry(handle_t *handle, struct dentry *dentry,
			     struct inode *inode)
{
	struct dx_frame frames[2], *frame;
	struct dx_entry *entries, *at;
	struct dx_hash_info hinfo;
	buf_t  bh;
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block * sb = dir->i_sb;
	struct ext3_dir_entry_2 *de;
	int err;

	frame = dx_probe(dentry, 0, &hinfo, frames, &err);
	if (!frame)
		return err;
	entries = frame->entries;
	at = frame->at;

	if (!(bh = ext3_bread(handle,dir, dx_get_block(frame->at), 0, &err)))
		goto cleanup;

	BUFFER_TRACE(bh, "get_write_access");
	err = ext3_journal_get_write_access(handle, bh);
	if (err)
		goto journal_error;

	err = add_dirent_to_buf(handle, dentry, inode, 0, bh);
	if (err != -ENOSPC) {
		bh = 0;
		goto cleanup;
	}

	/* Block full, should compress but for now just split */
	dxtrace(printk("using %u of %u node entries\n",
		       dx_get_count(entries), dx_get_limit(entries)));
	/* Need to split index? */
	if (dx_get_count(entries) == dx_get_limit(entries)) {
		u32 newblock;
		unsigned icount = dx_get_count(entries);
		int levels = frame - frames;
		struct dx_entry *entries2;
		struct dx_node *node2;
		buf_t bh2;

		if (levels && (dx_get_count(frames->entries) ==
			       dx_get_limit(frames->entries))) {
			ext3_warning(sb, __FUNCTION__,
				     "Directory index full!\n");
			err = -ENOSPC;
			goto cleanup;
		}
		bh2 = ext3_append (handle, dir, &newblock, &err);
		if (!(bh2))
			goto cleanup;
		node2 = (struct dx_node *)(buf_dataptr(bh2));
		entries2 = node2->entries;
		node2->fake.rec_len = cpu_to_le16(sb->s_blocksize);
		node2->fake.inode = 0;
		BUFFER_TRACE(frame->bh, "get_write_access");
		err = ext3_journal_get_write_access(handle, frame->bh);
		if (err)
			goto journal_error;
		if (levels) {
			unsigned icount1 = icount/2, icount2 = icount - icount1;
			unsigned hash2 = dx_get_hash(entries + icount1);
			dxtrace(printk("Split index %i/%i\n", icount1, icount2));

			BUFFER_TRACE(frame->bh, "get_write_access"); /* index root */
			err = ext3_journal_get_write_access(handle,
							     frames[0].bh);
			if (err)
				goto journal_error;

			memcpy ((char *) entries2, (char *) (entries + icount1),
				icount2 * sizeof(struct dx_entry));
			dx_set_count (entries, icount1);
			dx_set_count (entries2, icount2);
			dx_set_limit (entries2, dx_node_limit(dir));

			/* Which index block gets the new entry? */
			if (at - entries >= icount1) {
				frame->at = at = at - entries - icount1 + entries2;
				frame->entries = entries = entries2;
				swap(frame->bh, bh2);
			}
			dx_insert_block (frames + 0, hash2, newblock);
			dxtrace(dx_show_index ("node", frames[1].entries));
			dxtrace(dx_show_index ("node",
			       ((struct dx_node *) buf_dataptr(bh2))->entries));
			err = ext3_journal_dirty_metadata(handle, bh2);
			if (err)
				goto journal_error;
			brelse (bh2);
		} else {
			dxtrace(printk("Creating second level index...\n"));
			memcpy((char *) entries2, (char *) entries,
			       icount * sizeof(struct dx_entry));
			dx_set_limit(entries2, dx_node_limit(dir));

			/* Set up root */
			dx_set_count(entries, 1);
			dx_set_block(entries + 0, newblock);
			((struct dx_root *) buf_dataptr(frames[0].bh))->info.indirect_levels = 1;

			/* Add new access path frame */
			frame = frames + 1;
			frame->at = at = at - entries + entries2;
			frame->entries = entries = entries2;
			frame->bh = bh2;
			err = ext3_journal_get_write_access(handle,
							     frame->bh);
			if (err)
				goto journal_error;
		}
		ext3_journal_dirty_metadata(handle, frames[0].bh);
	}
	de = do_split(handle, dir, &bh, frame, &hinfo, &err);
	if (!de)
		goto cleanup;
	err = add_dirent_to_buf(handle, dentry, inode, de, bh);
	bh = 0;
	goto cleanup;

journal_error:
	ext3_std_error(dir->i_sb, err);
cleanup:
	if (bh)
		brelse(bh);
	dx_release(frames);
	return err;
}

/*
 * ext3_delete_entry deletes a directory entry by merging it with the
 * previous entry
 */
static int ext3_delete_entry (handle_t *handle, 
			      struct inode * dir,
			      struct ext3_dir_entry_2 * de_del,
			      buf_t  bh)
{
	struct ext3_dir_entry_2 * de, * pde;
    u32 size;
	int i;

	i = 0;
	pde = NULL;
	de = (struct ext3_dir_entry_2 *) buf_dataptr(bh);
    size = buf_size(bh);
	while (i < size) {
		if (!ext3_check_dir_entry("ext3_delete_entry", dir, de, bh, i))
			return -EIO;
		if (de == de_del)  {
			BUFFER_TRACE(bh, "get_write_access");
			ext3_journal_get_write_access(handle, bh);
			if (pde)
				pde->rec_len =
					cpu_to_le16(le16_to_cpu(pde->rec_len) +
						    le16_to_cpu(de->rec_len));
			else
				de->inode = 0;
      #ifdef linux
			dir->i_version++;
      #else
         dir->i_flag |= IN_DX_UPDATE;
      #endif
			BUFFER_TRACE(bh, "call ext3_journal_dirty_metadata");
			ext3_journal_dirty_metadata(handle, bh);
			return 0;
		}
		i += le16_to_cpu(de->rec_len);
		pde = de;
		de = (struct ext3_dir_entry_2 *)
			((char *) de + le16_to_cpu(de->rec_len));
	}
	return -ENOENT;
}

#ifndef linux
/* XXX - Undefine these, so ext2_lookup.c isn't polluted. */
#undef i_flags
#undef i_ino
#endif
