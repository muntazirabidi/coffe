/*
 * This file is part of COFFE
 * Copyright (C) 2019 Goran Jelic-Cizmek
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <gsl/gsl_integration.h>
#include <gsl/gsl_sf_bessel.h>
#include <gsl/gsl_spline2d.h>
#include <gsl/gsl_errno.h>

#ifdef HAVE_DOUBLE_EXPONENTIAL
#include "tanhsinh.h"
#endif

#include "common.h"
#include "background.h"
#include "integrals.h"
#include "twofast.h"


#ifndef NORM
#define NORM(X) (X*COFFE_H0)
#endif

/**
    parameters for the integrand of the form P(k) k^2 j_l(k r)/(k r)^n
**/

struct integrals_params
{
    int n, l;
    double r;
    struct coffe_interpolation result;
};


/**
    list of separations (in Mpc/h!) for the integral I^4_0 so it doesn't need to rely
    on user input
**/

static const double coffe_sep[] = {
    0.000001, 0.000005, 0.0001, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.4, 0.8, 1., 1.5,
    2., 3., 4., 5., 6., 7., 8., 9., 10., 11., 12., 13., 14., 15., 16., 17., 18., 19., 20.,
    21., 22., 23., 24., 25., 26., 27., 28., 29., 30., 32., 34., 36., 38., 40., 42., 44., 46.,
    48., 50., 52., 54., 56., 58., 60., 62., 64., 66., 68., 70., 72., 74., 76., 78., 80., 81., 82., 83.,
    84., 85., 86., 87., 88., 89., 90., 91., 92., 93., 94., 95., 95.5, 96., 96.5, 97., 97.5, 98.,
    98.5, 99., 99.5, 100., 100.5, 101., 101.5, 102., 102.5, 103., 103.5, 104., 104.5, 105.,
    106., 107., 108., 109., 110., 112., 114., 116., 118., 120., 124., 128., 132., 136., 140.,
    144., 148., 152., 156., 160., 164., 168., 172., 176., 180., 185., 190., 195., 200., 205.,
    210., 215., 220., 225., 230., 235., 240., 250., 260., 270., 280., 290., 300., 320., 340.,
    360., 380., 400., 420., 440., 460., 480., 500., 520., 540., 560., 580., 600., 620., 640.,
    660., 680., 700., 750., 800, 850, 900, 950, 1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350,
    1400, 1450, 1500, 1550, 1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950, 2000, 2050, 2100,
    2150, 2200, 2250, 2300, 2350, 2400, 2450, 2500, 2550, 2600, 2650, 2700, 2750, 2800, 2850,
    2900, 2950, 3000, 3050, 3100, 3150, 3200, 3250, 3300, 3350, 3400, 3450, 3500, 3550, 3600,
    3650, 3700, 3750, 3800, 3850, 3900, 3950, 4000, 4050, 4100, 4150, 4200, 4250, 4300, 4350,
    4400, 4450, 4500, 4550, 4600, 4650, 4700, 4750, 4800, 4850, 4900, 4950, 5000, 5050, 5100,
    5150, 5200, 5250, 5300, 5350, 5400, 5450, 5500, 5550, 5600, 5650, 5700, 5750, 5800, 5850,
    5900, 5950, 6000, 6050, 6100, 6150, 6200, 6250, 6300, 6350, 6400, 6450, 6500, 6550, 6600,
    7000, 8000, 9000, 10000, 11000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000
};


/**
    length of the above
**/

static const size_t coffe_sep_len = sizeof(coffe_sep)/sizeof(coffe_sep[0]);


/**
    list of separations below 1 Mpc/h for which we need to evaluate
    the convergent integrals
**/

static const double min_sep[] = {
    NORM(1E-6), NORM(1E-5), NORM(1E-4),
    NORM(1E-3), NORM(2E-3), NORM(5E-3),
    NORM(7E-3), NORM(8E-3), NORM(9E-3),
    NORM(1E-2), NORM(1.1E-2), NORM(1.2E-2),
    NORM(1.25E-2), NORM(1.3E-2), NORM(1.35E-2),
    NORM(1.5E-2), NORM(2E-2), NORM(2.5E-2),
    NORM(3E-2), NORM(3.5E-2), NORM(5E-2),
    NORM(7E-2), NORM(8E-2), NORM(9E-2),
    NORM(1E-1), NORM(1.1E-1), NORM(1.2E-1),
    NORM(1.25E-1), NORM(1.3E-1), NORM(1.35E-1),
    NORM(1.5E-1), NORM(1.75E-1), NORM(2E-1), NORM(2.5E-1),
    NORM(3E-1), NORM(3.5E-1), NORM(4E-1),
    NORM(5E-1), NORM(6E-1),
    NORM(7E-1), NORM(8E-1), NORM(9E-1)
};


