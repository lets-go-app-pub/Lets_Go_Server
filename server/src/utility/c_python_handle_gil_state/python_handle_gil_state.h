//
// Created by jeremiah on 8/29/22.
//

#pragma once

#include "Python.h"
#include <vector>

struct PythonHandleGilState {

    PyGILState_STATE gil_state;

    PythonHandleGilState() {
        //locks this thread for python instance
        gil_state = PyGILState_Ensure();
    }
    PythonHandleGilState(const PythonHandleGilState& copy) = delete;
    PythonHandleGilState(PythonHandleGilState& move) = delete;

    PythonHandleGilState& operator=(const PythonHandleGilState& copy) = delete;

    ~PythonHandleGilState() {
        //Must be done BEFORE the gil state is released.
        for(auto& x :object_to_dereference) {
            if(x) { Py_DECREF(x); }
        }

        //unlocks thread for python instance
        PyGILState_Release(gil_state);
    }

    void automaticallyDecrementObjectReference(PyObject* obj) {
        object_to_dereference.emplace_back(obj);
    }

private:

    //NOTE: This method of handling cPython references is fairly simplistic. It also doesn't have much
    // extendability to more complex situations (such as when using PyTuple_SetItem() because it
    // steals the reference instead of borrowing it). However, for most situations where the user is
    // granted control over the reference, it will work well (for example when using Py_BuildValue).
    //Can read more about reference counts in cPython in links below.
    // https://docs.python.org/3/c-api/intro.html#objects-types-and-reference-counts
    // https://python.readthedocs.io/en/v2.7.2/extending/extending.html#reference-counts
    std::vector<PyObject*> object_to_dereference;

};