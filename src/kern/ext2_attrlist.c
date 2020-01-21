/*
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
/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

static const char whatid[] __attribute__ ((unused)) =
"@(#) $Id: ext2_attrlist.c,v 1.13 2004/07/24 22:43:53 bbergstrand Exp $";

#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/vm.h>
#include <sys/syslog.h>

#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/ext2_mount.h>
#include <gnu/ext2fs/inode.h>
#include "ext2_apple.h"

/* Ext2 supported attributes */
#define EXT2_ATTR_CMN_NATIVE ( \
   ATTR_CMN_DEVID | ATTR_CMN_FSID | ATTR_CMN_OBJTYPE | ATTR_CMN_OBJTAG | \
   ATTR_CMN_OBJID | ATTR_CMN_OBJPERMANENTID | ATTR_CMN_MODTIME | ATTR_CMN_CHGTIME | \
   ATTR_CMN_ACCTIME | ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK | \
   ATTR_CMN_USERACCESS )
#define EXT2_ATTR_CMN_SUPPORTED EXT2_ATTR_CMN_NATIVE
#define EXT2_ATTR_VOL_NATIVE ( \
   ATTR_VOL_FSTYPE | ATTR_VOL_SIGNATURE | ATTR_VOL_SIZE | \
   ATTR_VOL_SPACEFREE | ATTR_VOL_SPACEAVAIL | ATTR_VOL_IOBLOCKSIZE | \
   ATTR_VOL_OBJCOUNT |  ATTR_VOL_FILECOUNT | ATTR_VOL_DIRCOUNT | \
   ATTR_VOL_MAXOBJCOUNT | ATTR_VOL_MOUNTPOINT | ATTR_VOL_NAME | ATTR_VOL_MOUNTPOINT | \
   ATTR_VOL_MOUNTFLAGS | ATTR_VOL_MOUNTEDDEVICE | ATTR_VOL_CAPABILITIES | \
   ATTR_VOL_ATTRIBUTES | ATTR_VOL_INFO )
/*  ATTR_VOL_FILECOUNT | ATTR_VOL_DIRCOUNT aren't really native, but close enough */
#define EXT2_ATTR_VOL_SUPPORTED EXT2_ATTR_VOL_NATIVE
#define EXT2_ATTR_DIR_NATIVE ( \
   ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT | ATTR_DIR_MOUNTSTATUS )
#define EXT2_ATTR_DIR_SUPPORTED EXT2_ATTR_DIR_NATIVE
#define EXT2_ATTR_FILE_NATIVE ( \
   ATTR_FILE_LINKCOUNT | ATTR_FILE_TOTALSIZE | ATTR_FILE_ALLOCSIZE | \
   ATTR_FILE_IOBLOCKSIZE | ATTR_FILE_CLUMPSIZE | ATTR_FILE_DEVTYPE )
#define EXT2_ATTR_FILE_SUPPORTED EXT2_ATTR_FILE_NATIVE
#define EXT2_ATTR_FORK_NATIVE 0
#define EXT2_ATTR_FORK_SUPPORTED 0

#define EXT2_ATTR_CMN_SETTABLE 0
#define EXT2_ATTR_VOL_SETTABLE ATTR_VOL_NAME
#define EXT2_ATTR_DIR_SETTABLE 0
#define EXT2_ATTR_FILE_SETTABLE 0
#define EXT2_ATTR_FORK_SETTABLE 0

static int ext2_attrcalcsize(struct attrlist *);
static int ext2_packattrblk(struct attrlist *, vnode_t , void **, void **);
static unsigned long DerivePermissionSummary(uid_t, gid_t, mode_t,
   mount_t , struct ucred *, struct proc *);
static int ext2_unpackattr(struct vnode*, struct ucred*,
   struct attrlist*, void*);

int
ext2_getattrlist(
	struct vnop_getattrlist_args /* {
	vnode_t a_vp;
	struct attrlist *a_alist
	struct uio *a_uio; // INOUT
	vfs_context_t a_context;
	} */ *ap)
{
   vnode_t vp = ap->a_vp;
   struct attrlist *alist = ap->a_alist;
   int blockSize, fixedSize, attrBufSize, err;
   void *abp, *atp, *vdp;
   
   if ((ATTR_BIT_MAP_COUNT != alist->bitmapcount) ||
         (0 != (alist->commonattr & ~ATTR_CMN_VALIDMASK)) ||
         (0 != (alist->volattr & ~ATTR_VOL_VALIDMASK)) ||
         (0 != (alist->dirattr & ~ATTR_DIR_VALIDMASK)) ||
         (0 != (alist->fileattr & ~ATTR_FILE_VALIDMASK)) ||
         (0 != (alist->forkattr & ~ATTR_FORK_VALIDMASK))
         ) {
      return (EINVAL);
   }
   
    /* 
     * Requesting volume information requires setting the ATTR_VOL_INFO bit and
     * volume info requests are mutually exclusive with all other info requests.
     * We only support VOL attributes.
     */
    if ((0 != alist->volattr) && ((0 == (alist->volattr & ATTR_VOL_INFO)) ||
         (0 != alist->dirattr) || (0 != alist->fileattr) ||
         (0 != alist->forkattr) )) {
      return (EINVAL);
    }
   
    /*
	 * Reject requests for unsupported options for now:
	 */
	if (!vnode_isvroot(vp) && (alist->commonattr & ATTR_CMN_NAME))
      return (EINVAL); /*No way to determine an inode's name*/
	if (alist->commonattr & (ATTR_CMN_NAMEDATTRCOUNT | ATTR_CMN_NAMEDATTRLIST | ATTR_CMN_FNDRINFO))
      return (EINVAL);
	if (alist->fileattr &
		(ATTR_FILE_FILETYPE |
		 ATTR_FILE_FORKCOUNT |
		 ATTR_FILE_FORKLIST |
		 ATTR_FILE_DATAEXTENTS |
		 ATTR_FILE_RSRCEXTENTS)) {
		return (EINVAL);
	};
   
   fixedSize = ext2_attrcalcsize(alist);
   blockSize = fixedSize + sizeof(u_long) /* length longword */;
   if (alist->commonattr & ATTR_CMN_NAME) blockSize += NAME_MAX;
   if (alist->commonattr & ATTR_CMN_NAMEDATTRLIST) blockSize += 0; /* XXX PPD */
   if (alist->volattr & ATTR_VOL_MOUNTPOINT) blockSize += PATH_MAX;
   if (alist->volattr & ATTR_VOL_NAME) blockSize += NAME_MAX;
   if (alist->fileattr & ATTR_FILE_FORKLIST) blockSize += 0; /* XXX PPD */
   
   attrBufSize = MIN(ap->a_uio->uio_resid, blockSize);
   abp = _MALLOC(attrBufSize, M_TEMP, 0);
   assert (abp != 0);
      
   atp = abp;
   *((u_long *)atp) = 0; /* Set buffer length in case of errors */
   ++((u_long *)atp); /* Reserve space for length field */
   vdp = ((char *)atp) + fixedSize; /* Point to variable-length storage */

   err = ext2_packattrblk(alist, vp, &atp, &vdp);
   if (0 != err)
      goto exit;
      
   /* Store length of fixed + var block */
   *((u_long *)abp) = ((char*)vdp - (char*)abp);
   /* Don't copy out more data than was generated */
   attrBufSize = MIN(attrBufSize, (char*)vdp - (char*)abp);
   
   err = uiomove((caddr_t)abp, attrBufSize, ap->a_uio);

exit:
   FREE(abp, M_TEMP);

   ext2_trace_return(err);
}

