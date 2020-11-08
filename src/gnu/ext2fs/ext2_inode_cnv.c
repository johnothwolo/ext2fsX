/*
 * Copyright (c) 1995 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Utah $Hdr$
 * $FreeBSD: src/sys/gnu/ext2fs/ext2_inode_cnv.c,v 1.12 2002/05/16 19:07:59 iedowse Exp $
 */
/*
* Large file, 32 bit UID/GID, and Big Endian support.
*
* Copyright 2003 Brian Bergstrand.
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
"@(#) $Id: ext2_inode_cnv.c,v 1.11 2004/07/24 22:43:53 bbergstrand Exp $";

/*
 * routines to convert on disk ext2 inodes into inodes and back
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/syslog.h>

#ifdef APPLE
#include "ext2_apple.h"
#endif

#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/ext2_extern.h>
#include <gnu/ext2fs/ext2_mount.h>
#include "ext2_byteorder.h"

void
ext2_print_inode(struct inode *in){
	int i;

	ext2_debug( "Inode: %5d", in->i_number);
	ext2_debug( /* "Inode: %5d" */
		" Type: %10s Mode: 0x%o Flags: 0x%x  Version: %d\n",
		"n/a", in->i_mode, in->i_flags, in->i_gen);
	ext2_debug( "User: %5lu Group: %5lu  Size: %qu\n",
		(unsigned long)in->i_uid, (unsigned long)in->i_gid,
		in->i_size);
	ext2_debug( "Links: %3d Blockcount: %d\n",
		in->i_nlink, in->i_blocks);
	ext2_debug( "ctime: 0x%x", in->i_ctime);
	ext2_debug( "atime: 0x%x", in->i_atime);
	ext2_debug( "mtime: 0x%x", in->i_mtime);
	ext2_debug( "BLOCKS: ");
	for(i=0; i < (in->i_blocks <= 24 ? ((in->i_blocks+1)/2): 12); i++)
		ext2_debug("%d ", in->i_db[i]);
	ext2_debug("\n");
}

/*
 *	raw ext2 inode to inode
 */
void
ext2_ei2i(struct ext2_inode *ei, struct inode *ip)
{
        int i;

    ip->i_nlink = le16_to_cpu(ei->i_links_count);
    /* Godmar thinks - if the link count is zero, then the inode is
       unused - according to ext2 standards. Ufs marks this fact
       by setting i_mode to zero - why ?
       I can see that this might lead to problems in an undelete.
    */
    ip->i_mode = le16_to_cpu(ei->i_links_count) ? le16_to_cpu(ei->i_mode) : 0;
    ip->i_size = le32_to_cpu(ei->i_size);
    ip->i_atime = le32_to_cpu(ei->i_atime);
    ip->i_mtime = le32_to_cpu(ei->i_mtime);
    ip->i_ctime = le32_to_cpu(ei->i_ctime);
    /* copy of on disk flags -- not currently modified */
    ip->i_e2flags = le32_to_cpu(ei->i_flags);
    ip->i_flags = 0;
    ip->i_flags |= (ip->i_e2flags & EXT2_APPEND_FL) ? APPEND : 0;
    ip->i_flags |= (ip->i_e2flags & EXT2_IMMUTABLE_FL) ? IMMUTABLE : 0;
    ip->i_blocks = le32_to_cpu(ei->i_blocks);
    ip->i_gen = le32_to_cpu(ei->i_generation);
    ip->i_uid = (u_int32_t)le16_to_cpu(ei->i_uid);
    ip->i_gid = (u_int32_t)le16_to_cpu(ei->i_gid);
    if(!(test_opt (ip->i_e2fs, NO_UID32))) {
        ip->i_uid |= le16_to_cpu(ei->i_uid_high) << 16;
        ip->i_gid |= le16_to_cpu(ei->i_gid_high) << 16;
    }
    if (S_ISREG(ip->i_mode))
        ip->i_size |= ((u_int64_t)le32_to_cpu(ei->i_size_high)) << 32;
    /* TBD: Otherwise, setup the dir acl */

    #if BYTE_ORDER == BIG_ENDIAN
    /* We don't want to swap the block addr's for a short symlink because
    * they contain a path name.
    */
    if (S_ISLNK(ip->i_mode) && ip->i_size < EXT2_MAXSYMLINKLEN) {
        /* Take advantage of the fact that i_ib follwows i_db. */
        bcopy(ei->i_block, ip->i_shortlink, ip->i_size);
        bzero(((char*)ip->i_shortlink)+ip->i_size, EXT2_MAXSYMLINKLEN - ip->i_size);
        return;
    }
    #endif
   
   /* Linux leaves the block #'s in LE order*/
	for(i = 0; i < NDADDR; i++)
		ip->i_db[i] = le32_to_cpu(ei->i_block[i]);
	for(i = 0; i < NIADDR; i++)
		ip->i_ib[i] = le32_to_cpu(ei->i_block[EXT2_NDIR_BLOCKS + i]);
}

