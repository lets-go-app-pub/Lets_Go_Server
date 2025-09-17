//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <grpc_function_server_template.h>
#include <request_helper_functions.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "bsoncxx/builder/stream/document.hpp"


#include "request_fields_functions.h"
#include "utility_general_functions.h"
#include "database_names.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void requestServerActivitiesImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::ServerActivitiesResponse* response
);

void requestServerActivities(
        const request_fields::InfoFieldRequest* request,
        request_fields::ServerActivitiesResponse* response
) {

    handleFunctionOperationException(
            [&] {
                requestServerActivitiesImplementation(request, response);
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

void requestServerActivitiesImplementation(
        const request_fields::InfoFieldRequest* request,
        request_fields::ServerActivitiesResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc [[maybe_unused]],
            const std::string& passed_error_message [[maybe_unused]]) {
    };

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ADMIN_AND_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id,
            store_error_message
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN);

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const bsoncxx::document::value merge_document = document{} << finalize;

    auto set_return_status = [&response](const ReturnStatus& return_status) {
        response->set_return_status(return_status);
    };

    /** NOTE: This does not need a transaction because the activities and categories are not part of the 'account'. **/

    auto set_success = [&response, &current_timestamp](const bsoncxx::document::view& /*user_account_doc_view*/) {

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

        if (requestAllServerCategoriesActivitiesHelper(
                response->mutable_server_categories(),
                response->mutable_server_activities(),
                accounts_db)
                ) { //if categories activities successfully returned
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp(current_timestamp.count());
        } else { //if function failed
            response->set_return_status(ReturnStatus::LG_ERROR);
        }

    };

    if (!request->login_info().admin_info_used()) { //if login from android

        grpcValidateLoginFunctionTemplate<true>(
                user_account_oid_str,
                login_token_str,
                installation_id,
                current_timestamp,
                merge_document,
                set_return_status,
                set_success
        );
    } else { //if login from desktop interface
        set_success(document{} << finalize);
    }

}

