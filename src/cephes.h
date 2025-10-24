/*
 * cephes.h - Declarations for external Cephes library functions
 *
 * This header declares functions from the external Cephes libmd.a library.
 * The library should be located at CEPHES_PATH (default: ../cephes_lib/)
 */

#ifndef _CEPHES_H_
#define _CEPHES_H_

/* Include mconf.h from external Cephes library */
#include "mconf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bessel function of order one (from Cephes libmd.a) */
double j1(double x);

/* Struve function (from Cephes libmd.a) */
double struve(double v, double x);

#ifdef __cplusplus
}
#endif

#endif /* _CEPHES_H_ */