/**
    length of the above
**/

static const size_t min_sep_len = sizeof(min_sep) / sizeof(min_sep[0]);


/**
    coefficients for the r -> 0 limit
**/

static double integrals_coefficients(int l)
{
    switch (l){
        case 0:
            return 1;
        case 1:
            return 1./3.;
        case 2:
            return 1./15.;
        case 3:
            return 1./105.;
        case 4:
            return 1./945.;
        default:
            return 0;
    }
}


/**
    structure needed for integration of renormalization term
**/

struct integrals_divergent_params
{
    struct coffe_interpolation result;
    double chi1, chi2;
};


/**
    integrand k^(2 + l - n)*P(k)
**/

static double integrals_prefactor(double k, void *p)
{
    struct integrals_params *test = (struct integrals_params *) p;
    return pow(k, 2 + test->l - test->n)*coffe_interp_spline(&test->result, k);
}


/**
    integrand k^2 P(k) j_l(k r)/(k r)^n
**/

static double integrals_bessel_integrand(
    double k,
#ifdef HAVE_DOUBLE_EXPONENTIAL
    const void *p
#else
    void *p
#endif
)
{
    struct integrals_params *integrand = (struct integrals_params *) p;
    double result;

    if (integrand->n == 0){
        switch (integrand->l){
            case 0:
                result = k*k
                   *coffe_interp_spline(&integrand->result, k)
                   *gsl_sf_bessel_j0(k*integrand->r);
                break;
            case 2:
                result = k*k
                   *coffe_interp_spline(&integrand->result, k)
                   *gsl_sf_bessel_j2(k*integrand->r);
                break;
            default:
                result = k*k
                   *coffe_interp_spline(&integrand->result, k)
                   *gsl_sf_bessel_jl(integrand->l, k*integrand->r);
                break;
        }
    }
    else{
        result = k*k
           *coffe_interp_spline(&integrand->result, k)
           *gsl_sf_bessel_jl(integrand->l, k* integrand->r)
           /pow(k*integrand->r, integrand->n);
    }
    return result;
}


/**
    integrates the above
**/

static double integrals_bessel(
    struct coffe_interpolation result,
    int n, int l, double sep,
    double kmin, double kmax
)
{
    struct integrals_params test;
    test.result.spline = result.spline;
    test.result.accel = result.accel;
    test.n = n;
    test.l = l;
    test.r = sep;

    double output, error;

#ifdef HAVE_DOUBLE_EXPONENTIAL
    output = tanhsinh_quad(
        &integrals_bessel_integrand,
        &test,
        kmin, kmax, 0.,
        &error, NULL
    );
#else

    double precision = 1E-5;
    gsl_function integrand;
    integrand.params = &test;
    integrand.function = &integrals_bessel_integrand;


    gsl_integration_workspace *wspace =
        gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);

    /* r^4 I^4_0 */
    gsl_integration_qag(
        &integrand, kmin, kmax, 0,
        precision, COFFE_MAX_INTSPACE,
        GSL_INTEG_GAUSS61, wspace,
        &output, &error
    );

    gsl_integration_workspace_free(wspace);
#endif

    return output*pow(sep, 4)/2./M_PI/M_PI;
}


/**
    renormalized integrand of I^4_0 at r = 0
**/

static double integrals_renormalization0_integrand(
    double k,
#ifdef HAVE_DOUBLE_EXPONENTIAL
    const void *p
#else
    void *p
#endif
)
{
    struct integrals_params *integrand = (struct integrals_params *) p;
    return coffe_interp_spline(&integrand->result, k)
        *(1. - pow(gsl_sf_bessel_j0(k*integrand->r), 2))/k/k;
}


/**
    integrates the above
**/

