/*
 * $Id: zmensur.c 7 2013-09-13 14:01:45Z yoshinobu $
 * 拡張メンズールを扱うための関数群
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cephes.h"

#include <sys/stat.h>
#include <ctype.h>
#include <complex.h>
#include <glib.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>

#include "kutils.h"
#include "zmensur.h"

char filecomment[256];
struct varlist* variable_list = NULL;
struct menlist* mensur_list = NULL;

/* ------------------------------ complex math wrappers ------------------------------*/
/* Use GSL complex math functions for portability */

static inline double complex gsl_to_c99_complex(gsl_complex z) {
    return GSL_REAL(z) + I * GSL_IMAG(z);
}

static inline gsl_complex c99_to_gsl_complex(double complex z) {
    gsl_complex result;
    GSL_SET_COMPLEX(&result, creal(z), cimag(z));
    return result;
}

#define csqrt(z) gsl_to_c99_complex(gsl_complex_sqrt(c99_to_gsl_complex(z)))
#define csin(z) gsl_to_c99_complex(gsl_complex_sin(c99_to_gsl_complex(z)))
#define ccos(z) gsl_to_c99_complex(gsl_complex_cos(c99_to_gsl_complex(z)))

/* ------------------------------ subroutines ------------------------------*/
mensur* create_men (double df,double db,double r,char* comm)
{
  mensur* buf = m_malloc( sizeof(mensur));
  buf->next = NULL;
  buf->prev = NULL;
  buf->side = NULL;

  buf->df = df;
  buf->db = db;
  buf->r = r;
  strncpy(buf->comment,comm,64);
  strcpy(buf->sidename,"");

  /* initialize admittance as non-zero */
  buf->y = 1.0;

  return buf;
}

mensur* get_first_men( mensur* inmen )
{
  mensur* buf = inmen;
  if( buf != NULL ){
    while( buf->prev != NULL )
      buf = buf->prev;
  }

  return buf;
}

mensur* get_last_men( mensur* inmen )
{
  mensur* buf = inmen;
  if( buf != NULL ){
    while( buf->next != NULL ){
      buf = buf->next;
    }
  }
  return buf;
}

/* join先を探す 
 * bheadはjoinするブランチの先頭アドレス
 */
mensur* get_join_men(mensur *inmen, mensur* bhead)
{
  mensur *n,*bh;
    
  n = inmen;
  while(1){
    if( n == NULL ){
      fprintf(stderr,"Caution! Cannot find joining point.\n");
      exit(-1);
    }
    if(n->side != NULL && n->s_type == JOIN ){
      bh = get_first_men(n->side);
      if( bh == bhead ) /* ok found */
	goto FOUND;
    }
	
    n = n->next;    
  }
 FOUND:
  return n;
}

mensur* prepend_men( mensur* inmen,double df,double db,double r,char* comm)
{
  mensur* new = create_men(df,db,r,comm);

  if( new == NULL ){
    fprintf(stderr,"cant create new mensur item\n");
    exit(-1);
  }
  if( inmen != NULL ){
    /* insert new to current pos if inmen has prev */
    if( inmen->prev != NULL ){
      inmen->prev->next = new;
      new->prev = inmen->prev;
    }
    inmen->prev = new;
    new->next = inmen;
  }

  return new;
}

mensur* append_men( mensur* inmen ,double df,double db,double r,char* comm)
{
  mensur* new;
  /*  mensur* last; */
  new = create_men( df,db,r,comm );
  if( new == NULL ){
    fprintf(stderr,"cant create new mensur item\n");
    exit(-1);
  }

  if( inmen != NULL ){
    /*  last = get_last_men( inmen ); */
    if( inmen->next != NULL ){
      inmen->next->prev = new;
      new->next = inmen->next;
    }
    inmen->next = new;
    new->prev = inmen;
  }

  return new;
}

/* 
 * remove last branch of men
 */
mensur* remove_last_men( mensur *inmen )
{
  mensur* buf,*last;

  last = get_last_men(inmen);

  buf = last->prev;
  free( last );
  buf->next = NULL;

  return buf;
}

mensur* remove_men( mensur* inmen )
{
  mensur *prev,*next,*buf,*out;
    
  buf = inmen;
  if( buf->next == NULL ){
    prev = buf->prev;
    prev->next = NULL;
    out = prev;
  }else if( buf->prev == NULL ){
    next = buf->next;
    next->prev = NULL;
    out = next;
  }else{
    prev = buf->prev;
    next = buf->next;
    prev->next = next;
    next->prev = prev;
    out = next;
  }
    
  free( buf );
  return out;
}

void dispose_men(mensur* inmen)
{
  while( inmen->prev != NULL )
    inmen = remove_last_men(inmen);

  free(inmen);
}

/*
 * 単位変換で使う
 */
void scale_men(mensur* men,double a )
{
  mensur* m = get_first_men(men);
  while( m != NULL ){
    m->df *= a;
    m->db *= a;
    m->r *= a;
	
    m = m->next;
  }
}

/* 
 * mensurを補間する
 * 単純に単一テーパを区分しているだけ
 * OBSOLETE: cannot handle side branch !
 */
