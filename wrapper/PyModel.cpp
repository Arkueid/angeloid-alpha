#include "PyModel.hpp"

#include "Python.hpp"

PyObject* PyModel_new(PyTypeObject* type, PyObject*, PyObject*) {
    auto* self = (PyModelObject*)PyObject_Malloc(sizeof(PyModelObject));
    if (!self)
        return nullptr;
    PyObject_Init((PyObject*)self, type);
    return (PyObject*)self;
}

int PyModel_init(PyModelObject* self, PyObject*, PyObject*) {
    self->model = new mmd::Model();
    return 0;
}

void PyModel_dealloc(PyModelObject* self) {
    delete self->model;
    PyObject_Free(self);
}

static PyObject* PyModel_Load(PyModelObject* self, PyObject* args) {
    const char* path;
    if (!PyArg_ParseTuple(args, "s", &path))
        return nullptr;
    self->model->load(std::filesystem::path((const char8_t*)path));
    Py_RETURN_NONE;
}

static PyObject* PyModel_Update(PyModelObject* self, PyObject* args) {
    float dt;
    if (!PyArg_ParseTuple(args, "f", &dt))
        return nullptr;
    self->model->update(dt);
    Py_RETURN_NONE;
}

static PyObject* PyModel_Draw(PyModelObject* self, PyObject* args) {
    int w, h;
    if (!PyArg_ParseTuple(args, "ii", &w, &h))
        return nullptr;
    self->model->draw(w, h);
    Py_RETURN_NONE;
}

