/* 
 *matutil.h
 */

#ifndef MATUTIL
#define MATUTIL

#ifndef SCALAR
#define SCALAR double
#endif
typedef SCALAR *vector, **matrix;

/* matutil.c */
void error(char *message);
vector newvec(int n);
matrix newmat(int nrow, int ncol);
vector new_vector(int n);
matrix new_matrix(int nrow, int ncol);
void free_vector(vector v);
void free_matrix(matrix a);
double innerproduct(int n, vector u, vector v);
void vecprint(vector v, int n, int perline, char *format);
void matprint(matrix a, int ncol, int perline, char *format);


#endif /* MATUTIL */
