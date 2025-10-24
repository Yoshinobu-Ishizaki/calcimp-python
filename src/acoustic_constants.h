/*
 * acoustic_constants.h
 * Context structure for acoustic constants and configuration
 * Thread-safe replacement for global variables
 */

#ifndef ACOUSTIC_CONSTANTS_H
#define ACOUSTIC_CONSTANTS_H

/* Configuration constants for calculation modes */
#define NONE 0
#define PIPE 1
#define BUFFLE 2
#define WALL 3

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/**
 * Acoustic constants and configuration context
 * This structure holds all temperature-dependent physical constants
 * and configuration flags that affect impedance calculations.
 */
typedef struct {
    /* Physical constants (temperature-dependent) */
    double c0;      /* speed of sound (m/s) */
    double rho;     /* density of air (kg/m^3) */
    double rhoc0;   /* acoustic impedance of air: rho * c0 */
    double nu;      /* kinematic viscosity coefficient (m^2/s) */

    /* Configuration flags */
    int rad_calc;      /* radiation impedance calculation mode: NONE, PIPE, BUFFLE */
    int dump_calc;     /* damping calculation mode: NONE, WALL */
    int sec_var_calc;  /* section variation calculation flag: TRUE, FALSE */
} acoustic_constants;

/**
 * Initialize acoustic constants from temperature
 *
 * @param ac Pointer to acoustic_constants structure to initialize
 * @param temperature Temperature in Celsius
 */
void init_acoustic_constants(acoustic_constants* ac, double temperature);

/**
 * Create acoustic constants with default configuration
 * Same as init_acoustic_constants but also sets default flags:
 * - rad_calc = PIPE
 * - dump_calc = WALL
 * - sec_var_calc = FALSE
 *
 * @param ac Pointer to acoustic_constants structure to initialize
 * @param temperature Temperature in Celsius
 */
void init_acoustic_constants_default(acoustic_constants* ac, double temperature);

#endif /* ACOUSTIC_CONSTANTS_H */
