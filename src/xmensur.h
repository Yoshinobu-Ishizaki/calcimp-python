/*
 * xmensur.h - XMENSUR format parser header
 * Parses XMENSUR format (.xmen files) and creates mensur structures
 */

#ifndef _XMENSUR_H_
#define _XMENSUR_H_

#include "zmensur.h"

/* defines reserved keywords and their flags */
enum{ MAIN = 1, END_MAIN, GROUP, END_GROUP, INSERT, SPLIT, BRANCH, MERGE };

/* Main function to read XMENSUR format file */
mensur* read_xmensur(const char *path);

#endif /* _XMENSUR_H_ */
