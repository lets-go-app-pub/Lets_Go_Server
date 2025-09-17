//
// Created by jeremiah on 8/10/21.
//

#include <python_send_sms_module.h>
#include <python_send_email_module.h>
#include <iostream>
#include "general_values.h"
#include "assert_macro.h"
#include <Python.h>

std::string getErrorLoadingModule(const std::string& module_name) {
    return "Unable to load " + module_name + " python module (" + module_name + ".py). "
            "This could mean several things including (but likely not limited to).\n"
            "1) An invalid module name was passed.\n"
            "2) The directory path was not properly set up before adding the module.\n"
            "3) The module itself has an error initializing (for example an import is not found).\n"
            "4) The module was not packaged with the virtual environment (systemd creates a virtual environment too).\n";
}

_ts* setupPythonModules() {

    Py_Initialize();
    PyRun_SimpleString("import sys");
    PyRun_SimpleString("import os");
    PyRun_SimpleString(general_values::PYTHON_PATH_STRING.c_str());

    //PyRun_SimpleString("print(sys.path)");

    const std::string send_sms_name = "send_sms";
    PyObject* send_sms_module_name = PyUnicode_FromString(send_sms_name.c_str());

    send_sms_module = PyImport_Import(send_sms_module_name);
    Py_DECREF(send_sms_module_name);
    assert_msg(send_sms_module, getErrorLoadingModule(send_sms_name));

    const std::string send_email_name = "send_email";
    PyObject* send_email_module_name = PyUnicode_FromString((char*) "send_email");

    send_email_module = PyImport_Import(send_email_module_name);
    Py_DECREF(send_email_module_name);
    assert_msg(send_email_module, getErrorLoadingModule(send_email_name));

    //NOTE: This must be done BEFORE any Python code is called.
    //NOTE: Do NOT call any python code from this thread unless PyEval_RestoreThread is called.
    return PyEval_SaveThread();
}