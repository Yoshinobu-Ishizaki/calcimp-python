#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject* add(PyObject* self, PyObject* args) {
    long a, b;
    if (!PyArg_ParseTuple(args, "ll", &a, &b)) {
        return NULL;
    }
    return PyLong_FromLong(a + b);
}

static PyMethodDef CalcimpMethods[] = {
    {"add", add, METH_VARARGS, "Add two integers."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef calcimpmodule = {
    PyModuleDef_HEAD_INIT,
    "calcimp",
    "A simple calculator implementation in C",
    -1,
    CalcimpMethods
};

PyMODINIT_FUNC PyInit_calcimp(void) {
    return PyModule_Create(&calcimpmodule);
}