static double integrals_renormalization0(
    struct coffe_interpolation result,
    int n, int l, double sep,
    double kmin, double kmax
)
{
    struct integrals_params test;
    test.result.spline = result.spline;
    test.result.accel = result.accel;
    test.n = n;
    test.l = l;
    test.r = sep;

    double output, error;

#ifdef HAVE_DOUBLE_EXPONENTIAL
    output = tanhsinh_quad(
        &integrals_renormalization0_integrand,
        &test,
        kmin, kmax, 0.,
        &error, NULL
    );
#else

    double precision = 1E-5;
    gsl_function integrand;
    integrand.params = &test;
    integrand.function = &integrals_renormalization0_integrand;

    gsl_integration_workspace *wspace =
        gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);

    /* renormalized term at r = 0 (depends on chi!) */
    gsl_integration_qag(
        &integrand, kmin, kmax, 0,
        precision, COFFE_MAX_INTSPACE,
        GSL_INTEG_GAUSS61, wspace,
        &output, &error
    );

    gsl_integration_workspace_free(wspace);
#endif

    return output/2./M_PI/M_PI;
}


/**
    integrand of the renormalization term (the divergent one)
**/

static double integrals_renormalization_integrand(
    double k,
#ifdef HAVE_DOUBLE_EXPONENTIAL
    const void *p
#else
    void *p
#endif
)
{
    struct integrals_divergent_params *integrand =
        (struct integrals_divergent_params *) p;
    return coffe_interp_spline(&integrand->result, k)
           *gsl_sf_bessel_j0(k*integrand->chi1)
           *gsl_sf_bessel_j0(k*integrand->chi2)/k/k;
}


/**
    integrates the above
**/

static double integrals_renormalization(
    struct coffe_interpolation result,
    double chi1, double chi2,
    double kmin, double kmax
)
{
    struct integrals_divergent_params test;
    test.result.spline = result.spline;
    test.result.accel = result.accel;
    test.chi1 = chi1;
    test.chi2 = chi2;

    double output, error;

#ifdef HAVE_DOUBLE_EXPONENTIAL
    output = tanhsinh_quad(
        &integrals_renormalization_integrand,
        &test,
        kmin, kmax, 0.,
        &error, NULL
    );
#else

    double precision = 1E-5;
    gsl_function integrand;
    integrand.params = &test;
    integrand.function = &integrals_renormalization_integrand;

    gsl_integration_workspace *wspace =
        gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);

    gsl_integration_qag(
        &integrand, kmin, kmax, 0,
        precision, COFFE_MAX_INTSPACE,
        GSL_INTEG_GAUSS61, wspace, &output, &error
    );

    gsl_integration_workspace_free(wspace);
#endif

    return output/2./M_PI/M_PI;
}


static double integrals_flatsky_integrand(
    double k,
    void *p
)
{
    struct integrals_params *integrand = (struct integrals_params *) p;

        /* k^2 */
    return k * k
        /* P(k) */
       * coffe_interp_spline(&integrand->result, k)
        /* J0(k * r) */
       * gsl_sf_bessel_J0(k * integrand->r)
        /* k * r */
       / (k * integrand->r);
}


/**
    integrates the above
**/

static double integrals_flatsky(
    struct coffe_interpolation result,
    double sep,
    double kmin,
    double kmax
)
{
    struct integrals_params test;
    test.result.spline = result.spline;
    test.result.accel = result.accel;
    test.r = sep;

    gsl_function integrand;
    integrand.params = &test;
    integrand.function = &integrals_flatsky_integrand;

    double output, error, precision = 1E-5;

    gsl_integration_workspace *wspace =
        gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);

    gsl_integration_qag(
        &integrand, kmin, kmax, 0,
        precision, COFFE_MAX_INTSPACE,
        GSL_INTEG_GAUSS61, wspace,
        &output, &error
    );

    gsl_integration_workspace_free(wspace);

    return output;
}