/* Cribbed from ufs_attrlist.c */
__private_extern__ int
ext2_setattrlist(struct vnop_setattrlist_args *ap)
{
	struct vnode	*vp = ap->a_vp;
	struct attrlist	*alist = ap->a_alist;
	size_t attrblocksize;
	void   *attrbufptr;
	int    error;

	if (vnode_vfsisrdonly(vp))
		return (EROFS);

	/*
	 * Check the attrlist for valid inputs (i.e. be sure we understand
	 * what caller is asking).
	 */
	if ((alist->bitmapcount != ATTR_BIT_MAP_COUNT) ||
	    ((alist->commonattr & ~ATTR_CMN_SETMASK) != 0) ||
	    ((alist->volattr & ~ATTR_VOL_SETMASK) != 0) ||
	    ((alist->dirattr & ~ATTR_DIR_SETMASK) != 0) ||
	    ((alist->fileattr & ~ATTR_FILE_SETMASK) != 0) ||
	    ((alist->forkattr & ~ATTR_FORK_SETMASK) != 0))
		return (EINVAL);

	/*
	 * Setting volume information requires setting the
	 * ATTR_VOL_INFO bit. Also, volume info requests are
	 * mutually exclusive with all other info requests.
	 */
	if ((alist->volattr != 0) &&
	    (((alist->volattr & ATTR_VOL_INFO) == 0) ||
	     (alist->dirattr != 0) || (alist->fileattr != 0) ||
	     alist->forkattr != 0))
		return (EINVAL);

	/*
	 * Make sure caller isn't asking for an attibute we don't support.
	 * Right now, all we support is setting the volume name.
	 */
	if ((alist->commonattr & ~EXT2_ATTR_CMN_SETTABLE) != 0 ||
	    (alist->volattr & ~(EXT2_ATTR_VOL_SETTABLE | ATTR_VOL_INFO)) != 0 ||
	    (alist->dirattr & ~EXT2_ATTR_DIR_SETTABLE) != 0 ||
	    (alist->fileattr & ~EXT2_ATTR_FILE_SETTABLE) != 0 ||
	    (alist->forkattr & ~EXT2_ATTR_FORK_SETTABLE) != 0)
		return (EOPNOTSUPP);

	/*
	 * Setting volume information requires a vnode for the volume root.
	 */
	if (alist->volattr && 0 == vnode_isvroot(vp))
		return (EINVAL);

	attrblocksize = ap->a_uio->uio_resid;
	if (attrblocksize < ext2_attrcalcsize(alist))
		return (EINVAL);

	MALLOC(attrbufptr, void *, attrblocksize, M_TEMP, M_WAITOK);

	error = uiomove((caddr_t)attrbufptr, attrblocksize, ap->a_uio);
	if (error)
		goto ErrorExit;

	error = ext2_unpackattr(vp, vfs_context_ucred(ap->a_context), alist, attrbufptr);

ErrorExit:
	FREE(attrbufptr, M_TEMP);
	return (error);
}


static int ext2_attrcalcsize(struct attrlist *attrlist)
{
	int size;
	attrgroup_t a;
	
#if ((ATTR_CMN_NAME			| ATTR_CMN_DEVID			| ATTR_CMN_FSID 			| ATTR_CMN_OBJTYPE 		| \
      ATTR_CMN_OBJTAG		| ATTR_CMN_OBJID			| ATTR_CMN_OBJPERMANENTID	| ATTR_CMN_PAROBJID		| \
      ATTR_CMN_SCRIPT		| ATTR_CMN_CRTIME			| ATTR_CMN_MODTIME			| ATTR_CMN_CHGTIME		| \
      ATTR_CMN_ACCTIME		| ATTR_CMN_BKUPTIME			| ATTR_CMN_FNDRINFO			| ATTR_CMN_OWNERID		| \
      ATTR_CMN_GRPID		| ATTR_CMN_ACCESSMASK		| ATTR_CMN_NAMEDATTRCOUNT	| ATTR_CMN_NAMEDATTRLIST| \
      ATTR_CMN_FLAGS		| ATTR_CMN_USERACCESS) != ATTR_CMN_VALIDMASK)
#error AttributeBlockSize: Missing bits in common mask computation!
#endif
	assert((attrlist->commonattr & ~ATTR_CMN_VALIDMASK) == 0);

