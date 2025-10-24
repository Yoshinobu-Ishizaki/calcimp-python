/*
 * cephes.h - Header for Cephes math library functions used in calcimp
 */

#ifndef _CEPHES_H_
#define _CEPHES_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Bessel function of order one (renamed to avoid conflicts with system libraries) */
double bessel_j1(double x);

/* Struve function */
double struve(double v, double x);

#ifdef __cplusplus
}
#endif

#endif /* _CEPHES_H_ */