/**
    computes the integrals I^n_l(r).
    If n >= l, returns the combination I^n_l(r) * r^(n - l).
**/
int coffe_integrals_renormalizable(
    double *output_x,
    double *output_y,
    const size_t output_len,
    const struct coffe_interpolation *spectrum,
    const int l,
    const int n,
    const double x_min,
    const double x_max
)
{
    /* do the FFTlog transform first */
    double *fft_x = coffe_malloc(sizeof(double) * output_len);
    double *fft_y = coffe_malloc(sizeof(double) * output_len);
    twofast_1bessel(
        fft_x,
        fft_y,
        output_len,
        spectrum->spline->x,
        spectrum->spline->y,
        spectrum->spline->size,
        l,
        n,
        COFFE_H0,
        x_min,
        x_min,
        x_max,
        0
    );

    double x0_result;

    if (n >= l){
        for (size_t i = 0; i < output_len; ++i){
            fft_y[i] *= pow(fft_x[i], n - l); // r^(n - l) * I^n_l(r)
        }
        double result_limit, error_limit;
        {
            struct integrals_params test;
            test.result.spline = spectrum->spline;
            test.result.accel = spectrum->accel;
            test.n = n;
            test.l = l;

            gsl_integration_workspace *space =
                gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);

            gsl_function integrand;
            integrand.function = &integrals_prefactor;
            integrand.params = &test;

            const double precision = 1E-5;
            gsl_integration_qag(
                &integrand,
                x_min,
                x_max,
                0,
                precision,
                COFFE_MAX_INTSPACE,
                GSL_INTEG_GAUSS61,
                space,
                &result_limit,
                &error_limit
            );
            gsl_integration_workspace_free(space);
        }
        x0_result = result_limit * integrals_coefficients(l) / 2. / M_PI / M_PI;
    }
    else{
        x0_result = 0.0;
    }
    const double x0 = 0.0;

    struct integrals_params test;
    test.result.spline = spectrum->spline;
    test.result.accel = spectrum->accel;
    test.n = n;
    test.l = l;

    double temp_result, temp_error;

#ifndef HAVE_DOUBLE_EXPONENTIAL
    const double precision = 1E-5;
    gsl_function integrand;
    integrand.function = &integrals_bessel_integrand;
    integrand.params = &test;

    gsl_integration_workspace *wspace =
        gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);
#endif

    output_x[0] = x0;
    output_y[0] = x0_result;

    /* setting the output for the smallest separations (under 1 Mpc/h) */
    for (size_t i = 1; i <= min_sep_len; ++i){
        test.r = min_sep[i - 1];
        output_x[i] = min_sep[i - 1]; // dimensionless!
#ifdef HAVE_DOUBLE_EXPONENTIAL
        temp_result = tanhsinh_quad(
            &integrals_bessel_integrand,
            &test,
            x_min,
            x_max,
            0.,
            &temp_error,
            NULL
        );
#else
        gsl_integration_qag(
            &integrand,
            x_min,
            x_max,
            0,
            precision,
            COFFE_MAX_INTSPACE,
            GSL_INTEG_GAUSS61,
            wspace,
            &temp_result,
            &temp_error
        );
#endif
        if (n > l){
            output_y[i] = pow(output_x[i], n - l) * temp_result / 2. / M_PI / M_PI;
        }
        else{
            output_y[i] = temp_result / 2. / M_PI / M_PI;
        }
    }

#ifndef HAVE_DOUBLE_EXPONENTIAL
    gsl_integration_workspace_free(wspace);
#endif

    for (size_t i = min_sep_len + 1; i < output_len + min_sep_len + 1; ++i){
        output_x[i] = fft_x[i - min_sep_len - 1];
        output_y[i] = fft_y[i - min_sep_len - 1];
    }

    free(fft_x);
    free(fft_y);

    return EXIT_SUCCESS;
}


/**
    computes all the nonzero I^n_l integrals
**/

