/* mlelr.c */

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
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_cdf.h>
#include "dataset.h"
#include "model.h"
#include "mlelr.h"
#include "interface.h"
#include "tabulate.h"

static const int MAX_ITER = 30;
static const double EPSILON = 1e-8;

static int cholesky(double **x, int order);
static int backsub(double **x, int order);
static int trimult(double **in, double **out, int order);

static int newton_raphson (
    double **X,     /* design matrix, N rows by K cols */
    double **Y,     /* response matrix, N rows by J-1 cols */
    double  *n,     /* vector of population counts, N rows */
    int      J,     /* number of discrete values of Y */
    int      N,     /* number of populations */
    int      K,     /* number of columns in X */
    double  *beta0, /* starting parameters, K * J-1 rows */
    double  *beta1, /* parameters after this iteration */
    double **xtwx,
    double  *loglike,
    double  *deviance  );



int mlelr (dataset *ds, model *mod) {

    int i, j, k, q;
    int popchange;
    int lastpop;
    int xr, xc;
    int xtabrows;
    int xtabcols;
    double **xtab;
    int levels;
    double **freq;
    int *intcolidx;
    double tgt;

    int     N;          /* number of populations (combinations of iv) */
    int     *popindex;  /* array mapping rows in xtab to population number */
    double  M;          /* the total frequency count, M */
    int     J;          /* number of response functions, unique values for Y */
    int     K;          /* number of columns required in design matrix X */
    double  **Y;        /* response matrix */
    double  **X;        /* design matrix */
    double  *n;         /* sum of observations in each population */
    int     *startcol;  /* starting location of each iv in X */
    int     *colspan;   /* number of columns occupied by each iv in X */
    double  *beta;      /* parameter estimates */
    char   **Xlabels;
    int     dummy = 0;

    double  *beta0;
    double  *beta_inf;
    double **xtwx;
    double *sigprms, *stderrs, *wald;
    double  *loglike, loglike0;
    double  *deviance;

    int iter;
    int convergence;
    int nrret;
    double chi1, chi2, df1, df2, chitest1, chitest2;


    printlog(VERBOSE, "Entering mlelr.\n");

    /***
        Step 1.  Build the crosstab as a precursor to the design matrix.

        <note why we need to do this>

        If we had a database library, this could be easily accomplished with a simple
        "select iv1, iv2, ..., dv, count(*) from ... group by ... order by ..."
        but instead of requiring a third-party database library, we can just write
        this routine ourselves.
    ***/
    tabulate(ds, mod);

    /* for convenient reference */
    xtab = mod->xtab->obs;
    xtabrows = mod->xtab->n;
    xtabcols = mod->xtab->nvars;


    /***
        Step 2.  Count the number of populations and set a population index
        for each row in the xtab.
    ***/

    popindex = (int *) emalloc(xtabrows * sizeof(int));

    /* set the population index for the first row */
    N = 1;
    M = xtab[0][xtabcols - 1];
    popindex[0] = 0;

    /* loop through remainder of xtab */
    for (i = 1; i < xtabrows; i++) {

        popchange = 0;

        /* check each independent variable against its predecessor */
        for (j = 0; j < xtabcols - 2; j++) {
            if (xtab[i][j] != xtab[i-1][j]) {
                popchange = 1;
                break;
            }
        }

        if (popchange) N++;
        popindex[i] = N - 1;
        M += xtab[i][xtabcols - 1];
    }


    /***
        Step 3.  Count number of cols needed in X and Y
    ***/

    /* number of cols in Y */
    J = mod->freqs[mod->numiv]->n;

    /* number of cols in X, including the intercept */
    for (i = 0, K = 1; i < mod->numiv; i++) {

        /* if direct effect, this variable will have 1 column in X */
        if (mod->direct[i]) {
            K += 1;
        }

        /* otherwise, it will have levelcount - 1 columns in X */
        else {
            K += (mod->freqs[i]->n - 1);
        }
    }

    /* add columns for interactions, if any */
    for (i = 0; i < mod->numints; i++) {
        k = 1;
        for (j = 0; j < mod->inttc[i]; j++) {
            if (!mod->direct[mod->ints[i][j]])
                k *= (mod->freqs[ mod->ints[i][j] ]->n - 1);
        }
        K += k;
    }


    /***
        Step 4.  Allocate space for model vectors and matrices

        X, the design matrix, has rows equal to the number of populations
            and cols equal to the count established above
        Y, the response matrix, has rows equal to the number of populations
            and cols equal to one minus the number of response functions
        n, the sum of the observation count in each population
    ***/


    X = (double **) emalloc(N * sizeof(double *));
    Y = (double **) emalloc(N * sizeof(double *));
    n = (double *)  emalloc(N * sizeof(double));

    for (i = 0; i < N; i++) {
        X[i] = (double *) emalloc(K * sizeof(double));
        Y[i] = (double *) emalloc(J * sizeof(double));
        /* initialize n and Y to 0 */
        n[i] = 0;
        for (j = 0; j < J; j++) {
            Y[i][j] = 0;
        }
    }

    startcol = (int *) emalloc(mod->numiv * sizeof(int));
    colspan = (int *) emalloc(mod->numiv * sizeof(int));

    /* column index of each term in an interaction
       initialize to hold the maximum number of interaction terms */
    if (mod->numints > 0) {
        /* find maximum interaction term count */
        for (i = 1, j = mod->inttc[0]; i < mod->numints; i++) {
            if (mod->inttc[i] > j)
                j = mod->inttc[i];
        }
        intcolidx = (int *) emalloc(j * sizeof(int));
    }
    else
        intcolidx = NULL;

    Xlabels = (char **) emalloc(K * sizeof(char *));

    /***
        Step 5.  Build X, Y, and n
    ***/

    /* if this option is set, use dummy coding instead of full-rank center-point */
    if (strcmp("dummy", get_option("params")) == 0) {
        dummy = 1;
    }

    lastpop = -1;
    xr = 0;
    xc = 0;

    /* loop for each row in xtab */
    for (i = 0; i < xtabrows; i++) {

        /* do if current row in xtab is a new population to code into X */
        if (popindex[i] != lastpop) {

            /* set intercept */
            xr = popindex[i];
            X[xr][0] = 1;
            xc = 1;

            /* do for each independent var */
            for (j = 0; j < xtabcols - 2; j++) {

                /* use actual value if direct effect */
                if (mod->direct[j]) {
                    X[xr][xc] = xtab[i][j];
                    xc += 1;
                }

                /* otherwise, use full-rank center-point parameterization */
                else {

                    /* do for each X column for this variable */
                    levels = mod->freqs[j]->n;

                    for (k = 0; k < levels - 1; k++) {

                        freq = mod->freqs[j]->obs;

                        if (xtab[i][j] == freq[k][0])
                            X[xr][xc] = 1;
                        else if (!dummy && xtab[i][j] == freq[levels - 1][0])
                            X[xr][xc] = -1;
                        else
                            X[xr][xc] = 0;

                        xc += 1;
                    }
                }

            } /* end loop for each ind var */

        } /* end if new pop */

        /* add count of Y-value to appropriate population */
        for (j = 0; j < J; j++) {
            if (xtab[i][xtabcols - 2] == mod->freqs[mod->numiv]->obs[j][0])
                break;
        }
        Y[xr][j] = xtab[i][xtabcols - 1];

        /* increment N */
        n[xr] += Y[xr][j];
        lastpop = xr;

    } /* end loop for each row in xtab */


    /* build labels for each parameter in the design matrix */
    Xlabels[0] = estrdup("Intercept");
    for (i = 0, k = 1; i < mod->numiv; i++) {
        if (mod->direct[i]) {
            Xlabels[k] = mod->ivnames[i];
            k += 1;
        }

        /* otherwise, it will have levelcount - 1 columns in X */
        else {
            for (j = 0; j < mod->freqs[i]->n - 1; j++) {
                Xlabels[k++] = mod->ivnames[i];
            }
        }
    }

    /* add columns for interactions, if any */
    for (i = 0, xc = k; i < mod->numints; i++) {
        k = 1;
        for (j = 0; j < mod->inttc[i]; j++) {
            if (!mod->direct[mod->ints[i][j]])
                k *= (mod->freqs[ mod->ints[i][j] ]->n - 1);
        }
        for (j = 0; j < k; j++) {
            Xlabels[xc++] = mod->intnames[i];
        }
    }

    /***
        Step 6.  Build the interactions portion of X
    ***/

    xc = 1;
    for (i = 0; i < xtabcols - 2; i++) {
        startcol[i] = xc;
        if (mod->direct[i])
            xc += 1;
        else
            xc += mod->freqs[i]->n - 1;
        colspan[i] = xc - startcol[i];
    }

    /* do for each set of interactions */
    for (i = 0; i < mod->numints; i++) {

        /* setup a counter for each term */
        for (j = 0; j < mod->inttc[i]; j++)
            intcolidx[j] = 1;

        q = 1;

        /* construct each column of the interaction */
        while (q) {

            /* multiply and write the current column */
            for (j = 0; j < N; j++) {
                tgt = 1.0;
                for (k = 0; k < mod->inttc[i]; k++)
                    tgt *= X[j][startcol[mod->ints[i][k]] + intcolidx[k] - 1];
                X[j][xc] = tgt;
            }

            /* move to next column in X */
            xc += 1;

            /* test for another column, starting with the last variable */
            q = 0;
            for (j = mod->inttc[i] - 1; j >= 0; j--) {
                if (!q) {
                    intcolidx[j] += 1;
                    if (intcolidx[j] > colspan[mod->ints[i][j]])
                        intcolidx[j] = 1;
                    else
                        q = 1;
                }
            }

        } /* end while */

    } /* end loop for each interaction */


    /***
        Step 7.  The Newton-Raphson loop
    ***/


    /* allocate space for beta arrays and covariance matrix */
    beta =     (double *) emalloc(K * (J - 1) * sizeof(double));
    beta0 =    (double *) emalloc(K * (J - 1) * sizeof(double));
    beta_inf = (double *) emalloc(K * (J - 1) * sizeof(double));

    xtwx = (double **) emalloc(K * (J - 1) * sizeof(double *));
    for (i = 0; i < K * (J - 1); i++) {
        xtwx[i] = (double *) emalloc(K * (J - 1) * sizeof(double));
    }

    /* allocate same amount of space for sigprms */
    sigprms = (double *) emalloc(K * (J - 1) * sizeof(double));
    stderrs = (double *) emalloc(K * (J - 1) * sizeof(double));
    wald =    (double *) emalloc(K * (J - 1) * sizeof(double));

    /* pointer to pass as arg to n-r to store log likelihood of new iteration */
    loglike = (double *) emalloc(sizeof(double));

    /* pointer to pass as arg to n-r to store deviance of new iteration */
    deviance = (double *) emalloc(sizeof(double));

    /* initialize starting betas to 0 */
    for (i = 0; i < (K * (J - 1)); i++) {
        beta[i] = 0;
        beta_inf[i] = 0;
    }

    iter = 0;
    convergence = 0;

    /* main N-R loop */
    while (iter < MAX_ITER && !convergence) {

        /* save betas from previous iteration */
        for (i = 0; i < (K * (J - 1)); i++) {
            beta0[i] = beta[i];
        }

        /* run an iteration, exit if failure */
        nrret = newton_raphson(X, Y, n, J, N, K, beta0, beta, xtwx, loglike, deviance);

        /* NOTE:  Backtracking code would go here, not currently implemented */

        /* test for convergence */
        convergence = 1;
        for (i = 0; i < (K * (J - 1)); i++) {
            if (fabs(beta[i] - beta0[i]) > EPSILON * fabs(beta0[i])) {
                convergence = 0;
                break;
            }
        }

        /* if this is the first iteration, record the initial LL */
        if (iter == 0)
            loglike0 = loglike[0];

        printlog(VERBOSE, "Iter: %d, LL: %f, Deviance: %f, Convergence: %d\n", iter, loglike[0], deviance[0], convergence);

        iter++;
    }

    /* significance tests */
    if (convergence) {

        /* test vs intercept-only model */
        chi1 = 2 * (loglike[0] - loglike0);
        df1 = (K * (J-1)) - J - 1;
        chitest1 = 1.0 - gsl_cdf_chisq_P(chi1, df1);

        /* test vs saturated model */
        chi2 = deviance[0];
        df2 = (N * (J - 1)) - (K * (J - 1));
        chitest2 = 1.0 - gsl_cdf_chisq_P(chi2, df2);

        /* significance of individual model parameters */
        for (i = 0; i < K * (J - 1); i++) {
            if (xtwx[i][i] > 0) {
                stderrs[i] = sqrt(xtwx[i][i]);
                wald[i] = pow((beta[i] / stderrs[i]), 2);
                sigprms[i] = 1.0 - gsl_cdf_chisq_P(wald[i], 1);
            }
            else {
                sigprms[i] = -1;
            }
        }

    }   /* end if convergence */

    /***
        Denoument:  Print the results
    ***/

    printout("\n=============================================================\n%s%s",
             "  Maximum Likelihood Estimation of Logistic Regression Model\n",
             "=============================================================\n\n"
    );

    printout("Model Summary\n%s",
             "==============\n");
    print_model(mod);
    printout("Number of populations: %d\n", N);
    printout("Total frequency: %f\n", M);
    printout("Response Levels: %d\n", J);
    printout("Number of columns in X: %d\n", K);

    printout("\nFrequency Table for Dependent Variable\n%s",
               "=======================================\n");
    print_dataset(mod->freqs[mod->numiv], 0, 0);

    printout("\nCrosstabulation of all Model Variables\n%s",
               "=======================================\n");
    print_dataset(mod->xtab, 0, 0);

    printout("\nDesign Matrix (all values rounded)\n%s",
               "===================================\n");
    for (i = 0; i < N; i++) {
        for (j = 0; j < K; j++) {
            printout("%4.0f  ", X[i][j]);
        }
        printout("\n");
    }

    printout("\nModel Results\n%s",
               "==============\n");

    printout("Number of Newton-Raphson iterations: %d\n", iter);
    printout("Convergence: ");
    if (convergence == 1)
        printout("YES\n");
    else
        printout("NO\n");

    if (convergence == 1) {
        printout("\nModel Fit Results\n%s",
                   "==================\n");
        printout("Test 1:  Fitted model vs. intercept-only model\n");
        printout("Initial log likelihood: %f\n", loglike0);
        printout("Final log likelihood:   %f\n", loglike[0]);
        printout("Chisq value: %10.4f, df: %5.0f, Pr(ChiSq): %8.4f\n\n", chi1, df1, chitest1);
        printout("Test 2:  Fitted model vs. saturated model\n");
        printout("Deviance: %f\n", deviance[0]);
        printout("Chisq value: %10.4f, df: %5.0f, Pr(ChiSq): %8.4f\n\n", chi2, df2, chitest2);

    }

    printout("\nMaximum Likelihood Parameter Estimates\n%s",
               "=======================================\n");
    printout("%20s%4s%12s%10s%12s%12s\n",
        "Parameter", "DV", "Estimate", "Std Err", "Wald Chisq", "Pr > Chisq");


    for (i = 0; i < K; i++) {
        for (j = 0; j < (J - 1); j++, q++) {
            printout("%20s%4d%12.8f%10.4f%12.4f%12.4f\n",
                Xlabels[i],
                j,
                beta[j*K+i],
                stderrs[j*K+i],
                wald[j*K+i],
                sigprms[j*K+i]
            );
        }
    }

    return 0;

}