#if ((ATTR_VOL_FSTYPE		| ATTR_VOL_SIGNATURE		| ATTR_VOL_SIZE				| ATTR_VOL_SPACEFREE 	| \
      ATTR_VOL_SPACEAVAIL	| ATTR_VOL_MINALLOCATION	| ATTR_VOL_ALLOCATIONCLUMP	| ATTR_VOL_IOBLOCKSIZE	| \
      ATTR_VOL_OBJCOUNT		| ATTR_VOL_FILECOUNT		| ATTR_VOL_DIRCOUNT			| ATTR_VOL_MAXOBJCOUNT	| \
      ATTR_VOL_MOUNTPOINT	| ATTR_VOL_NAME				| ATTR_VOL_MOUNTFLAGS		| ATTR_VOL_INFO 		| \
      ATTR_VOL_MOUNTEDDEVICE| ATTR_VOL_ENCODINGSUSED	| ATTR_VOL_CAPABILITIES		| ATTR_VOL_ATTRIBUTES) != ATTR_VOL_VALIDMASK)
#error AttributeBlockSize: Missing bits in volume mask computation!
#endif
	assert((attrlist->volattr & ~ATTR_VOL_VALIDMASK) == 0);

#if ((ATTR_DIR_LINKCOUNT | ATTR_DIR_ENTRYCOUNT | ATTR_DIR_MOUNTSTATUS) != ATTR_DIR_VALIDMASK)
#error AttributeBlockSize: Missing bits in directory mask computation!
#endif
	assert((attrlist->dirattr & ~ATTR_DIR_VALIDMASK) == 0);
#if ((ATTR_FILE_LINKCOUNT	| ATTR_FILE_TOTALSIZE		| ATTR_FILE_ALLOCSIZE 		| ATTR_FILE_IOBLOCKSIZE 	| \
      ATTR_FILE_CLUMPSIZE	| ATTR_FILE_DEVTYPE			| ATTR_FILE_FILETYPE		| ATTR_FILE_FORKCOUNT		| \
      ATTR_FILE_FORKLIST	| ATTR_FILE_DATALENGTH		| ATTR_FILE_DATAALLOCSIZE	| ATTR_FILE_DATAEXTENTS		| \
      ATTR_FILE_RSRCLENGTH	| ATTR_FILE_RSRCALLOCSIZE	| ATTR_FILE_RSRCEXTENTS) != ATTR_FILE_VALIDMASK)
#error AttributeBlockSize: Missing bits in file mask computation!
#endif
	assert((attrlist->fileattr & ~ATTR_FILE_VALIDMASK) == 0);

#if ((ATTR_FORK_TOTALSIZE | ATTR_FORK_ALLOCSIZE) != ATTR_FORK_VALIDMASK)
#error AttributeBlockSize: Missing bits in fork mask computation!
#endif
	assert((attrlist->forkattr & ~ATTR_FORK_VALIDMASK) == 0);

	size = 0;
	
	if ((a = attrlist->commonattr) != 0) {
        if (a & ATTR_CMN_NAME) size += sizeof(struct attrreference);
		if (a & ATTR_CMN_DEVID) size += sizeof(dev_t);
		if (a & ATTR_CMN_FSID) size += sizeof(fsid_t);
		if (a & ATTR_CMN_OBJTYPE) size += sizeof(fsobj_type_t);
		if (a & ATTR_CMN_OBJTAG) size += sizeof(fsobj_tag_t);
		if (a & ATTR_CMN_OBJID) size += sizeof(fsobj_id_t);
        if (a & ATTR_CMN_OBJPERMANENTID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_PAROBJID) size += sizeof(fsobj_id_t);
		if (a & ATTR_CMN_SCRIPT) size += sizeof(text_encoding_t);
		if (a & ATTR_CMN_CRTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_MODTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_CHGTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_ACCTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_BKUPTIME) size += sizeof(struct timespec);
		if (a & ATTR_CMN_FNDRINFO) size += 32 * sizeof(u_int8_t);
		if (a & ATTR_CMN_OWNERID) size += sizeof(uid_t);
		if (a & ATTR_CMN_GRPID) size += sizeof(gid_t);
		if (a & ATTR_CMN_ACCESSMASK) size += sizeof(u_long);
		if (a & ATTR_CMN_NAMEDATTRCOUNT) size += sizeof(u_long);
		if (a & ATTR_CMN_NAMEDATTRLIST) size += sizeof(struct attrreference);
		if (a & ATTR_CMN_FLAGS) size += sizeof(u_long);
		if (a & ATTR_CMN_USERACCESS) size += sizeof(u_long);
	};
	if ((a = attrlist->volattr) != 0) {
		if (a & ATTR_VOL_FSTYPE) size += sizeof(u_long);
		if (a & ATTR_VOL_SIGNATURE) size += sizeof(u_long);
		if (a & ATTR_VOL_SIZE) size += sizeof(off_t);
		if (a & ATTR_VOL_SPACEFREE) size += sizeof(off_t);
		if (a & ATTR_VOL_SPACEAVAIL) size += sizeof(off_t);
		if (a & ATTR_VOL_MINALLOCATION) size += sizeof(off_t);
		if (a & ATTR_VOL_ALLOCATIONCLUMP) size += sizeof(off_t);
		if (a & ATTR_VOL_IOBLOCKSIZE) size += sizeof(size_t);
		if (a & ATTR_VOL_OBJCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_FILECOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_DIRCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_MAXOBJCOUNT) size += sizeof(u_long);
		if (a & ATTR_VOL_MOUNTPOINT) size += sizeof(struct attrreference);
        if (a & ATTR_VOL_NAME) size += sizeof(struct attrreference);
        if (a & ATTR_VOL_MOUNTFLAGS) size += sizeof(u_long);
        if (a & ATTR_VOL_MOUNTEDDEVICE) size += sizeof(struct attrreference);
        if (a & ATTR_VOL_ENCODINGSUSED) size += sizeof(unsigned long long);
        if (a & ATTR_VOL_CAPABILITIES) size += sizeof(vol_capabilities_attr_t);
        if (a & ATTR_VOL_ATTRIBUTES) size += sizeof(vol_attributes_attr_t);
	};
	if ((a = attrlist->dirattr) != 0) {
		if (a & ATTR_DIR_LINKCOUNT) size += sizeof(u_long);
		if (a & ATTR_DIR_ENTRYCOUNT) size += sizeof(u_long);
		if (a & ATTR_DIR_MOUNTSTATUS) size += sizeof(u_long);
	};
	if ((a = attrlist->fileattr) != 0) {
		if (a & ATTR_FILE_LINKCOUNT) size += sizeof(u_long);
		if (a & ATTR_FILE_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_ALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_IOBLOCKSIZE) size += sizeof(size_t);
		if (a & ATTR_FILE_CLUMPSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_DEVTYPE) size += sizeof(u_long);
		if (a & ATTR_FILE_FILETYPE) size += sizeof(u_long);
		if (a & ATTR_FILE_FORKCOUNT) size += sizeof(u_long);
		if (a & ATTR_FILE_FORKLIST) size += sizeof(struct attrreference);
		if (a & ATTR_FILE_DATALENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_DATAEXTENTS) size += sizeof(extentrecord);
		if (a & ATTR_FILE_RSRCLENGTH) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCALLOCSIZE) size += sizeof(off_t);
		if (a & ATTR_FILE_RSRCEXTENTS) size += sizeof(extentrecord);
	};
	if ((a = attrlist->forkattr) != 0) {
		if (a & ATTR_FORK_TOTALSIZE) size += sizeof(off_t);
		if (a & ATTR_FORK_ALLOCSIZE) size += sizeof(off_t);
	};

    return size;
}

