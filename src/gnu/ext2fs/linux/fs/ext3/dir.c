/*
 *  linux/fs/ext3/dir.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/dir.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  ext3 directory handling functions
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 *
 * Hash Tree Directory indexing (c) 2001  Daniel Phillips
 *
 */
/*
 * Darwin/BSD support (c) 2003 Brian Bergstrand
 */ 
 
/*
 * These functions convert from the major/minor hash to an f_pos
 * value.
 * 
 * Currently we only use major hash number.  This is unfortunate, but
 * on 32-bit machines, the same VFS interface is used for lseek and
 * llseek, so if we use the 64 bit offset, then the 32-bit versions of
 * lseek/telldir/seekdir will blow out spectacularly, and from within
 * the ext2 low-level routine, we don't know if we're being called by
 * a 64-bit version of the system call or the 32-bit version of the
 * system call.  Worse yet, NFSv2 only allows for a 32-bit readdir
 * cookie.  Sigh.
 */
#define hash2pos(major, minor)	(major >> 1)
#define pos2maj_hash(pos)	((pos << 1) & 0xffffffff)
#define pos2min_hash(pos)	(0)

/*
 * This structure holds the nodes of the red-black tree used to store
 * the directory entry in hash order.
 */
struct fname {
	__u32		hash;
	__u32		minor_hash;
	struct rb_node	rb_hash; 
	struct fname	*next;
	__u32		inode;
	__u8		name_len;
	__u8		file_type;
	char		name[0];
};

/*
 * This functoin implements a non-recursive way of freeing all of the
 * nodes in the red-black tree.
 */
static void free_rb_tree_fname(struct rb_root *root)
{
	struct rb_node	*n = root->rb_node;
	struct rb_node	*parent;
	struct fname	*fname;

	while (n) {
		/* Do the node's children first */
		if ((n)->rb_left) {
			n = n->rb_left;
			continue;
		}
		if (n->rb_right) {
			n = n->rb_right;
			continue;
		}
		/*
		 * The node has no children; free it, and then zero
		 * out parent's link to it.  Finally go to the
		 * beginning of the loop and try to free the parent
		 * node.
		 */
		parent = n->rb_parent;
		fname = rb_entry(n, struct fname, rb_hash);
		while (fname) {
			struct fname * old = fname;
			fname = fname->next;
			kfree (old);
		}
		if (!parent)
			root->rb_node = 0;
		else if (parent->rb_left == n)
			parent->rb_left = 0;
		else if (parent->rb_right == n)
			parent->rb_right = 0;
		n = parent;
	}
	root->rb_node = 0;
}


struct dir_private_info *create_dir_info(loff_t pos)
{
	struct dir_private_info *p;
	
	p = kmalloc(sizeof(struct dir_private_info), GFP_KERNEL);
	if (!p)
		return NULL;
	p->root.rb_node = 0;
	p->curr_node = 0;
	p->extra_fname = 0;
	p->last_pos = 0;
	p->curr_hash = pos2maj_hash(pos);
	p->curr_minor_hash = pos2min_hash(pos);
	p->next_hash = 0;
	return p;
}

void ext3_htree_free_dir_info(struct dir_private_info *p)
{
	free_rb_tree_fname(&p->root);
	kfree(p);
}

/*
 * Given a directory entry, enter it into the fname rb tree.
 */
int ext3_htree_store_dirent(vnode_t dir_file, __u32 hash,
			     __u32 minor_hash,
			     struct ext3_dir_entry_2 *dirent)
{
	struct rb_node **p, *parent = NULL;
	struct fname * fname, *new_fn;
	struct dir_private_info *info;
	struct inode *ip;
	int len;
	
	ip = VTOI(dir_file);
   
	info = (struct dir_private_info *) ip->private_data;
	p = &info->root.rb_node;

	/* Create and allocate the fname structure */
	len = sizeof(struct fname) + dirent->name_len + 1;
	new_fn = kmalloc(len, GFP_KERNEL);
	if (!new_fn)
		return -ENOMEM;
	memset(new_fn, 0, len);
	new_fn->hash = hash;
	new_fn->minor_hash = minor_hash;
	new_fn->inode = le32_to_cpu(dirent->inode);
	new_fn->name_len = dirent->name_len;
	new_fn->file_type = dirent->file_type;
	memcpy(new_fn->name, dirent->name, dirent->name_len);
	new_fn->name[dirent->name_len] = 0;