void hokan_men( mensur* men, double step )
{
  int i,num;
  double t,df,db,r;
  double l = men->r;
  mensur *p,*next = men->next;

  if( l > step ){
    num = l/step;
    df = men->df;
    db = men->db;
    t = (db - df)/l;
	
    men->db = df + t*step;
    men->r = step;
	
    p = men;
    db = p->db;
    for( i = 0; i < num-1; i++ ){
      df = db;
      db = df + t*step;
      p = append_men(p,df,db,step,men->comment);
    }
    /* 最後のブランチの調整 */
    df = db;
    r = l - step*num;
    if( r > 0 ){
      /* テーパ計算の確認 */
      db = df + t*r;
      if( fabs( db - next->df ) < THRESHOLD ){
	/* 計算誤差を回避する */
	p = append_men(p,df,next->df,r,men->comment);
      }else{
	/* 次のメンズール要素と段差あり */
	p = append_men(p,df,db,r,men->comment);
      }
    }
    p->next = next;
    next->prev = p;
  }
  if( next != NULL ){
    /* recursive call */
    hokan_men(next,step);
  }
}

/* 
 * mensurを後ろから前に戻りながら細かく分割する
 * 単純に単一テーパを区分しているだけ
 * 分岐やループを正しく処理する(20130913)
 */
void divide_men( mensur* men, double step )
{
  int i,num;
  double t,df,db,r;
  double l;
  mensur *p,*prev; 

  p = get_last_men(men);

  while( p != NULL ){
    /* 分岐処理 */
    if( p->side != NULL ){
      /* recursive call */
      divide_men(p->side,step);
    }

    /* main process */
    prev = p->prev; /* remenber original previous cell */

    l = p->r;
    if( l > step ){ /* slicing */
      num = l/step;
      df = p->df;
      db = p->db;
      t = (db - df)/l;
	
      p->df = db - t*step;
      p->r = step;
      
      df = p->df;
      for( i = 0; i < num-1; i++ ){
	db = df;
	df = db - t*step;
	p = prepend_men(p,df,db,step,p->comment);
      }

      /* 最後のブランチの調整 */
      db = df;
      r = l - step*num;
      if( r > 0 ){
	/* テーパ計算の確認 */
	df = db - t*r;
	if( prev == NULL ){
	  p = prepend_men(p,df,db,r,p->comment); 
	}else{
	  if( fabs( df - prev->db ) < THRESHOLD ){
	    /* 計算誤差を回避する */
	    p = prepend_men(p,prev->db,db,r,p->comment);
	  }else{
	    /* 前のメンズール要素と段差あり */
	    p = prepend_men(p,df,db,r,p->comment);
	  }
	}
      }
    }/* end of slicing */
    
    p = p->prev;
  }
}


/*
 * 指定位置からlenの所までを切り取り捨てる
 * lenが+なら後方へ、-なら前方へ向かう
 */
mensur* cut_men( mensur* inmen, double len)
{
  mensur *buf,*new;
  double l,x,dnew;
    
  /* do nothing for len == 0 */
  buf = inmen;
  l = buf->r;
  if( len > 0 ){
    while( l < len && buf->next != NULL ){
      buf = buf->next;
      l += buf->r;
    }
    
    x = l - len;
    dnew = buf->db -  (buf->db - buf->df)/buf->r * x;
    buf->r = x;
    buf->df = dnew;

    new = buf;
    for( buf = inmen; buf != new; )
      buf = remove_men(buf);
	
  }else if( len < 0 ){
    len = -len;
    while( l < len && buf->prev != NULL ){
      buf = buf->prev;
      l += buf->r;
    }

    x = l - len;
    dnew = ((buf->db - buf->df)/buf->r * x) + buf->df;
    buf->r = x;
    buf->db = dnew;
	
    new = buf;
    for( buf = new->next; buf != inmen; ){
      buf = remove_men(buf);
    }
    buf = remove_men(buf);
	
  }
  return new;
}

mensur* print_men_core( mensur* inmen )
{
  mensur* buf = inmen;

  while( buf->next != NULL ){
    printf( "%f,%f,%f,%s\n",buf->df*1000,buf->db*1000,buf->r*1000,buf->comment);
    /* rejoint_menで対応するようになった
       if( buf->s_ratio > 0.5 ){
       print_men_core( buf->side );
       }
    */
    buf = buf->next;
  }
  printf( "%f,%f,%f,%s\n",buf->df*1000,buf->db*1000,buf->r*1000,buf->comment);

  return buf;
}

/*
 * メンズールを出力するだけ
 */
void print_men( mensur* inmen,char* comment )
{
  mensur* buf;

  printf("%s\n",comment);
  buf = get_first_men(inmen);
  buf = print_men_core(buf);
  /*add last*/
  if( buf->db != 0 || buf->r != 0 )
    printf( "%f,%f,%f,\n",buf->db*1000,0.0,0.0);
}

/*
 * 名前の通り。メンズールを逆順にして出力
 */
void print_men_reverse( mensur* inmen,char* comment )
{
  mensur* buf;

  printf("%s\n",comment);
  buf = get_last_men(inmen);
  while( buf->prev != NULL ){
    if( buf->db != 0 || buf->r != 0)
      printf( "%f,%f,%f,%s\n",buf->db*1000,buf->df*1000,buf->r*1000,buf->comment);
    buf = buf->prev;
  }
  printf( "%f,%f,%f,%s\n",buf->db*1000,buf->df*1000,buf->r*1000,buf->comment);
  printf( "%f,0,0,\n",buf->df*1000);
}

/*
 * XY形式に変換して出力
 * men2xyで使う
 */