static int
ext2_packvolattr (struct attrlist *alist,
			 struct inode *ip,	/* root inode */
			 void **attrbufptrptr,
			 void **varbufptrptr)
{
    struct proc *p;
    void *attrbufptr;
    void *varbufptr;
    struct ext2_sb_info *fs;
    mount_t mp;
    struct ext2mount *emp;
    attrgroup_t a;
    u_long attrlength;
    int err = 0;
    struct timespec ts;
	
	attrbufptr = *attrbufptrptr;
	varbufptr = *varbufptrptr;
	mp = ITOVFS(ip);
    emp = VFSTOEXT2(mp);
    fs = ip->i_e2fs;
   
    p = current_proc();

    if ((a = alist->commonattr) != 0) {
      
        if (a & ATTR_CMN_NAME) {
            attrlength = ext2_vol_label_len(fs->s_es->s_volume_name);

            ((struct attrreference *)attrbufptr)->attr_dataoffset = (u_int8_t *)varbufptr - (u_int8_t *)attrbufptr;
            ((struct attrreference *)attrbufptr)->attr_length = attrlength;
            /* copy name */
            (void) strncpy((unsigned char *)varbufptr, fs->s_es->s_volume_name, attrlength);
            *((unsigned char *)varbufptr + attrlength) = 0;

            /* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
            (u_int8_t *)varbufptr += attrlength + ((4 - (attrlength & 3)) & 3);
            ++((struct attrreference *)attrbufptr);
        };
        if (a & ATTR_CMN_DEVID) *((dev_t *)attrbufptr)++ = vnode_specrdev(emp->um_devvp);
        if (a & ATTR_CMN_FSID) *((fsid_t *)attrbufptr)++ =  vfs_statfs(mp)->f_fsid;
        if (a & ATTR_CMN_OBJTYPE) *((fsobj_type_t *)attrbufptr)++ = 0;
        if (a & ATTR_CMN_OBJTAG) *((fsobj_tag_t *)attrbufptr)++ = VT_EXT2;
        if (a & ATTR_CMN_OBJID)	{
            ((fsobj_id_t *)attrbufptr)->fid_objno = ROOTINO;
            ((fsobj_id_t *)attrbufptr)->fid_generation = ip->i_gen;
            ++((fsobj_id_t *)attrbufptr);
        };
        if (a & ATTR_CMN_OBJPERMANENTID) {
            ((fsobj_id_t *)attrbufptr)->fid_objno = ROOTINO;
            ((fsobj_id_t *)attrbufptr)->fid_generation = ip->i_gen;
            ++((fsobj_id_t *)attrbufptr);
        };
        if (a & ATTR_CMN_PAROBJID) {
            ((fsobj_id_t *)attrbufptr)->fid_objno = 0;
            ((fsobj_id_t *)attrbufptr)->fid_generation = 0;
            ++((fsobj_id_t *)attrbufptr);
        };
      
      if (a & ATTR_CMN_SCRIPT) *((text_encoding_t *)attrbufptr)++ = 0;
      
      ts.tv_sec = le32_to_cpu(fs->s_es->s_mkfs_time); ts.tv_nsec = 0;
		if (a & ATTR_CMN_CRTIME) *((struct timespec *)attrbufptr)++ = ts;
      ts.tv_sec = ip->i_mtime; ts.tv_nsec = ip->i_mtimensec;
		if (a & ATTR_CMN_MODTIME) *((struct timespec *)attrbufptr)++ = ts;
      ts.tv_sec = ip->i_ctime; ts.tv_nsec = ip->i_ctimensec;
		if (a & ATTR_CMN_CHGTIME) *((struct timespec *)attrbufptr)++ = ts;
		ts.tv_sec = ip->i_atime; ts.tv_nsec = ip->i_atimensec;
      if (a & ATTR_CMN_ACCTIME) *((struct timespec *)attrbufptr)++ = ts;
		if (a & ATTR_CMN_BKUPTIME) {
			((struct timespec *)attrbufptr)->tv_sec = 0;
			((struct timespec *)attrbufptr)->tv_nsec = 0;
			++((struct timespec *)attrbufptr);
		};
		if (a & ATTR_CMN_FNDRINFO) {
            bzero (attrbufptr, 32 * sizeof(u_int8_t));
            (u_int8_t *)attrbufptr += 32 * sizeof(u_int8_t);
		};
		if (a & ATTR_CMN_OWNERID) *((uid_t *)attrbufptr)++ = ip->i_uid;
		if (a & ATTR_CMN_GRPID) *((gid_t *)attrbufptr)++ = ip->i_gid;
		if (a & ATTR_CMN_ACCESSMASK) *((u_long *)attrbufptr)++ = (u_long)ip->i_mode;
		if (a & ATTR_CMN_FLAGS) *((u_long *)attrbufptr)++ = 0;
		if (a & ATTR_CMN_USERACCESS) {
         *((u_long *)attrbufptr)++ = DerivePermissionSummary(ip->i_uid,
            ip->i_gid, ip->i_mode, mp, p->p_ucred, p);
      }
	}
	
	if ((a = alist->volattr) != 0) {
        struct vfsstatfs *sbp;
        int numdirs = fs->s_dircount;

        sbp = vfs_statfs(mp);

        if (a & ATTR_VOL_FSTYPE) *((u_long *)attrbufptr)++ = (u_long)vfs_typenum(mp);
        if (a & ATTR_VOL_SIGNATURE) *((u_long *)attrbufptr)++ = (u_long)EXT2_SUPER_MAGIC;
        if (a & ATTR_VOL_SIZE) *((off_t *)attrbufptr)++ = (off_t)sbp->f_blocks * sbp->f_bsize;
        if (a & ATTR_VOL_SPACEFREE) *((off_t *)attrbufptr)++ = (off_t)sbp->f_bfree * sbp->f_bsize;
        if (a & ATTR_VOL_SPACEAVAIL) *((off_t *)attrbufptr)++ = (off_t)sbp->f_bavail * sbp->f_bsize;
        if (a & ATTR_VOL_MINALLOCATION) *((off_t *)attrbufptr)++ = sbp->f_bsize;
        if (a & ATTR_VOL_ALLOCATIONCLUMP) *((off_t *)attrbufptr)++ = sbp->f_bsize;
        if (a & ATTR_VOL_IOBLOCKSIZE) *((size_t *)attrbufptr)++ = sbp->f_iosize;
        if (a & ATTR_VOL_OBJCOUNT) *((u_long *)attrbufptr)++ = sbp->f_files - sbp->f_ffree;
        if (a & ATTR_VOL_FILECOUNT) *((u_long *)attrbufptr)++ = (sbp->f_files - sbp->f_ffree) - numdirs;
        if (a & ATTR_VOL_DIRCOUNT) *((u_long *)attrbufptr)++ = numdirs;
        if (a & ATTR_VOL_MAXOBJCOUNT) *((u_long *)attrbufptr)++ = sbp->f_files;
      if (a & ATTR_VOL_MOUNTPOINT) {
         ((struct attrreference *)attrbufptr)->attr_dataoffset =
               (char *)varbufptr - (char *)attrbufptr;
         ((struct attrreference *)attrbufptr)->attr_length =
               strlen(sbp->f_mntonname) + 1;
         attrlength = ((struct attrreference *)attrbufptr)->attr_length;
         /* round up to the next 4-byte boundary: */
         attrlength = attrlength + ((4 - (attrlength & 3)) & 3);
         (void) bcopy(sbp->f_mntonname, varbufptr, attrlength);
            
         /* Advance beyond the space just allocated: */
         (char *)varbufptr += attrlength;
         ++((struct attrreference *)attrbufptr);
      }
      if (a & ATTR_VOL_NAME) {
         attrlength = ext2_vol_label_len(fs->s_es->s_volume_name);
         ((struct attrreference *)attrbufptr)->attr_dataoffset = (u_int8_t *)varbufptr - (u_int8_t *)attrbufptr;
         ((struct attrreference *)attrbufptr)->attr_length = attrlength;
         /* Copy vol name */
         (void) strncpy((unsigned char *)varbufptr, fs->s_es->s_volume_name, attrlength);
         *((unsigned char *)varbufptr + attrlength) = 0;

         /* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
         (u_int8_t *)varbufptr += attrlength + ((4 - (attrlength & 3)) & 3);
         ++((struct attrreference *)attrbufptr);
      };
		if (a & ATTR_VOL_MOUNTFLAGS) *((u_long *)attrbufptr)++ = (u_long)(vfs_flags(mp) & 0xFFFFFFFF00000000ULL);
        if (a & ATTR_VOL_MOUNTEDDEVICE) {
            ((struct attrreference *)attrbufptr)->attr_dataoffset = (u_int8_t *)varbufptr - (u_int8_t *)attrbufptr;
            ((struct attrreference *)attrbufptr)->attr_length = strlen(sbp->f_mntfromname) + 1;
			attrlength = ((struct attrreference *)attrbufptr)->attr_length;
			attrlength = attrlength + ((4 - (attrlength & 3)) & 3);		/* round up to the next 4-byte boundary: */
			(void) bcopy(sbp->f_mntfromname, varbufptr, attrlength);
			
			/* Advance beyond the space just allocated: */
            (u_int8_t *)varbufptr += attrlength;
            ++((struct attrreference *)attrbufptr);
        };
        if (a & ATTR_VOL_ENCODINGSUSED) *((unsigned long long *)attrbufptr)++ = (unsigned long long)0;
        if (a & ATTR_VOL_CAPABILITIES) {
         int extracaps = 0;
         if (EXT3_HAS_COMPAT_FEATURE(fs, EXT3_FEATURE_COMPAT_HAS_JOURNAL))
            extracaps = VOL_CAP_FMT_JOURNAL /* | VOL_CAP_FMT_JOURNAL_ACTIVE */;
            
         /* Capabilities we support */
        	((vol_capabilities_attr_t *)attrbufptr)->capabilities[VOL_CAPABILITIES_FORMAT] =
            VOL_CAP_FMT_SYMBOLICLINKS|
            VOL_CAP_FMT_HARDLINKS|
            VOL_CAP_FMT_SPARSE_FILES|
            VOL_CAP_FMT_CASE_SENSITIVE|
            VOL_CAP_FMT_CASE_PRESERVING|
            VOL_CAP_FMT_FAST_STATFS|
            extracaps;
        	((vol_capabilities_attr_t *)attrbufptr)->capabilities[VOL_CAPABILITIES_INTERFACES] =
            VOL_CAP_INT_ATTRLIST|
            /* VOL_CAP_INT_NFSEXPORT| */
            VOL_CAP_INT_VOL_RENAME|
            VOL_CAP_INT_ADVLOCK|
            VOL_CAP_INT_FLOCK;
        	((vol_capabilities_attr_t *)attrbufptr)->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
        	((vol_capabilities_attr_t *)attrbufptr)->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
         
         /* Capabilities we know about */
        	((vol_capabilities_attr_t *)attrbufptr)->valid[VOL_CAPABILITIES_FORMAT] =
            VOL_CAP_FMT_PERSISTENTOBJECTIDS |
            VOL_CAP_FMT_SYMBOLICLINKS |
            VOL_CAP_FMT_HARDLINKS |
            VOL_CAP_FMT_JOURNAL |
            VOL_CAP_FMT_JOURNAL_ACTIVE |
            VOL_CAP_FMT_NO_ROOT_TIMES |
            VOL_CAP_FMT_SPARSE_FILES |
            VOL_CAP_FMT_ZERO_RUNS |
            VOL_CAP_FMT_CASE_SENSITIVE |
            VOL_CAP_FMT_CASE_PRESERVING |
            VOL_CAP_FMT_FAST_STATFS;
        	((vol_capabilities_attr_t *)attrbufptr)->valid[VOL_CAPABILITIES_INTERFACES] =
            VOL_CAP_INT_SEARCHFS |
            VOL_CAP_INT_ATTRLIST |
            VOL_CAP_INT_NFSEXPORT |
            VOL_CAP_INT_READDIRATTR |
            VOL_CAP_INT_EXCHANGEDATA |
            VOL_CAP_INT_COPYFILE |
            VOL_CAP_INT_ALLOCATE |
            VOL_CAP_INT_VOL_RENAME |
            VOL_CAP_INT_ADVLOCK |
            VOL_CAP_INT_FLOCK ;
        	((vol_capabilities_attr_t *)attrbufptr)->valid[VOL_CAPABILITIES_RESERVED1] = 0;
        	((vol_capabilities_attr_t *)attrbufptr)->valid[VOL_CAPABILITIES_RESERVED2] = 0;

            ++((vol_capabilities_attr_t *)attrbufptr);
        };
        if (a & ATTR_VOL_ATTRIBUTES) {
        	((vol_attributes_attr_t *)attrbufptr)->validattr.commonattr = EXT2_ATTR_CMN_NATIVE;
        	((vol_attributes_attr_t *)attrbufptr)->validattr.volattr = EXT2_ATTR_VOL_SUPPORTED;
        	((vol_attributes_attr_t *)attrbufptr)->validattr.dirattr = EXT2_ATTR_DIR_SUPPORTED;
        	((vol_attributes_attr_t *)attrbufptr)->validattr.fileattr = EXT2_ATTR_FILE_SUPPORTED;
        	((vol_attributes_attr_t *)attrbufptr)->validattr.forkattr = EXT2_ATTR_FORK_SUPPORTED;

        	((vol_attributes_attr_t *)attrbufptr)->nativeattr.commonattr = EXT2_ATTR_CMN_NATIVE;
        	((vol_attributes_attr_t *)attrbufptr)->nativeattr.volattr = EXT2_ATTR_VOL_NATIVE;
        	((vol_attributes_attr_t *)attrbufptr)->nativeattr.dirattr = EXT2_ATTR_DIR_NATIVE;
        	((vol_attributes_attr_t *)attrbufptr)->nativeattr.fileattr = EXT2_ATTR_FILE_NATIVE;
        	((vol_attributes_attr_t *)attrbufptr)->nativeattr.forkattr = EXT2_ATTR_FORK_NATIVE;

            ++((vol_attributes_attr_t *)attrbufptr);
        }
	}

	*attrbufptrptr = attrbufptr;
	*varbufptrptr = varbufptr;
   
   ext2_trace_return(err);
}

