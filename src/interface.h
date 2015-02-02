/* interface.h */

/*

Copyright (C) 2015 Scott A. Czepiel

    This file is part of mlelr.

    mlelr is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    mlelr is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with mlelr.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef INTERFACE_H__
#define INTERFACE_H__

/* globals */
extern const char *MLELR_WELCOME;
extern FILE *LOGFILE;
extern FILE *OUTFILE;
extern FILE *INPUTFILE;
extern int LOGLEVEL;

extern const int SILENT;
extern const int INFO;
extern const int VERBOSE;

struct options {
    int     n;
    int     size;
    char    **k;
    char    **v;
};

extern struct options options;


/* forward declarations for publically available functions defined in interface.c */
extern int input_handler (void);
extern void init_options (void);

extern void printlog (int loglevel, char *format, ...);
extern void printout (char *format, ...);

/* safe allocation of memory, see Kernighan & Pike */
extern void *emalloc (size_t n);
extern void *erealloc (void *vp, size_t n);
extern char *estrdup (char *s);


extern char *get_option (char *k);
extern void set_option (char *k, char *v);

#endif