void print_men_xy( mensur* inmen,char* comment,int show_stair)
{
  mensur* buf = inmen;
  double len = 0.0;

  buf = get_first_men(inmen);
    
  printf("%s\n",comment);
  while( buf->next != NULL ){
    printf("%f,%f,%s\n",len*1000,buf->df*1000,buf->comment);
    len += buf->r;

    /* 階段表示 */
    if( show_stair ){
      if( buf->db != buf->next->df ){
	printf("%f,%f,stair\n",len*1000,buf->db*1000);
	printf("%f,0,stair\n",len*1000);
      }
    }
    buf = buf->next;
  }
  printf("%f,%f,%s\n",len*1000,buf->df*1000,buf->comment);
  if( buf->db != 0 ){
    len += buf->r;
    printf("%f,%f,\n",len*1000,buf->db*1000);
  }
}

/*
 * XY形式データを返す
 */
GPtrArray *get_men_xy(mensur* men, int show_stair )
{
  double x; /* total length */
  struct xy *xy;
  GPtrArray* ar;
  
  ar = g_ptr_array_new();

  mensur *pm = get_first_men(men); /* 念のため */
  x = 0.0;
  while( pm != NULL ){
    xy = xy_new( x*1000.0, pm->df*1000.0, pm->comment );
    g_ptr_array_add( ar, xy );
    x += pm->r;

    /* 階段部分の値を追加する */
    if( pm->next != NULL && pm->db != pm->next->df ){
      if( show_stair ){
	xy = xy_new( x*1000.0, pm->db*1000.0, "stair" );
	g_ptr_array_add( ar, xy );
	xy = xy_new( x*1000.0, 0.0, "stair" );
	g_ptr_array_add( ar, xy );
      }
    }
    pm = pm->next;
  }
  
  return ar;
}

/*
 * mensur各位置での音圧を出力
 */
void print_pressure(mensur* men, int show_stair )
{
  double x; /* total length */
  double complex p;
  double mag;
    
  mensur *pm = get_first_men(men); /* 念のため */
    
  x = 0;
  while( pm != NULL ){
    p = pm->pi;
    mag = 20*log10( cabs(p) );
    printf("%f,%.10e,%.10e,%f,%s\n",x*1000,creal(p),cimag(p),mag,pm->comment);
    x += pm->r;
    
    /* 階段部分の値を追加する */
    if( pm->next != NULL && pm->db != pm->next->df ){
      if( show_stair ){
	printf("%f,%.10e,%.10e,%f,stair\n",x*1000, creal(p),cimag(p),mag );
	printf("%f,%.10e,%.10e,%f,stair\n",x*1000, creal(p),cimag(p),mag );
      }
    }
    pm = pm->next;
  }
}

/*
 * 音圧分布を返す
 */
GArray *get_pressure_dist( double f, mensur* men, int show_stair, acoustic_constants *ac )
{
  double complex p,det,z;
  double mag;
  GArray* ar;

  /* calc imput impedance first */
  input_impedance(f,men,1,&z,ac);

  ar = g_array_new( FALSE, FALSE, sizeof(double) );

  mensur *pm = get_first_men(men); /* 念のため */

  /* calc pressure from start to end */
  while( pm != NULL ){
    if( pm->prev )
      pm->pi = pm->prev->po;
    else{
      /* initial value */
      pm->pi = 0.02 + 0.0*I; /* 60dB HARD CODING */
      /* 60dB(SPL)=20*10^-6(Pa) * 10^(60/20) 2004.11.19 */
    }

    p = pm->pi;
    mag = 20*log10( cabs(p) );
    
    g_array_append_val( ar, mag );
    /* 階段部分の値を追加する */
    if( pm->next != NULL && pm->db != pm->next->df ){
      if( show_stair ){
	g_array_append_val( ar, mag );
	g_array_append_val( ar, mag );
      }
    }
    
    /* calc for next segment */
    pm->ui = pm->pi/pm->zi; 
    det = pm->m11*pm->m22 - pm->m12*pm->m21;
    pm->po = pm->m22*pm->pi - pm->m12*pm->ui;
    pm->uo = pm->m21*pm->pi + pm->m11*pm->ui;

    pm->po /= det;
    pm->uo /= det;

    pm = pm->next;
  }
  
  return ar;
}

/*
 * 部分メンズールとして定義されたデータを探して、接続する。
 */
void resolve_child( mensur *men ){
  mensur *p,*child; 

  p = men;

  while( p != NULL ){
    if( strlen(p->sidename) != 0 ){
      child = find_men( p->sidename );
      if( child != NULL ){
	if( p->s_type != JOIN ){
	  p->side = child;
	  /* recursive call */
	  resolve_child(child);
	}else
	  p->side = get_last_men(child);
      }else{
	/* p->sidenameで定義された部分メンズールが見つからない */
	/* error処理を作成しなくては... */
	fprintf(stderr,"%sで定義された部分メンズールが見つかりません\n",p->sidename);
      }
    }
    p = p->next;
  }
}

/*
 * メンズールの分岐合流指示の処理
 */
