/*
 * $Id: calcimp.c 3 2013-08-03 07:37:16Z yoshinobu $
 * calculate imput impedance of given mensur
 * 1999.05.03 - 
 * Yoshinobu Ishizkai
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <numpy/arrayobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* #include <math.h> */
#include <getopt.h>
#include "kutils.h"
#include "zmensur.h"
#include "calcimp.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* grobals */
extern int optind;
extern int optopt;
extern char* optarg;

char out_name[256];

extern char filecomment[];
extern struct menlist* mensur_list;
extern struct varlist* variable_list;
extern double rho,rhoc0,c0,nu;

#define MAX_FREQ 2000.0
#define STEP_FREQ 2.5
#define TEMPERATURE 24.0 /*QuickBasic時代の音程計算プログラムのデフォルト*/

double max_frq;
double step_frq;
unsigned long num_frq;
double temperature;
int verbose_mode;
int calc_transfer = 0; /*透過特性を計算するか?*/

extern int rad_calc, dump_calc, sec_var_calc;

/*
  double dumpfactor;
  double divlen; 
  int use_oldsax_mensur;
  int fing_id;
  fingdata *finglist;
*/
static char *message[] = {
  /*  
      " calcimp [-hvVS] [-m max_frq] [-s step_frq] [-n num] 
      [-t temp] [-F fing_id] [-o out_file] file.men", 
  */
  "Usage : calcimp [OPTION] \"file.men\"",
  " このプログラムは短いテーパー管の連続で楽器を近似し、入力インピーダンス",
  " を計算する。",
  " Fletcher & Rossing及び鎌倉友男の本を参考に管壁摩擦による",
  "  位相速度減少及びエネルギー損失を盛り込んでおり,",
  "  その数値は鎌倉の式を使って計算する。",
  " 計算中は連続条件の取扱が楽になるため音響インピーダンスp/uSを",
  " 使っているが、最終出力は音響インピーダンス密度p/uにしてある。",
  " 拡張メンズール形式zmensurに対応",
  " -O(オー)オプションを使用すると、出力音圧poと入力音圧pの比po/pを出力する。",
  "",
  " -h : show this message.",
  " -v : show version.",
  " -V : set verbose mode.",
  " -m : set max frequency ( default 2kHz).",
  " -s : set step frequancy ( default 2.5Hz ).",
  " -n : set max number of frequency ( -s の別指定法,-sより優先 )",
  " -t : set temperature ( default 24C ).",
  " -O : 透過特性po/pを出力する",
  " -R pipe/buffle/none : 放射特性の計算を指定する。",
  "    pipe = デフォルトフランジのない管の放射特性",
  "    buffle = 無限大フランジ付き管の放射特性",
  "    none = 放射特性を計算しない。単純開口端",
  " -D wall/none : 管壁摩擦計算方法を指定する。",
  "    wall = デフォルト。Fletcher&Rossingの教科書による摩擦計算をする。",
  "    none = 管壁摩擦計算をしない",
  " -o : 出力ファイル名を指定。指定しなければ入力ファイルの拡張子.menを",
  "      .impに変えたファイルが作成される。",
  "      - を指定すると標準出力が使われる。",
  " \"file.men\" --- mensur file.",
  NULL
};

void initialize()
{
  max_frq = MAX_FREQ;
  step_frq = STEP_FREQ; 
  num_frq = 0;
  temperature = TEMPERATURE; /* defaults */
  strcpy(out_name,"");

  verbose_mode = 0;

}

int parse_opt( int argc,char** argv )
{
  int c;
    
  while( (c = getopt(argc,argv,"hvm:s:n:t:Vo:OR:D:T")) != EOF ){
    switch( c ){
    case 'h':
      usage(message);
      break;
    case 'm':
      max_frq = atof( optarg );
      break;
    case 's':
      step_frq = atof( optarg );
      break;
    case 'n':
      num_frq = strtoul(optarg,NULL,10);
      break;
    case 't':
      temperature = atof( optarg );
      break;
    case 'v': /* show version and quit */
      show_version("calcimp");
      exit(0);
      break;
    case 'V':
      verbose_mode = 1;
      break;
    case 'o':
      strcpy(out_name,optarg);
      break;
    case 'O':
      calc_transfer = 1;
      break;
    case 'R':
      if( strcmp(optarg,"none") == 0 )
	rad_calc = NONE;
      else if( strcmp(optarg,"buffle") == 0 )
	rad_calc = BUFFLE;
      else 
	rad_calc = PIPE;
      break;
    case 'D':
      if( strcmp(optarg,"none") == 0 )
	dump_calc = NONE;
      else
	dump_calc = WALL;
      break;
    case 'T':
      /* 断面積変化を考慮した計算になるが、テーパが反転する
	 ところの計算がおかしくなる */
      sec_var_calc = TRUE;
      break;
    case '?':
      fprintf(stderr,"unknown option %c\n",optopt);
      break;
    }
  }
  return 0;
}

