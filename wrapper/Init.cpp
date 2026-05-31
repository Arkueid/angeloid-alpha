#include <stdio.h>
#include "Python.hpp"
#include "PyModel.hpp"
#include "PyCamera.hpp"
#include "../mmd/MMD.h"

#ifdef _WIN32
#include <Windows.h>
#endif

static PyObject* mmdcpp_init(PyObject*, PyObject* args) {
    const char* shaderDir = nullptr;
    const char* toonDir = nullptr;
    PyObject* blinkMorphsList = nullptr;
    
    if (!PyArg_ParseTuple(args, "ss|O", &shaderDir, &toonDir, &blinkMorphsList)) {
        return nullptr;
    }
    
    mmd::InitArgs initArgs;
    initArgs.shaderDir = std::filesystem::path((const char8_t*)shaderDir);
    initArgs.toonDir = std::filesystem::path((const char8_t*)toonDir);
    
    if (blinkMorphsList && PyList_Check(blinkMorphsList)) {
        Py_ssize_t n = PyList_Size(blinkMorphsList);
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject* item = PyList_GetItem(blinkMorphsList, i);
            if (PyUnicode_Check(item)) {
                const char* s = PyUnicode_AsUTF8(item);
                if (s) initArgs.blinkMorphs.push_back(s);
            }
        }
    }
    
    mmd::init(initArgs);
    Py_RETURN_NONE;
}

static PyObject* mmdcpp_glInit(PyObject*, PyObject*) {
    mmd::glInit();
    Py_RETURN_NONE;
}

static PyObject* mmdcpp_dispose(PyObject*, PyObject*) {
    mmd::dispose();
    Py_RETURN_NONE;
}

static PyObject* mmdcpp_initArgs(PyObject*, PyObject*) {
    const auto& args = mmd::initArgs();
    PyObject* dict = PyDict_New();
    PyDict_SetItemString(dict, "shaderDir", PyUnicode_FromString(args.shaderDir.string().c_str()));
    PyDict_SetItemString(dict, "toonDir", PyUnicode_FromString(args.toonDir.string().c_str()));
    
    PyObject* blinkList = PyList_New(args.blinkMorphs.size());
    for (size_t i = 0; i < args.blinkMorphs.size(); i++) {
        PyList_SetItem(blinkList, i, PyUnicode_FromString(args.blinkMorphs[i].c_str()));
    }
    PyDict_SetItemString(dict, "blinkMorphs", blinkList);
    
    return dict;
}

static PyMethodDef mmdcpp_methods[] = {
    {"init", mmdcpp_init, METH_VARARGS, "Initialize mmd module with shader and toon directories"},
    {"glInit", mmdcpp_glInit, METH_NOARGS, "Initialize OpenGL loader (glad)"},
    {"dispose", mmdcpp_dispose, METH_NOARGS, "Release all global resources"},
    {"initArgs", mmdcpp_initArgs, METH_NOARGS, "Get stored init arguments"},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef mmdcpp_module = {
    PyModuleDef_HEAD_INIT, "_angeloid", "MMD C++ port for Python", -1, mmdcpp_methods
};

PyMODINIT_FUNC PyInit__angeloid(void) {
#if WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    PyObject* m = PyModule_Create(&mmdcpp_module);
    if (!m) return nullptr;
    
    PyType_Spec modelSpec = {
        "_angeloid.Model", sizeof(PyModelObject), 0,
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, PyModel_slots
    };
    PyObject* modelType = PyType_FromSpec(&modelSpec);
    if (!modelType) {
        Py_DECREF(m);
        return nullptr;
    }
    int rc = PyModule_AddObject(m, "Model", modelType);
    if (rc < 0) {
        Py_DECREF(m);
        return nullptr;
    }
    
    PyType_Spec camSpec = {
        "_angeloid.Camera", sizeof(PyCameraObject), 0,
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, PyCamera_slots
    };
    PyObject* camType = PyType_FromSpec(&camSpec);
    if (!camType) {
        Py_DECREF(m);
        return nullptr;
    }
    rc = PyModule_AddObject(m, "Camera", camType);
    if (rc < 0) {
        Py_DECREF(m);
        return nullptr;
    }

    printf("[angeloid] C++ port, Python %s\n", PY_VERSION);
    return m;
}