mensur* build_men( char* inbuf )
{
  char *pe,*p = inbuf;
  char buf[256],sb[64],ss[128],*s;
  mensur* men = NULL;
  double rt,df,db,r;

  pe = inbuf + strlen(inbuf); /* end of inbuf */

  while( p != pe ){
    get_line(&p,buf);
    s = buf;
    if( *s == TH_CHAR || *s == AD_CHAR || *s == SP_CHAR || *s == JN_CHAR ){
      s++;
      get_word(&s,sb); /* name for splitting mensur */
      if( men != NULL ){
	if( strlen(men->sidename) != 0 ){
	  fprintf(stderr,"メンズールが重複して分岐しているため長さ0のメンズール単位が追加されます。\n");
	  fprintf(stderr,"今の分岐先:%s,次の分岐先:%s\n\n",men->sidename,sb);
	  sprintf(ss,"automaticaly added for new branch %s",sb);
	  men = append_men(men,men->db,men->db,0,ss);
	}
	strcpy( men->sidename,sb);
	get_word(&s,sb); /* splitting ratio */
	rt = atoval(sb) * 1000; /* unit restore for ratio */
	men->s_ratio = rt;

	if( *buf == AD_CHAR )
	  men->s_type = ADDON;
	else if( *buf == SP_CHAR )
	  men->s_type = SPLIT;
	else if( *buf == JN_CHAR )
	  men->s_type = JOIN;
	else if( *buf == TH_CHAR )
	  men->s_type = TONEHOLE;
	else{
	  fprintf(stderr,"Warning! Unknown identifier %s found.\n",buf);
	}
      }
	    
    }else if( *s == CM_CHAR || *s == '\0' ){
      /* do nothing */
    }else if( isvardef(s) ){
      /* do nothing */
    }else{
      get_word(&s,sb);
      df = atoval(sb);
      get_word(&s,sb);
      db = atoval(sb);
      get_word(&s,sb);
      r = atoval(sb);
      get_word(&s,sb);

      men = append_men(men,df,db,r,sb);

      /* この部分が将来的には変更が必要と思う。
	 何故なら終端が閉じているときの表現が難しいので。
	 * "DF,DB,0,comm"だと開口端で,"DF,0,0,comm"だと閉端と
	 規定したいところ。
	 * さらには、"ENDMEN"みたいなはっきりと終端であることを
	 示すフラグをファイルに書き込みたい。
      */
      if( db == 0 && r == 0 ){ /* last segment */
	break; /* exit while loop */
      }
    }
  }/* while */
    
  return get_first_men(men);
}

/*
 * 名前のついた部分メンズールのポインタを返す
 */
mensur* find_men(char* s)
{
  struct menlist* ml = mensur_list;
  mensur* men = NULL;

  while( ml != NULL ){
    if( strcmp(ml->name,s) == 0 ){
      men = ml->men;
      break;
    }
    ml = ml->next;
  }

  return men;
}

/*
 * 変数定義を解釈
 */
double atoval(char* s )
{
  double val = 0.0;
  char* ss = s;

  while( strlen(ss) > 0 ){
    if( isdigit(*ss) || *ss == '.' ){
      /* 書いてあるのは数字か小数点 */
      val = atof(ss);
      break;
    }else if( isalpha(*ss) ){
      val = find_var( ss );
      break;
    }else if( isspace(*ss) ){
      ss++;
      continue;
    }
  }
  return val * 0.001; /* unit conv (mm->m) */
}

/*
 * 名前から変数値を返す
 */
double find_var(char* s)
{
  struct varlist* vl = variable_list;
  double val = 0;

  while( vl != NULL ){
    if( strcmp(vl->name,s) == 0 ){
      val = vl->val;
      break;
    }
    vl = vl->next;
  }

  if( vl == NULL ){
    fprintf(stderr,"cant find variable \"%s\"\n",s);
  }

  return val;
}

/*
 * 文字列が変数定義をしているかどうかを判別する
 */
int isvardef( char* s )
{
  int colnum = 0;
  int flag = 0;
  char *p = s;
    
  while( *p != '\0' ){
    if( *p == ',' ){
      colnum++;
      break;
    }else if( *p == '=' && colnum == 0 ){
      /* 一列目でかつ=記号を含んでいれば、変数定義 */
      flag = 1;
      break;
    }
    p++;
  }

  return flag;
}

/*
 * 変数名に変数値を割り当てる
 */
void set_var(char* s )
{
  char* p = s;
  double x;
  struct varlist* var;

  while( *p != '=' )
    p++;

  x = atof(p+1);
  *p = '\0';
    
  var = m_malloc( sizeof(struct varlist) );
  var->next = variable_list;
  strcpy( var->name,s );
  var->val = x;
  variable_list = var;

}

/*
 * 変数定義しているところを処理
 */
void read_variables( char* inbuf )
{
  char* p = inbuf;
  char buf[256],*s;

  while( *p != '\0' ){
    get_line(&p,buf);
    s = buf;
    if( isvardef(s) ){
      /* variable definition */
      set_var(s);
    }
  }
}

/*
 * 部分メンズール定義を読み込んでグローバルなmensur_listに登録する
 */
void read_child_mensur( char* buf )
{
  char str[256],wd[64];
  char* p = buf,*s;
  struct menlist* ml;
  mensur* men;
    
  while( *p != '\0' ){
    get_line(&p,str);
    s = str;
    if( *s == CH_CHAR ){
      s++;
      get_word(&s,wd);
      men = build_men(p);
	    
      ml = m_malloc( sizeof( struct menlist ));
      ml->next = mensur_list; /* global data */
      ml->men = men;
      strcpy( ml->name, wd );
      mensur_list = ml;

      /* 余計な最後のデータを消しておく ---> 残すように変更 */
      /* remove_last_men( men ); */
    }
  }
}

/*
 * valve分岐をs_ratioに応じて繋ぎ直す 
 */
