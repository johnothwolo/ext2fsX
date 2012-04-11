/*
 * apprentice - make one pass through /etc/magic, learning its secrets.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include "file.h"
#include "magic.h"

#define	EATAB {while (isascii((unsigned char) *l) && \
		      isspace((unsigned char) *l))  ++l;}
#define LOWCASE(l) (isupper((unsigned char) (l)) ? \
			tolower((unsigned char) (l)) : (l))
#define SZOF(a)	(sizeof(a) / sizeof(a[0]))

int nmagic = 0;	/* number of valid magic[]s */
struct magic *magic;	/* array of magic entries */


static int getvalue	__P((struct magic *, char **));
static int hextoint	__P((int));
static char *getstr	__P((char *, char *, int, int *));
static int parse	__P((char *, int *, int));
static void eatsize	__P((char **));

static int maxmagic = 0;



void mdump(struct magic *m)
{
	static const char *typ[] = { "invalid", "byte", "short", "invalid",
				     "long", "string", "date", "beshort",
				     "belong", "bedate", "leshort", "lelong",
				     "ledate" };
	(void) fputc('[', stderr);
	(void) fprintf(stderr, ">>>>>>>> %d" + 8 - (m->cont_level & 7),
		       m->offset);

	if (m->flag & INDIR)
		(void) fprintf(stderr, "(%s,%d),",
			       /* Note: in.type is unsigned */
			       (m->in.type < SZOF(typ)) ? 
					typ[m->in.type] : "*bad*",
			       m->in.offset);

	(void) fprintf(stderr, " %s%s", (m->flag & UNSIGNED) ? "u" : "",
		       /* Note: type is unsigned */
		       (m->type < SZOF(typ)) ? typ[m->type] : "*bad*");
	if (m->mask != ~((unsigned int)0)) {
		if(STRING != m->type)
			(void) fprintf(stderr, " & %.8x", m->mask);
		else {
			(void) fputc('/', stderr); 
			if (m->mask & STRING_IGNORE_LOWERCASE) 
				(void) fputc(CHAR_IGNORE_LOWERCASE, stderr);
			if (m->mask & STRING_COMPACT_BLANK) 
				(void) fputc(CHAR_COMPACT_BLANK, stderr);
			if (m->mask & STRING_COMPACT_OPTIONAL_BLANK) 
				(void) fputc(CHAR_COMPACT_OPTIONAL_BLANK,
				stderr);
		}
	}

	(void) fprintf(stderr, ",%c", m->reln);

	if (m->reln != 'x') {
		switch (m->type) {
		case BYTE:
		case SHORT:
		case LONG:
		case LESHORT:
		case LELONG:
		case BESHORT:
		case BELONG:
			(void) fprintf(stderr, "%d", m->value.l);
			break;
		case STRING:
			showstr(stderr, m->value.s, -1);
			break;
		case DATE:
		case LEDATE:
		case BEDATE:
			{
				time_t t = m->value.l;
				char *rt, *pp = ctime(&t);

				if ((rt = strchr(pp, '\n')) != NULL)
					*rt = '\0';
				(void) fprintf(stderr, "%s,", pp);
				if (rt)
					*rt = '\n';
			}
			break;
		default:
			(void) fputs("*bad*", stderr);
			break;
		}
	}
	(void) fprintf(stderr, ",\"%s\"]\n", m->desc);
}




int apprentice()
/* 0: Fehler, sonst: ok */
{
	char line[BUFSIZ+1];
        int lineno;
        int maxmagic = MAXMAGIS;
	magic = (struct magic *) calloc(sizeof(struct magic), maxmagic);
	if (magic == NULL) return 0;


	for (lineno = 1; lineno < MAXMAGIS; lineno++)
        {
        	strncpy(line, magic_types[lineno], BUFSIZ);
		if (line[0]=='#')	/* comment, do not parse */
			continue;
		if (strlen(line) <= 1)  /* null line, garbage, etc */
			continue;
		if (parse(line, &nmagic, 0) != 0)
                {
                  printf("error parsing '%s'\n", line);
  		  return 0;
                }
	}
	return 1;
}

/*
 * extend the sign bit if the comparison is to be signed
 */
