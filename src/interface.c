/* interface.c */

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
#include <errno.h>
#include <stdarg.h>
#include "interface.h"
#include "dataset.h"
#include "model.h"
#include "csv.h"
#include "mlelr.h"
#include "tabulate.h"


typedef int int_fp_v (void);

typedef struct {
    char      *name;      /* the name of the command */
    int_fp_v  *f;         /* pointer to the function to call for this command */
    char      *desc;      /* brief description of command */
} COMMAND;


struct options options;

/* declarations for functions used to process commands */
static int cmd_quit   (void);
static int cmd_comment (void);
static int cmd_import (void);
static int cmd_print (void);
static int cmd_weight (void);
static int cmd_table (void);
static int cmd_logreg (void);
static int cmd_option (void);
static int cmd_help (void);

static void eprintf (char *fmt, ...);

/* structure of commands */
COMMAND cmds[] = {
    {"import", cmd_import, "Import a delimited text file."},
    {"print",  cmd_print,  "Print a dataset."},
    {"table",  cmd_table,  "Univariate frequency tabulation."},
    {"logreg", cmd_logreg, "Estimate a logistic regression model."},
    {"weight", cmd_weight, "Assign a weight variable to the dataset."},
    {"option", cmd_option, "Set a global option."},
    {"help",   cmd_help,   "Print some help on command syntax."},
    {"q",      cmd_quit,   "Exit the program."},
    {"quit",   cmd_quit,   "Exit the program."},
    {"#",      cmd_comment, "This line is a comment."},
    {(char *) NULL, (int_fp_v *) NULL, (char *) NULL}
};

/* set globals */
const char *MLELR_WELCOME = "mlelr - a reference implementation of logistic regression in C\nversion: 1.0\n";
FILE *LOGFILE;
FILE *OUTFILE;
FILE *INPUTFILE;
int LOGLEVEL;
const int SILENT    = 0;
const int INFO      = 1;
const int VERBOSE   = 2;


int input_handler (void) {

    int i;
    int retval;
    char *word;

    /* display a prompt if we are reading from stdin */
    if (INPUTFILE == stdin) {
        printout("%s", "mlelr-> ");
    }

    /* csvgetline will return NULL in one of two situations:
        1) we're out of memory
        2) we reached the EOF marker in an input file
        In both cases, we want to terminate the program. */
    if (csvgetline(INPUTFILE, ' ', 1) == NULL) {
        if (INPUTFILE != stdin) {
            printlog(VERBOSE, "%s\n", "Processing of input file is complete.");
        }
        else {
            printlog(VERBOSE, "%s\n", "Error reading input line from console.");
        }
        return 1;
    }

    /* ignore blank or unparseable lines */
    if (csvnfield() == 0) {
        return 0;
    }

    /* get the first word from the input */
    word = csvfield(0);

    /* walk through array of commands, execute if found */
    for (i = 0; cmds[i].name; i++) {

        /* test the command name */
        if (strcmp(word, cmds[i].name) == 0) {
            retval = cmds[i].f();
            return retval;
        }
    }

    /* command not found, warn but do not signal to exit */
    printlog(INFO, "%s%s\n%s\n", "Warning:  Command not found: ", word, "Enter 'help' for a list of available commands.");

    return 0;
}




/***

    Command functions processed by the input handler.

    These functions should check the syntax of the command buffer, and if valid,
    pass the command to an appropriate function defined in its proper module.

    It is important that these functions return 0 on success (except for quit),
    since the input handler will terminate if it receives a nonzero response.

    If the wrapped function returns an error, it should be handled here so that
    we can return 0 and subsequent commands can be read from the input file.
    The only reason to return a nonzero value would be if the error is significant
    enough that it warrants premature and immediate termination of the program.

    We are calling these "cmd" functions simply to distinguish them from other
    parts of the program.  They are being called by the input handler after
    locating this function pointer in the cmds[] array above.

***/

static int cmd_quit (void) {

    /* say goodbye! */
    printlog(INFO, "%s\n", "Exiting.  Bye!");

    /* this is the only call function that should return nonzero */
    return 1;
}


static int cmd_comment (void) {

    /* do nothing, it's a comment */
    return 0;
}


