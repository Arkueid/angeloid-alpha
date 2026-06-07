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

static PyObject* PyCamera_ToggleMode(PyCameraObject* self, PyObject*) {
    self->cam->toggleMode();
    Py_RETURN_NONE;
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
    // Try (int button, int action, int mods) first for multi-button support
    int button, action, mods;
    if (PyArg_ParseTuple(args, "iii", &button, &action, &mods)) {
        self->cam->onMouseButton(button, action, mods);
        Py_RETURN_NONE;
    }
    PyErr_Clear();
    // Fall back to (bool) for backward compat
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

static PyObject* PyCamera_GetMode(PyCameraObject* self, void*) {
    return PyLong_FromLong((long)self->cam->mode());
}

static int PyCamera_SetMode(PyCameraObject* self, PyObject* value, void*) {
    if (!PyLong_Check(value)) return -1;
    self->cam->setMode((CameraMode)PyLong_AsLong(value));
    return 0;
}

static PyObject* PyCamera_GetTargetX(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->targetX);
}

static int PyCamera_SetTargetX(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->targetX = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetTargetY(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->targetY);
}

static int PyCamera_SetTargetY(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->targetY = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetTargetZ(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->targetZ);
}

static int PyCamera_SetTargetZ(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->targetZ = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetOrbitDistance(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->orbitDistance);
}

static int PyCamera_SetOrbitDistance(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->orbitDistance = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetOrbitLatitude(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->orbitLatitude);
}

static int PyCamera_SetOrbitLatitude(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->orbitLatitude = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetOrbitLongitude(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->orbitLongitude);
}

static int PyCamera_SetOrbitLongitude(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->orbitLongitude = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyObject* PyCamera_GetOrbitFov(PyCameraObject* self, void*) {
    return PyFloat_FromDouble(self->cam->orbitFov);
}

static int PyCamera_SetOrbitFov(PyCameraObject* self, PyObject* value, void*) {
    if (!PyFloat_Check(value)) return -1;
    self->cam->orbitFov = (float)PyFloat_AsDouble(value);
    return 0;
}

static PyGetSetDef PyCamera_getset[] = {
    {"x", (getter)PyCamera_GetX, (setter)PyCamera_SetX, nullptr},
    {"y", (getter)PyCamera_GetY, (setter)PyCamera_SetY, nullptr},
    {"z", (getter)PyCamera_GetZ, (setter)PyCamera_SetZ, nullptr},
    {"rotX", (getter)PyCamera_GetRotX, (setter)PyCamera_SetRotX, nullptr},
    {"rotY", (getter)PyCamera_GetRotY, (setter)PyCamera_SetRotY, nullptr},
    {"speed", (getter)PyCamera_GetSpeed, (setter)PyCamera_SetSpeed, nullptr},
    {"mode", (getter)PyCamera_GetMode, (setter)PyCamera_SetMode, nullptr},
    {"targetX", (getter)PyCamera_GetTargetX, (setter)PyCamera_SetTargetX, nullptr},
    {"targetY", (getter)PyCamera_GetTargetY, (setter)PyCamera_SetTargetY, nullptr},
    {"targetZ", (getter)PyCamera_GetTargetZ, (setter)PyCamera_SetTargetZ, nullptr},
    {"orbitDistance", (getter)PyCamera_GetOrbitDistance, (setter)PyCamera_SetOrbitDistance, nullptr},
    {"orbitLatitude", (getter)PyCamera_GetOrbitLatitude, (setter)PyCamera_SetOrbitLatitude, nullptr},
    {"orbitLongitude", (getter)PyCamera_GetOrbitLongitude, (setter)PyCamera_SetOrbitLongitude, nullptr},
    {"orbitFov", (getter)PyCamera_GetOrbitFov, (setter)PyCamera_SetOrbitFov, nullptr},
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
    {"toggleMode", (PyCFunction)PyCamera_ToggleMode, METH_NOARGS, "Toggle between FPS and Orbit camera mode"},
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
