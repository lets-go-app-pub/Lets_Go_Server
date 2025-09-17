//
// Created by jeremiah on 9/20/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <report_handled_move_reason.h>
#include <report_helper_functions.h>

#include "handle_reports.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void dismissReportImplementation(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
);

void dismissReport(
        const handle_reports::ReportUnaryCallRequest* request,
        handle_reports::ReportResponseUnaryCallResponse* response
) {
    handleFunctionOperationException(
            [&] {
                dismissReportImplementation(request, response);
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

void dismissReportImplementation(
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
        std::string login_token_str;
        std::string installation_id;
        std::string user_account_oid_str;

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
        if (admin_privilege_element &&
            admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
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
        error_func("User id '" + request->user_oid() + "' is invalid.");
        return;
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    moveOutstandingReportsToHandled(
            mongo_cpp_client,
            current_timestamp,
            bsoncxx::oid{request->user_oid()},
            request->login_info().admin_name(),
            ReportHandledMoveReason::REPORT_HANDLED_REASON_REPORTS_DISMISSED
    );

    response->set_successful(true);
}