//
// Created by jeremiah on 3/19/21.
//

#include <grpc_function_server_template.h>
#include <handle_function_operation_exception.h>

#include "bsoncxx/builder/stream/document.hpp"


#include "request_fields_functions.h"
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void requestPhoneNumberImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
);

void requestPhoneNumber(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
) {

    handleFunctionOperationException(
            [&] {
                requestPhoneNumberImplementation(request, response);
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

void requestPhoneNumberImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
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

    bsoncxx::document::value merge_document = document{} << finalize;

    std::shared_ptr<document> projection_document = std::make_shared<document>();

    (*projection_document)
            << user_account_keys::PHONE_NUMBER << 1;

    auto set_return_status = [&response](const ReturnStatus& return_status) {
        response->set_return_status(return_status);
    };

    auto set_success = [&response](const bsoncxx::document::view& user_account_doc_view) {
        auto phone_number_element = user_account_doc_view[user_account_keys::PHONE_NUMBER];
        if (phone_number_element
            && phone_number_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
            std::string phone_number = phone_number_element.get_string().value.to_string();

            response->set_timestamp(-1); //phone number is a bit special, it doesn't have a timestamp (the timestamp for other requests is the current time)
            response->set_return_string(std::move(phone_number));
            response->set_return_status(ReturnStatus::SUCCESS);

        } else { //if element does not exist or is not type utf8
            logElementError(__LINE__, __FILE__, phone_number_element,
                            user_account_doc_view, bsoncxx::type::k_utf8, user_account_keys::PHONE_NUMBER,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

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
