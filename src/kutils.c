/*
 * $Id: kutils.c 3 2013-08-03 07:37:16Z yoshinobu $
 * kanutils関係のutility関数
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "kutils.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef __USE_ISOC99
#define __USE_ISOC99
#endif
#include <ctype.h>

/* #define CM_CHAR '%' */ /* comment line beginning flag */

size_t eat_comment( char* s )
{
    /* char cc = '#'; */
    char buf[MAX_LINE_BUF]; /* danger but ...*/
    char* p = s;
    char* q = buf;

    if( strlen(s) > MAX_LINE_BUF-1 ){
	fprintf(stderr,"string is longer than %d bytes.\n", MAX_LINE_BUF);
	return -1;
    }

    strcpy( buf,s );
    while( *q != '\0' ){
	if( *q == CM_CHAR || *q == '\n'){
	    *p = '\0';
	    break;
	}else
	    *p++ = *q++;
    }
    
    return strlen(s);
}

/* 
 * read str from file
 * for replacement to fgets
 * handle LF,CR,CR+LF
 */
char* fgetstr( char* s, int size, FILE* file )
{
  int i = 0;
  char *p,c,cc;

  p = s;

  do {
    c = fgetc(file);
    *p++ = c;
    i++;
  }while( c != '\n' && c != '\r' && c != '\0' && c != EOF && i < size-1 );

  switch (c){
  case EOF:
      if( i == 1 ) s = NULL;
      break;
  case '\r':
      /* 次の文字がLFだったら捨てる */
      if( (cc = fgetc(file)) != '\n' ) ungetc(cc,file);
  case '\n':
      --p;
      break;
  }
  /* 読み込んだ最後の文字は終末文字に書き換え */
  *p = '\0';
  return s;
}

void eol_to_lf( char* buf )
{
/* 改行コードの混在もうまく処理する */
    char* p = buf;

    while( *p != '\0' ){
	if( *p == '\r' ){
	    if( *(p+1) == '\n' )
		*p = ',';
	    else
		*p = '\n';
	}
	p++;
    }
}

/* eat blank chars */
void eat_blank( char *buf )
{
    long len;
    char *cbuf,*p,*q;
    
    len = strlen( buf );
    cbuf = m_malloc( len+1 );
    memcpy( cbuf,buf,len+1);

    p = cbuf;
    q = buf;
    
    while( *p != '\0' ){
	if( !isblank(*p) ){
	    *q++ = *p;
	}
	p++;
    }
}

void skip_to_nextline( char** p )
{
    while( **p != '\n' && **p != '\r' && **p != '\0' )
	(*p)++;

    if( **p == '\r' ){
	(*p)++;
	if( **p == '\n' )
	    (*p)++;
    }else if( **p == '\n' ){
	(*p)++;
    }
}

void get_word(char** p,char buf[])
{
    char *s = buf;
    
    while( **p != ',' && **p != '\r' && **p != '\n' && **p != '\0' ){
	*s++ = **p;
	(*p)++;
    }
    *s = '\0';

    if( **p != '\0' ){
	(*p)++;
	while( **p == '\n' || **p == '\r' )
	   (*p)++; 
	
    }
}

void get_line( char** p, char buf[] )
{
    char *s = buf;

    while( **p != '\n' && **p != '\0' ){
	*s++ = **p;
	(*p)++;
    }
    *s = '\0';

    if( **p != '\0' )
	(*p)++;

    /*  eat_comment(buf); *//* dont eat here */
}

void usage(char* message[])
{
    while( *message != NULL ){
	printf("%s\n",*message);
	message++;
    }

    exit(0);
}

void check_ptr(void* ptr)
{
    if( ptr == NULL ){
	fprintf(stderr,
		"Can't assign memory at %s line %u.\n",__FILE__,__LINE__);
	exit(-1);
    }
}

void* m_malloc( size_t size )
{
    void* ptr;

    ptr = malloc( size );
    check_ptr(ptr);
    
    return ptr;
}

void* m_realloc(void* ptr, size_t size )
{
    ptr = realloc(ptr,size);
    check_ptr(ptr);

    return ptr;
}

void* m_calloc( size_t nmemb, size_t size)
{
    void* ptr;

    ptr = calloc( nmemb, size);
    check_ptr(ptr);

    return ptr;
}

double distance1( double x0, double x1 )
{
  return fabs( x0-x1 );
}

double distance2( double x0, double y0, double x1, double y1 )
{
  return( sqrt((x0-x1)*(x0-x1) + (y0-y1)*(y0-y1)) );
}

double distance3(int x0,int y0,int z0, int x1,int y1,int z1)
{
  return ( sqrt( (x0-x1)*(x0-x1) + (y0-y1)*(y0-y1) + (z0-z1)*(z0-z1) ) );
}

/*
 * 角度を0,2PIの間に納める。
 * 単位はRadian
 */
void adjust_angle( double* ang )
{
  if( *ang >= 0 && *ang <= PI2 )
    return;
  
  while( *ang < 0 )
    *ang += PI2;
  while( *ang > PI2 )
    *ang -= PI2;
}

/*
 * ベクトルからその角度を求める。単位はRadian ( 0 - 2 Pi )
 */
double vec_angle( double x, double y )
{
  double a;

  if( x > 0 ){
    if( y > 0 ){
      return atan( y/x );
    }else{
      return atan( y/x ) + PI2;
    }
  }else if( x < 0 ){
    return atan( y/x ) + PI;
  }else{
    if( y > 0 )
      return PIH;
    else
      return 3*PIH;
  }
}

/*
 * プログラム名を表示するのみ
 */
void show_version( char* prgname )
{
  printf( prgname );
#ifdef PACKAGE_STRING
  printf("( %s )\n",PACKAGE_STRING);
#else
  printf("\n");
#endif
  exit(0);
}

/*
 * allocate memory, then copy string
 */
char* copy_string( char* src )
{
  size_t n = strlen( src );
  char* dest = m_malloc( n+1 );

  strcpy( dest, src );

  return dest;
}

