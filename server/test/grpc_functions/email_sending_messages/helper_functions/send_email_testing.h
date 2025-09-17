//
// Created by jeremiah on 8/29/22.
//

#pragma once

#include <string>
#include <vector>
#include "Python.h"
#include "python_handle_gil_state.h"
#include "helper_functions/helper_functions.h"

struct SendEmailTesting : public SendEmailInterface {

    struct Parameters {
        PyObject* pFunc;
        PyObject* pArgs;

        Parameters() = delete;

        Parameters(
                PyObject* _pFunc,
                PyObject* _pArgs
        ) : pFunc(_pFunc),
            pArgs(_pArgs) {}
    };

    std::vector<Parameters> sent_emails;

    long status_code = 202; //202 is success inside send_email
    std::string response_headers;
    std::string response_body;

    PyObject* sendEmail(PyObject* pFunc, PyObject* pArgs) override {
        Py_INCREF(pFunc);
        Py_INCREF(pArgs);
        sent_emails.emplace_back(Parameters{pFunc, pArgs});
        return Py_BuildValue(
                "(lss)",
                status_code,
                response_headers.c_str(),
                response_body.c_str()
        );
    }

    ~SendEmailTesting() {
        //Locks this thread for python instance before forcing decrement reference.
        PythonHandleGilState gil_state;
        for(auto& x : sent_emails) {
            if(x.pFunc) { gil_state.automaticallyDecrementObjectReference(x.pFunc); }
            if(x.pArgs) { gil_state.automaticallyDecrementObjectReference(x.pArgs); }
        }
    }
};