int
signextend(m, v)
	struct magic *m;
	unsigned int v;
{
	if (!(m->flag & UNSIGNED))
		switch(m->type) {
		/*
		 * Do not remove the casts below.  They are
		 * vital.  When later compared with the data,
		 * the sign extension must have happened.
		 */
		case BYTE:
			v = (char) v;
			break;
		case SHORT:
		case BESHORT:
		case LESHORT:
			v = (short) v;
			break;
		case DATE:
		case BEDATE:
		case LEDATE:
		case LONG:
		case BELONG:
		case LELONG:
			v = (int) v;
			break;
		case STRING:
			break;
		default:
			fprintf(stderr, "can't happen: m->type=%d\n",
				m->type);
			return -1;
		}
	return v;
}

/*
 * parse one line from magic file, put into magic[index++] if valid
 */
static int
parse(l, ndx, check)
	char *l;
	int *ndx, check;
{
	int i = 0, nd = *ndx;
	struct magic *m;
	char *t, *s;

#define ALLOC_INCR	100
	if (nd+1 >= maxmagic){
		maxmagic += ALLOC_INCR;
		if ((m = (struct magic *) realloc(magic, sizeof(struct magic) * 
						  maxmagic)) == NULL) {
			(void) fprintf(stderr, "Out of memory.\n");
			if (magic)
				free(magic);
			if (check)
				return -1;
			else
				exit(1);
		}
		magic = m;
		memset(&magic[*ndx], 0, sizeof(struct magic) * ALLOC_INCR);
	}
	m = &magic[*ndx];
	m->flag = 0;
	m->cont_level = 0;

	while (*l == '>') {
		++l;		/* step over */
		m->cont_level++; 
	}

	if (m->cont_level != 0 && *l == '(') {
		++l;		/* step over */
		m->flag |= INDIR;
	}
	if (m->cont_level != 0 && *l == '&') {
                ++l;            /* step over */
                m->flag |= ADD;
        }

	/* get offset, then skip over it */
	m->offset = (int) strtoul(l,&t,0);
        if (l == t)
		fprintf(stderr, "offset %s invalid", l);
        l = t;

	if (m->flag & INDIR) {
		m->in.type = LONG;
		m->in.offset = 0;
		/*
		 * read [.lbs][+-]nnnnn)
		 */
		if (*l == '.') {
			l++;
			switch (*l) {
			case 'l':
				m->in.type = LELONG;
				break;
			case 'L':
				m->in.type = BELONG;
				break;
			case 'h':
			case 's':
				m->in.type = LESHORT;
				break;
			case 'H':
			case 'S':
				m->in.type = BESHORT;
				break;
			case 'c':
			case 'b':
			case 'C':
			case 'B':
				m->in.type = BYTE;
				break;
			default:
				fprintf(stderr, "indirect offset type %c invalid", *l);
				break;
			}
			l++;
		}
		s = l;
		if (*l == '+' || *l == '-') l++;
		if (isdigit((unsigned char)*l)) {
			m->in.offset = strtoul(l, &t, 0);
			if (*s == '-') m->in.offset = - m->in.offset;
		}
		else
			t = l;
		if (*t++ != ')') 
			fprintf(stderr, "missing ')' in indirect offset");
		l = t;
	}


	while (isascii((unsigned char)*l) && isdigit((unsigned char)*l))
		++l;
	EATAB;

#define NBYTE		4
#define NSHORT		5
#define NLONG		4
#define NSTRING 	6
#define NDATE		4
#define NBESHORT	7
#define NBELONG		6
#define NBEDATE		6
#define NLESHORT	7
#define NLELONG		6
#define NLEDATE		6

	if (*l == 'u') {
		++l;
		m->flag |= UNSIGNED;
	}

	/* get type, skip it */
	if (strncmp(l, "char", NBYTE)==0) {	/* HP/UX compat */
		m->type = BYTE;
		l += NBYTE;
	} else if (strncmp(l, "byte", NBYTE)==0) {
		m->type = BYTE;
		l += NBYTE;
	} else if (strncmp(l, "short", NSHORT)==0) {
		m->type = SHORT;
		l += NSHORT;
	} else if (strncmp(l, "long", NLONG)==0) {
		m->type = LONG;
		l += NLONG;
	} else if (strncmp(l, "string", NSTRING)==0) {
		m->type = STRING;
		l += NSTRING;
	} else if (strncmp(l, "date", NDATE)==0) {
		m->type = DATE;
		l += NDATE;
	} else if (strncmp(l, "beshort", NBESHORT)==0) {
		m->type = BESHORT;
		l += NBESHORT;
	} else if (strncmp(l, "belong", NBELONG)==0) {
		m->type = BELONG;
		l += NBELONG;
	} else if (strncmp(l, "bedate", NBEDATE)==0) {
		m->type = BEDATE;
		l += NBEDATE;
	} else if (strncmp(l, "leshort", NLESHORT)==0) {
		m->type = LESHORT;
		l += NLESHORT;
	} else if (strncmp(l, "lelong", NLELONG)==0) {
		m->type = LELONG;
		l += NLELONG;
	} else if (strncmp(l, "ledate", NLEDATE)==0) {
		m->type = LEDATE;
		l += NLEDATE;
	} else {
		fprintf(stderr, "type %s invalid", l);
		return -1;
	}
	/* New-style anding: "0 byte&0x80 =0x80 dynamically linked" */
	if (*l == '&') {
		++l;
		m->mask = signextend(m, strtoul(l, &l, 0));
		eatsize(&l);
	} else if (STRING == m->type) {
		m->mask = 0L;
		if (*l == '/') { 
			while (!isspace(*++l)) {
				switch (*l) {
				case CHAR_IGNORE_LOWERCASE:
					m->mask |= STRING_IGNORE_LOWERCASE;
					break;
				case CHAR_COMPACT_BLANK:
					m->mask |= STRING_COMPACT_BLANK;
					break;
				case CHAR_COMPACT_OPTIONAL_BLANK:
					m->mask |=
					    STRING_COMPACT_OPTIONAL_BLANK;
					break;
				default:
					fprintf(stderr, "string extension %c invalid",
					    *l);
					return -1;
				}
			}
		}
	} else
		m->mask = ~0L;
	EATAB;
  
	switch (*l) {
	case '>':
	case '<':
	/* Old-style anding: "0 byte &0x80 dynamically linked" */
	case '&':
	case '^':
	case '=':
  		m->reln = *l;
  		++l;
		if (*l == '=') {
		   /* HP compat: ignore &= etc. */
		   ++l;
		}
		break;
	case '!':
		if (m->type != STRING) {
			m->reln = *l;
			++l;
			break;
		}
		/* FALL THROUGH */
	default:
		if (*l == 'x' && isascii((unsigned char)l[1]) && 
		    isspace((unsigned char)l[1])) {
			m->reln = *l;
			++l;
			goto GetDesc;	/* Bill The Cat */
		}
  		m->reln = '=';
		break;
	}
  	EATAB;
  
	if (getvalue(m, &l))
		return -1;
	/*
	 * TODO finish this macro and start using it!
	 * #define offsetcheck {if (offset > HOWMANY-1) 
	 *	magwarn("offset too big"); }
	 */

	/*
	 * now get last part - the description
	 */
GetDesc:
	EATAB;
	if (l[0] == '\b') {
		++l;
		m->nospflag = 1;
	} else if ((l[0] == '\\') && (l[1] == 'b')) {
		++l;
		++l;
		m->nospflag = 1;
	} else
		m->nospflag = 0;
	while ((m->desc[i++] = *l++) != '\0' && i<MAXDESC)
		/* NULLBODY */;

	if (check) {
		mdump(m);
	}
	++(*ndx);		/* make room for next */
	return 0;
}

