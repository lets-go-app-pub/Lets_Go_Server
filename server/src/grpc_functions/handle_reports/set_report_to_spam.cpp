//
// Created by jeremiah on 9/21/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <AdminLevelEnum.grpc.pb.h>
#include <store_mongoDB_error_and_exception.h>

#include "handle_reports.h"


#include "utility_general_functions.h"
#include "admin_privileges_vector.h"
#include "connection_pool_global_variable.h"
#include "handle_function_operation_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setReportToSpamImplementation(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
);

void setReportToSpam(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setReportToSpamImplementation(request, response);
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request);
}

void setReportToSpamImplementation(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    auto error_func = [&response](const std::string& error_str) {
        response->set_successful(false);
        response->set_error_message(error_str);
    };

    {
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

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            response->set_successful(false);
            error_func("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        } else if (!admin_info_doc_value) {
            error_func("Could not find admin document.");
            return;
        }

    }

    AdminLevelEnum admin_level;

    {
        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();

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
            error_func("Error stored on server.");
            return;
        }
    }

    if (!admin_privileges[admin_level].handle_reports()) {
        error_func("Admin level " + AdminLevelEnum_Name(admin_level) +
                   " does not have 'handle_reports' access.");
        return;
    }

    if (isInvalidOIDString(request->user_oid())) {
        error_func("User id '" + request->user_oid() +
                   "' is invalid.");
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    try {
        bsoncxx::stdx::optional<mongocxx::result::update> admin_update;

        //NOTE: This was found above when login info was checked, it should exist (although
        // technically it COULD have been removed in between).
        admin_update = admin_accounts_collection.update_one(
                document{}
                        << admin_account_key::NAME << request->login_info().admin_name()
                << finalize,
                document{}
                        << "$inc" << open_document
                                << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << 1
                        << close_document
                << finalize
        );

        if (!admin_update) {
            error_func("Error updating admin account.\n!admin_update received.");
            return;
        } else if (admin_update->modified_count() != 1) {
            error_func(
                    "Error updating admin account.\nmodified_count: " + std::to_string(admin_update->modified_count()) +
                    ".");
            return;
        }

        user_accounts_collection.update_one(
                document{}
                        << "_id" << bsoncxx::oid{request->user_oid()}
                << finalize,
                document{}
                        << "$inc" << open_document
                                << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << 1
                        << close_document
                << finalize
        );

        //NOTE: if the user account OID is found or not doesn't really matter, the account could easily have been
        // removed

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME,
                "admin_account_name", request->login_info().admin_name()
        );
        error_func("Error stored on server. Exception inside set_report_to_spam.cpp.");
        return;
    }

    response->set_successful(true);

}