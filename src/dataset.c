/* dataset.c */

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
#include <math.h>
#include <errno.h>
#include "dataset.h"
#include "interface.h"
#include "csv.h"

/***
    SYSMIS

    This is borrowed from SPSS and is short for "System Missing", meaning a logically null value.
    I am also borrowing the actual value used by SPSS, since it would be highly rare to observe
    this value in a real-world dataset.  The value is the largest negative number expressible with
    double (64 bit) precision.
***/
const double SYSMIS = 0xffefffffffffffff;

struct dataspace dataspace;


/* static function declarations */
static int compare_obs (const void *v1, const void *v2);
static int string_to_double(char *str, double* d);


/* public function definitions */

void init_dataspace (void) {

    /* called once at program instantiation to initialize the dataspace */
    dataspace.n = 0;
    dataspace.size = 1;
    dataspace.datasets = (dataset *) emalloc(dataspace.size * sizeof(dataset));

    printlog(VERBOSE, "Dataspace initialized with space for %d datasets.\n", dataspace.size);

}


int import_dataset (char *handle, char *filename, char delim) {

    /* import a delimited text file as a new dataset */

    int i, j;
    FILE *ifp;
    char **varnames;
    int nvars = 0;
    char *line;
    double *obs;
    dataset *ds;

    printlog(INFO, "%s%s\n", "Importing dataset from file: ", filename);

    /* try to open file */
    if ((ifp = fopen(filename, "r")) == NULL) {
        printlog(INFO, "%s%s\n", "Error:  Could not open file: ", filename);
        return -1;
    }

    /***
        read variable names from first row
    ***/

    if (csvgetline(ifp, delim, 0) == NULL) {
        printlog(INFO, "%s%s\n", "Error:  File is empty, ", filename);
        return -1;
    }

    /* set number of variables to number of fields parsed */
    if ((nvars = csvnfield()) < 1) {
        printlog(INFO, "%s\n", "Error:  No variable names found.  Check that delimiter string is correct.");
        return -1;
    }

    printlog(INFO, "%s%d\n", "Number of variables found: ", nvars);

    /* allocate space to store variable names */
    varnames = (char **) emalloc(nvars * sizeof(char *));

    /* read in varnames */
    printlog(INFO, "%s", "Variable names: ");
    for (i = 0; i < nvars; i++) {
        varnames[i] = estrdup(csvfield(i));
        printlog(INFO, "%s ", varnames[i]);
    }
    printlog(INFO, "\n");

    /***
        add a new dataset to the dataspace
    ***/

    ds = add_dataset(handle, nvars, varnames, 1);
    obs = (double *) emalloc(nvars * sizeof(double));

    /* read the data until end of file */
    for (i = 0; (line = csvgetline(ifp, delim, 0)) != NULL; i++) {

        /* for each line, fail if we do not have the expected number of fields */
        if (csvnfield() != nvars) {
            printlog(INFO, "%s%d%s%d%s%d%s\n%s\n", "Error:  Invalid field count at row: ", i + 2,
                ".  Fields expected: ", nvars, ".  Fields found: ", csvnfield(), ".  Failed record: ", line);
            return -1;
        }

        /* parse the data in each field and store in obs */
        for (j = 0; j < nvars; j++) {

            if (string_to_double(csvfield(j), &obs[j]) < 0) {

                /* set to sysmis if could not read as double,
                   this is a debatable solution, but really
                   only affects edge cases unlikely to come up
                   in practice */
                obs[j] = SYSMIS;
            }
        }

        /* add this observation to the dataset */
        add_observation(ds, obs);

    }

    printlog(INFO, "%s%d\n", "Number of observations read: ", ds->n);
    fclose(ifp);
    printlog(INFO, "%s\n", "Import complete.");

    return 0;
}


void print_dataset(dataset *ds, int n, int header) {

    int i, j;

    if (ds == NULL) {
        printout("%s%s\n", "Error: invalid dataset!");
        return;
    }

    if (n == 0) {
        n = ds->n;
    }

    if (header) {
        printout("%s%s\n", "Dataset: ", ds->handle);
        printout("%s%d\n", "Number of observations: ", ds->n);
        printout("%s%d\n", "Number of variables: ", ds->nvars);
    }

    for (i = 0; i < ds->nvars; i++) {
        printout("%16s", ds->varnames[i]);
    }
    printout("\n");
    for (i = 0; i < n; i++) {
        for (j = 0; j < ds->nvars; j++) {
            printout("%16.2f", ds->obs[i][j]);
        }
        printout("\n");
    }

    return;

}


dataset *add_dataset (char *handle, int nvars, char **varnames, int is_public) {

    dataset *datasets, *ds;

    printlog(VERBOSE, "%s%s\n%s%d\n", "add_dataset called with these arguments:\nHandle: ",
        handle, "Number of variables: ", nvars);

    /* grow the array of datasets if needed */
    if (is_public && 1 + dataspace.n >= dataspace.size) {
        dataspace.size *= 2;
        printlog(VERBOSE, "Reallocating the dataspace with space for %d datasets\n", dataspace.size);
        datasets = (dataset *) erealloc(dataspace.datasets, dataspace.size * sizeof(dataset));
        dataspace.datasets = datasets;
    }

    /* initialize the new dataset */
    if (is_public) {
        ds = &dataspace.datasets[dataspace.n];
        dataspace.n++;
    }
    else {
        ds = (dataset *) emalloc(sizeof(dataset));
    }

    ds->handle = estrdup(handle);
    ds->n = 0;
    ds->size = 1;
    ds->nvars = nvars;
    ds->varnames = varnames;
    ds->weight = -1;
    ds->values = (double *) emalloc(ds->size * ds->nvars * sizeof(double));
    ds->obs = (double **) emalloc(ds->size * ds->nvars * sizeof(double));

    if (is_public) {
        printlog(INFO, "%s%s\n", "New dataset created with handle: ", ds->handle);
    }
    else {
        printlog(VERBOSE, "%s%s\n", "New dataset created with handle: ", ds->handle);
    }

    return ds;
}