/* 
 * Read a numeric value from a pointer, into the value union of a magic 
 * pointer, according to the magic type.  Update the string pointer to point 
 * just after the number read.  Return 0 for success, non-zero for failure.
 */
static int
getvalue(m, p)
	struct magic *m;
	char **p;
{
	int slen;

	if (m->type == STRING) {
		*p = getstr(*p, m->value.s, sizeof(m->value.s), &slen);
		m->vallen = slen;
	} else
		if (m->reln != 'x') {
			m->value.l = signextend(m, strtoul(*p, p, 0));
			eatsize(p);
		}
	return 0;
}

/*
 * Convert a string containing C character escapes.  Stop at an unescaped
 * space or tab.
 * Copy the converted version to "p", returning its length in *slen.
 * Return updated scan pointer as function result.
 */
static char *
getstr(s, p, plen, slen)
	char	*s;
	char	*p;
	int	plen, *slen;
{
	char	*origs = s, *origp = p;
	char	*pmax = p + plen - 1;
	int	c;
	int	val;

	while ((c = *s++) != '\0') {
		if (isspace((unsigned char) c))
			break;
		if (p >= pmax) {
			fprintf(stderr, "String too long: %s\n", origs);
			break;
		}
		if(c == '\\') {
			switch(c = *s++) {

			case '\0':
				goto out;

			default:
				*p++ = (char) c;
				break;

			case 'n':
				*p++ = '\n';
				break;

			case 'r':
				*p++ = '\r';
				break;

			case 'b':
				*p++ = '\b';
				break;

			case 't':
				*p++ = '\t';
				break;

			case 'f':
				*p++ = '\f';
				break;

			case 'v':
				*p++ = '\v';
				break;

			/* \ and up to 3 octal digits */
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
				val = c - '0';
				c = *s++;  /* try for 2 */
				if(c >= '0' && c <= '7') {
					val = (val<<3) | (c - '0');
					c = *s++;  /* try for 3 */
					if(c >= '0' && c <= '7')
						val = (val<<3) | (c-'0');
					else
						--s;
				}
				else
					--s;
				*p++ = (char)val;
				break;

			/* \x and up to 2 hex digits */
			case 'x':
				val = 'x';	/* Default if no digits */
				c = hextoint(*s++);	/* Get next char */
				if (c >= 0) {
					val = c;
					c = hextoint(*s++);
					if (c >= 0)
						val = (val << 4) + c;
					else
						--s;
				} else
					--s;
				*p++ = (char)val;
				break;
			}
		} else
			*p++ = (char)c;
	}
out:
	*p = '\0';
	*slen = p - origp;
	return s;
}


