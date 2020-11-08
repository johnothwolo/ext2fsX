/*
* Copyright 2003-2006 Brian Bergstrand.
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
"@(#) $Id: ext2_apple.c,v 1.29 2006/08/27 15:34:46 bbergstrand Exp $";

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <vfs/vfs_support.h>
//#include <machine/spl.h>
#include <sys/kauth.h>
#include <sys/sysctl.h>

#include "ext2_apple.h"
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/inode.h>
#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_extern.h>

// missing clib functions (as of 10.4.x)
__private_extern__ char*
e_strrchr(const char *s, int c)
{
   char *name;
   int i;
   
   name = NULL;
   i = strlen(s);
   for (--i; i >= 0; --i) {
      if ((char)c == s[i]) {
         name = (char*)&s[i];
         break;
      }
   }
   return (name);
}


__private_extern__
int e2securelevel()
{
#ifdef notyet
    // kernel's sysctlbyname is a neutered version that only exports a small subset of the mib. Why??
    int val = 0;
    size_t sz = sizeof(val);
    (void)sysctlbyname("kern.securelevel", &val, &sz, NULL, 0);
    return (val);
#endif
    return (1); // default level
}

/* VNode Ops */

__private_extern__ int
ext2_ioctl(struct vnop_ioctl_args *ap)
/* {
      vnode_t a_vp;
      u_long a_command;
      caddr_t a_data;
      int a_fflag;
      vfs_context_t a_context;
} */
{
   struct inode *ip = VTOI(ap->a_vp);
   struct m_ext2fs *fs;
   int err = 0, super;
   u_int32_t flags, oldflags;
   kauth_cred_t cred = vfs_context_ucred(ap->a_context);
   
   super = (0 == kauth_cred_getuid(cred));
   fs = ip->i_e2fs;
   
   switch (ap->a_command) {
      case IOCBASECMD(EXT2_IOC_GETFLAGS):
         flags = ip->i_e2flags & EXT2_FL_USER_VISIBLE;
         bcopy(&flags, ap->a_data, sizeof(u_int32_t));
      break;
      
      case IOCBASECMD(EXT2_IOC_SETFLAGS):
         if (ip->i_e2fs->s_rd_only)
            return (EROFS);
         
         if (cred->cr_posix.cr_uid != ip->i_uid && !super)
            return (EACCES);
         
         bcopy(ap->a_data, &flags, sizeof(u_int32_t));
         
         if (!S_ISDIR(ip->i_mode))
            flags &= ~EXT3_DIRSYNC_FL;
            
         oldflags = ip->i_e2flags;
         
         /* Update e2flags incase someone went through chflags and the
          inode has not been sync'd to disk yet. */
         if (ip->i_flags & APPEND)
            oldflags |= EXT2_APPEND_FL;
         
         if (ip->i_flags & IMMUTABLE)
            oldflags |= EXT2_IMMUTABLE_FL;
         
         /* Root owned files marked APPEND||IMMUTABLE can only be unset
            when the kernel is not protected. */
         if ((flags ^ oldflags) & (EXT2_APPEND_FL | EXT2_IMMUTABLE_FL)) {
            if (!super && (oldflags & (EXT2_APPEND_FL | EXT2_IMMUTABLE_FL)))
               err = EPERM;
            if (super && (oldflags & (EXT2_APPEND_FL | EXT2_IMMUTABLE_FL)) &&
                  ip->i_uid == 0)
               err = securelevel_gt(cred, 0);
            if (err)
               return(err);
         }
         
         if ((flags ^ oldflags) & (EXT3_JOURNAL_DATA_FL)) {
            if (!super)
               return (EACCES);
         }
         
         if (!super) {
            flags = flags & EXT2_FL_USER_MODIFIABLE;
            flags |= oldflags & ~EXT2_FL_USER_MODIFIABLE;
         } else {
            flags |= oldflags;
         }
         ip->i_e2flags = flags;
         ip->i_flag |= IN_CHANGE|IN_MODIFIED;
         
         /* Update the BSD flags */
         if (ip->i_e2flags & EXT2_APPEND_FL)
            ip->i_flags |= super ? APPEND : UF_APPEND;
         else
            ip->i_flags &= ~(super ? APPEND : UF_APPEND);
         
         if (ip->i_e2flags & EXT2_IMMUTABLE_FL)
            ip->i_flags |= super ? IMMUTABLE : UF_IMMUTABLE;
         else
            ip->i_flags &= ~(super ? IMMUTABLE : UF_IMMUTABLE);
         
         err = ext2_update(ap->a_vp, 0);
         if (!err)
            VN_KNOTE(ap->a_vp, NOTE_ATTRIB);
      break;
      
      case IOCBASECMD(EXT2_IOC_GETVERSION):
         bcopy(&ip->i_gen, ap->a_data, sizeof(int32_t));
      break;
      
      case IOCBASECMD(EXT2_IOC_SETVERSION):
         if (cred->cr_posix.cr_uid != ip->i_uid && !super)
            err = EACCES;
         else
            err = ENOTSUP;
      break;
      
      case IOCBASECMD(EXT2_IOC_GETSBLOCK):
         lock_super(fs);
         bcopy(fs->s_es, ap->a_data, sizeof(struct ext2fs));
         unlock_super(fs);
      break;
      
      default:
         err = ENOTTY;
      break;
   }
   
   nop_ioctl(ap);
   return (err);
}

#if DIAGNOSTIC
__private_extern__ void ext2_checkdir_locked(vnode_t dvp)
{
   buf_t  bp;
   struct ext2_dir_entry_2 *ep;
   char *dirbuf;
   struct inode *dp;
   int error;
   
   dp = VTOI(dvp);
   if (is_dx(dp))
      return;

   off_t offset = 0, size = 0;
   int live = 0;
   while (size < dp->i_size) {
   
    assert(0 != (dp->i_flag & IN_LOOK));

    IULOCK(dp);
    error = ext2_blkatoff(dvp, (off_t)offset, &dirbuf, &bp);
    IXLOCK(dp);
    if (error)
        return;
   
   while (size < dp->i_size && (size - offset) < EXT2_BLOCK_SIZE(dp->i_e2fs)) {
      ep = (struct ext2_dir_entry_2 *)
			((char *)buf_dataptr(bp) + (size - offset));
      if (ep->rec_len == 0)
         break;
      live++;
      size += le16_to_cpu(ep->rec_len);
      if (ep->file_type & 0xF0)
        panic("ext2: dir (%d) zombie entry %s", dp->i_number, ep->name);
   }
   
   buf_brelse(bp);
   
   if ((size - offset) != EXT2_BLOCK_SIZE(dp->i_e2fs)) {
	   panic("ext2: dir (%u) entries at offset '%lld' do not match block size '%lu'!\n", dp->i_number, offset, EXT2_BLOCK_SIZE(dp->i_e2fs));
   }
   
   offset += EXT2_BLOCK_SIZE(dp->i_e2fs);
   } // (size < dp->i_size)
   
   if (size != dp->i_size) {
      panic("ext2: dir (%d) entries do not match dir size! (%qu,%qu)\n",
         dp->i_number, size, dp->i_size);
   }
}
#endif