mensur* rejoint_men( mensur* men )
{
  mensur *p,*q,*s,*ss;
    
  p = get_first_men(men);

  while( p->next != NULL ){
    if( p->s_ratio > 0.5 ){
      if( p->side != NULL && p->s_type == ADDON ){
	/* 繋ぎ直し */
	q = get_last_men(p->side);
	q = remove_men(q); /*最後は除く(1個前が返される)*/
	s = p->next;

	q->next = s;
	s->prev = q;
	p->next = p->side;
	p->side->prev = p;
	ss = create_men(s->df,s->db,s->r,s->comment);
	p->side = ss;
	p->s_ratio = 1 - p->s_ratio;
	append_men(ss,ss->db,0,0,"");
      }else if( p->side != NULL && p->s_type == SPLIT ){
	/* join先を探す */
	s = get_join_men(p,p->side);

	q = s->side;
	q = remove_men(q); /*最後は除く(1個前が返される)*/

	ss = p->next;
	p->side->prev = p;
	p->next = p->side;
	p->side = ss;
	ss->prev = NULL;
	p->s_ratio = 1 - p->s_ratio;

	s->next->prev = q;
	q->next = s->next;
	s->next = s->side = NULL;
	s->s_type = 0;
		
	ss = append_men(s,s->db,0,0,"");
	q->side = ss;
	q->s_type = JOIN;
	q->s_ratio = 1 - s->s_ratio;
	s->s_ratio = 0;
      }
	    
      /* recursive call to rejoint orginal branch */
      /* rejoint_men(p); */
    }else{
      /* s_ratio < 0.5なのでメインのメンズールには変更がない */
    }

    p = p->next;
  }

  return men; /* same value that used input */
}

/*
 * メンズールファイルを読み込む
 * 変数定義や部分メンズール定義を処理した後、分岐や合流部分を処理して
 * 全体を適切に繋ぐ。
 */
mensur* read_mensur( const char *path )
{
  int err;
  FILE* infile;
  mensur *men = NULL;
  struct stat fstatus;
  unsigned long readbytes;
  char *readbuffer,*p; 

  if( (err = stat(path,&fstatus)) != 0 ){
    fprintf(stderr,"open err at read_mensur : %d\n",err);
    exit(-1);
  }
    
  readbytes = fstatus.st_size;
  readbuffer = malloc( readbytes + 1 );
  if( readbuffer == NULL ){
    fprintf(stderr,"cant assign memory for read.\n");
    exit(-1);
  }

  infile = fopen(path,"r");
  fread(readbuffer,1,readbytes,infile);
  readbuffer[readbytes] = '\0';
  fclose(infile);

  eol_to_lf( readbuffer );
  /*  eat_blank( readbuffer ); */

  read_variables( readbuffer );
  read_child_mensur( readbuffer );

  p = readbuffer;
  get_line( &p,filecomment ); /* 最初の行はファイルコメント */
  men = build_men(p);

  resolve_child(men);
  men = rejoint_men(men); /* valve分岐をs_ratioに応じて繋ぎ直す */

#ifdef DEBUG
  print_men( mensur,filecomment );
#endif

  return get_first_men(men); /* this is first segment */
}

/*
 * メンズールの要素の個数を返す
 */
unsigned int count_men( mensur* men )
{
  unsigned int i = 0;
  mensur* p = get_first_men(men);

  while( p != NULL ){
    i++;
    p = p->next;
  }

  return i;
}

/*
 * mensurの全ブランチにわたって伝達行列の合成を計算する
 */
void transmission_matrix( mensur *men, mensur* end,
			  complex double* m11,complex double* m12,
			  complex double* m21, complex double* m22,
			  acoustic_constants *ac )
{
  mensur* pm;

  complex double n11,n12,n21,n22;
  complex double z11,z12,z21,z22; /* 合成行列 */
  complex double x11,x12,x21,x22;
    
  if( end == NULL ){
    pm = get_last_men(men);
    /* 最後は終端セルだからまず手前に一つ戻す */
    pm = pm->prev; 
  }else
    pm = end;

  z11 = pm->m11;
  z12 = pm->m12;
  z21 = pm->m21;
  z22 = pm->m22;

  while( pm != men ){
    pm = pm->prev; 
    n11 = pm->m11;
    n12 = pm->m12;
    n21 = pm->m21;
    n22 = pm->m22;

    x11 = n11*z11 + n12*z21;
    x12 = n11*z12 + n12*z22;
    x21 = n21*z11 + n22*z21;
    x22 = n21*z12 + n22*z22;

    z11 = x11; z12 = x12;
    z21 = x21; z22 = x22;
  }

  *m11 = z11; *m12 = z12;
  *m21 = z21; *m22 = z22;
}

/*
 * 入口出口の断面積変化率を計算する
 */
void sec_var_ratio1(mensur* men, double *out_t1, double *out_t2 )
{
  double t1,t2,st;

  t1 = t2 = 0;

  if( men != NULL && men->r > 0 ){
    st = (men->db - men->df)/2/men->r;
    t1 = PI*st*men->df;
    t2 = PI*st*men->db;
  }
    
  *out_t1 = t1;
  *out_t2 = t2;
}

/*
 * 前後のセグメントでの断面積変化率を考慮して入口出口の
 * 断面積変化率を計算する。
 */
void sec_var_ratio(mensur* men, double *out_t1, double *out_t2 )
{
  mensur *m1,*m2;
  double t11,t12,t21,t22,t01,t02,t1,t2;

  sec_var_ratio1(men,&t01,&t02);

  m2 = men->next;
  if( m2 != NULL ){
    sec_var_ratio1(m2,&t21,&t22);
    t2 = ( t02 + t21 )/2;
  }else{
    t2 = t02;
  }

  m1 = men->prev;
  if( m1 != NULL ){
    sec_var_ratio1(m1,&t11,&t12);
    t1 = ( t01 + t12 )/2;
  }else{
    t1 = t01;
  }

  *out_t1 = t1;
  *out_t2 = t2;
}

