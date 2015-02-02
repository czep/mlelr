/* model.h */

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

#ifndef MODEL_H__
#define MODEL_H__

/***
    model

    Store all relevant variables necessary to specify a model.
***/
typedef struct {
    char    *dvname;   /* name of the dependent variable */
    int     dv;        /* dependent variable index in dataset */
    int     numiv;     /* number of independent variables */
    int     maxiv;     /* space allocated for iv array */
    char    **ivnames; /* names of the independent variables */
    int     *iv;       /* indices of the independent variables (main effects only) */
    int     *direct;   /* for each iv, stores 1 if direct effect else 0 */
    int     numints;   /* number of interactions */
    int     maxints;   /* space allocated for interactions matrix */
    int     *inttc;    /* interaction term count */
    int     **ints;    /* interactions matrix, rows=interactions, cols=index in numiv */
    char    **intnames;/* array of interaction names, eg. "var1*var2" */

    dataset *xtab;      /* cross-tabulation of all model variables */
    dataset **freqs;    /* array of frequency tables for all model variables */

} model;

enum model_variable {
    DEPENDENT,
    MAIN,
    DIRECT,
    NEW_INTERACTION,
    INTERACTION
};


/* forward declarations for publically available functions defined in model.c */

extern void init_model (model *mod);
extern void delete_model (model *mod);
extern void print_model (model *mod);
extern int add_model_variable (model *mod, dataset *ds, char *varname, int vartype);

#endif
