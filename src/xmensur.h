/*
 * xmensur.h - XMENSUR format parser header
 * Parses XMENSUR format (.xmen files) and creates mensur structures
 */

#ifndef _XMENSUR_H_
#define _XMENSUR_H_

#include "zmensur.h"

/* defines reserved keywords and their flags for parsing */
/* Note: SPLIT and JOIN are already defined in zmensur.h as connection types */
/* These are marker types for parsing only */
enum{ XMAIN = 1, XEND_MAIN, XGROUP, XEND_GROUP, XINSERT, XSPLIT, XBRANCH, XMERGE };

/* terminator */
enum{ XOPEN_END=0, XCLOSED_END };

/* Main function to read XMENSUR format file */
mensur* read_xmensur(const char *path);

#endif /* _XMENSUR_H_ */
