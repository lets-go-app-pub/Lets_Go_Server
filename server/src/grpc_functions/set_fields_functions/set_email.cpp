//
// Created by jeremiah on 3/19/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <grpc_function_server_template.h>
#include <handle_function_operation_exception.h>
#include <admin_functions_for_set_values.h>
#include <store_info_to_user_statistics.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setEmailImplementation(
        const setfields::SetEmailRequest* request,
        setfields::SetFieldResponse* response
);

void setEmail(
        const setfields::SetEmailRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setEmailImplementation(request, response);
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

void setEmailImplementation(
        const setfields::SetEmailRequest* request,
        setfields::SetFieldResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const auto store_error_message = [&](
            bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
            const std::string& passed_error_message
    ) {
        response->set_error_string(passed_error_message);
        admin_info_doc_value = std::move(returned_admin_info_doc);
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
    } else if (request->login_info().admin_info_used()) {
        if(!checkForUpdateUserPrivilege(response->mutable_error_string(), admin_info_doc_value)) {
            response->set_return_status(LG_ERROR);
            return;
        } else if(isInvalidOIDString(user_account_oid_str)) {
            response->set_return_status(INVALID_USER_OID);
            return;
        }
    }

    const std::string& email = request->set_email(); //email address
    if (isInvalidEmailAddress(email)) { //if email is invalid
        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("Invalid email address info passed.");
        } else { //user
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        }
        return;
    }

    //NOTE: It is possible that the server updates an email address to the exact same one
    // setting it to not verified. However, this case should be checked for on the client and the
    // desktop interface.

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document merge_document;
    merge_document
        << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << bsoncxx::types::b_bool{true}
        << user_account_keys::EMAIL_TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
        << user_account_keys::EMAIL_ADDRESS << open_document
            << "$literal" << bsoncxx::types::b_string{email}
        << close_document;

    const auto store_info_to_user_statistics = [&](
            mongocxx::client& mongo_cpp_client,
            mongocxx::database& accounts_db
    ) {
        auto push_update_doc = document{}
            << user_account_statistics_keys::EMAIL_ADDRESSES << open_document
                << user_account_statistics_keys::email_addresses::EMAIL << email
                << user_account_statistics_keys::email_addresses::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
            << close_document
        << finalize;

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                bsoncxx::oid{user_account_oid_str},
                push_update_doc,
                current_timestamp
        );
    };

    if (!request->login_info().admin_info_used()) { //user request

        const auto set_return_status = [&response](
                const ReturnStatus& return_status
        ) {
            response->set_return_status(return_status);
        };

        const auto set_success = [&response, &current_timestamp](
                const bsoncxx::document::view& /*user_account_doc_view*/) {
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp(current_timestamp.count());
        };

        grpcValidateLoginFunctionTemplate<true>(
                user_account_oid_str,
                login_token_str,
                installation_id,
                current_timestamp,
                merge_document,
                set_return_status,
                set_success,
                nullptr,
                store_info_to_user_statistics
        );

    } else { //admin request

        const auto success_func = [&response, &current_timestamp]() {
            response->set_return_status(SUCCESS);
            response->set_timestamp(current_timestamp.count());
        };

        const auto error_func = [&response](const std::string& error_str) {
            response->set_return_status(LG_ERROR);
            response->set_error_string(error_str);
        };

        updateUserAccountWithDocument(
                user_account_oid_str,
                merge_document,
                success_func,
                error_func,
                store_info_to_user_statistics
        );
    }
}


