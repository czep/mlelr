/* Copyright (C) 1999 Lucent Technologies */
/* Excerpted from 'The Practice of Programming' */
/* by Brian W. Kernighan and Rob Pike */

/***

    Changes to the original:
    Replaced "static char fieldsep[]" with a 'delim' arg to csvgetline
    Added "compress" argument to csvgetline

    This csv library is from the aforementioned text and continues to be the
    reigning champion of csv libraries ever implemented.  It is simple, clean,
    and highly useful in solving a common programming problem:  how to parse
    structured text.  One might accuse me of cheating by not writing this
    myself, but I have no illusions that any csv library that I write would
    even come close to the perfection that is Kernighan and Pike.

    I have, however, made two important changes.  The original version assumes
    that the delimiter character is always a comma.  That would seem to be
    perfectly reasonable given the fact that the name of the library itself
    stipulates the fact that we are specifically interested in "comma separated
    values".  I usually find tab-delimited datasets easier to manage, and they
    are in any case equally as common as csv datasets.  So, I have refactored
    the delimiter character to be an argument to the csvgetline function.

    The second change is to add a "compress" argument which if 1 will treat
    consecutive delimiters as one character.  This is most useful in parsing
    commands as it allows us to ignore multiple simultaneous spaces.

***/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "csv.h"

enum { NOMEM = -2 };          /* out of memory signal */

static char *line    = NULL;  /* input chars */
static char *sline   = NULL;  /* line copy used by split */
static int  maxline  = 0;     /* size of line[] and sline[] */
static char **field  = NULL;  /* field pointers */
static int  maxfield = 0;     /* size of field[] */
static int  nfield   = 0;     /* number of fields in field[] */

static char *advquoted(char *p, char delim);
static int split(char delim, int compress);

/* endofline: check for and consume \r, \n, \r\n, or EOF */
static int endofline(FILE *fin, int c)
{
	int eol;

	eol = (c=='\r' || c=='\n');
	if (c == '\r') {
		c = getc(fin);
		if (c != '\n' && c != EOF)
			ungetc(c, fin);	/* read too far; put c back */
	}
	return eol;
}

/* reset: set variables back to starting values */
static void reset(void)
{
	free(line);	/* free(NULL) permitted by ANSI C */
	free(sline);
	free(field);
	line = NULL;
	sline = NULL;
	field = NULL;
	maxline = maxfield = nfield = 0;
}

/* csvgetline:  get one line, grow as needed */
/* sample input: "LU",86.25,"11/4/1998","2:19PM",+4.0625 */
char *csvgetline(FILE *fin, char delim, int compress)
{
	int i, c;
	char *newl, *news;

	if (line == NULL) {			/* allocate on first call */
		maxline = maxfield = 1;
		line = (char *) malloc(maxline);
		sline = (char *) malloc(maxline);
		field = (char **) malloc(maxfield*sizeof(field[0]));
		if (line == NULL || sline == NULL || field == NULL) {
			reset();
			return NULL;		/* out of memory */
		}
	}
	for (i=0; (c=getc(fin))!=EOF && !endofline(fin,c); i++) {
		if (i >= maxline-1) {	/* grow line */
			maxline *= 2;		/* double current size */
			newl = (char *) realloc(line, maxline);
			if (newl == NULL) {
				reset();
				return NULL;
			}
			line = newl;
			news = (char *) realloc(sline, maxline);
			if (news == NULL) {
				reset();
				return NULL;
			}
			sline = news;


		}
		line[i] = c;
	}
	line[i] = '\0';
	if (split(delim, compress) == NOMEM) {
		reset();
		return NULL;			/* out of memory */
	}
	return (c == EOF && i == 0) ? NULL : line;
}

/* split: split line into fields */
static int split(char delim, int compress)
{
	char *p, **newf;
	char *sepp; /* pointer to temporary separator character */
	int sepc;   /* temporary separator character */

	nfield = 0;
	if (line[0] == '\0')
		return 0;
	strcpy(sline, line);
	p = sline;

	do {
		if (nfield >= maxfield) {
			maxfield *= 2;			/* double current size */
			newf = (char **) realloc(field,
						maxfield * sizeof(field[0]));
			if (newf == NULL)
				return NOMEM;
			field = newf;
		}
		/* compress subsequent delimiter characters */
		if (compress) {
		    while (*p != '\0' && *p == delim) {
		        p++;
		    }
		}
		if (*p == '"')
			sepp = advquoted(++p, delim);	/* skip initial quote */
		else
			sepp = p + strcspn(p, &delim);
		sepc = sepp[0];
		sepp[0] = '\0';				/* terminate field */
		field[nfield++] = p;
		p = sepp + 1;
	} while (sepc == (int) delim);

	return nfield;
}

/* advquoted: quoted field; return pointer to next separator */
static char *advquoted(char *p, char delim)
{
	int i, j;

	for (i = j = 0; p[j] != '\0'; i++, j++) {
		if (p[j] == '"' && p[++j] != '"') {
			/* copy up to next separator or \0 */
			int k = strcspn(p+j, &delim);
			memmove(p+i, p+j, k);
			i += k;
			j += k;
			break;
		}
		p[i] = p[j];
	}
	p[i] = '\0';
	return p + j;
}

/* csvfield:  return pointer to n-th field */
char *csvfield(int n)
{
	if (n < 0 || n >= nfield)
		return NULL;
	return field[n];
}

/* csvnfield:  return number of fields */
int csvnfield(void)
{
	return nfield;
}
