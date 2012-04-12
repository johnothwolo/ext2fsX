/* 
 * If linux/types.h is already been included, assume it has defined
 * everything we need.  (cross fingers)  Other header files may have 
 * also defined the types that we need.
 */
#if (!defined(_LINUX_TYPES_H) && !defined(_BLKID_TYPES_H) && \
	!defined(_EXT2_TYPES_H))
#define _EXT2_TYPES_H


#ifdef __U8_TYPEDEF
typedef __U8_TYPEDEF __u8;
#else
typedef unsigned char __u8;
#endif

#ifdef __S8_TYPEDEF
typedef __S8_TYPEDEF __s8;
#else
typedef signed char __s8;
#endif

#ifdef __U16_TYPEDEF
typedef __U16_TYPEDEF __u16;
#else
typedef	unsigned short	__u16;
#endif /* __U16_TYPEDEF */

#ifdef __S16_TYPEDEF
typedef __S16_TYPEDEF __s16;
#else
typedef	short		__s16;
#endif /* __S16_TYPEDEF */


#ifdef __U32_TYPEDEF
typedef __U32_TYPEDEF __u32;
#else
typedef	unsigned int	__u32;
#endif /* __U32_TYPEDEF */

#ifdef __S32_TYPEDEF
typedef __S32_TYPEDEF __s32;
#else
typedef	int		__s32;
#endif /* __S32_TYPEDEF */

#ifdef __U64_TYPEDEF
typedef __U64_TYPEDEF __u64;
#else
typedef unsigned long long	__u64;
#endif /* __U64_TYPEDEF */

#ifdef __S64_TYPEDEF
typedef __S64_TYPEDEF __s64;
#else
#if defined(__GNUC__)
typedef __signed__ long long 	__s64;
#else
typedef signed long long 	__s64;
#endif /* __GNUC__ */
#endif /* __S64_TYPEDEF */

#undef __S8_TYPEDEF
#undef __U8_TYPEDEF
#undef __S16_TYPEDEF
#undef __U16_TYPEDEF
#undef __S32_TYPEDEF
#undef __U32_TYPEDEF
#undef __S64_TYPEDEF
#undef __U64_TYPEDEF

#endif /* _*_TYPES_H */

/* These defines are needed for the public ext2fs.h header file */
#define HAVE_SYS_TYPES_H 1
#undef WORDS_BIGENDIAN
