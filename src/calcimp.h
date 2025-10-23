/*
 * calcimp.h
 * header file for calcimp
 * $Id: calcimp.h 3 2013-08-03 07:37:16Z yoshinobu $
 */

#ifndef _CALCIMP_
#define _CALCIMP_

#ifndef DOMAIN
#include "mconf.h" /* use cephes math library */
#endif
/*  #include "../mensur/mensur.h" */

struct spd {
    double x;
    double spd_r;
    double spd_i;
};

struct spdlist {
    unsigned int count;
    struct spd* data;
};


/*-------------------- prototypes --------------------*/

void initialize(void);
int parse_opt(int argc, char **argv);
int main(int argc, char **argv);

#endif /* _CALCIMP_ */
