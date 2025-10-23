/*
 * $Id: xydata.c 3 2013-08-03 07:37:16Z yoshinobu $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kutils.h"
#include "xydata.h"

/*
 * XYデータを作る
 */
struct xy *xy_new( double x, double y, char* s )
{
  struct xy *xy;

  xy = (struct xy*)malloc( sizeof( struct xy ) );
  xy->x = x;
  xy->y = y;
  strcpy( xy->comment, s );

  return xy;
}

/*
 * xylistを作成
 */
struct xylist * create_xylist( unsigned int num )
{
  struct xylist* lst = NULL;

  lst = (struct xylist*)m_malloc( sizeof(struct xylist) );
  if( lst ){
    lst->count = num;
    strcpy(lst->cmtstr,"");
    lst->data = (struct xy*)m_malloc( sizeof(struct xy)*num );
    if( lst->data == NULL ){
      free(lst);
      lst = NULL;
    }
  }
  return lst;
}

void resize_xylist( struct xylist* lst, int num )
{
  lst->data = (struct xy*)m_realloc( (void*)lst->data, sizeof(struct xy)*num );
  lst->count = num;
}

/* 
 * xyデータを読み込む
 */
struct xylist* read_xy( char* inpath, int with_comment )
{
  FILE* infile;
  char buf[BUFSIZE];
  char wd[BUFSIZE];
  char* pb;
  struct xylist *lst;
  struct xy* pt;
  unsigned int i,n;

  if( inpath == NULL )
    infile = stdin;
  else{
    infile = fopen(inpath,"r");
    if( infile == NULL ){
      fprintf(stderr,"Error : can't open \"%s\"\n",inpath);
      exit(-1);
    }
  }
    
  lst = create_xylist( 256 );
  i = n = 0;
  pt = lst->data;
  strcpy( lst->cmtstr,"" );
  
  while( fgetstr(buf,BUFSIZE-1,infile) != NULL ){
    if( i == 0 && with_comment ){
      strncpy( lst->cmtstr, buf, BUFSIZE );
      i++;
    }else{
      eat_comment(buf);
      pb = buf;
      get_word(&pb,wd);
      if( strlen(wd) ){
	pt->x = atof(wd);
	get_word(&pb,wd);
	if( strlen(wd) ){
	  pt->y = atof(wd);
	  
	  get_word(&pb,wd);
	  if( strlen(wd) )
	    strncpy( pt->comment, wd, BUFSIZE );
	  
	  pt++; n++;
	  
	  if( n == lst->count ){
	    resize_xylist( lst, n+256 );
	    pt = lst->data + n;
	  }
	}else{
	  fprintf(stderr,"line does not contain valid x,y pair : \"%s\"\n",buf);
	}
      }
    }
  }
    
  resize_xylist( lst, n );

  return lst;
}

void sort_xy( struct xylist *inxy )
{
  unsigned int i,j,k;
  struct xy *pt;
  double x1,x2,y1,y2,d,dd;

  pt = inxy->data;

  for( i = 0; i < inxy->count-2 ; i++){
    dd = -1;
    x1 = pt[i].x;
    y1 = pt[i].y;

    for( j = i+1 ; j < inxy->count; j++ ){
      x2 = pt[j].x;
      y2 = pt[j].y;
      d = sqrt( (x2-x1)*(x2-x1) + (y2-y1)*(y2-y1) );
      if( dd < 0 || d < dd ){
	dd = d;
	k = j;
      }
    }/* j loop */
    if( k != i + 1 ){
      /* swap k,i+1 */
      /* ここは非効率。もともとある程度並んでいたら間に挿入する方が良い */
      x1 = pt[k].x;
      y1 = pt[k].y;
      pt[k].x = pt[i+1].x;
      pt[k].y = pt[i+1].y;
      pt[i+1].x = x1;
      pt[i+1].y = y1;
    }
  }/* i loop */
}

void print_xy( struct xylist* xy )
{
  struct xy *p = xy->data;
  int i,n = xy->count;

  for(i = 0; i < n; i++,p++){
    printf("%f,%f\n",p->x,p->y);
  }
}

void dispose_xy(struct xylist* inxy)
{
  if( inxy != NULL ){
    if( inxy->data != NULL )
      free( inxy->data );

    /* if( inxy->cmtstr != NULL ) 
       free( inxy->cmtstr ); *//* stack cannot freed */

    free( inxy );
  }
}

/*
 * Y値を半分にする。データは書き換えられる 
 */

void yhalf_xy( struct xylist* xy )
{
  unsigned int i;
  struct xy *p;

  for( i = 0; i < xy->count; i++ ){
    p = xy->data + i;
    p->y *= 0.5;
  }
}