static PyObject* calculate_impedance(const char* filename, double max_freq, double step_freq, 
                                      unsigned long num_freq, double temperature) {
    mensur *mensur;
    double complex *imp;
    int n_imp;
    double frq, mag, re, im, S;
    int i;
    PyObject *freq_array, *real_array, *imag_array, *mag_array, *result_tuple;
    npy_intp dims[1];

    /* Initialize constants based on temperature */
    c0 = 331.45 * sqrt(temperature / 273.16 + 1);
    rho = 1.2929 * (273.16 / (273.16 + temperature));
    rhoc0 = rho * c0;
    
    double mu = (18.2 + 0.0456*(temperature - 25)) * 1.0e-6;
    nu = mu/rho;

    /* Read mensur file */
    mensur = read_mensur(filename);
    if (mensur == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to read mensur file");
        return NULL;
    }

    /* Calculate number of points */
    if (num_freq > 0) {
        step_freq = max_freq / (double)num_freq;
    }
    n_imp = max_freq / step_freq + 1;
    dims[0] = n_imp;

    /* Allocate memory for impedance calculations */
    imp = (double complex*)calloc(n_imp, sizeof(double complex));
    if (imp == NULL) {
        PyErr_NoMemory();
        return NULL;
    }

    /* Get initial cross-sectional area */
    S = PI * pow(get_first_men(mensur)->df, 2) / 4;

    /* Calculate impedance */
    for (i = 0; i < n_imp; i++) {
        frq = i * step_freq;
        if (i == 0) {
            imp[i] = 0.0;
        } else {
            input_impedance(frq, mensur, 1, &imp[i]);
            imp[i] *= S;  /* Convert to acoustic impedance density */
        }
    }

    /* Create numpy arrays */
    freq_array = PyArray_SimpleNew(1, dims, NPY_DOUBLE);
    real_array = PyArray_SimpleNew(1, dims, NPY_DOUBLE);
    imag_array = PyArray_SimpleNew(1, dims, NPY_DOUBLE);
    mag_array = PyArray_SimpleNew(1, dims, NPY_DOUBLE);

    if (!freq_array || !real_array || !imag_array || !mag_array) {
        Py_XDECREF(freq_array);
        Py_XDECREF(real_array);
        Py_XDECREF(imag_array);
        Py_XDECREF(mag_array);
        free(imp);
        PyErr_NoMemory();
        return NULL;
    }

    /* Fill arrays with data */
    double *freq_data = (double*)PyArray_DATA((PyArrayObject*)freq_array);
    double *real_data = (double*)PyArray_DATA((PyArrayObject*)real_array);
    double *imag_data = (double*)PyArray_DATA((PyArrayObject*)imag_array);
    double *mag_data = (double*)PyArray_DATA((PyArrayObject*)mag_array);

    for (i = 0; i < n_imp; i++) {
        freq_data[i] = i * step_freq;
        real_data[i] = creal(imp[i]);
        imag_data[i] = cimag(imp[i]);
        mag = real_data[i] * real_data[i] + imag_data[i] * imag_data[i];
        mag_data[i] = (mag > 0) ? 10 * log10(mag) : mag;
    }

    free(imp);

    /* Create return tuple */
    result_tuple = PyTuple_New(4);
    if (!result_tuple) {
        Py_DECREF(freq_array);
        Py_DECREF(real_array);
        Py_DECREF(imag_array);
        Py_DECREF(mag_array);
        return NULL;
    }

    PyTuple_SET_ITEM(result_tuple, 0, freq_array);
    PyTuple_SET_ITEM(result_tuple, 1, real_array);
    PyTuple_SET_ITEM(result_tuple, 2, imag_array);
    PyTuple_SET_ITEM(result_tuple, 3, mag_array);

    return result_tuple;
}

static PyObject* py_calcimp(PyObject* self, PyObject* args, PyObject* kwargs) {
    const char* filename;
    double max_freq = MAX_FREQ;
    double step_freq = STEP_FREQ;
    unsigned long num_freq = 0;
    double temperature = TEMPERATURE;
    static char* kwlist[] = {"filename", "max_freq", "step_freq", "num_freq", "temperature", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ddkd", kwlist,
                                    &filename, &max_freq, &step_freq, &num_freq, &temperature)) {
        return NULL;
    }

    return calculate_impedance(filename, max_freq, step_freq, num_freq, temperature);
}