static void
ext2_packcommonattr (struct attrlist *alist,
				struct inode *ip,
				void **attrbufptrptr,
				void **varbufptrptr)
{
	vnode_t vp;
    void *attrbufptr;
	void *varbufptr;
	attrgroup_t a;
	u_long attrlength;
    struct timespec ts;
	int vroot;
    
	attrbufptr = *attrbufptrptr;
	varbufptr = *varbufptrptr;
	
    vp = ITOV(ip);
    vroot = vnode_isvroot(vp);
    if ((a = alist->commonattr) != 0) {
		struct ext2mount *emp = VFSTOEXT2(ITOVFS(ip));

        if (a & ATTR_CMN_NAME) {
			/* special case root since we know how to get it's name
			if (ITOV(ip)->v_flag & VROOT) {
				attrlength = strlen(emp->um_e2fs->s_es->s_volume_name) + 1;
				(void) strncpy((unsigned char *)varbufptr, emp->um_e2fs->s_es->s_volume_name, attrlength);
            *((unsigned char *)varbufptr + attrlength) = 0;
        	} else */ {
            attrlength = 0; // No way to determine the name
         }

			((struct attrreference *)attrbufptr)->attr_dataoffset = (u_int8_t *)varbufptr - (u_int8_t *)attrbufptr;
			((struct attrreference *)attrbufptr)->attr_length = attrlength;
            /* Advance beyond the space just allocated and round up to the next 4-byte boundary: */
            (u_int8_t *)varbufptr += attrlength + ((4 - (attrlength & 3)) & 3);
            ++((struct attrreference *)attrbufptr);
        };
		if (a & ATTR_CMN_DEVID) *((dev_t *)attrbufptr)++ = vnode_specrdev(ip->i_devvp);
		if (a & ATTR_CMN_FSID) *((fsid_t *)attrbufptr)++ = vfs_statfs(vnode_mount(vp))->f_fsid;
		if (a & ATTR_CMN_OBJTYPE) *((fsobj_type_t *)attrbufptr)++ = vnode_vtype(vp);
		if (a & ATTR_CMN_OBJTAG) *((fsobj_tag_t *)attrbufptr)++ = vnode_tag(vp);
        if (a & ATTR_CMN_OBJID)	{
			if (vroot)
				((fsobj_id_t *)attrbufptr)->fid_objno = ROOTINO;	/* force root */
			else
            	((fsobj_id_t *)attrbufptr)->fid_objno = ip->i_number;
			((fsobj_id_t *)attrbufptr)->fid_generation = ip->i_gen;
			++((fsobj_id_t *)attrbufptr);
		};
        if (a & ATTR_CMN_OBJPERMANENTID)	{
			if (vroot)
				((fsobj_id_t *)attrbufptr)->fid_objno = ROOTINO;	/* force root */
			else
            	((fsobj_id_t *)attrbufptr)->fid_objno = ip->i_number;
            ((fsobj_id_t *)attrbufptr)->fid_generation = ip->i_gen;
            ++((fsobj_id_t *)attrbufptr);
        };
		if (a & ATTR_CMN_PAROBJID) {

			if (vroot)
				((fsobj_id_t *)attrbufptr)->fid_objno = ROOTINO;
			else
            	((fsobj_id_t *)attrbufptr)->fid_objno = 0;
			((fsobj_id_t *)attrbufptr)->fid_generation = 0;
			++((fsobj_id_t *)attrbufptr);
		};
      if (a & ATTR_CMN_SCRIPT) *((text_encoding_t *)attrbufptr)++ = 0;
      
      if (vroot) {
          ts.tv_sec = le32_to_cpu(emp->um_e2fs->s_es->s_mkfs_time); ts.tv_nsec = 0;
      } else {
         ts.tv_sec = ip->i_ctime; ts.tv_nsec = ip->i_ctimensec;
      }
		if (a & ATTR_CMN_CRTIME) *((struct timespec *)attrbufptr)++ = ts;
            ts.tv_sec = ip->i_mtime; ts.tv_nsec = ip->i_mtimensec;
		if (a & ATTR_CMN_MODTIME) *((struct timespec *)attrbufptr)++ = ts;
            ts.tv_sec = ip->i_ctime; ts.tv_nsec = ip->i_ctimensec;
		if (a & ATTR_CMN_CHGTIME) *((struct timespec *)attrbufptr)++ = ts;
            ts.tv_sec = ip->i_atime; ts.tv_nsec = ip->i_atimensec;
		if (a & ATTR_CMN_ACCTIME) *((struct timespec *)attrbufptr)++ = ts;
		if (a & ATTR_CMN_BKUPTIME) {
			((struct timespec *)attrbufptr)->tv_sec = 0;
			((struct timespec *)attrbufptr)->tv_nsec = 0;
			++((struct timespec *)attrbufptr);
		};
		if (a & ATTR_CMN_FNDRINFO) {
      #if 0
			struct finder_info finfo = {0};

			finfo.fdFlags = 0;
			finfo.fdLocation.v = -1;
			finfo.fdLocation.h = -1;
			if (ITOV(ip)->v_type == VREG) {
				finfo.fdType = 0;
				finfo.fdCreator = 0;
			}
            bcopy (&finfo, attrbufptr, sizeof(finfo));
            (u_int8_t *)attrbufptr += sizeof(finfo);
            bzero (attrbufptr, EXTFNDRINFOSIZE);
            (u_int8_t *)attrbufptr += EXTFNDRINFOSIZE;
      #else
         bzero (attrbufptr, 32 * sizeof(u_int8_t));
         (u_int8_t *)attrbufptr += 32 * sizeof(u_int8_t);
      #endif
		}
		if (a & ATTR_CMN_OWNERID) *((uid_t *)attrbufptr)++ = ip->i_uid;
		if (a & ATTR_CMN_GRPID) *((gid_t *)attrbufptr)++ = ip->i_gid;
		if (a & ATTR_CMN_ACCESSMASK) *((u_long *)attrbufptr)++ = (u_long)ip->i_mode;
		if (a & ATTR_CMN_FLAGS) *((u_long *)attrbufptr)++ = 0; /* could also use ip->i_flag */
		if (a & ATTR_CMN_USERACCESS) {
			*((u_long *)attrbufptr)++ =
				DerivePermissionSummary(ip->i_uid,
										ip->i_gid,
										ip->i_mode,
										ITOVFS(ip),
										current_proc()->p_ucred,
										current_proc());
		};
	};
	
	*attrbufptrptr = attrbufptr;
	*varbufptrptr = varbufptr;
}


