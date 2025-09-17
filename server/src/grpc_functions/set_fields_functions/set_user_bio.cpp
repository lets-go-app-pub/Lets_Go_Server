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
#include "server_parameter_restrictions.h"
#include "update_single_other_user.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setUserBioImplementation(
        setfields::SetBioRequest* request,
        setfields::SetFieldResponse* response
);

void setUserBio(
        setfields::SetBioRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setUserBioImplementation(request, response);
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

void setUserBioImplementation(
        setfields::SetBioRequest* request,
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

    trimTrailingWhitespace(*request->mutable_set_string());

    if (request->set_string().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO) {
        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("User bio was too large.");
        } else { //user
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        }
        return;
    }

    if (request->set_string().empty()) {
        request->set_set_string("~");
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::types::b_date mongodb_current_date{current_timestamp};

    bsoncxx::builder::stream::document merge_document;
    merge_document
            << user_account_keys::BIO_TIMESTAMP << mongodb_current_date
            << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << mongodb_current_date
            << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << buildUpdateSingleOtherUserProjectionDoc<true>(mongodb_current_date)
            << user_account_keys::BIO << open_document
                << "$literal" << request->set_string()
            << close_document;

    const auto store_info_to_user_statistics = [&](
            mongocxx::client& mongo_cpp_client,
            mongocxx::database& accounts_db
    ) {
        auto push_update_doc = document{}
                << user_account_statistics_keys::BIOS << open_document
                    << user_account_statistics_keys::bios::BIOS << request->set_string()
                    << user_account_statistics_keys::bios::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
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
                const ReturnStatus& returnStatus
        ) {
            response->set_return_status(returnStatus);
        };

        const auto set_success = [&response, &current_timestamp](
                const bsoncxx::document::view& /*user_account_doc_view*/
        ) {
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp(current_timestamp.count());
        };

        grpcValidateLoginFunctionTemplate<false>(
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

        const auto error_func = [&response](
                const std::string& error_str
        ) {
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
