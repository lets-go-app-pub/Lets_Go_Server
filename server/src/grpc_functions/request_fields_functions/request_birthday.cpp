//
// Created by jeremiah on 3/19/21.
//
#include <request_helper_functions.h>
#include <global_bsoncxx_docs.h>
#include <grpc_function_server_template.h>
#include <handle_function_operation_exception.h>

#include "bsoncxx/builder/stream/document.hpp"


#include "request_fields_functions.h"
#include "utility_general_functions.h"
#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void requestBirthdayImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::BirthdayResponse* response
);

void requestBirthday(
        const request_fields::InfoFieldRequest* request,
        request_fields::BirthdayResponse* response
) {

    handleFunctionOperationException(
            [&] {
                requestBirthdayImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            },
            __LINE__, __FILE__, request
    );
}

void requestBirthdayImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::BirthdayResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN);

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const bsoncxx::document::value age_calculator_document = buildDocumentToCalculateAge(current_timestamp);

    bsoncxx::document::value merge_document = document{}
                //calculate age before returning
                << user_account_keys::AGE << age_calculator_document.view()
            << finalize;

    std::shared_ptr<document> projection_document = std::make_shared<document>();

    (*projection_document)
            << user_account_keys::BIRTH_YEAR << 1
            << user_account_keys::BIRTH_MONTH << 1
            << user_account_keys::BIRTH_DAY_OF_MONTH << 1
            << user_account_keys::AGE << 1;

    auto set_return_status = [&response](const ReturnStatus& return_status) {
        response->set_return_status(return_status);
    };

    auto set_success = [&response, &current_timestamp](const bsoncxx::document::view& user_account_doc_view) {
        if (requestBirthdayHelper(
                user_account_doc_view,
                response->mutable_birthday_info()
        )) { //function was successful
            response->set_timestamp(current_timestamp.count());
            response->set_return_status(ReturnStatus::SUCCESS);
        } else { //function failed
            response->set_return_status(ReturnStatus::LG_ERROR);
        }
    };

    grpcValidateLoginFunctionTemplate<true>(
            user_account_oid_str,
            login_token_str,
            installation_id,
            current_timestamp,
            merge_document,
            set_return_status,
            set_success,
            projection_document
    );

}

