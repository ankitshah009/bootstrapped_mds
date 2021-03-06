#include <omp.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "mds_pertubations.h"

#define ERROR_BUF_SZ 2000
#define DBL_MAX 1.79769e+308
double error_buf[ERROR_BUF_SZ] = {DBL_MAX};
/*
 * Max number of dimensions to reduce to is 1000
 */

double
single_pertub_error(double* d_current, double* d_goal,
                    double* xs, int row, int pertub_dim,
                    int x_rows, int x_cols, double step)
{
    double error = 0;
    int ll;
    int d_idx = x_rows * row;
    int x_idx = x_cols * row;

    //#pragma omp parallel for reduction (+:error)
    for(ll = 0; ll < x_rows; ll++)
    {
        double d_prev, before, after, diff1, diff;
        if(row != ll)
        {
            d_prev = d_current[d_idx + ll] * d_current[d_idx + ll];
            diff1 = (xs[x_idx + pertub_dim] - xs[ll * x_cols + pertub_dim]);
            before = diff1 * diff1;
            after = (diff1 + step) * (diff1 + step);
            diff = d_goal[d_idx + ll] - sqrt(d_prev - before + after);
            error += diff * diff;
        }
    }
    return error;
}


pertub_res
min_pertub_error(double* xs, double radius, double* d_current,
                 double* d_goal,
                 double* bs_prob,
                 int ii, double a_bs, double prob_thresh,
                 int x_rows, int x_cols,
                 double percent, int n_jobs)
{
    int jj;
    int min_jj = 0;
    struct pertub_res optimum;
    int n_coords = 2 * x_cols;
    int p_idx = ii*n_coords;
    optimum.error = DBL_MAX;

#pragma omp parallel num_threads(n_jobs)
    {
        srand((int)time(NULL) ^ omp_get_thread_num());
#pragma omp for nowait
        for(jj=0; jj < n_coords; jj++)
        {            
            if ((double)rand() /
                (double)((unsigned)RAND_MAX + 1) > bs_prob[p_idx + jj])
            {
                error_buf[jj] = DBL_MAX;
                continue;
            }
            double step = jj < x_cols ? radius : -radius;
            error_buf[jj] = single_pertub_error(
                d_current, d_goal, xs, ii, jj % x_cols,
                x_rows, x_cols, step);
        }
    }

    for(jj=0; jj < n_coords; jj++) {
        if(error_buf[jj] < optimum.error) {
            min_jj = jj;
            optimum.error = error_buf[min_jj];
            bs_prob[p_idx + jj] -= bs_prob[p_idx + jj] < prob_thresh ?
            0. : a_bs;
        }
    }

    optimum.k = min_jj % x_cols;
    optimum.step = min_jj < x_cols ? radius : -radius;
    optimum.coord_ind = min_jj;

    bs_prob[p_idx + min_jj] += 2 * a_bs;

    return optimum;
}