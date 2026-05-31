#pragma once
#include "Python.hpp"
#include "../mmd/Camera.h"

struct PyCameraObject {
    PyObject_HEAD
    Camera* cam;
};

extern PyType_Spec PyCamera_spec;
extern PyType_Slot PyCamera_slots[];
extern PyMethodDef PyCamera_methods[];

PyObject* PyCamera_new(PyTypeObject* type, PyObject* args, PyObject* kwds);
int PyCamera_init(PyCameraObject* self, PyObject* args, PyObject* kwds);
void PyCamera_dealloc(PyCameraObject* self);
