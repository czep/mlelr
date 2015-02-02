#ifndef CSV_H__
#define CSV_H__

/* Copyright (C) 1999 Lucent Technologies */
/* Excerpted from 'The Practice of Programming' */
/* by Brian W. Kernighan and Rob Pike */

/* csv.h: interface for csv library */

extern char *csvgetline(FILE *f, char delim, int compress); /* read next input line */
extern char *csvfield(int n);	  /* return field n */
extern int csvnfield(void);		  /* return number of fields */

#endif