static PyMethodDef CalcimpMethods[] = {
    {"calcimp", (PyCFunction)py_calcimp, METH_VARARGS | METH_KEYWORDS,
     "Calculate input impedance of a tube.\n\n"
     "Parameters:\n"
     "    filename (str): Path to the mensur file\n"
     "    max_freq (float, optional): Maximum frequency (default: 2000.0)\n"
     "    step_freq (float, optional): Frequency step (default: 2.5)\n"
     "    num_freq (int, optional): Number of frequency points (overrides step_freq if > 0)\n"
     "    temperature (float, optional): Temperature in Celsius (default: 24.0)\n\n"
     "Returns:\n"
     "    tuple: (frequencies, real_part, imaginary_part, magnitude_db)"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef calcimpmodule = {
    PyModuleDef_HEAD_INIT,
    "calcimp",
    "Extension module for calculating input impedance of tubes",
    -1,
    CalcimpMethods
};

PyMODINIT_FUNC PyInit_calcimp(void) {
    import_array();  /* Initialize numpy */
    return PyModule_Create(&calcimpmodule);
}

int main( int argc, char **argv )
{
  int i,n_imp;
  char *in_name,*s;
  FILE *fout;
  mensur *mensur,*m;
  double complex *imp,*p,mu;
  double frq,mag,re,im,S;

  initialize();

  /* arg 処理 */
  parse_opt( argc, argv );
  if( num_frq > 0 ){
    step_frq = max_frq/(double)num_frq;
  }

  /* temp°の場合の定数値(SI単位系) */
  c0 = 331.45 * sqrt(temperature / 273.16 + 1);
  rho = 1.2929 * ( 273.16 / (273.16 + temperature) );
  rhoc0 = rho * c0;
    
  mu = (18.2 + 0.0456*(temperature - 25)) * 1.0e-6;
  /* 粘性係数。理工学辞典の表から線形近似した式。*/
  nu = mu/rho; /* 動粘性係数 */

  in_name = argv[optind];

  /* parse_optの中でout_nameが指定されていない場合 */
  if( strlen(out_name) == 0 ){
    strcpy(out_name,in_name);
    s = strrchr(out_name,'.');
    if( s == NULL ){
      strcat(out_name,".imp");
    }else{
      strcpy(s,".imp");
    }
  }
  mensur = read_mensur(in_name);
#if 0
  print_men(mensur,filecomment);
  return 0;
#endif

  if( mensur != NULL ){
    n_imp = max_frq / step_frq + 1;
    imp = (double complex*)calloc( n_imp,sizeof(double complex) );
    S = PI*pow(get_first_men(mensur)->df,2)/4;
    /*  fprintf(stderr,"danmenseki %f\n",S); */

#if 0
    /* 有効長までメンズールを切り詰める */
    trunc_men( mensur );
    print_men(mensur,filecomment);

    /* horn関数を計算しておく */
    horn_function(mensur);

    for( m = get_first_men(mensur); m != NULL; m = m->next ){
      printf("%f,%f,%f,%f\n",m->df,m->db,m->r,m->hf);
    }
#endif
    /* インピーダンスの計算 */
    for( i = 0,p = imp; i < n_imp; i++,p++ ){
      frq = i * step_frq;

      if( verbose_mode )
	/*fprintf(stderr,"calculating freq %.2f begin:",frq );*/
	fprintf(stderr,".");

      if( i == 0 ){
	*p = 0.0;
      }else{
	/* calc_ximp_fletcher(frq,mensur,p); */
	input_impedance(frq,mensur,1,p);
		
	if( !calc_transfer ){
	  /* 入力インピーダンスを音響インピーダンス密度に変換する */
	  *p *= S;
	}else{
	  /* 入口出口での音圧の伝達特性 */
	  *p = ((get_last_men(mensur))->pi)/(mensur->pi);
	}
      }
    }/*for*/
    if( verbose_mode )
      fprintf(stderr,"\n");
	
    /* インピーダンスの出力 */
    if( strcmp(out_name,"-") == 0 )
      fout = stdout;
    else
      fout = fopen(out_name,"w");

    if( fout != NULL ){
      fprintf( fout,"freq,imp.real,imp.imag,mag\n" );
      for( i = 0,p = imp ; i < n_imp; i++,p++ ){
	frq = i * step_frq;
	re = creal(*p);
	im = cimag(*p);
	mag = re*re + im*im;
	if( mag > 0 )
	  mag = 10 * log10(mag); /* dB! */
	fprintf( fout,"%f,%.10E,%.10E,%.10E\n",frq,re,im,mag );
      }
      fclose(fout);
    }
  }/* mensur */

  return 0;
}

