/*
* Copyright 2003,2006 Brian Bergstrand.
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
static const char niwhatid[] __attribute__ ((unused)) =
"@(#) $Id: linux_namei_compat.c,v 1.10 2006/07/01 20:45:04 bbergstrand Exp $";

/* This is meant to be included in ext2_lookup.c only. */

#ifndef EXT3_LINUX_NAMEI_COMPAT_C
#define EXT3_LINUX_NAMEI_COMPAT_C

#include <string.h>
#include <sys/uio.h>

/* Linux compat types */

#ifndef u32
#define u32 __u32
#endif
#define u16 __u16
#define u8 __u8

#define EXT3_FT_UNKNOWN EXT2_FT_UNKNOWN

#define i_version i_gen

/* XXX - The EXT2_BLOCK_SIZE_BITS macro expects the on disk superblock
   This macro expects the in core superblock. */
#define EXT3_BLOCK_SIZE_BITS(s) ((s)->s_blocksize_bits)

#define super_block m_ext2fs
#define i_sb i_e2fs
#define s_id fs_fsmnt

/* -- BDB --
   handle_t is an opaque journal transaction type on Linux.
   We don't support journaling (yet), so for now we just use
   it as a context handle to pass info from the BSD routines. */
struct e2_handle {
   struct componentname *h_cnp;
   vfs_context_t h_vctx;
};
typedef struct e2_handle handle_t;

/* -- BDB --
   I think this is Linux's equiv to the namei cache (on steriods).
   Implement the bare essentials.*/
struct dentry {
   struct dentry * d_parent;	/* parent directory */
   struct {
      char *name;
      __u32 len;
   } d_name;
   struct inode  * d_inode;
};

/* Linux compat fn's */

#define BUFFER_TRACE(bh,msg)

static __inline__
int ext3_journal_get_write_access(handle_t *handle, buf_t bp) {return (0);}

static __inline__
int ext3_journal_dirty_metadata(handle_t *handle, buf_t bp)
{
   // XXX bh->b_flags |= B_NORELSE;
   // There is no B_NORELSE flag in Tiger, so get a ref on the buffer again
   daddr64_t blk = buf_blkno(bp);
   vnode_t vp = buf_vnode(bp);
   int bytes = (int)buf_count(bp);
   int err = -buf_bwrite(bp);
   if (bp != buf_getblk(vp, blk, bytes, 0, 0, BLK_ONLYVALID))
      panic("ext3: dirty buffer %lld not found!", blk);
   return (err);
}

static __inline__
int ext3_mark_inode_dirty(handle_t *handle, struct inode *inode)
{
   inode->i_flag |= IN_MODIFIED;
   return (0);
}

static __inline__
int ext3_check_dir_entry (const char * function, struct inode * dir,
			  struct ext3_dir_entry_2 * de,
			  buf_t  bp,
			  unsigned long offset)
{
   if (!dirchk && de->rec_len)
      return (1);
   /* The return value needs to be reversed for the Linux routines. */
   return (!(ext2_dirbadentry_locked(ITOV(dir), de, offset)));
}

static
buf_t ext3_bread(handle_t *handle, struct inode * inode,
				long block, int create, int * errp)
{
   buf_t bp;
   struct	ucred *cred;
   int blksize = inode->i_e2fs->s_blocksize, err;
   
   cred = (handle && handle->h_vctx) ? vfs_context_ucred(handle->h_vctx) : NOCRED;
   *errp = -EIO;
   if (create) {
      assert(NOCRED != cred);
      /* Allocate a new block. */
      err = ext2_balloc(inode, block, blksize, cred, &bp, 0);
      if (!err) {
         *errp = 0;
         return (bp);
      }
      
      *errp = -err;
      return (NULL);
   }

   bp = NULL;
   IULOCK(inode); // XXX the dx routines do not handle locking internally
   *errp = -(buf_bread(ITOV(inode), (daddr64_t)block, blksize, cred, &bp));
   IXLOCK(inode);
   return (bp);
}

#define GFP_KERNEL M_EXT3DIRPRV
static __inline__
void* kmalloc(size_t len, int type)
{
   void *p;
   MALLOC(p, void*, len, type, M_WAITOK);
   return (p);
}

/* XXX - Assuming GFP_KERNEL MALLOC type */
static __inline__
void kfree(void *p)
{
   FREE(p, GFP_KERNEL);
}

#define brelse buf_brelse

/* fs/ext3/dir.c support */

static __inline__
void update_atime(struct inode *ip)
{
 (ip)->i_flag |= IN_ACCESS;
 IULOCK(ip); // XXX the dx routines do not handle locking internally
 ext2_itimes(ITOV(ip));
 IXLOCK(ip);
}

struct filldir_args {
   struct uio *uio;
   int cookies;
   int count;
};

typedef int (*filldir_t)(void *, const char *, int, loff_t, ino_t, unsigned);
#define EXT2_FILLDIR_ENOSPC -99999

/* Note: offset is useless */
static int bsd_filldir (void *buf, const char *name, int namlen, loff_t offset,
		      ino_t ino, unsigned int d_type)
{
   struct filldir_args *fdp = (struct filldir_args*)buf;
   struct dirent de;
   int err;
   struct uio *uio = fdp->uio;
   
   de.d_fileno = ino;
   de.d_type = d_type;
   de.d_namlen = namlen;
   de.d_reclen = GENERIC_DIRSIZ(&de);
   
   /* Make sure there's enough room. */
   if (uio_resid(uio) <  de.d_reclen)
      return (EXT2_FILLDIR_ENOSPC);
   
   bcopy(name, de.d_name, de.d_namlen);
   bzero(de.d_name + de.d_namlen,
         de.d_reclen - offsetof(struct dirent, d_name) -
         de.d_namlen);
   
   err = uiomove((caddr_t)&de, de.d_reclen, uio);
   if (!err) {
      fdp->count += de.d_reclen;
      fdp->cookies++;
   }
   
   return (err);
}

#endif /* EXT3_LINUX_NAMEI_COMPAT_C */
