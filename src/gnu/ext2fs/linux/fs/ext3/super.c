/*
 * Darwin/BSD support (c) 2003 Brian Bergstrand
 */ 
/*
 *  linux/fs/ext3/super.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/inode.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  Big-endian to little-endian byte-swapping/bitmaps by
 *        David S. Miller (davem@caip.rutgers.edu), 1995
 */

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: super.c,v 1.2 2006/02/04 21:23:08 bbergstrand Exp $";

#include <stdarg.h>
#include <string.h>
#include <sys/param.h>
#include <sys/systm.h>

#include <gnu/ext2fs/ext2_fs.h>
#include "ext2_apple.h"
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <ext2_byteorder.h>

#define super_block ext2_sb_info
#define s_id fs_fsmnt

const char *ext3_decode_error(struct super_block * sb, int errno, char nbuf[16])
{
	char *errstr = NULL;

	switch (errno) {
	case -EIO:
		errstr = "IO failure";
		break;
	case -ENOMEM:
		errstr = "Out of memory";
		break;
	case -EROFS:
		/*if (!sb || EXT3_SB(sb)->s_journal->j_flags & JFS_ABORT)
			errstr = "Journal has aborted";
		else*/
			errstr = "Readonly filesystem";
		break;
	default:
		/* If the caller passed in an extra buffer for unknown
		 * errors, textualise them now.  Else we just return
		 * NULL. */
		if (nbuf) {
			/* Check for truncated error codes... */
			if (snprintf(nbuf, 16, "error %d", -errno) >= 0)
				errstr = nbuf;
		}
		break;
	}

	return errstr;
}

/* __ext3_std_error decodes expected errors from journaling functions
 * automatically and invokes the appropriate error response.  */

void __ext3_std_error (struct super_block * sb, const char * function,
		       int errno)
{
	char nbuf[16];
	const char *errstr = ext3_decode_error(sb, errno, nbuf);

	printk (KERN_CRIT "EXT3-fs error (device %s) in %s: %s\n",
		sb->s_id, function, errstr);

	if (sb->s_rd_only)
      return;
   
   if (test_opt (sb, ERRORS_PANIC))
		panic ("EXT3-fs (device %s): panic forced after error\n",
		       sb->s_id);
}

void ext3_warning (struct super_block * sb, const char * function,
		   const char * fmt, ...)
{
	va_list args;
   char *error_buf;
   
   MALLOC(error_buf, char *, 1024, M_TEMP, M_WAITOK);

	va_start (args, fmt);
	vsnprintf (error_buf, 1024, fmt, args);
	va_end (args);
	printk ("EXT3-fs warning (device %s): %s: %s\n",
		sb->s_id, function, error_buf);
   
   FREE(error_buf, M_TEMP);
}


void ext3_update_dynamic_rev(struct super_block *sb)
{
	struct ext3_super_block *es = EXT3_SB(sb)->s_es;

	if (le32_to_cpu(es->s_rev_level) > EXT3_GOOD_OLD_REV)
		return;

	ext3_warning(sb, __FUNCTION__,
		     "updating to rev %d because of new feature flag, "
		     "running e2fsck is recommended",
		     EXT3_DYNAMIC_REV);

	es->s_first_ino = cpu_to_le32(EXT3_GOOD_OLD_FIRST_INO);
	es->s_inode_size = cpu_to_le16(EXT3_GOOD_OLD_INODE_SIZE);
	es->s_rev_level = cpu_to_le32(EXT3_DYNAMIC_REV);
	/* leave es->s_feature_*compat flags alone */
	/* es->s_uuid will be set by e2fsck if empty */

	/*
	 * The rest of the superblock fields should be zero, and if not it
	 * means they are likely already in use, so leave them alone.  We
	 * can leave it up to e2fsck to clean up any inconsistencies there.
	 */
}
