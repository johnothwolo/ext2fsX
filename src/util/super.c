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

static const char super_whatid[] __attribute__ ((unused)) =
"@(#) $Id: super.c,v 1.3 2004/02/29 22:54:54 bbergstrand Exp $";

extern char* __progname;

#define EXT_SUPER_SIZE 4096
#define EXT_SUPER_OFF 1024
static int extsuper_read (const char *device, char **bout, struct ext2_super_block **sbp)
{
   int fd;
   ssize_t bytes;
   char *buf;
   
   *bout = NULL;
   *sbp = NULL;
   
   buf = malloc(EXT_SUPER_SIZE);
   if (!buf) {
      fprintf(stderr, "%s: malloc failed, %s\n", __progname,
			strerror(ENOMEM));
      return (ENOMEM);
   }
   
   fd = open(device, O_RDONLY, 0);
   if (fd < 0) {
      free(buf);
      fprintf(stderr, "%s: open '%s' failed, %s\n", __progname, device,
			strerror(errno));
      return (errno);
   }
   
   /* Read the first 4K. When reading/writing from a raw device, we must
      do so in the native block size (or a multiple thereof) of the device.*/
   bytes = read (fd, buf, EXT_SUPER_SIZE);
   close(fd);
   
   if (EXT_SUPER_SIZE != bytes) {
      free(buf);
      fprintf(stderr, "%s: device read '%s' failed, %s\n", __progname, device,
			strerror(errno));
      return (errno);
   }
   
    /* Superblock starts at offset 1024 (block 2). */
   *sbp = (struct ext2_super_block*)(buf+EXT_SUPER_OFF);
   if (EXT2_SUPER_MAGIC != le16_to_cpu((*sbp)->s_magic)) {
      free(buf);
      *sbp = NULL;
      fprintf(stderr, "%s: device '%s' appears not to contain a valid filesystem\n",
         __progname, device);
      return (EINVAL);
   }
   
   *bout = buf;
   return (0);
}

#ifdef EXT_SUPER_ISCLEAN

static int extsuper_isclean(char *devpath)
{
   char *buf;
   struct ext2_super_block *sbp;
   int err = 0;
   
   if (0 == (err = extsuper_read(devpath, &buf, &sbp))) {
      if ((le16_to_cpu(sbp->s_state) & EXT2_VALID_FS) == 0 ||
         (le16_to_cpu(sbp->s_state) & EXT2_ERROR_FS))
            err = EINVAL;
   }
   
   free(buf);
   return (err);
}

#endif /* EXT_SUPER_ISCLEAN */

#ifdef EXT_SUPER_UUID

static CFStringRef extsuper_uuid(const char *device)
{
   CFStringRef str;
   CFUUIDRef uuid;
   CFUUIDBytes *ubytes;
   char *buf;
   struct ext2_super_block *sbp;
   
   if (extsuper_read(device, &buf, &sbp))
      return (NULL);
   
   ubytes = (CFUUIDBytes*)&sbp->s_uuid[0];
   uuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *ubytes);
   free(buf);
   if (uuid) {
      str = CFUUIDCreateString(kCFAllocatorDefault, uuid);
      CFRelease(uuid);
      return (str);
   }
   return (NULL);
}

#endif /* EXT_SUPER_UUID */
