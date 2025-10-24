/*
 * acoustic_constants.c
 * Implementation of acoustic constants initialization
 */

#include <math.h>
#include "acoustic_constants.h"

void init_acoustic_constants(acoustic_constants* ac, double temperature) {
    /* Calculate speed of sound (m/s) */
    ac->c0 = 331.45 * sqrt(temperature / 273.16 + 1);

    /* Calculate air density (kg/m^3) */
    ac->rho = 1.2929 * (273.16 / (273.16 + temperature));

    /* Calculate acoustic impedance */
    ac->rhoc0 = ac->rho * ac->c0;

    /* Calculate kinematic viscosity (m^2/s) */
    double mu = (18.2 + 0.0456 * (temperature - 25)) * 1.0e-6;
    ac->nu = mu / ac->rho;
}

void init_acoustic_constants_default(acoustic_constants* ac, double temperature) {
    /* Initialize physical constants */
    init_acoustic_constants(ac, temperature);

    /* Set default configuration flags */
    ac->rad_calc = PIPE;
    ac->dump_calc = WALL;
    ac->sec_var_calc = FALSE;
}