/* Single hex char to int; -1 if not a hex char. */
static int
hextoint(c)
	int c;
{
	if (!isascii((unsigned char) c))
		return -1;
	if (isdigit((unsigned char) c))
		return c - '0';
	if ((c >= 'a')&&(c <= 'f'))
		return c + 10 - 'a';
	if (( c>= 'A')&&(c <= 'F'))
		return c + 10 - 'A';
	return -1;
}


/*
 * Print a string containing C character escapes.
 */
void
showstr(fp, s, len)
	FILE *fp;
	const char *s;
	int len;
{
	char	c;

	for (;;) {
		c = *s++;
		if (len == -1) {
			if (c == '\0')
				break;
		}
		else  {
			if (len-- == 0)
				break;
		}
		if(c >= 040 && c <= 0176)	/* TODO isprint && !iscntrl */
			(void) fputc(c, fp);
		else {
			(void) fputc('\\', fp);
			switch (c) {
			
			case '\n':
				(void) fputc('n', fp);
				break;

			case '\r':
				(void) fputc('r', fp);
				break;

			case '\b':
				(void) fputc('b', fp);
				break;

			case '\t':
				(void) fputc('t', fp);
				break;

			case '\f':
				(void) fputc('f', fp);
				break;

			case '\v':
				(void) fputc('v', fp);
				break;

			default:
				(void) fprintf(fp, "%.3o", c & 0377);
				break;
			}
		}
	}
}

/*
 * eatsize(): Eat the size spec from a number [eg. 10UL]
 */
static void
eatsize(p)
	char **p;
{
	char *l = *p;

	if (LOWCASE(*l) == 'u') 
		l++;

	switch (LOWCASE(*l)) {
	case 'l':    /* long */
	case 's':    /* short */
	case 'h':    /* short */
	case 'b':    /* char/byte */
	case 'c':    /* char/byte */
		l++;
		/*FALLTHROUGH*/
	default:
		break;
	}

	*p = l;
}