int coffe_integrals_init(
    const struct coffe_parameters_t *par,
    const struct coffe_background_t *bg,
    struct coffe_integrals_t *integral
)
{
    clock_t start, end;
    start = clock();

    if (par->verbose)
        printf("Calculating integrals of Bessel functions...\n");

    gsl_error_handler_t *default_handler =
        gsl_set_error_handler_off();

    for (int j = 0; j<9; ++j){
        if (par->nonzero_terms[j].n != -1 && par->nonzero_terms[j].l != -1){
            const int n = par->nonzero_terms[j].n;
            const int l = par->nonzero_terms[j].l;
            if (n == 4 && l == 0){
                double *result =
                    (double *)coffe_malloc(sizeof(double)*coffe_sep_len);
                double *separations =
                    (double *)coffe_malloc(sizeof(double)*coffe_sep_len);

                double *result0 =
                    (double *)coffe_malloc(sizeof(double)*coffe_sep_len);

                #pragma omp parallel for num_threads(par->nthreads)
                for (size_t i = 0; i<coffe_sep_len; ++i){
                    separations[i] = NORM(coffe_sep[i]); // dimensionless!

                    result[i] = integrals_bessel(
                        par->power_spectrum_norm,
                        n, l, separations[i],
                        par->k_min_norm, par->k_max_norm
                    );

                    result0[i] = integrals_renormalization0(
                        par->power_spectrum_norm,
                        n, l, separations[i],
                        par->k_min_norm, par->k_max_norm
                    );

                }
                coffe_init_spline(
                    &(integral[j].result),
                    separations,
                    result,
                    coffe_sep_len,
                    par->interp_method
                );
                separations[0] = 0.0;
                result0[0] = 0.0;
                coffe_init_spline(
                    &integral[j].renormalization0,
                    separations,
                    result0,
                    coffe_sep_len,
                    par->interp_method
                );
                free(separations);
                free(result);
                free(result0);

                const size_t nbins = 200;
                double *result2d = (double *)coffe_malloc(sizeof(double)*(nbins + 1)*(nbins + 1));

                double chi_min = 0.;
                double chi_max;
                if (par->output_type == 0){
                    chi_max = coffe_interp_spline(&bg->comoving_distance, par->z_mean);
                }
                else if (par->output_type == 1 || par->output_type == 2){
                    chi_max = coffe_interp_spline(&bg->comoving_distance, par->z_mean + par->deltaz); // dimensionless
                }
                else if (par->output_type == 3){
                    chi_max = coffe_interp_spline(&bg->comoving_distance, par->z_max); // dimensionless
                }
                else if (par->output_type == 6){
                    chi_max = coffe_interp_spline(&bg->comoving_distance, par->z_mean) + 300.*COFFE_H0;
                }
                else{
                    chi_max = 0.;
                }
                double *chi_array = (double *)coffe_malloc(sizeof(double)*(nbins + 1));
                for (size_t i = 0; i<=nbins; ++i){
                    chi_array[i] = (chi_min + (double)i/nbins*(chi_max - chi_min)); // dimensionless
                }

                #pragma omp parallel for num_threads(par->nthreads) collapse(2)
                for (size_t i = 0; i<=nbins; ++i){
                    for (size_t k = 0; k<=nbins; ++k){
                        result2d[k*(nbins + 1) + i] = integrals_renormalization(
                            par->power_spectrum_norm,
                            chi_array[i], chi_array[k],
                            par->k_min_norm, par->k_max_norm
                        );
                    }
                }

                integral[8].renormalization.spline =
                    gsl_spline2d_alloc(
                        gsl_interp2d_bicubic,
                        (nbins + 1), (nbins + 1)
                    );
                integral[8].renormalization.xaccel =
                    gsl_interp_accel_alloc();
                integral[8].renormalization.yaccel =
                    gsl_interp_accel_alloc();

                gsl_spline2d_init(
                    integral[8].renormalization.spline,
                    chi_array, chi_array, result2d,
                    (nbins + 1), (nbins + 1)
                );

                free(chi_array);
                free(result2d);
            }
            else{
                /* if integral is not divergent, use implementation of 2FAST */
                const size_t npoints = par->bessel_bins;
                double *final_sep =
                    (double *)coffe_malloc(sizeof(double) * (npoints + min_sep_len + 1));

                double *final_result =
                    (double *)coffe_malloc(sizeof(double) * (npoints + min_sep_len + 1));

                coffe_integrals_renormalizable(
                    final_sep,
                    final_result,
                    npoints,
                    &par->power_spectrum_norm,
                    l,
                    n,
                    par->k_min_norm,
                    par->k_max_norm
                );

                coffe_init_spline(
                    &(integral[j].result),
                    final_sep,
                    final_result,
                    npoints,
                    par->interp_method
                );
                free(final_sep);
                free(final_result);
            }
            integral[j].n = n, integral[j].l = l;
            integral[j].flag = 1;
        }
    }
    if (par->flatsky_density_lensing || par->flatsky_lensing_lensing){
        const size_t npoints = (size_t)par->bessel_bins;
        double *sep =
            (double *)coffe_malloc(sizeof(double) * npoints);
        double *result =
            (double *)coffe_malloc(sizeof(double) * npoints);
        twofast_1bessel(
            sep,
            result,
            npoints,
            par->power_spectrum_norm.spline->x,
            par->power_spectrum_norm.spline->y,
            par->power_spectrum_norm.spline->size,
            -0.5,
            0.5,
            COFFE_H0,
            par->k_min_norm,
            par->k_min_norm,
            par->k_max_norm,
            0
        );

        /* we're off by a factor sqrt(2 / pi) * 2 * pi^2, so we rescale first */
        /* note that we're actually computing r * I(r) since that's well defined at r = 0 */
        for (size_t i = 0; i < npoints; ++i)
            result[i] *= sep[i] * sqrt(2. / M_PI) * 2 * M_PI * M_PI;

        double *final_sep =
            (double *)coffe_malloc(sizeof(double) * (npoints + min_sep_len + 1));

        double *final_result =
            (double *)coffe_malloc(sizeof(double) * (npoints + min_sep_len + 1));

        double result_limit, error_limit;
        {
            struct integrals_params test;
            test.result.spline = par->power_spectrum_norm.spline;
            test.result.accel = par->power_spectrum_norm.accel;
            test.n = 1;
            test.l = 0;

            const double x_min = par->k_min_norm;
            const double x_max = par->k_max_norm;

            gsl_integration_workspace *space =
                gsl_integration_workspace_alloc(COFFE_MAX_INTSPACE);

            gsl_function integrand;
            integrand.function = &integrals_prefactor;
            integrand.params = &test;

            const double precision = 1E-5;
            gsl_integration_qag(
                &integrand,
                x_min,
                x_max,
                0,
                precision,
                COFFE_MAX_INTSPACE,
                GSL_INTEG_GAUSS61,
                space,
                &result_limit,
                &error_limit
            );
            gsl_integration_workspace_free(space);
        }

        final_sep[0] = 0.0;
        final_result[0] = result_limit;

        /* I think this part doesn't need parallelization */
        //#pragma omp parallel for num_threads(par->nthreads)
        for (size_t i = 1; i <= min_sep_len; ++i){
            final_sep[i] = min_sep[i - 1];
            final_result[i] =
                final_sep[i]
              * integrals_flatsky(
                    par->power_spectrum_norm,
                    final_sep[i],
                    par->k_min_norm,
                    par->k_max_norm
                );
        }

        for (size_t i = min_sep_len + 1; i < min_sep_len + npoints + 1; ++i){
            final_sep[i] = sep[i - min_sep_len - 1];
            final_result[i] = result[i - min_sep_len - 1];
        }
        coffe_init_spline(
            &(integral[9].result),
            final_sep,
            final_result,
            npoints + min_sep_len + 1,
            par->interp_method
        );
        free(final_sep);
        free(final_result);
        free(sep);
        free(result);

        integral[9].flag = 1;
    }

    gsl_set_error_handler(default_handler);
    end = clock();

    if (par->verbose)
        printf("Integrals of Bessel functions calculated in %.2f s\n",
            (double)(end - start) / CLOCKS_PER_SEC);

    return EXIT_SUCCESS;
}

int coffe_integrals_free(
    struct coffe_integrals_t integral[]
)
{
    for (size_t i = 0; i<8; ++i){
        if (integral[i].flag){
            coffe_free_spline(&integral[i].result);
            integral[i].flag = 0;
        }
    }
    if (integral[8].n == 4 && integral[8].l == 0 && integral[8].flag){
        coffe_free_spline(&integral[8].result);
        coffe_free_spline(&integral[8].renormalization0);
        gsl_spline2d_free(integral[8].renormalization.spline);
        gsl_interp_accel_free(integral[8].renormalization.xaccel);
        gsl_interp_accel_free(integral[8].renormalization.yaccel);
        integral[8].flag = 0;
    }
    // code for flatsky
    if (integral[9].n == -10 && integral[9].l == -10 && integral[9].flag){
        coffe_free_spline(&integral[9].result);
        integral[9].flag = 0;
    }
    return EXIT_SUCCESS;
}

#undef NORM
