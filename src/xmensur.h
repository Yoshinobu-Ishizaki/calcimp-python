/*
 * xmensur.h - XMENSUR format parser header
 * Parses XMENSUR format (.xmen files) and creates mensur structures
 */

#ifndef _XMENSUR_H_
#define _XMENSUR_H_

#include "zmensur.h"

/* Main function to read XMENSUR format file */
mensur* read_xmensur(const char *path);

#endif /* _XMENSUR_H_ */