/*
 * 各menのセグメントでインピーダンスを計算する
 */
void do_calc_imp( double frq, mensur* men, acoustic_constants *ac )
{
  double complex z,z1,z2,k,x,m11,m12,m21,m22,n11,n12,n21,n22;
  /* z : acoustic impedance z = p/u
     p : pressure
     u : volume velocity
     k : wave number */
  double d,d1,d2,w,L,aa,r1,r2,s1,s2,ss,t1,t2;
  mensur* nm;

  /* まず出口端と次のセグメントの入力端との連続条件 */
  men->po = men->next->pi;/* should be removed in future? */
  men->uo = men->next->ui;/* should be removed in future? */
  men->zo = men->next->zi;/* new code for calcimp, default zo without branch. */
  
  if( men->side != NULL ){/* has side branch */
    if( men->s_type == TONEHOLE ){
      /* 音孔としての扱い */
      input_impedance(frq,men->side,men->s_ratio,&z1,ac);
      z2 = men->next->zi;
      z = z1*z2/(z1+z2);
      men->po = men->next->pi;/* should be removed in future? */
      men->uo = men->po/z;/* should be removed in future? */
      men->zo = z; /* new code for calimp */

    }else if( men->s_type == ADDON && men->s_ratio > 0 ){
      /* ループ管のインピーダンス */
      input_impedance(frq,men->side,1,&z1,ac);/* z1 は未使用 */
      transmission_matrix(men->side,NULL,&m11,&m12,&m21,&m22,ac);
	    
      /*↓これは間違いだった(20040928:Yoshinobu Ishizaki)*/
      /*  z1 = m12/( m12*m21+(1-m11)*(1+m22) ); */
      z1 = m12/(m12*m21-(1-m11)*(1-m22));

      z1 /= men->s_ratio;/* 面積比の調整 */
      z2 = men->next->zi;
      z2 /= (1 - men->s_ratio);/* 面積比の調整 */	
      z = z1*z2/(z1+z2);

      men->po = men->next->pi; /* should be removed in future? */
      men->uo = men->po/z; /* should be removed in future? */
      men->zo = z; /* new code for calcimp */

    }else if( men->s_type == SPLIT && men->s_ratio > 0 ){
      /* 複合管のインピーダンス */
      input_impedance(frq,men->side,1,&z1,ac); /* z1 は未使用 */
      transmission_matrix(men->side,NULL,&m11,&m12,&m21,&m22,ac);

      nm = get_join_men(men,men->side);
      transmission_matrix(men->next,nm,&n11,&n12,&n21,&n22,ac);

      /* 面積比を掛ける */
      m12 /= (1 - men->s_ratio);
      m21 *= (1 - men->s_ratio);
      n12 /= men->s_ratio;
      n21 *= men->s_ratio;

      z2 = nm->next->zi;
      z = (m12*n12 + (m12*n11 + m11*n12)*z2)/
	(m22*n12 + m12*n22 + ((m12 + n12)*(m21 + n21) - 
			      (m11 - n11)*(m22 - n22))*z2);

      men->po = nm->next->pi; /* should be remove in future? */
      men->uo = men->po/z; /* should be remove in future? */
      men->zo = z; /* new code for calcimp */
    }
  }

  /* 
   * 伝達行列を計算し、それと出口側のpo,uoから入口側のpi,uiを計算する
   * 詳細はMathematicaによるWebstar方程式.nb.pdfを参照すること
   */
  w = PI2*frq;

  if( men->r == 0.0 ){
    men->pi = men->po;
    men->ui = men->uo;
    men->m11 = men->m22 = 1.0;
    men->m12 = men->m21 = 0.0;
  }else{
    d1 = men->df;
    d2 = men->db;
    d = (d1+d2) * 0.5;
    L = men->r;

    /* 
       Fletcherの教科書の方法
       (ちょっとラフなので使わないが大きくは違わなかった。)
       v = c0 * (1 - VRATIO/d/sqrt(frq));
       aa = ARATIO * sqrt(frq)/d;
       k = w/v - I*aa;
    */

#if 1
    /* より正確に計算する。詳細は"管壁摩擦再考"の文書を参照(2004.11.18) */
    aa = (1+(GMM-1)/sqrt(Pr))*sqrt(2*w*ac->nu)/ac->c0/d;
#else
    /* 古典吸収による損失。小さすぎて合わない 
       教科書にも通常壁面損失より小さいと書いてあるが…
    */
    aa = pow(frq,2)*(1.9191330643902223e-11 + 0.8101467155352282*
		     (6.807568405969152e-6/
		      (75.05860674577968 + 0.013322922491579913*
		       pow(frq,2)) + 1.347451648197375e-6/
		      (13.066677226339555 + 0.07653055039763432*
		       pow(frq,2))));
#endif

    if( ac->dump_calc == WALL ){
#if 1
      k = csqrt( (w/ac->c0)*(w/ac->c0 - 2*(I-1)*aa) );
#else
      /* これも近似式だが大して結果は変わらない */
      /*  k = (w/ac->c0 + aa) - I*aa; */
      /* 敢えて虚部だけにaaをいれる。何故かこの方が音程面でうまくいく */
      /* HRで全体的に音程を高く見積もってしまっている。やっぱり不採用 */
      k = w/ac->c0 - I*aa;
#endif
    }else if( ac->dump_calc == NONE ){
      k = w/ac->c0;
    }
    x = k*L;

    if( ac->sec_var_calc ){
      /* 断面積変化率を考慮した計算を行う */
      /* テーパ,直管両方をまとめた式を使っている */
      /* 計算してみたら、テーパ反転があるところで著しく実際と異なる
	 特性となってしまって、ダメ */
      s1 = PI/4*d1*d1;
      s2 = PI/4*d2*d2;
      ss = sqrt(s1*s2);
      sec_var_ratio(men,&t1,&t2);

      men->m11 = ( 2*k*s2*ccos(x) - t2*csin(x) )/(2*k*ss);
      men->m12 = ( I * ac->rhoc0 * csin(x) )/ss;
      men->m21 = ( -2*I*k*( s2*t1-s1*t2 )*ccos(x) +
		   I*( 4*k*k*s1*s2 + t1*t2 )*csin(x) )/
	(4*ac->rhoc0*k*k*ss);
      men->m22 = (2*k*s1*ccos(x) + t1*csin(x) )/(2*k*ss);

    }else{
      /* 断面積変化率を考慮しない */
      if( men->df == men->db ){
	/* straight */
	s1 = PI/4*d*d;
	men->m11 = men->m22 = ccos(x);
	men->m12 = I*ac->rhoc0*csin(x)/s1;
	men->m21 = I*s1*csin(x)/ac->rhoc0;
      }else{
	/* taper */
	r1 = d1/2;
	r2 = d2/2;

	men->m11 = ( r2*x*ccos(x) -(r2-r1)*csin(x))/(r1*x);
	men->m12 = I*ac->rhoc0*csin(x)/(PI*r1*r2);
	men->m21 = -I*PI*( (r2-r1)*(r2-r1)*x*ccos(x) -
			   ((r2-r1)*(r2-r1) + x*x*r1*r2 )*csin(x) )/
	  (k*k*L*L*ac->rhoc0);
	men->m22 = ( r1*x*ccos(x) + (r2-r1)*csin(x))/(r2*x);
		
	/* 以下逆テーパ問題で変更する前までの式を念のため残しておく 
	   men->m11 = ( x*d2*ccos(x)+(d1-d2)*csin(x) )/(x*d1);
	   men->m12 = 4*I*rho*w*csin(x)/(k*PI*d1*d2);
	   men->m21 = -I*PI*( x*(d1-d2)*(d1-d2)*ccos(x) - 
	   ( (d1-d2)*(d1-d2) + x*x*d1*d2 )*csin(x) ) / 
	   (4*k*L*L*rho*w);
	   men->m22 = ( x*d1*ccos(x)-(d1-d2)*csin(x) )/(x*d2);
	*/
      }
    }
    men->pi = men->m11*men->po + men->m12*men->uo;
    men->ui = men->m21*men->po + men->m22*men->uo;
    
    /* new code for calcimp */
    if( men->next->y != 0.0 ){  
      men->zi = (men->m11*men->zo + men->m12)/(men->m21*men->zo + men->m22 ); 
    }else{
      /* impedance of next men cell is infinity! */
      men->zi = men->m11/men->m21;
    }
  }
}