static void
ext2_packdirattr(struct attrlist *alist,
			struct inode *ip,
			void **attrbufptrptr,
			void **varbufptrptr)
{
    vnode_t vp;
    void *attrbufptr;
    attrgroup_t a;
    int filcnt, dircnt;
	
	attrbufptr = *attrbufptrptr;
	filcnt = dircnt = 0;
	
	vp = ITOV(ip);
    a = alist->dirattr;
	if ((vnode_vtype(vp) == VDIR) && (a != 0)) {
		if (a & ATTR_DIR_LINKCOUNT) {
			*((u_long *)attrbufptr)++ = ip->i_nlink;
		}
		if (a & ATTR_DIR_ENTRYCOUNT) {
			/* exclude '.' and '..' from total caount */
			*((u_long *)attrbufptr)++ = ((ip->i_nlink <= 2) ? 0 : (ip->i_nlink - 2));
		}
		if (a & ATTR_DIR_MOUNTSTATUS) {
			if (vnode_mountedhere(vp)) {
				*((u_long *)attrbufptr)++ = DIR_MNTSTATUS_MNTPOINT;
			} else {
				*((u_long *)attrbufptr)++ = 0;
			};
		};
	};
	
	*attrbufptrptr = attrbufptr;
}


static void
ext2_packfileattr(struct attrlist *alist,
			 struct inode *ip,
			 void **attrbufptrptr,
			 void **varbufptrptr)
{
    vnode_t vp;
    void *attrbufptr = *attrbufptrptr;
    void *varbufptr = *varbufptrptr;
    attrgroup_t a = alist->fileattr;
    struct ext2_sb_info *fs = ip->i_e2fs;
	
	vp = ITOV(ip);
    if ((vnode_vtype(vp) == VREG) && (a != 0)) {
		if (a & ATTR_FILE_LINKCOUNT)
			*((u_long *)attrbufptr)++ = ip->i_nlink;
		if (a & ATTR_FILE_TOTALSIZE)
			*((off_t *)attrbufptr)++ = (off_t)ip->i_size;
		if (a & ATTR_FILE_ALLOCSIZE)
			*((off_t *)attrbufptr)++ = (off_t)EXT2_BLOCK_SIZE(fs) * (off_t)ip->i_blocks;
		if (a & ATTR_FILE_IOBLOCKSIZE)
			*((u_long *)attrbufptr)++ = EXT2_BLOCK_SIZE(fs);
		if (a & ATTR_FILE_CLUMPSIZE)
			*((u_long *)attrbufptr)++ = ip->i_e2fs->s_frag_size;
		if (a & ATTR_FILE_DEVTYPE)
			*((u_long *)attrbufptr)++ = (u_long)vnode_specrdev(ip->i_devvp);
		if (a & ATTR_FILE_DATALENGTH)
			*((off_t *)attrbufptr)++ = (off_t)ip->i_size;
		if (a & ATTR_FILE_DATAALLOCSIZE)
			*((off_t *)attrbufptr)++ = (off_t)EXT2_BLOCK_SIZE(fs) * (off_t)ip->i_blocks;
		if (a & ATTR_FILE_RSRCLENGTH)
			*((off_t *)attrbufptr)++ = (off_t)0;
		if (a & ATTR_FILE_RSRCALLOCSIZE)
			*((off_t *)attrbufptr)++ = (off_t)0;
	}
	
	*attrbufptrptr = attrbufptr;
	*varbufptrptr = varbufptr;
}

