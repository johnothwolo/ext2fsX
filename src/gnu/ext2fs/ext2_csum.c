/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017, Fedor Uporov
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
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/mount.h>

#include <gnu/ext2fs/fs.h>
#include <gnu/ext2fs/ext2_fs.h>
#include <gnu/ext2fs/ext2_fs_sb.h>
#include <gnu/ext2fs/ext2_dinode.h>
#include <gnu/ext2fs/inode.h>
//#include <gnu/ext2fs/ext2_dir.h>
//#include <gnu/ext2fs/htree.h>
//#include <gnu/ext2fs/ext2_extattr.h>
#include <gnu/ext2fs/ext2_extern.h>


int
ext2_is_dirent_tail(struct inode *ip, struct ext2_dir_entry_2 *ep)
{
    struct ext2_sb_info *fs;
    struct ext2fs_direct_tail *tp;

    fs = ip->i_e2fs;

    if (!EXT2_HAS_RO_COMPAT_FEATURE(fs, EXT4_FEATURE_RO_COMPAT_METADATA_CSUM))
        return (0);

    tp = (struct ext2fs_direct_tail *)ep;
    if (tp->e2dt_reserved_zero1 == 0 &&
        tp->e2dt_rec_len == sizeof(struct ext2fs_direct_tail) &&
        tp->e2dt_reserved_zero2 == 0 &&
        tp->e2dt_reserved_ft == EXT2_FT_DIR_CSUM)
        return (1);

    return (0);
}
