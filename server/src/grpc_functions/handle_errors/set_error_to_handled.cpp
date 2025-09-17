//
// Created by jeremiah on 11/4/21.
//

#include "handle_errors.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "fresh_errors_keys.h"
#include "handled_errors_list_keys.h"
#include "server_parameter_restrictions.h"

#include <StatusEnum.pb.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>
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

void setErrorToHandledImplementation(
        const handle_errors::SetErrorToHandledRequest* request,
        handle_errors::SetErrorToHandledResponse* response
);

void setErrorToHandled(
        const handle_errors::SetErrorToHandledRequest* request,
        handle_errors::SetErrorToHandledResponse* response
) {

    handleFunctionOperationException(
            [&] {
                setErrorToHandledImplementation(request, response);
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

void setErrorToHandledImplementation(
        const handle_errors::SetErrorToHandledRequest* request,
        handle_errors::SetErrorToHandledResponse* response
) {

    auto return_error_to_client = [&](const std::string& error_message) {
        response->set_success(false);
        response->set_error_msg(error_message);
    };
    std::string admin_name;

    {
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

        const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            return_error_to_client(
                    "ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        } else if (!admin_info_doc_value) {
            return_error_to_client("Could not find admin document.");
            return;
        }

        bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
        AdminLevelEnum admin_level;

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

    }

    if (!errorParametersGood(request->error_parameters(), return_error_to_client)) {
        //error was already stored
        return;
    }

    if (request->reason_for_description().size() < server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON < request->reason_for_description().size()) {
        return_error_to_client("Passed description '" + request->reason_for_description() + "' was invalid.");
        return;
    }

    if (!ErrorHandledMoveReason_IsValid(request->reason())) {
        return_error_to_client("Passed reason '" + std::to_string(request->reason()) + "' was invalid.");
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];
    mongocxx::collection handled_errors_list_collection = errors_db[collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    //Thoughts on 'should a transaction be used for the operations?'.
    // The problems arise with how sendErrorToServerMongoDb(), storeMongoDBErrorAndException() and the python
    // web server handle errors. They first check the HANDLED_ERRORS_LIST_COLLECTION to see if the error is 'handled'.
    // Then if it is not handled they will store the error inside FRESH_ERRORS_COLLECTION_NAME. However, because
    // there is a gap between these two actions, it is possible that something slips in between them. Also read/write
    // concern should be taken into account. It is possible that it was committed, however, not distributed to the
    // entire replica set yet.
    // Normally the solution is fairly straightforward. This function to set errors to handled will be called less
    // than the functions to store errors and so this function would be wrapped inside a transaction. However, inside
    // setMatchDocToHandled() (called below) when moving the documents from FRESH_ERRORS_COLLECTION to
    // HANDLED_ERRORS_LIST_COLLECTION it could potentially run a large amount of inserts. This is discouraged inside
    // a transaction, also the code needs to be re-written because it uses a merge stage at the end of the pipeline
    // (which is not allowed inside a transaction).
    // The end conclusion is that a message slipping through (leaking) is not that big of a deal. Even if it does
    // happen the user of the desktop interface can see it and (hopefully) handle it at that point.

    if(request->reason() != ErrorHandledMoveReason::ERROR_HANDLED_REASON_DELETE_ALL_DO_NOT_SET_HANDLED) {

        try {
            mongocxx::options::update opts;
            opts.upsert(true);

            //upsert a document
            handled_errors_list_collection.update_one(
                document{}
                    << handled_errors_list_keys::ERROR_ORIGIN << request->error_parameters().error_origin()
                    << handled_errors_list_keys::VERSION_NUMBER << (int) request->error_parameters().version_number()
                    << handled_errors_list_keys::FILE_NAME << request->error_parameters().file_name()
                    << handled_errors_list_keys::LINE_NUMBER << (int) request->error_parameters().line_number()
                << finalize,
                document{}
                    << "$setOnInsert" << open_document
                        << handled_errors_list_keys::ERROR_ORIGIN << request->error_parameters().error_origin()
                        << handled_errors_list_keys::VERSION_NUMBER << (int) request->error_parameters().version_number()
                        << handled_errors_list_keys::FILE_NAME << request->error_parameters().file_name()
                        << handled_errors_list_keys::LINE_NUMBER << (int) request->error_parameters().line_number()
                        << handled_errors_list_keys::ERROR_HANDLED_MOVE_REASON << request->reason()
                        << handled_errors_list_keys::DESCRIPTION << request->reason_for_description()
                    << close_document
                << finalize,
                opts
            );

        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::HANDLED_ERRORS_LIST_COLLECTION_NAME,
                "request", request->DebugString()
            );

            response->set_error_msg("Exception thrown when inserting error to handled collection.");
            return;
        }
    }

    auto match_doc = document{}
            << fresh_errors_keys::ERROR_ORIGIN << request->error_parameters().error_origin()
            << fresh_errors_keys::VERSION_NUMBER << (int) request->error_parameters().version_number()
            << fresh_errors_keys::FILE_NAME << request->error_parameters().file_name()
            << fresh_errors_keys::LINE_NUMBER << (int) request->error_parameters().line_number()
            << finalize;

    if(!setMatchDocToHandled<true>(
            request->reason_for_description(),
            return_error_to_client,
            admin_name,
            fresh_errors_collection,
            current_timestamp,
            match_doc)
    ) {
        //error already set
        return;
    }

    response->set_success(true);
}