int
ext2_packattrblk(struct attrlist *alist,
			vnode_t vp,
			void **attrbufptrptr,
			void **varbufptrptr)
{
	struct inode *ip = VTOI(vp);

	if (alist->volattr != 0) {
		return (ext2_packvolattr (alist, ip, attrbufptrptr, varbufptrptr));
   } else {
      ext2_packcommonattr(alist, ip, attrbufptrptr, varbufptrptr);
      
      switch (vnode_vtype(vp)) {
         case VDIR:
            ext2_packdirattr(alist, ip, attrbufptrptr, varbufptrptr);
         break;
         
         case VREG:
            ext2_packfileattr(alist, ip, attrbufptrptr, varbufptrptr);
         break;
         
         /* Without this the compiler complains about VNON,VBLK,VCHR,VLNK,VSOCK,VFIFO,VBAD and VSTR
            not being handled...
         */
         default:
         break;
      }
   }
   
   return (0);
}

static int
ext2_check_label(const char *label)
{
   int c, i;

   for (i = 0, c = 0; i < EXT2_VOL_LABEL_LENGTH; i++) {
      c = (u_char)*label++;
      if (c < ' ' + !i || strchr(EXT2_VOL_LABEL_INVAL_CHARS, c))
         break;
   }
   return ((i && !c) ? 0 : EINVAL);
}

