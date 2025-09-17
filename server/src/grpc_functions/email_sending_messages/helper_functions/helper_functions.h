//
// Created by jeremiah on 8/16/21.
//
#pragma once

#include "Python.h"

#include <functional>

#include <mongocxx/collection.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/oid.hpp>

struct SendEmailInterface {
    //A GIL is expected to be locked before this is called.
    virtual PyObject* sendEmail(PyObject* pFunc, PyObject* pArgs) = 0;
};

struct SendEmailProduction : public SendEmailInterface {
    //A GIL is expected to be locked before this is called.
    PyObject* sendEmail(PyObject* pFunc, PyObject* pArgs) override {
        return PyObject_CallObject(pFunc, pArgs);
    }
};

struct DummySendEmailProduction : public SendEmailInterface {
    PyObject* sendEmail(PyObject*, PyObject*) override {
        return Py_BuildValue(
                "(lss)",
                202L, //202 is success
                "dummy_response_headers",
                "dummy_response_body"
        );
    }
};

//sends an email with the passed information
//returns false if an error occurs and the email fails to send
//returns true if the email is sent successfully
bool send_email_helper(
        const std::string& email_prefix,
        const std::string& email_address,
        const std::string& email_verification_subject,
        const std::string& email_verification_content,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        SendEmailInterface* send_email_object
);

//stores the document inside the passed collection, if an exception is thrown in which
// the key already exists, then one more attempt will be made before the exception is thrown
//returns true if successfully inserted (upserted) document
//returns false if failed to upsert document
bool attempt_to_generate_unique_code_twice(
        std::string& randomly_generated_code,
        mongocxx::collection& collection_to_insert_to,
        const bsoncxx::document::view& find_document,
        const std::function<bsoncxx::document::value(const std::string& code)>& generate_update_document
        );