/* model.c */

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
#include "model.h"
#include "interface.h"


void init_model (model *mod) {

    mod->maxiv = 1;
    mod->numiv = 0;
    mod->ivnames = (char **) emalloc(mod->maxiv * sizeof(char *));
    mod->iv = (int *) emalloc(mod->maxiv * sizeof(int));
    mod->direct = (int *) emalloc(mod->maxiv * sizeof(int));
    mod->maxints = 1;
    mod->numints = 0;
    mod->inttc = (int *) emalloc(mod->maxints * sizeof(int));
    mod->ints = (int **) emalloc(mod->maxints * sizeof(int *));
    mod->intnames = (char **) emalloc(mod->maxints * sizeof(char *));

}

void delete_model (model *mod) {
    /* This feature is currently unavailable. */
    return;
}

void print_model (model *mod) {

    int i, j;

    printout("Dependent variable: %s\n", mod->dvname);
    printlog(VERBOSE, "\tAt position: %d\n", mod->dv);
    printout("Number of independent variables: %d\n", mod->numiv);
    printlog(VERBOSE, "Space allocated for independent variable array: %d\n", mod->maxiv);
    for (i = 0; i < mod->numiv; i++) {
        printout("Effect %d: %s", 1+i, mod->ivnames[i]);
        if (mod->direct[i] == 1)
            printout(" (DIRECT)\n");
        else
            printout("\n");
        printlog(VERBOSE, "\tAt position: %d\n", mod->iv[i]);
    }

    printout("Number of interactions: %d\n", mod->numints);
    printlog(VERBOSE, "Space allocated for interactions: %d\n", mod->maxints);
    for (i = 0; i < mod->numints; i++) {
        printout("Interaction %d: %s, %d terms [", 1+i, mod->intnames[i], mod->inttc[i]);
        for (j = 0; j < mod->inttc[i]; j++) {
            printout("%d", mod->ints[i][j]);
            if (j < mod->inttc[i] - 1)
                printout(", ");
            else
                printout("]\n");
        }
    }

}


int add_model_variable (model *mod, dataset *ds, char *varname, int vartype) {

    int varidx, ividx;
    int i;
    int found = 0;
    char *intname;

    /* check for a recognized variable type */
    switch (vartype) {
        case DEPENDENT:
            printlog(VERBOSE, "%s%s\n", "Adding DEPENDENT variable: ", varname);
            break;
        case MAIN:
            printlog(VERBOSE, "%s%s\n", "Adding MAIN EFFECT variable: ", varname);
            break;
        case DIRECT:
            printlog(VERBOSE, "%s%s\n", "Adding DIRECT EFFECT variable: ", varname);
            break;
        case NEW_INTERACTION:
            printlog(VERBOSE, "%s%s\n", "Adding FIRST INTERACTION variable: ", varname);
            break;
        case INTERACTION:
            printlog(VERBOSE, "%s%s\n", "Adding INTERACTION variable: ", varname);
            break;
        default:
            printlog(INFO, "%s%d%s%s\n", "Error: unrecognized variable type: ", vartype, " for variable: ", varname);
            return 1;
    }

    /* find the variable name and fail if not found */
    varidx = find_varname(ds, varname);
    if (varidx == -1) {
        return 1;
    }

    /* add the DEPENDENT variable to the model */
    if (vartype == DEPENDENT) {
        mod->dv = varidx;
        mod->dvname = estrdup(varname);
        return 0;
    }

    /* search the array of main effects */
    for (i = 0; i < mod->numiv; i++) {
        if (varidx == mod->iv[i]) {
            found = 1;
            ividx = i;
            break;
        }
    }

    /* warn and return success, if MAIN or DIRECT variable already exists */
    if (found && (vartype == MAIN || vartype == DIRECT)) {
        printlog(INFO, "%s%s\n", "Warning: variable already exists in model: ", varname);
        return 0;
    }

    else if (!found) {

        /* warn if this is an interaction effect without a main effect */
        if (vartype == NEW_INTERACTION || vartype == INTERACTION) {
            printlog(INFO, "%s%s\n", "Warning: this interaction variable will also be added as a main effect: ", varname);
        }

        /* space check */
        if (1 + mod->numiv > mod->maxiv) {
            mod->maxiv *= 2;
            mod->ivnames = (char **) erealloc(mod->ivnames, mod->maxiv * sizeof(char *));
            mod->iv = (int *) erealloc(mod->iv, mod->maxiv * sizeof(int));
            mod->direct = (int *) erealloc(mod->direct, mod->maxiv * sizeof(int));
        }

        /* add this variable as a main effect */
        mod->iv[mod->numiv] = varidx;
        mod->ivnames[mod->numiv] = estrdup(varname);
        mod->direct[mod->numiv] = (vartype == DIRECT) ? 1 : 0;
        printlog(VERBOSE, "Setting direct array index %d to value %d\n", mod->numiv, mod->direct[mod->numiv]);
        mod->numiv++;

    }

    /* add a new interaction */
    if (vartype == NEW_INTERACTION) {

        /* space check */
        if (1 + mod->numints > mod->maxints) {
            mod->maxints *= 2;
            mod->inttc = (int *) erealloc(mod->inttc, mod->maxints * sizeof(int));
            mod->ints = (int **) erealloc(mod->ints, mod->maxints * sizeof(int *));
            mod->intnames = (char **) erealloc(mod->intnames, mod->maxints * sizeof(char *));
        }

        /* add this variable to the newly created interaction */
        mod->ints[mod->numints] = (int *) emalloc(1 * sizeof(int));

        /* set the index of the interaction variable in the iv array (not the dataset) */
        mod->ints[mod->numints][0] = ividx;

        mod->inttc[mod->numints] = 1;
        mod->intnames[mod->numints] = estrdup(varname);
        mod->numints++;

    }

    /* append to an existing interaction */
    if (vartype == INTERACTION) {

        /* search the terms in the latest interaction and warn if already found */
        for (i = 0, found = 0; i < mod->inttc[mod->numints-1]; i++) {
            if (ividx == mod->ints[mod->numints-1][i]) {
                found = 1;
                break;
            }
        }

        /* warn if found */
        if (found) {
            printlog(INFO, "%s%s\n", "Warning: interaction variable already exists: ", varname);
            return 0;
        }

        /* add this variable to the latest interaction */
        mod->ints[mod->numints-1] = (int *) erealloc(mod->ints[mod->numints-1], (1 + mod->inttc[mod->numints-1]) * sizeof(int));
        mod->ints[mod->numints-1][mod->inttc[mod->numints-1]] = ividx;
        intname = (char *) emalloc( strlen(mod->intnames[mod->numints-1]) + strlen(varname) + 2 );
        strcpy(intname, mod->intnames[mod->numints-1]);
        strcat(intname, "*");
        strcat(intname, varname);
        mod->intnames[mod->numints-1] = estrdup(intname);
        mod->inttc[mod->numints-1] += 1;

    }

    return 0;
}
