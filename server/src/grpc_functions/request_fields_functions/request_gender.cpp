//
// Created by jeremiah on 3/19/21.
//
#include <request_helper_functions.h>
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

void requestGenderImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
);

void requestGender(
        const request_fields::InfoFieldRequest* request,
        request_fields::InfoFieldResponse* response
) {

    handleFunctionOperationException(
            [&] {
                requestGenderImplementation(request, response);
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

void requestGenderImplementation(
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
            << user_account_keys::GENDER << 1;

    auto set_return_status = [&response](const ReturnStatus& return_status){
        response->set_return_status(return_status);
    };

    auto set_success = [&response, &current_timestamp](
            const bsoncxx::document::view& user_account_doc_view
    ){
        if(requestGenderHelper(
                user_account_doc_view,
                response->mutable_return_string()))
        { //if gender successfully returned
            response->set_timestamp(current_timestamp.count());
            response->set_return_status(ReturnStatus::SUCCESS);
        }
        else { //if function failed
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
