/*
 * $Id: zmensur.h 6 2013-09-13 13:16:15Z yoshinobu $
 * 拡張メンズールを扱うための関数群
 */

#ifndef _ZMENSUR_
#define _ZMENSUR_

#include <math.h>
#include <complex.h>

#ifndef DOMAIN
#include "mconf.h"
#endif

#include <glib.h>

#include "xydata.h"
#include "acoustic_constants.h"

struct men_s {
  double df,db,r;
  char comment[64],sidename[16];
  struct men_s *prev,*next,*side;
  int s_type; /* type of side branch */
  double hf; /* horn function at outer end */
  double s_ratio; /* ratio of side branching */
  double complex zi, zo; /* retain impedance in mensur cell */
  double complex y; /* in case of infinity impedance */ 

  double complex ui,pi; /* will not be used by calcimp */
  double complex uo,po; /* will not be used by calcimp */
  double complex m11,m12,m21,m22; /* transmission matrix */
};
typedef struct men_s mensur;

/*
  typedef struct {
  int th[64];
  char name[64];
  } fingdata;
*/

struct menlist {
  char name[256];
  mensur* men;
  struct menlist *next;
};

struct varlist {
  char name[64];
  double val;
  struct varlist *next;
};

enum { TONEHOLE = 1, ADDON,SPLIT,JOIN };

/* 
 * 管壁摩擦の係数
 * Fletcher & Rossing による日本語版「楽器の物理学」の191p。
 * 但し、教科書が半径を使った式であるのに対し、こちらは直径を使うので
 * それに係数値を合わせてある。
 */
#define VRATIO 3.3e-3 /* Fletcher & Rossing */
/* #define VRATIO 2.3e-3  ARSISに合わせる??? */
#define ARATIO 6.0e-5

#define GMM 1.4 /* 比熱比 */
#define Pr 0.72 /* Prandtl数 */

/* ------------------------------ prototype ------------------------------ */
/* zmensur.c */
mensur *create_men(double df, double db, double r, char *comm);
mensur *get_first_men(mensur *inmen);
mensur *get_last_men(mensur *inmen);
mensur *get_join_men(mensur *inmen, mensur *bhead);
mensur *prepend_men(mensur *inmen, double df, double db, double r, char *comm);
mensur *append_men(mensur *inmen, double df, double db, double r, char *comm);
mensur *remove_last_men(mensur *inmen);
mensur *remove_men(mensur *inmen);
void dispose_men(mensur *inmen);
void scale_men(mensur *men, double a);
void hokan_men(mensur *men, double step);
void divide_men( mensur* men, double step );
mensur *cut_men(mensur *inmen, double len);
mensur *print_men_core(mensur *inmen);
void print_men(mensur *inmen, char *comment);
void print_men_reverse(mensur *inmen, char *comment);
void print_men_xy(mensur *inmen, char *comment, int show_stair);
GPtrArray *get_men_xy(mensur* men, int show_stair );
void print_pressure(mensur *men, int show_stair);
GArray *get_pressure(mensur* men, int show_stair );
GArray *get_pressure_dist(double frq, mensur* men, int show_stair, acoustic_constants *ac);
void resolve_child(mensur *men);
mensur *build_men(char *inbuf);
mensur *find_men(char *s);
double atoval(char *s);
double find_var(char *s);
int isvardef(char *s);
void set_var(char *s);
void read_variables(char *inbuf);
void read_child_mensur(char *buf);
mensur *rejoint_men(mensur *men);
mensur *read_mensur(const char *path);
unsigned int count_men(mensur *men);
void transmission_matrix(mensur *men, mensur *end, _Complex double *m11, _Complex double *m12, _Complex double *m21, _Complex double *m22, acoustic_constants *ac);
void sec_var_ratio1(mensur *men, double *out_t1, double *out_t2);
void sec_var_ratio(mensur *men, double *out_t1, double *out_t2);
void do_calc_imp(double frq, mensur *men, acoustic_constants *ac);
void get_imp(mensur *men, double _Complex *out_z);
void rad_imp(double frq, double d, double _Complex *zr, acoustic_constants *ac);
void input_impedance(double frq, mensur *men, double e_ratio, double _Complex *out_z, acoustic_constants *ac);
mensur *trunc_men(mensur *inmen);
void horn_function(mensur *inmen);

#endif /* _ZMENSUR_ */
