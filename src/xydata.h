#ifndef _XYDATA_
#define _XYDATA_

#include "kutils.h"

struct xy {
    double x,y;
    char comment[BUFSIZE];
};

struct xylist {
  unsigned int count;
  char cmtstr[1024];
  struct xy *data;
};

/*************************prototypes*************************/
/* xydata.c */
struct xy *xy_new( double x, double y, char* s );
struct xylist *create_xylist(unsigned int  num);
void resize_xylist(struct xylist *lst, int num);
struct xylist* read_xy( char* inpath, int with_comment );
void sort_xy( struct xylist *inxy );
void print_xy(struct xylist *xy);
void dispose_xy(struct xylist *inxy);
void yhalf_xy( struct xylist* xy );

#endif /* _XYDATA_ */
