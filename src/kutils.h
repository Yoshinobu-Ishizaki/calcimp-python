#ifndef _KUTILS_
#define _KUTILS_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdlib.h>
#include "matutil.h"
#include <math.h>

/* Common definitions */
#ifndef PI
#define PI 3.14159265358979323846
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern int verbose_mode;

enum { STRAIGHT,TAPER,HORN };
enum{ R3D_FORMAT,DXF_FORMAT };
enum{ CENTER,RIGHT,LEFT };

#ifndef MIN
#define MIN(a,b) ((a)>(b))?(b):(a)
#define MAX(a,b) ((a)>(b))?(a):(b)
#endif

#ifndef PI
#ifdef M_PI
#define PI M_PI
#else
#define PI 3.14159265358979323846 /* copied from math.h */
#endif
#define PI2 (PI*2)
#endif

#define PIH (PI*0.5)

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define THRESHOLD (1.0E-10)

#define D2R(a) (PI/180.0*a)
#define R2D(a) (a/PI*180.0)

#define MAX_LINE_BUF 1024
#define BUFSIZE 256

/* ���̸��̤ˤ�벻®����������ͤμ¸��� */
#define KIRCHHOFF_CONST 0.004253682 
/* ���̸��̤ˤ�벻®����������ͤ������� */
/*#define KIRCHHOFF_CONST 0.00402961*/


#define CM_CHAR '%' /* comment beginning flag */
/*  #define RD_CHAR '<' */ /* file input flag */
#define AD_CHAR '+' /* flag for ADDON type split */
#define SP_CHAR '>' /* flag for SPLIT */
#define JN_CHAR '<' /* flag for JOIN */
#define TH_CHAR '-' /* flag for TONEHOLE type split */
#define CH_CHAR '$' /* flag for child mensur */

/*************************prototypes*************************/
/* kutils.c */
size_t eat_comment(char *s);
char *fgetstr(char *s, int size, FILE *file);
void eol_to_lf(char *buf);
void eat_blank(char *buf);
void skip_to_nextline(char **p);
void get_word(char **p, char buf[]);
void get_line(char **p, char buf[]);
void usage(char *message[]);
void check_ptr(void *ptr);
void *m_malloc(size_t size);
void *m_realloc(void *ptr, size_t size);
void *m_calloc(size_t nmemb, size_t size);
double distance1(double x0, double x1);
double distance2(double x0, double y0, double x1, double y1);
double distance3(int x0, int y0, int z0, int x1, int y1, int z1);
void adjust_angle(double *ang);
double vec_angle(double x, double y);
void show_version(char *prgname);
char* copy_string( char* src );
  
#ifdef __cplusplus
}
#endif

#endif /* _KUTILS_ */