void get_imp(mensur* men,double complex* out_z)
{
  *out_z = men->zi;
}

/*
 * 放射インピーダンスを計算する
 * 無限バッフル中の円盤による放射としての計算を行って
 * それを元にフランジ無しに近似
 * 城戸健一編著 日本音響学会編纂 基礎音響工学の90p
 * k : 波数
 * d : 直径
 * zr : 放射インピーダンス = p/u, pは音圧,uは体積速度
 */
#if 1
void rad_imp( double frq,double d,double complex* zr, acoustic_constants *ac )
{
  double a,x,s;
  double re,im;
  double k;
  double j1_result, struve_result;


  k = PI2*frq/ac->c0;

  if( d <= 0.0 ) return; /* エラー処理がない! */

  a = 0.5*d;
  x = k * d;
  /* フレッチャーの本によると断面積で割る必要あり!2004.11.19 */
  s = PI*a*a;
#if 0
  printf( "k=%f,f=%f,2ka=%f\n",k,f,x);
#endif


  /* j1は1次のベッセル関数。struveはストルーブ関数 */
  /* 音響インピーダンスにするために断面積で割る */
  j1_result = j1(x);

  struve_result = struve(1, x);

  re = ac->rhoc0/s * ( 1 - j1_result/(k*a) );
  im = ac->rhoc0/s * struve_result / (k*a);
#if 0
  printf( "radiation impedance is %f,%f at freq %f\n",c->r,c->i,\
	  PI2 / k );
#endif

  if( ac->rad_calc == BUFFLE ){
    *zr = re + I*im;
  }else if( ac->rad_calc == PIPE ){
    /* フランジが無い場合はざっと実部が0.5倍で虚部が0.7倍らしい */
    *zr = 0.5*re + 0.7*I*im;
  }else if( ac->rad_calc == NONE ){
    *zr = 0.0;
  }
}
#else
/*
 * Schwinger & Levineのフランジ無し放射特性計算の近似式を使う
 */
void rad_imp( double frq,double d,double complex* zr, acoustic_constants *ac )
{
  double a,x,w,L,R,s,k,y;
  double complex bunbo,bunsi;

  a = 0.5*d;
  w = PI2*frq;
  k = w/ac->c0;
  x = k*a;
  s = PI*a*a;

  if( x < 1 ){
    R = (1 + pow(x,4)/6*(19/12-log(0.577216*x)) ) * exp(-x*x/2);
  }else{
    R = sqrt(PI*x)*(1+3/32/x/x)*exp(-x);
  }
    
  if( x < 4.31082 ){
    L = a/PI*( 1.92809 + 
	       x*(-0.0955095 + x*(-0.269353 + 
				  x*(0.113335 + x*(-0.0161873)))) 
	       );
  }else
    L = 0.0;
    
  y = 2*k*L;

  bunbo = 1 + R * ( cos(y) + I*sin(y) );
  bunsi = 1 - R * ( cos(y) + I*sin(y) );

  *zr = ac->rhoc0/s * bunsi/bunbo;
}
#endif
/*
 * input impedance 計算
 * e_ratioは終端断面積の調整因子(e_ratio*dを真の直径として計算する)
 */
