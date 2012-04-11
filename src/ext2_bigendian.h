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
 * $Revision: 1.6 $
 */

#ifdef __ppc__
 
static __inline__ void __arch_swap_16 (u_int16_t from, u_int16_t *to)
{
   __asm__ volatile("sthbrx %1, 0, %2" : "=m" (*to) : "r" (from), "r" (to));
}

static __inline__ void __arch_swap_32 (u_int32_t from, u_int32_t *to)
{
   __asm__ volatile("stwbrx %1, 0, %2" : "=m" (*to) : "r" (from), "r" (to));
}
 
#endif // __ppc__
 
#define le16_to_cpu(x) e2_swap16((x))
#define le16_to_cpup(p) e2_swap16p((p))

#define cpu_to_le16(x) e2_swap16((x))
#define cpu_to_le16p(p) e2_swap16p((p))

#define le32_to_cpu(x) e2_swap32((x))
#define le32_to_cpup(p) e2_swap32p((p))

#define cpu_to_le32(x) e2_swap32((x))
#define cpu_to_le32p(p) e2_swap32p((p))

#define le64_to_cpu(x) e2_swap64((x))
#define le64_to_cpup(p) e2_swap64p((p))

#define cpu_to_le64(x) e2_swap64((x))
#define cpu_to_le64p(p) e2_swap64p((p))


#define be16_to_cpu(x) (u_int16_t)(x)
#define cpu_to_be16(x) (u_int16_t)(x)

#define be32_to_cpu(x) (u_int32_t)(x)
#define cpu_to_be32(x) (u_int32_t)(x)

#define be64_to_cpu(x) (u_int64_t)(x)
#define cpu_to_be64(x) (u_int64_t)(x)
