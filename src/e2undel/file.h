#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef BSD
#include <linux/limits.h>
#else
#include <limits.h>
#endif
#include <unistd.h>

#include "common.h"

#define MAXDESC	50		/* max leng of text description */
#define MAXstring 32		/* max leng of "string" types */

#define CHAR_IGNORE_LOWERCASE           'c'
#define CHAR_COMPACT_BLANK              'B'
#define CHAR_COMPACT_OPTIONAL_BLANK     'b'

#define BIT(A)   (1 << (A))


struct magic
{
  short flag;		
#define INDIR	1		/* if '>(...)' appears,  */
#define	UNSIGNED 2		/* comparison is unsigned */
#define ADD	4		/* if '>&' appears,  */
  short cont_level;	/* level of ">" */
  struct
  {
    unsigned char type;	/* byte short long */
    int offset;	/* offset from indirection */
  } in;
  int offset;		/* offset to magic number */

  unsigned char reln;	/* relation (0=eq, '>'=gt, etc) */
  unsigned char type;	/* int, short, long or string. */
  char vallen;		/* length of string value, if any */

#define BYTE	1
#define	SHORT	2
#define	LONG	4
#define	STRING	5
#define	DATE	6
#define	BESHORT	7
#define	BELONG	8
#define	BEDATE	9
#define	LESHORT	10
#define	LELONG	11
#define	LEDATE	12
  union VALUETYPE
  {
    unsigned char b;
    unsigned short h;
    unsigned int l;
    char s[MAXstring];
    unsigned char hs[2];    /* 2 bytes of a fixed-endian "short" */
    unsigned char hl[4];    /* 2 bytes of a fixed-endian "long" */
  } value;                  /* either number or string */
  unsigned int mask;        /* mask before comparison with value */
  char nospflag;	    /* supress space character */
  char desc[MAXDESC];	    /* description */
};


#define STRING_IGNORE_LOWERCASE		BIT(0)
#define STRING_COMPACT_BLANK		BIT(1)
#define STRING_COMPACT_OPTIONAL_BLANK	BIT(2)


extern struct magic *magic;	/* array of magic entries		*/
extern int nmagic;		/* number of valid magic[]s 		*/


int is_tar(unsigned char *buf, int size);
void mdump(struct magic *m);
int signextend(struct magic *m, unsigned int v);
void showstr(FILE *fp, const char *s, int len);

extern int nmagic;
extern struct magic *magic;