static PyObject* PyModel_EnablePhysics(PyModelObject* self, PyObject* args) {
    int on;
    if (!PyArg_ParseTuple(args, "p", &on))
        return nullptr;
    self->model->enablePhysics(on != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_PhysicsEnabled(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->physicsEnabled() ? 1 : 0);
}

static PyObject* PyModel_ShowRigidBodies(PyModelObject* self, PyObject* args) {
    int v;
    if (!PyArg_ParseTuple(args, "p", &v))
        return nullptr;
    self->model->showRigidBodies(v != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_LoadVmd(PyModelObject* self, PyObject* args) {
    const char* path;
    if (!PyArg_ParseTuple(args, "s", &path)) return nullptr;
    int id = self->model->loadVmd(std::filesystem::path((const char8_t*)path));
    return PyLong_FromLong(id);
}

static PyObject* PyModel_PlayVmd(PyModelObject* self, PyObject* args) {
    int trackId;
    PyObject* callback = nullptr;
    if (!PyArg_ParseTuple(args, "i|O", &trackId, &callback))
        return nullptr;

    if (callback && !Py_IsNone(callback) && PyCallable_Check(callback)) {
        Py_INCREF(callback);
        auto cb = [callback](int id) {
            PyGILState_STATE s = PyGILState_Ensure();
            PyObject* r = PyObject_CallFunction(callback, "i", id);
            if (r)
                Py_DECREF(r);
            else
                PyErr_Print();
            Py_XDECREF(callback);
            PyGILState_Release(s);
        };
        self->model->playVmd(trackId, cb);
    }
    else {
        self->model->playVmd(trackId);
    }
    Py_RETURN_NONE;
}

static PyObject* PyModel_PauseVmd(PyModelObject* self, PyObject* args) {
    int trackId;
    if (!PyArg_ParseTuple(args, "i", &trackId))
        return nullptr;
    self->model->pauseVmd(trackId);
    Py_RETURN_NONE;
}

static PyObject* PyModel_StopVmd(PyModelObject* self, PyObject* args) {
    int trackId;
    if (!PyArg_ParseTuple(args, "i", &trackId))
        return nullptr;
    self->model->stopVmd(trackId);
    Py_RETURN_NONE;
}

static PyObject* PyModel_RemoveVmd(PyModelObject* self, PyObject* args) {
    int trackId;
    if (!PyArg_ParseTuple(args, "i", &trackId))
        return nullptr;
    self->model->removeVmd(trackId);
    Py_RETURN_NONE;
}

static PyObject* PyModel_PlayAllVmd(PyModelObject* self, PyObject*) {
    self->model->playAllVmd();
    Py_RETURN_NONE;
}

static PyObject* PyModel_PauseAllVmd(PyModelObject* self, PyObject*) {
    self->model->pauseAllVmd();
    Py_RETURN_NONE;
}

static PyObject* PyModel_StopAllVmd(PyModelObject* self, PyObject*) {
    self->model->stopAllVmd();
    Py_RETURN_NONE;
}

static PyObject* PyModel_IsVmdPlaying(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->isVmdPlaying() ? 1 : 0);
}

static PyObject* PyModel_VmdTrackCount(PyModelObject* self, PyObject*) {
    return PyLong_FromLong(self->model->vmdTrackCount());
}

static PyObject* PyModel_IsVmdPlayingTrack(PyModelObject* self, PyObject* args) {
    int trackId;
    if (!PyArg_ParseTuple(args, "i", &trackId))
        return nullptr;
    return PyBool_FromLong(self->model->isVmdPlaying(trackId) ? 1 : 0);
}

static PyObject* PyModel_VmdCurrentFrame(PyModelObject* self, PyObject* args) {
    int trackId;
    if (!PyArg_ParseTuple(args, "i", &trackId))
        return nullptr;
    return PyFloat_FromDouble(self->model->vmdCurrentFrame(trackId));
}

static PyObject* PyModel_VmdMaxFrame(PyModelObject* self, PyObject* args) {
    int trackId;
    if (!PyArg_ParseTuple(args, "i", &trackId))
        return nullptr;
    return PyFloat_FromDouble(self->model->vmdMaxFrame(trackId));
}

static PyObject* PyModel_SetVmdFrame(PyModelObject* self, PyObject* args) {
    int trackId;
    float frame;
    if (!PyArg_ParseTuple(args, "if", &trackId, &frame))
        return nullptr;
    self->model->setVmdFrame(trackId, frame);
    Py_RETURN_NONE;
}

static PyObject* PyModel_LoadVpd(PyModelObject* self, PyObject* args) {
    const char* path;
    if (!PyArg_ParseTuple(args, "s", &path)) return nullptr;
    int id = self->model->loadVpd(std::filesystem::path((const char8_t*)path));
    return PyLong_FromLong(id);
}

static PyObject* PyModel_ApplyVpd(PyModelObject* self, PyObject* args) {
    int vpdId;
    if (!PyArg_ParseTuple(args, "i", &vpdId))
        return nullptr;
    self->model->applyVpd(vpdId);
    Py_RETURN_NONE;
}

static PyObject* PyModel_ResetPose(PyModelObject* self, PyObject*) {
    self->model->resetPose();
    Py_RETURN_NONE;
}

static PyObject* PyModel_SyncVpdPose(PyModelObject* self, PyObject*) {
    self->model->syncVpdPose();
    Py_RETURN_NONE;
}

static PyObject* PyModel_VpdApplied(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->vpdApplied() ? 1 : 0);
}

static PyObject* PyModel_RemoveVpd(PyModelObject* self, PyObject* args) {
    int vpdId;
    if (!PyArg_ParseTuple(args, "i", &vpdId))
        return nullptr;
    self->model->removeVpd(vpdId);
    Py_RETURN_NONE;
}

static PyObject* PyModel_ShowModel(PyModelObject* self, PyObject* args) {
    int v;
    if (!PyArg_ParseTuple(args, "p", &v))
        return nullptr;
    self->model->showModel(v != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_ShowOutline(PyModelObject* self, PyObject* args) {
    int v;
    if (!PyArg_ParseTuple(args, "p", &v))
        return nullptr;
    self->model->showOutline(v != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_ShowToon(PyModelObject* self, PyObject* args) {
    int v;
    if (!PyArg_ParseTuple(args, "p", &v))
        return nullptr;
    self->model->showToon(v != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_GetShowModel(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->showModel() ? 1 : 0);
}

static PyObject* PyModel_GetShowOutline(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->showOutline() ? 1 : 0);
}

static PyObject* PyModel_GetShowToon(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->showToon() ? 1 : 0);
}

static PyObject* PyModel_IsSkinned(PyModelObject* self, PyObject*) {
    return PyBool_FromLong(self->model->isSkinned() ? 1 : 0);
}

static PyObject* PyModel_SetSkinning(PyModelObject* self, PyObject* args) {
    int on;
    if (!PyArg_ParseTuple(args, "p", &on))
        return nullptr;
    self->model->setSkinning(on != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_SetMorphWeight(PyModelObject* self, PyObject* args) {
    const char* name;
    float weight;
    if (!PyArg_ParseTuple(args, "sf", &name, &weight))
        return nullptr;
    self->model->setMorphWeight(name, weight);
    Py_RETURN_NONE;
}

static PyObject* PyModel_SavedMorphWeight(PyModelObject* self, PyObject* args) {
    const char* name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return nullptr;
    return PyFloat_FromDouble(self->model->savedMorphWeight(name));
}

static PyObject* PyModel_ClearMorphs(PyModelObject* self, PyObject*) {
    self->model->clearMorphs();
    Py_RETURN_NONE;
}

static PyObject* PyModel_SetMorphWeights(PyModelObject* self, PyObject* args) {
    PyObject* dict;
    if (!PyArg_ParseTuple(args, "O", &dict))
        return nullptr;
    if (!PyDict_Check(dict)) {
        PyErr_SetString(PyExc_TypeError, "Expected a dict");
        return nullptr;
    }

    std::unordered_map<std::string, float> weights;
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(dict, &pos, &key, &value)) {
        if (PyUnicode_Check(key) && PyFloat_Check(value)) {
            const char* k = PyUnicode_AsUTF8(key);
            float v = (float)PyFloat_AsDouble(value);
            if (k)
                weights[k] = v;
        }
    }
    self->model->setMorphWeights(weights);
    Py_RETURN_NONE;
}

static PyObject* PyModel_SetIdleBlink(PyModelObject* self, PyObject* args) {
    int on;
    if (!PyArg_ParseTuple(args, "p", &on))
        return nullptr;
    self->model->setIdleBlink(on != 0);
    Py_RETURN_NONE;
}

static PyObject* PyModel_MorphCount(PyModelObject* self, PyObject*) {
    return PyLong_FromLong(self->model->morphCount());
}

static PyObject* PyModel_InteractableMorphs(PyModelObject* self, PyObject*) {
    auto morphs = self->model->interactableMorphs();
    PyObject* list = PyList_New(morphs.size());
    for (size_t i = 0; i < morphs.size(); i++) {
        PyList_SetItem(list, i, PyLong_FromLong(morphs[i]));
    }
    return list;
}

static PyObject* PyModel_MorphName(PyModelObject* self, PyObject* args) {
    int index;
    if (!PyArg_ParseTuple(args, "i", &index))
        return nullptr;
    return PyUnicode_FromString(self->model->morphName(index).c_str());
}

static PyObject* PyModel_ModelName(PyModelObject* self, PyObject*) {
    return PyUnicode_FromString(self->model->modelName().c_str());
}

static PyObject* PyModel_ModelScale(PyModelObject* self, PyObject*) {
    return PyFloat_FromDouble(self->model->modelScale());
}

static PyObject* PyModel_ModelMatrix(PyModelObject* self, PyObject*) {
    const float* m = self->model->modelMatrix();
    PyObject* list = PyList_New(16);
    for (int i = 0; i < 16; i++) {
        PyList_SetItem(list, i, PyFloat_FromDouble(m[i]));
    }
    return list;
}

static PyMethodDef PyModel_methods[] = {
    {"load", (PyCFunction)PyModel_Load, METH_VARARGS, "Load PMX model from path"},
    {"update", (PyCFunction)PyModel_Update, METH_VARARGS, "Update model with delta time"},
    {"draw", (PyCFunction)PyModel_Draw, METH_VARARGS, "Draw model with screen dimensions"},
    {"enablePhysics", (PyCFunction)PyModel_EnablePhysics, METH_VARARGS, "Enable/disable physics"},
    {"physicsEnabled", (PyCFunction)PyModel_PhysicsEnabled, METH_VARARGS,
     "Check if physics is enabled"},
    {"showRigidBodies", (PyCFunction)PyModel_ShowRigidBodies, METH_VARARGS,
     "Show/hide rigid bodies"},
    {"loadVmd", (PyCFunction)PyModel_LoadVmd, METH_VARARGS, "Load VMD animation"},
    {"playVmd", (PyCFunction)PyModel_PlayVmd, METH_VARARGS, "Play VMD animation"},
    {"pauseVmd", (PyCFunction)PyModel_PauseVmd, METH_VARARGS, "Pause VMD animation"},
    {"stopVmd", (PyCFunction)PyModel_StopVmd, METH_VARARGS, "Stop VMD animation"},
    {"removeVmd", (PyCFunction)PyModel_RemoveVmd, METH_VARARGS, "Remove VMD animation"},
    {"playAllVmd", (PyCFunction)PyModel_PlayAllVmd, METH_VARARGS, "Play all VMD animations"},
    {"pauseAllVmd", (PyCFunction)PyModel_PauseAllVmd, METH_VARARGS, "Pause all VMD animations"},
    {"stopAllVmd", (PyCFunction)PyModel_StopAllVmd, METH_VARARGS, "Stop all VMD animations"},
    {"isVmdPlaying", (PyCFunction)PyModel_IsVmdPlaying, METH_VARARGS,
     "Check if any VMD is playing"},
    {"vmdTrackCount", (PyCFunction)PyModel_VmdTrackCount, METH_VARARGS, "Get VMD track count"},
    {"isVmdPlayingTrack", (PyCFunction)PyModel_IsVmdPlayingTrack, METH_VARARGS,
     "Check if specific VMD track is playing"},
    {"vmdCurrentFrame", (PyCFunction)PyModel_VmdCurrentFrame, METH_VARARGS,
     "Get current frame of VMD track"},
    {"vmdMaxFrame", (PyCFunction)PyModel_VmdMaxFrame, METH_VARARGS, "Get max frame of VMD track"},
    {"setVmdFrame", (PyCFunction)PyModel_SetVmdFrame, METH_VARARGS, "Set frame of VMD track"},
    {"loadVpd", (PyCFunction)PyModel_LoadVpd, METH_VARARGS, "Load VPD pose"},
    {"applyVpd", (PyCFunction)PyModel_ApplyVpd, METH_VARARGS, "Apply VPD pose"},
    {"resetPose", (PyCFunction)PyModel_ResetPose, METH_VARARGS, "Reset to bind pose"},
    {"syncVpdPose", (PyCFunction)PyModel_SyncVpdPose, METH_VARARGS, "Sync VPD pose"},
    {"vpdApplied", (PyCFunction)PyModel_VpdApplied, METH_VARARGS, "Check if VPD is applied"},
    {"removeVpd", (PyCFunction)PyModel_RemoveVpd, METH_VARARGS, "Remove VPD pose"},
    {"showModel", (PyCFunction)PyModel_ShowModel, METH_VARARGS, "Show/hide model"},
    {"showOutline", (PyCFunction)PyModel_ShowOutline, METH_VARARGS, "Show/hide outline"},
    {"showToon", (PyCFunction)PyModel_ShowToon, METH_VARARGS, "Show/hide toon shading"},
    {"getShowModel", (PyCFunction)PyModel_GetShowModel, METH_VARARGS, "Get model visibility"},
    {"getShowOutline", (PyCFunction)PyModel_GetShowOutline, METH_VARARGS, "Get outline visibility"},
    {"getShowToon", (PyCFunction)PyModel_GetShowToon, METH_VARARGS, "Get toon visibility"},
    {"isSkinned", (PyCFunction)PyModel_IsSkinned, METH_VARARGS, "Check if skinned"},
    {"setSkinning", (PyCFunction)PyModel_SetSkinning, METH_VARARGS, "Enable/disable skinning"},
    {"setMorphWeight", (PyCFunction)PyModel_SetMorphWeight, METH_VARARGS, "Set morph weight"},
    {"savedMorphWeight", (PyCFunction)PyModel_SavedMorphWeight, METH_VARARGS,
     "Get saved morph weight"},
    {"clearMorphs", (PyCFunction)PyModel_ClearMorphs, METH_VARARGS, "Clear all morphs"},
    {"setMorphWeights", (PyCFunction)PyModel_SetMorphWeights, METH_VARARGS,
     "Set multiple morph weights from dict"},
    {"setIdleBlink", (PyCFunction)PyModel_SetIdleBlink, METH_VARARGS, "Enable/disable idle blink"},
    {"morphCount", (PyCFunction)PyModel_MorphCount, METH_VARARGS, "Get morph count"},
    {"interactableMorphs", (PyCFunction)PyModel_InteractableMorphs, METH_VARARGS,
     "Get interactable morph indices"},
    {"morphName", (PyCFunction)PyModel_MorphName, METH_VARARGS, "Get morph name by index"},
    {"modelName", (PyCFunction)PyModel_ModelName, METH_VARARGS, "Get model name"},
    {"modelScale", (PyCFunction)PyModel_ModelScale, METH_VARARGS, "Get model scale"},
    {"modelMatrix", (PyCFunction)PyModel_ModelMatrix, METH_VARARGS, "Get model matrix (16 floats)"},
    {NULL, NULL, 0, NULL}};

static PyGetSetDef PyModel_getset[] = {{NULL, NULL, NULL, NULL, NULL}};

PyType_Slot PyModel_slots[] = {
    {Py_tp_new, (void*)PyModel_new},         {Py_tp_init, (void*)PyModel_init},
    {Py_tp_dealloc, (void*)PyModel_dealloc}, {Py_tp_methods, PyModel_methods},
    {Py_tp_getset, PyModel_getset},          {0, NULL}};

PyType_Spec PyModel_spec = {"_angeloid.Model", sizeof(PyModelObject), 0,
                            Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, PyModel_slots};
