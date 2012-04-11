/*
* Copyright 2004 Brian Bergstrand.
*
* Redistribution and use in source and binary forms, with or without modification, 
* are permitted provided that the following conditions are met:
*
* 1.    Redistributions of source code must retain the above copyright notice, this list of
*     conditions and the following disclaimer.
* 2.    Redistributions in binary form must reproduce the above copyright notice, this list of
*     conditions and the following disclaimer in the documentation and/or other materials provided
*     with the distribution.
* 3.    The name of the author may not be used to endorse or promote products derived from this
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
#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main (int argc, char *argv[])
{
	int fd, len;
	char path[PATH_MAX+1];
	
	if (2 != argc)
		return (EINVAL);
	
	if (0 == realpath(argv[1], path))
		return (errno);
	
	len = strlen(path) + 1;
	fd = open(path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR);
	if (fd >= 0) {
		int blkct, again;
		off_t blksz, total=len, where = 0;
		printf("writing %d bytes at offset %qu\n", len, where);
		(void)write(fd, path, (size_t)len);

//where = 12288;
where = 2048;
printf("writing %d bytes at offset %qu\n", len, where);
(void)lseek(fd, where, SEEK_SET);
(void)write(fd, path, (size_t)len);
total += len;
goto close;

		for (blkct=12; blkct<13; ++blkct) {
			for (blksz=1024; blksz<=4096; blksz<<=1) {
				again = 1;
				where = blksz * blkct;
			dowrite:
				total += len;
				printf("writing %d bytes at offset %qu (%uKB, %uMB)\n", len,
					where, (unsigned)(where>>10), (unsigned)(where>>20));
				(void)lseek(fd, where, SEEK_SET);
				(void)write(fd, path, (size_t)len);
				if (again) {
					again = 0;
					where = blksz << blkct;
					goto dowrite;
				}
			}
		}
close:
		close(fd);
		printf ("wrote %qu bytes total\n", total);
	} else
		return (errno);
	
	return (0);
}
