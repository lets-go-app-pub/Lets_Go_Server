//
// Created by jeremiah on 8/16/21.
//

#include <Python.h>
#include <string>
#include <store_mongoDB_error_and_exception.h>
#include <store_info_to_user_statistics.h>

#include "python_send_email_module.h"
#include "helper_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_statistics_keys.h"
#include "thread_pool_global_variable.h"
#include "python_handle_gil_state.h"

bool send_email_helper(
        const std::string& email_prefix,
        const std::string& email_address,
        const std::string& email_verification_subject,
        const std::string& email_verification_content,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        SendEmailInterface* send_email_object
) {

    if (send_email_object == nullptr) {
        const std::string error_string = "SendEmailInterface was passed as null, this should never happen.\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "email_prefix", email_prefix,
                "email_verification_subject", email_verification_subject,
                "email_verification_content", email_verification_content,
                "user_account_oid", user_account_oid
        );

        return false;
    }

    //locks this thread for python instance
    PythonHandleGilState gil_state;

    PyObject* pFunc = PyObject_GetAttrString(send_email_module, (char*) "send_email_func");
    gil_state.automaticallyDecrementObjectReference(pFunc);

    //NOTE: The parenthesis in the format string guarantee that the result is a Python tuple.
    // Py_BuildValue is superior in a sense to PyTuple_Pack because it can take in the c type
    // values. Otherwise, the values must be converted to PyObject* before being sent in with
    // functions such as PyUnicode_FromString().
    PyObject* pArgs = Py_BuildValue(
            "(ssss)",
            email_prefix.c_str(),
            email_address.c_str(),
            email_verification_subject.c_str(),
            email_verification_content.c_str()
    );
    gil_state.automaticallyDecrementObjectReference(pArgs);

    if (!pArgs || !PyTuple_Check(pArgs) || !pFunc || !PyCallable_Check(pFunc)) {
        const std::string error_string = "A value from cPython was invalid when attempting to send email.\n"
                                  "pArgs: " + (pArgs ? std::string("true\nPyTuple_Check(Args): ").append(
                PyTuple_Check(pArgs) ? "true" : "false") : "false") + '\n' +
                                  "pFunc: " + (pFunc ? std::string("true\nPyCallable_Check(pFunc): ").append(
                PyCallable_Check(pFunc) ? "true" : "false") : "false") + '\n';

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "email_prefix", email_prefix,
                "email_verification_subject", email_verification_subject,
                "email_verification_content", email_verification_content,
                "user_account_oid", user_account_oid
        );

        return false;
    }

    PyObject* pReturnValue;

#if defined(_ACCEPT_SEND_EMAIL_COMMANDS) || defined(LG_TESTING)
    pReturnValue = send_email_object->sendEmail(pFunc, pArgs);
#else
    pReturnValue = DummySendEmailProduction().sendEmail(pFunc, pArgs);
#endif

    gil_state.automaticallyDecrementObjectReference(pReturnValue);

    std::string format;

    if (pReturnValue && PyTuple_Check(pReturnValue)) { //if the return value was a tuple

        long status_code = 0;
        char* response_headers = nullptr;
        char* response_body = nullptr;

        bool ok = PyArg_ParseTuple(pReturnValue, "lss", &status_code, &response_headers, &response_body);

        if (!ok || status_code != 202) { //email failed to send
            const std::string error_string = "Error occurred when sending email.\n";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "response_headers", response_headers ? std::string(response_headers) : "nullptr",
                    "response_body", response_body ? std::string(response_body) : "nullptr",
                    "ok", ok?"true":"false",
                    "status_code", std::to_string(status_code)
            );

            return false;
        }
        else {
            thread_pool.submit([
                                       email_address = email_address,
                                       email_prefix = email_prefix,
                                       email_verification_subject = email_verification_subject,
                                       email_verification_content = email_verification_content,
                                       currentTimestamp = currentTimestamp,
                                       user_account_oid = user_account_oid
                               ] {

                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

                //NOTE: Do not want database access running when the GIL for the python process is also locked. This is
                // essentially a mutex lock, and it prevents other threads from running python scrips when it is locked.

                auto push_update_doc =
                    bsoncxx::builder::stream::document{}
                        << user_account_statistics_keys::EMAIL_SENT_TIMES << bsoncxx::builder::stream::open_document
                            << user_account_statistics_keys::email_sent_times::EMAIL_ADDRESS << email_address
                            << user_account_statistics_keys::email_sent_times::EMAIL_PREFIX << email_prefix
                            << user_account_statistics_keys::email_sent_times::EMAIL_SUBJECT << email_verification_subject
                            << user_account_statistics_keys::email_sent_times::EMAIL_CONTENT << email_verification_content
                            << user_account_statistics_keys::email_sent_times::TIMESTAMP << bsoncxx::types::b_date{currentTimestamp}
                        << bsoncxx::builder::stream::close_document
                    << bsoncxx::builder::stream::finalize;

                storeInfoToUserStatistics(
                        mongo_cpp_client,
                        accounts_db,
                        user_account_oid,
                        push_update_doc,
                        currentTimestamp
                );

            });

        }

    } else { //return value was NOT a tuple
        const std::string error_string = "Return type from send_email inside send_email.py was NOT type Tuple.\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::PENDING_ACCOUNT_COLLECTION_NAME);

        return false;
    }

    return true;
}