static int cmd_import (void) {

    char delim;
    char *handle;
    char *filename;
    int retval;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_import'");

    /* warn and return if we do not have the required number of arguments */
    if (csvnfield() != 4) {
        printlog(INFO, "%s\n", "Syntax error: import expects 3 arguments:  handle filename delimiter");
        return 0;
    }

    printlog(VERBOSE, "%s%s\n%s%s\n%s%s\n", "Arguments to import:\nHandle: ", csvfield(1),
        "Filename: ", csvfield(2), "Delimiter: ", csvfield(3));

    /* make copies of the arguments because subsequent calls to csvgetline will overwrite them */
    handle = estrdup(csvfield(1));
    filename = estrdup(csvfield(2));

    /* accept literal "\t" as tab-delimiter */
    if (strncmp(csvfield(3), "\\t", 2) == 0) {
        delim = '\t';
        printlog(VERBOSE, "%s\n", "Parsed delimiter: tab");
    }
    else {
        delim = csvfield(3)[0];
        if (delim == ' ')
            printlog(VERBOSE, "%s\n", "Parsed delimiter: space");
        else if (delim == ',')
            printlog(VERBOSE, "%s\n", "Parsed delimiter: comma");
        else
            printlog(VERBOSE, "%s%c\n", "Parsed delimiter: ", delim);
    }

    retval = import_dataset(handle, filename, delim);

    printlog(VERBOSE, "%s%d\n", "Return value from import_dataset: ", retval);

    free(handle);
    free(filename);

    return 0;
}


static int cmd_print (void) {

    int numlines;
    char *handle;
    dataset *ds;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_print'");

    /* warn and return if we do not have the required number of arguments */
    if (csvnfield() != 3) {
        printlog(INFO, "%s\n", "Syntax error: print expects 2 arguments:  handle numlines");
    }

    handle = estrdup(csvfield(1));
    numlines = atoi(csvfield(2));

    printlog(VERBOSE, "%s%s%s%d\n", "Arguments to print:\nHandle: ", handle, "\nNumber of lines: ", numlines);

    ds = find_dataset(handle);

    if (ds == NULL) {
        printlog(INFO, "%s%s\n", "Error:  dataset not found: ", handle);
    }
    else {
        print_dataset(ds, numlines, 1);
    }

    free(handle);

    return 0;
}


static int cmd_weight (void) {

    char *handle, *varname;
    dataset *ds;
    int var;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_weight'");

    /* warn and return if we do not have the required number of arguments */
    if (csvnfield() != 3) {
        printlog(INFO, "%s\n", "Syntax error: weight expects 2 arguments:  handle varname");
    }

    handle = estrdup(csvfield(1));
    varname = estrdup(csvfield(2));

    printlog(VERBOSE, "%s%s%s%s\n", "Arguments to weight:\nHandle: ", handle, "\nWeight variable: ", varname);

    ds = find_dataset(handle);

    if (ds == NULL) {
        printlog(INFO, "%s%s\n", "Error:  dataset not found: ", handle);
        free(handle);
        free(varname);
        return 0;
    }

    var = find_varname(ds, varname);

    if (var == -1) {
        printlog(INFO, "%s%s\n", "Error:  variable not found: ", varname);
    }
    else {
        set_weight_variable(ds, var);
    }

    free(handle);
    free(varname);

    return 0;
}


static int cmd_table (void) {

    char *handle, *varname;
    dataset *ds;
    int var;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_table'");

    /* warn and return if we do not have the required number of arguments */
    if (csvnfield() != 3) {
        printlog(INFO, "%s\n", "Syntax error: table expects 2 arguments:  handle varname");
    }

    handle = estrdup(csvfield(1));
    varname = estrdup(csvfield(2));

    printlog(VERBOSE, "%s%s%s%s\n", "Arguments to table:\nHandle: ", handle, "\nVariable: ", varname);

    ds = find_dataset(handle);

    if (ds == NULL) {
        printlog(INFO, "%s%s\n", "Error:  dataset not found: ", handle);
        free(handle);
        free(varname);
        return 0;
    }

    var = find_varname(ds, varname);

    if (var == -1) {
        printlog(INFO, "%s%s\n", "Error:  variable not found: ", varname);
    }
    else {
        frequency_table(ds, var);
    }

    free(handle);
    free(varname);

    return 0;
}


