#include "PyCamera.hpp"
#include "Python.hpp"

PyObject* PyCamera_new(PyTypeObject* type, PyObject*, PyObject*) {
    auto* self = (PyCameraObject*)PyObject_Malloc(sizeof(PyCameraObject));
    if (!self) return nullptr;
    PyObject_Init((PyObject*)self, type);
    return (PyObject*)self;
}

int PyCamera_init(PyCameraObject* self, PyObject*, PyObject*) {
    self->cam = &Camera::instance();
    return 0;
}

void PyCamera_dealloc(PyCameraObject* self) {
    PyObject_Free(self);
}

static PyObject* PyCamera_Reset(PyCameraObject* self, PyObject*) {
    self->cam->reset();
    Py_RETURN_NONE;
}

static PyObject* PyCamera_Update(PyCameraObject* self, PyObject* args) {
    float dt;
    int w, a, s, d, e, q;
    if (!PyArg_ParseTuple(args, "fpppppp", &dt, &w, &a, &s, &d, &e, &q)) return nullptr;
    self->cam->update(dt, w, a, s, d, e, q);
    Py_RETURN_NONE;
}

static PyObject* PyCamera_OnMouseButton(PyCameraObject* self, PyObject* args) {
    int pressed;
    if (!PyArg_ParseTuple(args, "p", &pressed)) return nullptr;
    self->cam->onMouseButton(pressed != 0);
    Py_RETURN_NONE;
}

static PyObject* PyCamera_OnCursorPos(PyCameraObject* self, PyObject* args) {
    double x, y;
    if (!PyArg_ParseTuple(args, "dd", &x, &y)) return nullptr;
    self->cam->onCursorPos(x, y);
    Py_RETURN_NONE;
}

static PyObject* PyCamera_OnScroll(PyCameraObject* self, PyObject* args) {
    double yoffset;
    if (!PyArg_ParseTuple(args, "d", &yoffset)) return nullptr;
    self->cam->onScroll(yoffset);
    Py_RETURN_NONE;
}

static PyObject* PyCamera_ViewMatrix(PyCameraObject* self, PyObject*) {
    auto mat = self->cam->viewMatrix();
    PyObject* list = PyList_New(16);
    for (int i = 0; i < 16; i++) {
        PyList_SetItem(list, i, PyFloat_FromDouble(mat[i]));
    }
    return list;
}

static PyObject* PyCamera_ProjectionMatrix(PyCameraObject*, PyObject* args) {
    int w, h;
    float fov = 45.0f, nearPlane = 0.1f, farPlane = 500.0f;
    if (!PyArg_ParseTuple(args, "ii|fff", &w, &h, &fov, &nearPlane, &farPlane)) return nullptr;
    auto mat = Camera::projectionMatrix(w, h, fov, nearPlane, farPlane);
    PyObject* list = PyList_New(16);
    for (int i = 0; i < 16; i++) {
        PyList_SetItem(list, i, PyFloat_FromDouble(mat[i]));
    }
    return list;
}

static PyObject* PyCamera_GetX(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->x);
}

static int PyCamera_SetX(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->x = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetY(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->y);
}

static int PyCamera_SetY(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->y = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetZ(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->z);
}

static int PyCamera_SetZ(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->z = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetRotX(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->rotX);
}

static int PyCamera_SetRotX(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->rotX = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetRotY(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->rotY);
}

static int PyCamera_SetRotY(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->rotY = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetSpeed(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->speed);
}

static int PyCamera_SetSpeed(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->speed = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyGetSetDef PyCamera_getset[] = {
    {"x", (getter)PyCamera_GetX, (setter)PyCamera_SetX, nullptr},
    {"y", (getter)PyCamera_GetY, (setter)PyCamera_SetY, nullptr},
    {"z", (getter)PyCamera_GetZ, (setter)PyCamera_SetZ, nullptr},
    {"rotX", (getter)PyCamera_GetRotX, (setter)PyCamera_SetRotX, nullptr},
    {"rotY", (getter)PyCamera_GetRotY, (setter)PyCamera_SetRotY, nullptr},
    {"speed", (getter)PyCamera_GetSpeed, (setter)PyCamera_SetSpeed, nullptr},
    {nullptr}
};

PyMethodDef PyCamera_methods[] = {
    {"reset", (PyCFunction)PyCamera_Reset, METH_NOARGS, "Reset camera to default position"},
    {"update", (PyCFunction)PyCamera_Update, METH_VARARGS, "Update camera with delta time and key states"},
    {"onMouseButton", (PyCFunction)PyCamera_OnMouseButton, METH_VARARGS, "Handle mouse button event"},
    {"onCursorPos", (PyCFunction)PyCamera_OnCursorPos, METH_VARARGS, "Handle cursor position event"},
    {"onScroll", (PyCFunction)PyCamera_OnScroll, METH_VARARGS, "Handle scroll event"},
    {"viewMatrix", (PyCFunction)PyCamera_ViewMatrix, METH_NOARGS, "Get 4x4 view matrix (column-major)"},
    {"projectionMatrix", (PyCFunction)PyCamera_ProjectionMatrix, METH_VARARGS | METH_STATIC, "Get 4x4 projection matrix (column-major)"},
    {nullptr}
};

PyType_Slot PyCamera_slots[] = {
    {Py_tp_new, (void*)PyCamera_new},
    {Py_tp_init, (void*)PyCamera_init},
    {Py_tp_dealloc, (void*)PyCamera_dealloc},
    {Py_tp_methods, PyCamera_methods},
    {Py_tp_getset, PyCamera_getset},
    {0, nullptr}
};

PyType_Spec PyCamera_spec = {
    "_angeloid.Camera", sizeof(PyCameraObject), 0,
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, PyCamera_slots
};