void input_impedance (double frq, mensur* men, double e_ratio,
		      double complex* out_z, acoustic_constants *ac )
{
  double complex p,u,z;


  mensur* pm = get_last_men(men);

  p = 0.02 + 0.0*I; /* 60dB(SPL)=20*10^-6 * 10^(60/20) 2004.11.19 */


  /* 終端 */
  if( pm->df <= 0 || e_ratio == 0 ){/* closed end */
    /*  pm->pi = 1.0 + 0.0*I; */
    pm->pi = p;
    pm->ui = 0.0 + 0.0*I;
    /* set admittance to 0 */
    pm->y = 0.0;

  }else{/* open end */
    rad_imp(frq,(pm->df)*e_ratio,&z,ac);

    /*  u = 1.0 + 0.0 *I; */
    /*  p = z*u; */
    /* 開口端でのu値を適当に決めちゃっているが、それで良いのか? */
    /* 2004.11.19
       ->良くない?
       開口端補正が入るために、ここでの音圧が一定と仮定し,
       それから体積速度を求めるべきであった。
       音圧は適当なところで60dB(SPL)とした。
       が,この値は計算のアルゴリズム上どんな値でも結果に影響しない。
       今までのようにuからpを求めるのではなく、pからuを求めることが重要
       かと思ったが結果は変わらなかった。
       むしろ、理論式と合わないと思うのだが、
       u=p/z*sとすると音程はともかくインピーダンス曲線が良くあう。
    */
    if( ac->rad_calc != NONE )
      u = p/z;
    else{
      u = 1.0;
      p = 0.0;
      z = 0.0;
    }

    pm->pi = p;
    pm->ui = u;
    pm->zi = z;
  }

  do{
    pm = pm->prev;
    do_calc_imp(frq,pm,ac);
  }while( pm->prev != NULL );

  /* input impedance at initial mensur cell */
  get_imp(pm,out_z);
}

/* 
 * 有効長までメンズールを切り詰める
 * ベル側球面波として考えた場合、ベル端面近辺では波面がベルから
 * 飛び出してしまうので、その分をあらかじめ切り詰めておく。
 */
mensur* trunc_men( mensur *inmen )
{
  mensur *m,*n;
  double x,xb,d1,d2,L,ll,dd,t,l1,l2;

  m = n = get_last_men( inmen );

  m = m->prev;
  xb = 0.0;
  do{
    d1 = m->df;
    d2 = m->db;
    L = m->r;
    xb += L;

    if( d2 <= d1 )
      break; /* do not truncate */
    t =  atan( (d2-d1)/(2*L) );
    l1 = (1-cos(t))/sin(t)*d1/2;
    l2 = L + (1-cos(t))/sin(t)*d2/2;
    if( l1 < xb && l2 > xb ){
      /* OK. here, it must be truncated */
      x = d1/2/tan(t);
      ll = (x+xb)*cos(t) - x;
      dd = (x+xb)*sin(t)*2;
      while( n != m ){
	n = remove_men(n);
      }
      /* rewrite data */
      m->db = dd;
      m->r = ll;
      append_men(m,dd,0,0,"truncated");
      break;
    }else{
      /* otherwise next step */
      m = m->prev;
    }
  }while(1);

  return m;
}

/* 
 * 近似ホーン関数計算
 * 作ってみたが、実際には使えないようだ
 */
void horn_function( mensur *inmen )
{
  mensur* m = get_last_men( inmen );
  mensur *n;
  double a,r1,r2,d1,d2,d3,h;
  int flag = 1; /* 1 for forward, -1 for backward, 0 for 0 hornfunc */

  while( m != NULL ){
    if( flag < 2 ){
      flag = 1;
      n = m->next;
      if( n == NULL )
	flag = 0;
      else if( m->r == 0 || m->df >= m->db )
	flag = 0;
      else if( n->r == 0 || n->df == n->db ){
	flag = -1;
	n = m->prev;
	if( n == NULL )
	  flag = 0;
	else if( n->r == 0 || n->df == n->db )
	  flag = 0;
      }
    }   
	 
    switch(flag ){
    case 0:
      h = 0.0;
      break;
    case 1:/* forward calc */
      d1 = m->df;
      d2 = m->db;
      d3 = n->db;
      r1 = m->r;
      r2 = n->r;
      a = 2*( d3*r1 +d1*r2 - d2*(r1+r2) )/( r1*r2*(r1+r2) );
      h = a/d2;
      break;
    case -1:
      /* backward calc */
      d1 = n->df;
      d2 = m->df;
      d3 = m->db;
      r1 = n->r;
      r2 = m->r;
      a = 2*( d3*r1 +d1*r2 - d2*(r1+r2) )/( r1*r2*(r1+r2) );
      h = a/d3;
      break;
    case 2:
      /* surpress further calc */
      h = 0.0;
      break;
    }
	
    if( h < 0 ){
      flag = 2;
      h = 0.0;
    }
    m->hf = h;
    m = m->prev;
  }
}
