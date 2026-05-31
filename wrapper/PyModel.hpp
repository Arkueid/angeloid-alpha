#pragma once
#include "Python.hpp"
#include "../mmd/Model.h"

struct PyModelObject {
    PyObject_HEAD
    mmd::Model* model;
};

extern PyType_Spec PyModel_spec;
extern PyType_Slot PyModel_slots[];
extern PyMethodDef PyModel_methods[];

PyObject* PyModel_new(PyTypeObject* type, PyObject* args, PyObject* kwds);
int PyModel_init(PyModelObject* self, PyObject* args, PyObject* kwds);
void PyModel_dealloc(PyModelObject* self);
