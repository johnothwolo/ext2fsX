/* $FreeBSD: src/sys/gnu/ext2fs/i386-bitops.h,v 1.5 1999/11/15 23:16:06 obrien Exp $ */
/*
 * this is mixture of i386/bitops.h and asm/string.h
 * taken from the Linux source tree 
 *
 * XXX replace with Mach routines or reprogram in C
 */
#ifndef _SYS_GNU_EXT2FS_I386_BITOPS_H_
#define	_SYS_GNU_EXT2FS_I386_BITOPS_H_

/*
 * Copyright 1992, Linus Torvalds.
 */

/*
 * These have to be done with inline assembly: that way the bit-setting
 * is guaranteed to be atomic. All bit operations return 0 if the bit
 * was cleared before the operation and != 0 if it was not.
 *
 * bit 0 is the LSB of addr; bit 32 is the LSB of (addr+1).
 */

/*
 * Some hacks to defeat gcc over-optimizations..
 */
struct __dummy { unsigned long a[100]; };
#define ADDR (*(struct __dummy *) addr)

static __inline__ int set_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btsl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

static __inline__ int clear_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btrl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

static __inline__ int change_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btcl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit),"=m" (ADDR)
		:"ir" (nr));
	return oldbit;
}

/*
 * This routine doesn't need to be atomic, but it's faster to code it
 * this way.
 */
static __inline__ int test_bit(int nr, void * addr)
{
	int oldbit;

	__asm__ __volatile__("btl %2,%1\n\tsbbl %0,%0"
		:"=r" (oldbit)
		:"m" (ADDR),"ir" (nr));
	return oldbit;
}

/*
 * ffz = Find First Zero in word. Undefined if no zero exists,
 * so code should check against ~0UL first..
 */
static __inline__ unsigned long ffz(unsigned long word)
{
	__asm__("bsf %1,%0"
		:"=r" (word)
		:"r" (~word));
	return word;
}

/*
 * Find-bit routines..
 */
static __inline__ long find_first_zero_bit(void * addr, unsigned long size)
{
#ifdef __i386__
	int res;
	int _count = (size + 31) >> 5;

	if (!size)
		return 0;
	__asm__("			\n\
		cld			\n\
		movl $-1,%%eax		\n\
		xorl %%edx,%%edx	\n\
		repe; scasl		\n\
		je 1f			\n\
		xorl -4(%%edi),%%eax	\n\
		subl $4,%%edi		\n\
		bsfl %%eax,%%edx	\n\
1:		subl %%ebx,%%edi	\n\
		shll $3,%%edi		\n\
		addl %%edi,%%edx"
		: "=c" (_count), "=D" (addr), "=d" (res)
		: "0" (_count), "1" (addr), "b" (addr)
		: "ax");
	return res;
#else
	//TODO: use asm code?
#define BITS_PER_LONG 64
	const unsigned long *p = addr;
	unsigned long result = 0;
	unsigned long tmp;
	
	while (size & ~(BITS_PER_LONG-1)) {
		if (~(tmp = *(p++)))
			goto found;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	
	tmp = (*p) | (~0UL << size);
	if (tmp == ~0UL)	/* Are any bits zero? */
		return result + size;	/* Nope. */
found:
	return result + ffz(tmp);

#endif
}

static __inline__ long find_next_zero_bit (const unsigned long * addr, long size, long offset)
{
#ifdef __i386__
	unsigned long * p = ((unsigned long *) addr) + (offset >> 5);
	int set = 0, bit = offset & 31, res;
	
	if (bit) {
		/*
		 * Look for zero in first byte
		 */
		__asm__("			\n\
			bsfl %1,%0		\n\
			jne 1f			\n\
			movl $32, %0		\n\
1:			"
			: "=r" (set)
			: "r" (~(*p >> bit)));
		if (set < (32 - bit))
			return set + offset;
		set = 32 - bit;
		p++;
	}
	/*
	 * No zero yet, search remaining full bytes for a zero
	 */
	res = find_first_zero_bit (p, size - 32 * (p - (unsigned long *) addr));
	return (offset + set + res);
#else
	const unsigned long * p = addr + (offset >> 6);
	unsigned long set = 0;
	unsigned long res, bit = offset&63;

	if (bit) {
		/*
		 * Look for zero in first word
		 */
		asm("bsfq %1,%0\n\t"
			"cmoveq %2,%0"
			: "=r" (set)
			: "r" (~(*p >> bit)), "r"(64L));
		if (set < (64 - bit))
			return set + offset;
		set = 64 - bit;
		p++;
	}
	/*
	 * No zero yet, search remaining full words for a zero
	 */
	res = find_first_zero_bit (p, size - 64 * (p - addr));
	return (offset + set + res);
#endif
}

/* 
 * memscan() taken from linux asm/string.h
 */
/*
 * find the first occurrence of byte 'c', or 1 past the area if none
 */
static __inline__ char * memscan(void * addr, unsigned char c, size_t size)
{
        if (!size)
                return addr;
        __asm__("			\n\
		cld			\n\
                repnz; scasb		\n\
                jnz 1f			\n\
                dec %%edi		\n\
1:              "
                : "=D" (addr), "=c" (size)
                : "0" (addr), "1" (size), "a" (c));
        return addr;
}

#define ext2_set_bit set_bit
#define ext2_clear_bit clear_bit
#define ext2_test_bit test_bit
#define ext2_find_first_zero_bit find_first_zero_bit
#define ext2_find_next_zero_bit find_next_zero_bit

#endif /* !_SYS_GNU_EXT2FS_I386_BITOPS_H_ */