/* Cribbed from ufs_attrlist.c */
/*
 * Unpack the volume-related attributes from a setattrlist call into the
 * appropriate in-memory and on-disk structures.
 */
static int
ext2_unpackvolattr(
    struct vnode	*vp,
    struct ucred	*cred,
    attrgroup_t		 attrs,
    void		*attrbufptr)
{
	int		 error;
   struct ext2_sb_info *fsp = (VTOI(vp))->i_e2fs;
	attrreference_t *attrref;

	error = 0;

	if (attrs & ATTR_VOL_NAME) {
		char	*name;
		int	 name_length;

		attrref = attrbufptr;
		name = ((char*)attrbufptr) + attrref->attr_dataoffset;
		name_length = strlen(name);
      
      if (name_length > EXT2_VOL_LABEL_LENGTH) {
         log(LOG_WARNING, "ext2: Warning volume label too long, truncating.\n");
         name_length = EXT2_VOL_LABEL_LENGTH;
      }
      
		error = ext2_check_label(name);
      if (name_length && !error) {
         lock_super(fsp);
         bzero(fsp->s_es->s_volume_name, EXT2_VOL_LABEL_LENGTH);
         bcopy (name, fsp->s_es->s_volume_name, name_length);
         fsp->s_dirt = 1;
         unlock_super(fsp);
      }
      else
         error = EINVAL;

		/* Advance buffer pointer past attribute reference */
		attrbufptr = ++attrref;
	}

	return (error);
}

/*
 * Unpack the attributes from a setattrlist call into the
 * appropriate in-memory and on-disk structures.  Right now,
 * we only support the volume name.
 */
int ext2_unpackattr(
    struct vnode	*vp,
    struct ucred	*cred,
    struct attrlist	*alist,
    void		*attrbufptr)
{
	int error;

	error = 0;

	if (alist->volattr != 0) {
		error = ext2_unpackvolattr(vp, cred, alist->volattr,
		    attrbufptr);
	}

	return (error);
}

/* Cribbed from hfs/hfs_attrlist.c */
unsigned long
DerivePermissionSummary(uid_t obj_uid, gid_t obj_gid, mode_t obj_mode,
			mount_t mp, struct ucred *cred, struct proc *p)
{
	register gid_t *gp;
	unsigned long permissions;
	int i;

	if (obj_uid == UNKNOWNUID)
		obj_uid = console_user;

	/* User id 0 (root) always gets access. */
	if (cred->cr_uid == 0) {
		permissions = R_OK | W_OK | X_OK;
		goto Exit;
	};

	/* Otherwise, check the owner. */
	if (obj_uid == cred->cr_uid) {
		permissions = ((unsigned long)obj_mode & S_IRWXU) >> 6;
		goto Exit;
	}

	/* Otherwise, check the groups. */
	if (! (vfs_flags(mp) & MNT_UNKNOWNPERMISSIONS)) {
		for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++) {
			if (obj_gid == *gp) {
				permissions = ((unsigned long)obj_mode & S_IRWXG) >> 3;
				goto Exit;
			}
		}
	}

	/* Otherwise, settle for 'others' access. */
	permissions = (unsigned long)obj_mode & S_IRWXO;

Exit:
	return (permissions);    
}
