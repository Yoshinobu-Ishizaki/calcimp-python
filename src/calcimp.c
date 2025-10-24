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
#include <glib.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include "kutils.h"
#include "zmensur.h"
#include "calcimp.h"
#include "acoustic_constants.h"


static PyObject* calculate_impedance(const char* filename, double max_freq, double step_freq,
                                      unsigned long num_freq, double temperature) {
    mensur *mensur;
    double complex *imp;
    int n_imp;
    double frq, mag, S;
    int i;
    PyObject *freq_array, *real_array, *imag_array, *mag_array, *result_tuple;
    npy_intp dims[1];
    acoustic_constants ac;

    /* Initialize acoustic constants based on temperature with default configuration */
    init_acoustic_constants_default(&ac, temperature);

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
            input_impedance(frq, mensur, 1, &imp[i], &ac);
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
    double max_freq = 2000.0;
    double step_freq = 2.5;
    unsigned long num_freq = 0;
    double temperature = 24.0;
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