static int newton_raphson (
    double **X,     /* design matrix, N rows by K cols */
    double **Y,     /* response matrix, N rows by J-1 cols */
    double  *n,     /* vector of population counts, N rows */
    int      J,     /* number of discrete values of Y */
    int      N,     /* number of populations */
    int      K,     /* number of columns in X */
    double  *beta0, /* starting parameters, K * J-1 rows */
    double  *beta1, /* parameters after this iteration */
    double **xtwx,
    double  *loglike,
    double  *deviance  ) {


    /* local variable declarations */
    double **pi;
    double  *g;     /* gradient vector: first derivative of ll */
    double **H;     /* Hessian matrix: second derivative of ll */

    double   denom, q1, w1, w2, sum1;
    double  *numer;

    double   devtmp;
    int      ret;
    int i, j, k, jj, kk, jprime, kprime;
    /* end local variable declarations */


    /* setup and memory allocation */
    ret = -1;

    pi = (double **) emalloc(N * sizeof(double *));
    for (i = 0; i < N; i++) {
        pi[i] = (double *) emalloc(J * sizeof(double));
    }

    numer = (double *) emalloc(J * sizeof(double));

    g = (double *) emalloc((K * (J - 1)) * sizeof(double));
    H = (double **) emalloc((K * (J - 1)) * sizeof(double *));

    for (i = 0; i < (K * (J - 1)); i++) {
        H[i] = (double *) emalloc((K * (J - 1)) * sizeof(double));
    }

    /* initializations */
    for (i = 0; i < (K * (J - 1)); i++) {
        g[i] = 0;
        for (j = 0; j < (K * (J - 1)); j++)
            H[i][j] = 0;
    }

    loglike[0] = 0;
    deviance[0] = 0;


    /* main loop for each row (population) in the design matrix */
    for (i = 0; i < N; i++) {

        /* matrix multiplication of one row of X * Beta */

        denom = 1.0;
        jj = 0;

        for (j = 0; j < J - 1; j++) {
            sum1 = 0;
            for (k = 0; k < K; k++)
                sum1 += X[i][k] * beta0[jj++];
            numer[j] = exp(sum1);
            denom += numer[j];
        }

        /* calculate predicted probabilities */
        for (j = 0; j < J - 1; j++)
            pi[i][j] = numer[j] / denom;

        /* omitted category */
        pi[i][j] = 1.0 / denom;

        /* increment log likelihood */
        loglike[0] += gsl_sf_lngamma(n[i] + 1);
        for (j = 0; j < J; j++) {
            loglike[0] = loglike[0] - gsl_sf_lngamma(Y[i][j] + 1) + Y[i][j] * log(pi[i][j]);
        }

        /* increment deviance */
        for (j = 0; j < J; j++) {
            if (Y[i][j] > 0)
                devtmp = 2 * Y[i][j] * log(Y[i][j] / (n[i] * pi[i][j]));
            else
                devtmp = 0;
            deviance[0] += devtmp;
        }

        /* increment first and second derivatives */
        for (j = 0, jj = 0; j < J - 1; j++) {

            /* terms for first derivative, see Eq. 32 */
            q1 = Y[i][j] - n[i] * pi[i][j];

            /* terms for second derivative, see Eq. 37 */
            w1 = n[i] * pi[i][j] * (1 - pi[i][j]);

            for (k = 0; k < K; k++) {

                /* first derivative term in Eq. 23 */
                g[jj] += q1 * X[i][k];

                /* increment the current pop's contribution to the 2nd derivative */

                /* jprime = j (see Eq. 37) */

                kk = jj - 1;
                for (kprime = k; kprime < K; kprime++) {
                    kk += 1;
                    H[jj][kk] += w1 * X[i][k] * X[i][kprime];
                    H[kk][jj] = H[jj][kk];
                }

                /* jprime != j (see Eq. 37) */

                for (jprime = j + 1; jprime < J - 1; jprime++) {
                    w2 = -n[i] * pi[i][j] * pi[i][jprime];
                    for (kprime = 0; kprime < K; kprime++) {
                        kk += 1;
                        H[jj][kk] += w2 * X[i][k] * X[i][kprime];
                        H[kk][jj] = H[jj][kk];
                    }
                }
                jj++;
            }
        }
    } /* end loop for each row in design matrix */


    /* compute xtwx * beta0 + x(y-mu) (see Eq. 40) */
    for (i = 0; i < K * (J - 1); i++) {
        sum1 = 0;
        for (j = 0; j < K * (J - 1); j++)
            sum1 += H[i][j] * beta0[j];
        g[i] += sum1;
    }

    /* invert xtwx */
    if (cholesky(H, K * (J - 1))) return 11;
    if (backsub(H, K * (J - 1))) return 12;
    if (trimult(H, xtwx, K * (J - 1))) return 13;

    /* solve for new betas */
    for (i = 0; i < K * (J - 1); i++) {
        sum1 = 0;
        for (j = 0; j < K * (J - 1); j++) {
            sum1 += xtwx[i][j] * g[j];
        }
        beta1[i] = sum1;
    }


    /* free local memory */
    free(numer);
    free(g);
    for (i = 0; i < N; i++)
        free(pi[i]);
    free(pi);
    for (i = 0; i < K * (J - 1); i++)
        free(H[i]);
    free(H);

    return 0;
}