static int cmd_logreg (void) {

    dataset *ds;
    model   *mod;
    char    syntax_error_msg[] = "Syntax error: logreg expects a dataset handle, followed by a dependent variable name, followed by \" = \" (note the spaces), followed by one or more main effects and optional interaction effects.\nSpecify interactions with an asterisk, as in var1*var2\nSpecify direct effects by preceding with \"direct.\", as in direct.var1";
    int     i;
    char    *varname;
    char    *endvar;
    int     retval = 0;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_logreg'");

    /***
        Expected:
        csvfield(0) == "logreg"
        csvfield(1) == dataset handle
        csvfield(2) == dependent variable name
        csvfield(3) == "="
        csvfield(4) == start of main effects
    ***/

    /* primary syntax check:  require a variable name followed by "=" followed by at least one variable */
    if (csvnfield() < 5 || strcmp("=", csvfield(3)) != 0) {
        printlog(INFO, "%s\n", syntax_error_msg);
        return 0;
    }

    /* attempt to locate the dataset */
    if ((ds = find_dataset(csvfield(1))) == NULL) {
        printlog(INFO, "%s%s\n", "Dataset not found: ", csvfield(1));
        return 0;
    }
    printlog(VERBOSE, "%s%s\n", "Dataset found with handle: ", ds->handle);

    mod = (model *) emalloc(sizeof(model));
    init_model(mod);

    /* add the dependent variable to the model */
    if (add_model_variable(mod, ds, csvfield(2), DEPENDENT) != 0) {
        printlog(INFO, "%s%s%s%s\n", "Dependent variable name not found: ", csvfield(2), " in dataset: ", ds->handle);
        delete_model(mod);
        return 0;
    }

    /* parse the independent variable effects */
    for (i = 4; i < csvnfield(); i++) {

        /* is this an interaction? */
        if (strchr(csvfield(i), '*') != NULL) {

            varname = csvfield(i);
            endvar = varname;

            while (varname != NULL) {
                endvar = strchr(varname, '*');
                if (endvar != NULL) endvar[0] = '\0';
                if (strcmp(varname, csvfield(i)) == 0) {
                    retval = add_model_variable(mod, ds, varname, NEW_INTERACTION);
                }
                else {
                    retval = add_model_variable(mod, ds, varname, INTERACTION);
                }
                if (retval) break;
                if (endvar == NULL) {
                    varname = NULL;
                }
                else {
                    varname = endvar + 1;
                }
            }

        }

        /* is this a direct effect? */
        else if (strlen(csvfield(i)) >= 8 && strncmp("direct.", csvfield(i), 7) == 0) {
            varname = csvfield(i) + 7;
            retval = add_model_variable(mod, ds, varname, DIRECT);
        }

        /* otherwise this is a categorical main effect */
        else {
            varname = csvfield(i);
            retval = add_model_variable(mod, ds, varname, MAIN);
        }

        /* error check */
        if (retval != 0) {
            printlog(INFO, "%s\n", syntax_error_msg);
            delete_model(mod);
            return 0;
        }

    }   /* end parsing independent variables */


    /* NOTE:  We do not check for duplicate interactions */

    /***
        Pass control to the mlelr module.

        At this point, the syntax of the model has been checked and the model
        struct can now be handed off to the mlelr function where the real
        work happens.
    ***/
    retval = mlelr(ds, mod);

    printlog(VERBOSE, "Return value from mlelr function: %d\n", retval);

    return 0;
}


static int cmd_option (void) {

    char *k, *v;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_option'");

    /* warn and return if we do not have the required number of arguments */
    if (csvnfield() != 3) {
        printlog(INFO, "%s\n", "Syntax error: option expects 2 arguments:  key value");
    }

    k = csvfield(1);
    v = csvfield(2);

    printlog(VERBOSE, "%s%s%s%s\n", "Arguments to option:\nKey: ", k, "\nValue: ", v);

    set_option(k, v);

    return 0;
}