	while (*p) {
		parent = *p;
		fname = rb_entry(parent, struct fname, rb_hash);

		/*
		 * If the hash and minor hash match up, then we put
		 * them on a linked list.  This rarely happens...
		 */
		if ((new_fn->hash == fname->hash) &&
		    (new_fn->minor_hash == fname->minor_hash)) {
			new_fn->next = fname->next;
			fname->next = new_fn;
			return 0;
		}

		if (new_fn->hash < fname->hash)
			p = &(*p)->rb_left;
		else if (new_fn->hash > fname->hash)
			p = &(*p)->rb_right;
		else if (new_fn->minor_hash < fname->minor_hash)
			p = &(*p)->rb_left;
		else /* if (new_fn->minor_hash > fname->minor_hash) */
			p = &(*p)->rb_right;
	}

	rb_link_node(&new_fn->rb_hash, parent, p);
	rb_insert_color(&new_fn->rb_hash, &info->root);
	return 0;
}



/*
 * This is a helper function for ext3_dx_readdir.  It calls filldir
 * for all entres on the fname linked list.  (Normally there is only
 * one entry on the linked list, unless there are 62 bit hash collisions.)
 */
static int call_filldir(vnode_t  filp, void * dirent,
			filldir_t filldir, struct fname *fname)
{
	loff_t	curr_pos;
	struct inode *inode = VTOI(filp);
   struct dir_private_info *info = inode->private_data;
	struct super_block * sb;
	int error;

	sb = inode->i_e2fs;

	if (!fname) {
		printk("ext2: call_filldir: called with null fname?!?\n");
		return 0;
	}
	curr_pos = hash2pos(fname->hash, fname->minor_hash);
	while (fname) {
		error = filldir(dirent, fname->name,
				fname->name_len, curr_pos, 
				fname->inode,
				FTTODT(fname->file_type));
		if (error) {
			inode->f_pos = curr_pos;
			info->extra_fname = fname->next;
			return error;
		}
		fname = fname->next;
	}
	return 0;
}

static int ext3_release_dir (vnode_t , struct inode *);

static int ext3_dx_readdir(vnode_t  filp,
			 void * dirent, filldir_t filldir)
{
	struct inode *inode = VTOI(filp);
   struct dir_private_info *info = inode->private_data;
	struct fname *fname;
	int	ret;

	if (!info) {
		info = create_dir_info(inode->f_pos);
		if (!info)
			return -ENOMEM;
		inode->private_data = info;
      inode->private_data_relse = ext3_release_dir;
	}

#ifdef linux
	if (inode->f_pos == EXT3_HTREE_EOF)
		return 0;	/* EOF */
#else
/* --BDB --
   Not sure why linux does the above, but if we do, then re-read's of
   a directory will not work. So we take a different behavior. */
   if (inode->f_pos == EXT3_HTREE_EOF) {
      info->last_pos = EXT3_HTREE_EOF;
      inode->f_pos = 0;
   } 
#endif
	/* Some one has messed with f_pos; reset the world */
	if (info->last_pos != inode->f_pos) {
		free_rb_tree_fname(&info->root);
		info->curr_node = 0;
		info->extra_fname = 0;
		info->curr_hash = pos2maj_hash(inode->f_pos);
		info->curr_minor_hash = pos2min_hash(indoe->f_pos);
	}

	/*
	 * If there are any leftover names on the hash collision
	 * chain, return them first.
	 */
	if (info->extra_fname &&
	    call_filldir(filp, dirent, filldir, info->extra_fname))
		goto finished;

	if (!info->curr_node)
		info->curr_node = rb_first(&info->root);

	while (1) {
		/*
		 * Fill the rbtree if we have no more entries,
		 * or the inode has changed since we last read in the
		 * cached entries. 
		 */
		if ((!info->curr_node) ||
		    (inode->i_flag & IN_DX_UPDATE)) {
			info->curr_node = 0;
			free_rb_tree_fname(&info->root);
			inode->i_flag &= ~IN_DX_UPDATE;
			ret = ext3_htree_fill_tree(filp, info->curr_hash,
						   info->curr_minor_hash,
						   &info->next_hash);
			if (ret < 0)
				return ret;
			if (ret == 0) {
				inode->f_pos = EXT3_HTREE_EOF;
				break;
			}
			info->curr_node = rb_first(&info->root);
		}

		fname = rb_entry(info->curr_node, struct fname, rb_hash);
		info->curr_hash = fname->hash;
		info->curr_minor_hash = fname->minor_hash;
		if (call_filldir(filp, dirent, filldir, fname))
			break;

		info->curr_node = rb_next(info->curr_node);
		if (!info->curr_node) {
			if (info->next_hash == ~0) {
				inode->f_pos = EXT3_HTREE_EOF;
				break;
			}
			info->curr_hash = info->next_hash;
			info->curr_minor_hash = 0;
		}
	}
finished:
	info->last_pos = inode->f_pos;
	update_atime(inode);
	return 0;
}

static int ext3_release_dir (vnode_t  vp, struct inode * filp)
{
	if (filp->private_data)
		ext3_htree_free_dir_info(filp->private_data);

	return 0;
}