static int cholesky(double **x, int order) {

    int i, j, k;
    double sum;
    int ret = 0;

    for (i = 0; i < order; i++) {
        sum = 0;
        for (j = 0; j < i; j++)
            sum += x[j][i] * x[j][i];
        if (sum >= x[i][i]) {
            ret = 1;
            return ret;
        }
        x[i][i] = sqrt(x[i][i] - sum);
        for (j = i + 1; j < order; j++) {
            sum = 0;
            for (k = 0; k < i; k++)
                sum += x[k][i] * x[k][j];
            x[i][j] = (x[i][j] - sum) / x[i][i];
        }
    }

    return ret;
}

static int backsub(double **x, int order) {

    int i, j, k;
    double sum;

    if (x[0][0] == 0) return 1;

    x[0][0] = 1 / x[0][0];
    for (i = 1; i < order; i++) {
        if (x[i][i] == 0) return 1;
        x[i][i] = 1 / x[i][i];
        for (j = 0; j < i; j++) {
            sum = 0;
            for (k = j; k < i; k++)
                sum += x[j][k] * x[k][i];
            x[j][i] = -sum * x[i][i];
        }
    }

    return 0;
}

static int trimult(double **in, double **out, int order) {

    int i, j, k, m;
    double sum;

    for (i = 0; i < order; i++) {
        for (j = 0; j < order; j++) {
            sum = 0;
            if (i > j)
                m = i;
            else
                m = j;
            for (k = m; k < order; k++)
                sum += in[i][k] * in[j][k];
            out[i][j] = sum;
        }
    }

    return 0;
}
