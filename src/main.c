/* main.c */

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dataset.h"
#include "interface.h"

/* forward declarations */
void print_help (void);
void init (void);


void init (void) {

    LOGFILE = stderr;
    OUTFILE = stdout;
    INPUTFILE = stdin;
    LOGLEVEL = INFO;

}


void print_help (void) {

    printlog(LOGLEVEL, "%s%s", MLELR_WELCOME,
    "Available command line arguments:\n \
    -f or -file:    read and execute commands from the named file\n \
    -o or -out:     redirect output to a file \n \
    -l or -log:     redirect log messages to a file\n \
    -v or -verbose: log extra detail to the log file\n \
    -s or -silent:  suppress all logging information\n \
    -h or -help:    print this message\n"
    );

}


int main (int argc, char *argv[]) {

    char *p;
    int curarg     = 1;
    int badarg     = 0;
    int arg_input  = 0;
    int arg_out    = 0;
    int arg_log    = 0;
    int retval     = 0;
    int loglevel   = INFO;

    init();

    /* process command line arguments */
    while (curarg < argc && !badarg) {

        /* strip leading '-' if exists */
        p = argv[curarg];
        if (p[0] == '-') p = p + 1;

        /* Process current arg */
        switch (p[0]) {

            /* -f or -file */
            case 'f':

                if (strlen(p) == 1 ||  strcmp(p, "file") == 0) {
                    arg_input = curarg + 1;
                    if (arg_input >= argc) badarg = 1;
                    curarg = curarg + 2;
                }
                else {
                    badarg = 1;
                }
                break;

            /* -o or -out */
            case 'o':

                if (strlen(p) == 1 ||  strcmp(p, "out") == 0) {
                    arg_out = curarg + 1;
                    if (arg_out >= argc) badarg = 1;
                    curarg = curarg + 2;
                }
                else {
                    badarg = 1;
                }
                break;

            /* -l or -log */
            case 'l':

                if (strlen(p) == 1 ||  strcmp(p, "log") == 0) {
                    arg_log = curarg + 1;
                    if (arg_log >= argc) badarg = 1;
                    curarg = curarg + 2;
                }
                else {
                    badarg = 1;
                }
                break;

            /* -v or -verbose */
            case 'v':

                if (strlen(p) == 1 ||  strcmp(p, "verbose") == 0) {
                    loglevel = VERBOSE;
                    curarg++;
                }
                else {
                    badarg = 1;
                }
                break;

            /* -s or -silent */
            case 's':

                if (strlen(p) == 1 ||  strcmp(p, "silent") == 0) {
                    loglevel = SILENT;
                    curarg++;
                }
                else {
                    badarg = 1;
                }
                break;

            /* -h or -help */
            case 'h':

                if (strlen(p) == 1 ||  strcmp(p, "help") == 0) {
                    /* we can't accept -h with any other arguments */
                    if (curarg == 1) {
                        print_help();
                        return 0;
                    }
                    else {
                        badarg = 1;
                    }
                }
                else {
                    badarg = 1;
                }
                break;

            default:
                badarg = 1;

        }   /* end switch on current arg */

    }   /* end while valid args exist */

    /* notify and exit if badarg */
    if (badarg) {
        print_help();
        fprintf(stderr, "%s\n", "Error: invalid command line argument.");
        return 0;
    }

    /* set INPUTFILE */
    if (arg_input && (INPUTFILE = fopen(argv[arg_input], "r")) == NULL) {
        print_help();
        fprintf(stderr, "%s%s\n", "Error: Unable to open input file: ", argv[arg_input]);
        return 0;
    }

    /* set OUTFILE */
    if (arg_out && (OUTFILE = fopen(argv[arg_out], "w")) == NULL) {
        print_help();
        fprintf(stderr, "%s%s\n", "Error: Unable to open output file: ", argv[arg_out]);
        return 0;
    }

    /* set LOGFILE */
    if (arg_log && (LOGFILE = fopen(argv[arg_log], "a")) == NULL) {
        print_help();
        fprintf(stderr, "%s%s\n", "Error: Unable to open log file: ", argv[arg_input]);
        return 0;
    }

    /* update the global loglevel */
    LOGLEVEL = loglevel;

    /* initialize the dataspace and global options */
    init_dataspace();
    init_options();

    /* print a welcome message */
    printlog(INFO, "%s", MLELR_WELCOME);

    /* continue to call our input handler function until we receive a non-zero value
        indicating it is time to quit */
    while (!retval) {
        retval = input_handler();
    }

    /* close any open files */
    if (arg_input && INPUTFILE != stdin) fclose(INPUTFILE);
    if (arg_out && OUTFILE != stdout) fclose(OUTFILE);
    if (arg_log && LOGFILE != stderr) fclose(LOGFILE);

    return 0;
}
