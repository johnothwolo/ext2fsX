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
 * $Revision: 1.4 $
 */

#ifndef _EXT2_BYTEORDER_H
#define _EXT2_BYTEORDER_H

#include <sys/types.h>

#if BYTE_ORDER == BIG_ENDIAN
#include <ext2_bigendian.h>
#define E2Q_HIGH 0
#define E2Q_LOW  1
#elif BYTE_ORDER == LITTLE_ENDIAN
#include <ext2_littleendian.h>
#define E2Q_HIGH 1
#define E2Q_LOW  0
#else
#error "Unknown architecture!"
#endif

static __inline__ __attribute__((__const__)) 
u_int16_t e2_swap16 (u_int16_t val)
{
   u_int16_t n;
   
   __arch_swap_16(val, &n);
   
   return (n);
}

static __inline__
u_int16_t e2_swap16p (const u_int16_t *val)
{
   u_int16_t n,v;
   
   v = *val;
   __arch_swap_16(v, &n);
   
   return (n);
}

static __inline__ __attribute__((__const__)) 
u_int32_t e2_swap32 (u_int32_t val)
{
   u_int32_t n;
   
   __arch_swap_32(val, &n);
   
   return (n);
}

static __inline__
u_int32_t e2_swap32p (const u_int32_t *val)
{
   u_int32_t n,v;
   
   v = *val;
   __arch_swap_32(v, &n);
   
   return (n);
}

typedef union { 
    uint64_t eq;
    uint32_t el[2]; 
} e2q;

static __inline__ __attribute__((__const__)) 
u_int64_t e2_swap64 (u_int64_t val)
{
   e2q n;
   
   __arch_swap_32(((e2q*)&val)->el[E2Q_HIGH], &n.el[E2Q_LOW]);
   __arch_swap_32(((e2q*)&val)->el[E2Q_LOW], &n.el[E2Q_HIGH]);
   
   return (n.eq);
}

static __inline__
u_int64_t e2_swap64p (const u_int64_t *val)
{
   e2q n;
   
   __arch_swap_32(((e2q*)val)->el[E2Q_HIGH], &n.el[E2Q_LOW]);
   __arch_swap_32(((e2q*)val)->el[E2Q_LOW], &n.el[E2Q_HIGH]);
   
   return (n.eq);
}

#endif // _EXT2_BYTEORDER_H