static int cmd_help (void) {

    int i;

    printlog(VERBOSE, "%s\n", "Entering 'cmd_help'");

    printout("%s%s", MLELR_WELCOME, "Available commands:\n");

    /* walk through array of commands and print help text */
    for (i = 0; cmds[i].name; i++) {
        printout("%    -12s%s\n", cmds[i].name, cmds[i].desc);
    }

    return 0;
}


void init_options (void) {

    /* Run once at program start-up */

    options.n = 0;
    options.size = 1;

    options.k = (char **) emalloc(options.size * sizeof(char *));
    options.v = (char **) emalloc(options.size * sizeof(char *));

    set_option("params", "centerpoint");


}


char *get_option (char *k) {

    int i;

    for (i = 0; i < options.n; i++) {
        if (strncmp(k, options.k[i], strlen(options.k[i])) == 0) {
            return options.v[i];
        }
    }

    return NULL;

}


void set_option (char *k, char *v) {

    int i, found;

    for (i = 0, found = 0; i < options.n; i++) {
        if (strncmp(k, options.k[i], strlen(options.k[i])) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        if (1 + options.n >= options.size) {
            options.size *= 2;
            options.k = (char **) erealloc(options.k, options.size * sizeof(char *));
            options.v = (char **) erealloc(options.v, options.size * sizeof(char *));
        }
        options.k[options.n] = estrdup(k);
        options.v[options.n] = estrdup(v);
        options.n++;
    }
    else {
        options.v[i] = estrdup(v);
    }

}




/***

    Input/output convenience functions

***/

void printlog (int loglevel, char *format, ...) {

    va_list args;

    /* only print if the loglevel argument is less than or equal to the
       global LOGLEVEL variable.

       Examples:
       If LOGLEVEL == VERBOSE, then calls to log(INFO, ...) or log(VERBOSE, ...)
       will be printed.  If LOGLEVEL == INFO, then calls to log(VERBOSE, ...) will
       not be printed.  If LOGLEVEL == SILENT, then no calls should be printed.
    */
    if (loglevel <= LOGLEVEL) {
        va_start(args, format);
        vfprintf(LOGFILE, format, args);
        va_end(args);
    }

}


void printout (char *format, ...) {
    va_list args;
    va_start(args, format);
	vfprintf(OUTFILE, format, args);
	va_end(args);
}




/***

    Wrappers for memory allocation.

    When using malloc and other related functions that request dynamic memory,
    a common pattern is to test whether the result is NULL which would signal
    that the memory call failed.  If this happens, there is very little else
    we can do.  We have to assume that we have run out of memory.  We can't
    request any more memory since subsequent calls will also be likely to fail
    until memory is made available at the OS level.

    Our only recourse at this point is to print out an error message and
    terminate.  There may be more graceful ways of handling the situation,
    but for our demonstration purposes, we are not going to try to continue
    if we cannot access the required memory.

    Use these wrapper functions anytime the memory call is mandatory.
    The NULL test is done here, so by calling these functions, you can
    assume that the memory allocation was successful if the function returns.

    Derived (as with most of my good ideas) from Kernighan & Pike, TPOP:
    http://cm.bell-labs.com/cm/cs/tpop/

***/

void *emalloc (size_t n) {

	void *p;

	p = malloc(n);
	if (p == NULL) {
		eprintf("Fatal error!  malloc of %u bytes failed:", n);
	}
	return p;

}

void *erealloc (void *vp, size_t n) {

	void *p;

	p = realloc(vp, n);
	if (p == NULL) {
		eprintf("Fatal error!  realloc of %u bytes failed:", n);
	}
	return p;

}

char *estrdup (char *s) {

	char *t;

	t = (char *) malloc(strlen(s)+1);
	if (t == NULL) {
		eprintf("Fatal error!  estrdup(\"%.20s\") failed:", s);
	}
	strcpy(t, s);
	return t;

}

static void eprintf (char *fmt, ...) {

	va_list args;

	fflush(stdout);
	va_start(args, fmt);
	vfprintf(LOGFILE, fmt, args);
	va_end(args);

	if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
		fprintf(LOGFILE, " %s", strerror(errno));
	fprintf(LOGFILE, "\n");
	exit(2);

}
