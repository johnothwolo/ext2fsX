//
//  ext2_debug.c
//  ext2_kext
//
//  Created by John Othwolo on 4/23/20.
//


#include <libkern/OSDebug.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <stdarg.h>
#include <kern/debug.h>

#include <gnu/ext2fs/ext2_fs.h>

void
__ext2_debug(const char *file, int line,
		const char *function, const char *fmt, ...)
{
	va_list args;
	const char *filename;
	int len;
	char err_buf[1024];

	/*
	 * We really want strrchr() here but that is not exported so do it by
	 * hand.
	 */
	filename = file;
	if (filename) {
		for (len = strlen(filename); len > 0; len--) {
			if (filename[len - 1] == '/') {
				filename += len;
				break;
			}
		}
	}
//	lck_spin_lock(&ntfs_err_buf_lock);
	va_start(args, fmt);
	vsnprintf(err_buf, sizeof(err_buf), fmt, args);
	va_end(args);
	printf("EXT2-fs DEBUG (%s, %d): %s(): %s\n", filename ? filename : "",
		   line, function ? function : "", err_buf);
//	lck_spin_unlock(&ntfs_err_buf_lock);
}
