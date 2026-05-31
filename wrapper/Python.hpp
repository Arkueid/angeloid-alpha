#pragma once
#define Py_LIMITED_API 0x03090000
#include <Python.h>
#ifndef Py_IsNone
#define Py_IsNone(o) (o == Py_None)
#endif

static inline const char* PyUnicode_AsUTF8_Wrapper(PyObject *unicode) {
    PyObject *bytes = PyUnicode_AsEncodedString(unicode, "utf-8", NULL);
    if (!bytes) return NULL;
    const char *s = PyBytes_AsString(bytes);
    Py_DECREF(bytes);
    return s;
}
#define PyUnicode_AsUTF8(u) PyUnicode_AsUTF8_Wrapper(u)