void add_observation (dataset *ds, double *obs) {

    int i;

    /* grow the array of values if needed */
    if (1 + ds->n >= ds->size) {
        ds->size *= 2;
        printlog(VERBOSE, "Reallocating the array of values in dataset '%s' with space for %d observations\n", ds->handle, ds->size);
        ds->values = (double *) erealloc(ds->values, ds->size * ds->nvars * sizeof(double));

        /* set dataset pointers */
        ds->obs = (double **) erealloc(ds->obs, ds->size * ds->nvars * sizeof(double *));
        for (i = 0; i < ds->size; i++) {
            ds->obs[i] = &ds->values[i * ds->nvars];
        }
    }

    printlog(VERBOSE, "Adding obs to dataset '%s': %d\n", ds->handle, ds->n);

    /* append the observations to the dataset */
    for (i = 0; i < ds->nvars; i++) {
        ds->values[i + (ds->n * ds->nvars)] = obs[i];
    }

    ds->n++;

}


void sort_dataset(dataset *ds, int n_cols) {

    compare_obs(NULL, &n_cols);
    qsort(ds->values, ds->n, ds->nvars * sizeof(double), compare_obs);

    return;
}


int compare_obs (const void *v1, const void *v2) {

    static int sort_columns = 1;
    const double *a = (const double *) v1;
    const double *b = (const double *) v2;
    int i;

    /* trickery */
    if (v1 == NULL) {
        sort_columns = *(int *) v2;
        return 0;
    }

    for (i = 0; i < sort_columns && a[i] == b[i]; i++);

    return (a[i] > b[i]) - (a[i] < b[i]);

}


int find_observation(dataset *ds, double *obs, int n_vars) {

    int i, j;
    int found = 0;

    /* search the first n_vars in dataset that match obs */
    for (i = 0; i < ds->n; i++) {
        for (j = 0; j < n_vars; j++) {
            if (obs[j] != ds->obs[i][j]) {
                break;
            }
        }
        if (j == n_vars) {
            found = 1;
            break;
        }
    }

    if (found)
        return i;
    else
        return -1;

}


dataset *find_dataset (char *handle) {

    /* search the dataspace for a dataset with the given handle */
    int i;
    dataset *ds;

    printlog(VERBOSE, "%s%s\n", "Searching for dataset: ", handle);

    /* linear search is fine until n exceeds 30 or thereabouts */
    for (i = 0; i < dataspace.n; i++) {
        if (strcmp(handle, dataspace.datasets[i].handle) == 0) {
            break;
        }
    }

    /* if we have exhausted the entire dataspace without finding the handle, return NULL */
    if (i < dataspace.n) {
        ds = &dataspace.datasets[i];
        printlog(VERBOSE, "%s%d\n", "Found at index: ", i);
    }
    else {
        ds = NULL;
    }

    return ds;

}


int find_varname (dataset *ds, char *varname) {

    /* search for and return the index of a variable name in the given dataset */
    int i;

    printlog(VERBOSE, "%s%s%s%s\n", "Searching for variable name: ", varname, " in dataset: ", ds->handle);

    for (i = 0; i < ds->nvars; i++) {
        /*printlog(VERBOSE, "%s%d%s%s\n", "Compare to variable ", i, ", name: ", ds->varnames[i]);*/
        if (strcmp(varname, ds->varnames[i]) == 0) {
            printlog(VERBOSE, "%s%d\n", "Found at index: ", i);
            break;
        }
    }

    /* return the index of the variable, else -1 */
    if (i < ds->nvars) {
        return i;
    }
    else {
        printlog(VERBOSE, "%s\n", "Variable not found!");
        return -1;
    }

}


int set_weight_variable (dataset *ds, int var) {

    /* set and return the index of the variable, else -1 */
    if (var < ds->nvars) {
        printlog(INFO, "Setting weight variable to: '%s' (%d)\n", ds->varnames[var], var);
        ds->weight = var;
    }
    else {
        printlog(INFO, "%s%d\n", "Weight variable out of range: ", var);
        ds->weight = -1;
    }

    return ds->weight;

}




/* static function definitions */


static int string_to_double(char *str, double* d) {

    char *endptr;

    *d = strtod(str, &endptr);

    /***
    Possible outcomes of strtod:
        0 Successful conversion
        1  Successful conversion with overflow, result set to +Infinity
        2  Successful conversion with overflow, result set to -Infinity
        3  Successful conversion with underflow, result set to 0
        4  Successful conversion, result parsed as NaN
    -1  Failed conversion

    pseudo code:
    if retval == 0
        if errno != 0 then underflow occurred, treat as successful
        else if endptr == str then conversion failed
        else success
    else if retval isinf() or isnan()
        then one of the special cases was detected

    ***/

    if (*d == 0) {
        if (errno != 0) {
            /* Success, underflow */
            return 3;
        }
        else if (endptr - str == 0) {
            /* FAIL */
            return -1;
        }
        else {
            /* Success */
            return 0;
        }
    }
    else if (isnan(*d))           return 4;   /* Success, NaN */
    else if (isinf(*d) && *d > 0) return 1;   /* Success, +Inf */
    else if (isinf(*d) && *d < 0) return 2;   /* Success, -Inf */
    else return 0;  /* Success */

}
