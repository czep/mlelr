/* tabulate.c */

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
#include "tabulate.h"

static char *freqvars[] = {"Value", "Freq"};

int frequency_table (dataset *ds, int var) {

    dataset *freq;
    int i, j, found;
    double obs[2];
    double target;
    double weight;
    char *handle;
    char *handle_prefix = "Frequency table for: ";

    printlog(VERBOSE, "Building frequency table for variable '%s' in dataset '%s'\n", ds->varnames[var], ds->handle);


    handle = (char *) emalloc( strlen(handle_prefix) + strlen(ds->varnames[var]) + 1 );
    strcpy(handle, handle_prefix);
    strcat(handle, ds->varnames[var]);

    freq = add_dataset(handle, 2, freqvars, 0);

    /* loop for each observation in the dataset */
    for (i = 0; i < ds->n; i++) {

        target = ds->obs[i][var];
        weight = (ds->weight == -1) ? 1.0 : ds->obs[i][ds->weight];

        /* search for the target value in the existing frequency table */
        for (j = 0, found = 0; j < freq->n; j++) {
            if (target == freq->obs[j][0]) {
                found = 1;
                break;
            }
        }

        /* if not found, add to the frequency table */
        if (!found) {
            obs[0] = target;
            obs[1] = weight;
            add_observation(freq, obs);
        }
        /* otherwise, increment the frequency table */
        else {
            freq->obs[j][1] += weight;
        }

    }
    sort_dataset(freq, 1);
    print_dataset(freq, 0, 1);

    return 0;
}

int tabulate (dataset *ds, model *mod) {

    int i, j, k;
    char **varnames;
    int found;
    double freq_obs[2];
    double target;
    double *obs;
    double weight;

    printlog(VERBOSE, "Tabulating...\n");

    /* initialize data structures */
    varnames = (char **) emalloc((2 + mod->numiv) * sizeof(char *));
    for (i = 0; i < mod->numiv; i++) {
        varnames[i] = mod->ivnames[i];
    }
    varnames[i++] = mod->dvname;
    varnames[i] = estrdup("_Count");
    mod->xtab = add_dataset("_mlelr_xtab", 2 + mod->numiv, varnames, 0);

    /* array of datasets to store univariate frequencies */
    mod->freqs = (dataset **) emalloc((2 + mod->numiv) * sizeof(dataset *));
    for (i = 0; i < mod->numiv; i++) {
        mod->freqs[i] = add_dataset("_mlelr_freq_iv", 2, freqvars, 0);
    }
    mod->freqs[i] = add_dataset("_mlelr_freq_dv", 2, freqvars, 0);

    obs = (double *) emalloc((2 + mod->numiv) * sizeof(double));

    /* loop for each observation in the dataset */
    for (i = 0; i < ds->n; i++) {

        /* get the weight for the current observation */
        weight = (ds->weight == -1) ? 1.0 : ds->obs[i][ds->weight];

        /* the weight must be positive otherwise we will ignore the entire observation */
        if (weight > 0) {

            /* loop for each variable in the crosstab */
            for (j = 0; j <= mod->numiv; j++) {

                /* get the target value from the current observation */
                if (j < mod->numiv)
                    target = ds->obs[i][mod->iv[j]];
                else
                    target = ds->obs[i][mod->dv];

                /* search for the target in the existing frequency table */
                for (k = 0, found = 0; k < mod->freqs[j]->n; k++) {
                    if (target == mod->freqs[j]->obs[k][0]) {
                        found = 1;
                        break;
                    }
                }

                /* if not found, add to the frequency table */
                if (!found) {
                    freq_obs[0] = target;
                    freq_obs[1] = weight;
                    add_observation(mod->freqs[j], freq_obs);
                }
                /* otherwise, increment the frequency table */
                else {
                    mod->freqs[j]->obs[k][1] += weight;
                }

                obs[j] = target;

            }   /* end loop for each variable in the crosstab */

            /* search for the obs in the xtab */
            found = find_observation(mod->xtab, obs, 1 + mod->numiv);
            if (found == -1) {
                obs[1 + mod->numiv] = weight;
                add_observation(mod->xtab, obs);
            }
            else {
                mod->xtab->obs[found][1 + mod->numiv] += weight;
            }

        } /* end test for positive weight */

    }   /* end loop for each observation in the dataset */

    /* sort the freq and xtab datasets */
    for (i = 0; i < 1 + mod->numiv; i++) {
        sort_dataset(mod->freqs[i], 1);
    }
    sort_dataset(mod->xtab, 1 + mod->numiv);

    printlog(VERBOSE, "Tabulation complete.\n");

    return 0;

}
