//
// Created by jeremiah on 11/3/21.
//

#include "handle_errors.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "server_parameter_restrictions.h"

#include <StatusEnum.pb.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <handle_function_operation_exception.h>
#include <utility_general_functions.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <connection_pool_global_variable.h>
#include <store_mongoDB_error_and_exception.h>
#include <extract_data_from_bsoncxx.h>
#include <helper_functions/handle_errors_helper_functions.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void deleteSingleErrorImplementation(
        const handle_errors::DeleteSingleErrorRequest* request,
        handle_errors::DeleteSingleErrorResponse* response
);

void deleteSingleError(
        const handle_errors::DeleteSingleErrorRequest* request,
        handle_errors::DeleteSingleErrorResponse* response
) {
    handleFunctionOperationException(
            [&] {
                deleteSingleErrorImplementation(request, response);
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request);
}

void deleteSingleErrorImplementation(
        const handle_errors::DeleteSingleErrorRequest* request,
        handle_errors::DeleteSingleErrorResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id,
            store_error_message
    );

    auto return_error_to_client = [&](const std::string& error_message) {
        response->set_success(false);
        response->set_error_msg(error_message);
    };

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        return_error_to_client("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
        return;
    } else if (!admin_info_doc_value) {
        return_error_to_client("Could not find admin document.");
        return;
    }

    const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;
    std::string admin_name;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element
        && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                admin_privilege_element, admin_info_doc_view,
                bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        return_error_to_client("Error stored on server.");
        return;
    }

    auto admin_name_element = admin_info_doc_view[admin_account_key::NAME];
    if (admin_name_element
        && admin_name_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        admin_name = admin_name_element.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(
                __LINE__, __FILE__,
                admin_name_element, admin_info_doc_view,
                bsoncxx::type::k_utf8, admin_account_key::NAME,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );

        return_error_to_client("Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].handle_error_messages()) {
        return_error_to_client("Admin level " + AdminLevelEnum_Name(admin_level) +
                               " does not have 'request_error_messages' access.");
        return;
    }

    if (isInvalidOIDString(request->error_oid())) {
        return_error_to_client("Passed oid '" + request->error_oid() + "' was invalid.");
        return;
    }

    if (request->reason_for_description().size() < server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON < request->reason_for_description().size()) {
        return_error_to_client("Passed description '" + request->reason_for_description() + "' was invalid.");
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_Collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::oid error_oid{request->error_oid()};

    auto match_doc = document{}
            << "_id" << error_oid
            << finalize;

    //Can see comments inside set_error_to_handled.cpp for more info on why a transaction is not used here.

    if(!setMatchDocToHandled<false>(
            request->reason_for_description(),
            return_error_to_client,
            admin_name,
            fresh_errors_Collection,
            current_timestamp,
            match_doc)
    ) {
        //error already set
        return;
    }

    response->set_success(true);

}