#define fs_high2lowuid(uid) ((uid) > 65535 ? (u_int16_t)65534 : (u_int16_t)(uid))
#define fs_high2lowgid(gid) ((gid) > 65535 ? (u_int16_t)65534 : (u_int16_t)(gid))

#define low_16_bits(x)	((x) & 0xFFFF)
#define high_16_bits(x)	(((x) & 0xFFFF0000) >> 16)

/*
 *	inode to raw ext2 inode
 */
void
ext2_i2ei(struct inode *ip,struct ext2_inode *ei)
{
	int i;

	ei->i_mode = cpu_to_le16(ip->i_mode);
        ei->i_links_count = cpu_to_le16(ip->i_nlink);
	/* 
	   Godmar thinks: if dtime is nonzero, ext2 says this inode
	   has been deleted, this would correspond to a zero link count
	 */
	ei->i_dtime = ip->i_nlink ? 0 : cpu_to_le32(ip->i_mtime);
	ei->i_size = cpu_to_le32(ip->i_size);
	ei->i_atime = cpu_to_le32(ip->i_atime);
	ei->i_mtime = cpu_to_le32(ip->i_mtime);
	ei->i_ctime = cpu_to_le32(ip->i_ctime);
	/*ei->i_flags = ip->i_flags;*/
	/* ei->i_flags = 0; -- BDB - use flags originally read from disk */
   
    if (ip->i_flags & APPEND)
      ip->i_e2flags |= EXT2_APPEND_FL;
    else
      ip->i_e2flags &= ~EXT2_APPEND_FL;

    if (ip->i_flags & IMMUTABLE)
      ip->i_e2flags |= EXT2_IMMUTABLE_FL;
    else
      ip->i_e2flags &= ~EXT2_IMMUTABLE_FL;

    ei->i_flags = cpu_to_le32(ip->i_e2flags);

    ei->i_blocks = cpu_to_le32(ip->i_blocks);
    ei->i_generation = cpu_to_le32(ip->i_gen);
    ei->i_uid = cpu_to_le32(ip->i_uid);
    ei->i_gid = cpu_to_le32(ip->i_gid);
    if(!(test_opt(ip->i_e2fs, NO_UID32))) {
        ei->i_uid_low = cpu_to_le16(low_16_bits(ip->i_uid));
        ei->i_gid_low = cpu_to_le16(low_16_bits(ip->i_gid));
    /*
    * Fix up interoperability with old kernels. Otherwise, old inodes get
    * re-used with the upper 16 bits of the uid/gid intact
    */
        if(!ei->i_dtime) {
            ei->i_uid_high = cpu_to_le16(high_16_bits(ip->i_uid));
            ei->i_gid_high = cpu_to_le16(high_16_bits(ip->i_gid));
        } else {
            ei->i_uid_high = 0;
            ei->i_gid_high = 0;
        }
    } else {
        ei->i_uid_low = cpu_to_le16(fs_high2lowuid(ip->i_uid));
        ei->i_gid_low = cpu_to_le16(fs_high2lowgid(ip->i_gid));
        ei->i_uid_high = 0;
        ei->i_gid_high = 0;
    }
   
   if (S_ISREG(ip->i_mode)) {
      ei->i_size_high = cpu_to_le32(ip->i_size >> 32);
      if (ip->i_size > 0x7fffffffULL) {
         struct m_ext2fs *sb = ip->i_e2fs;
         if (!EXT2_HAS_RO_COMPAT_FEATURE(sb,
            EXT2_FEATURE_RO_COMPAT_LARGE_FILE) ||
            EXT2_SB(sb)->s_es->s_rev_level == cpu_to_le32(EXT2_GOOD_OLD_REV)) {
            /* First large file, add the flag to the superblock. */
            lock_super (sb);
            ext3_update_dynamic_rev(sb);
            EXT2_SET_RO_COMPAT_FEATURE(sb, EXT2_FEATURE_RO_COMPAT_LARGE_FILE);
            sb->s_dirt = 1;
            unlock_super (sb);
         }
      }
   }
   
   #if BYTE_ORDER == BIG_ENDIAN
   /* We don't want to swap the block addr's for a short symlink because
    * they contain a path name.
    */
   if (S_ISLNK(ip->i_mode) && ip->i_size < EXT2_MAXSYMLINKLEN) {
      /* Take advantage of the fact that i_ib follwows i_db. */
      bcopy(ip->i_shortlink, ei->i_block, ip->i_size);
      bzero(((char*)ei->i_block)+ip->i_size, EXT2_MAXSYMLINKLEN - ip->i_size);
      return;
   }
   #endif
   
   /* Linux leaves the block #'s in LE order*/
	/* XXX use memcpy */
	for(i = 0; i < NDADDR; i++)
		ei->i_block[i] = cpu_to_le32(ip->i_db[i]);
	for(i = 0; i < NIADDR; i++)
		ei->i_block[EXT2_NDIR_BLOCKS + i] = cpu_to_le32(ip->i_ib[i]);
}
