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

void setFirstNameImplementation(
        const setfields::SetStringRequest* request,
        setfields::SetFieldResponse* response
);

void setFirstName(
        const setfields::SetStringRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setFirstNameImplementation(request, response);
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

void setFirstNameImplementation(
        const setfields::SetStringRequest* request,
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

    std::string first_name = request->set_string(); //first name

    //name is required to be at least size 2 and made up of only characters of the alphabet
    if (first_name.size() < 2 || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME < first_name.size()) {
        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("Name must be between size " + std::to_string(2) +
                                       " and " + std::to_string(
                    server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME) +
                                       ".\nNOTE: Bytes mean characters for the alphabet.");
        } else { //user
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        }
        return;
    }

    for (char c : first_name) {
        if (!isalpha(c)) {
            if (request->login_info().admin_info_used()) { //admin
                response->set_return_status(ReturnStatus::LG_ERROR);
                response->set_error_string(
                        "Name must be made up of only characters of the alphabet.\n'" + std::to_string(c) +
                        "' is invalid.");
            } else { //user
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            }
            return;
        }
    }

    //make the name lowercase with an upper case first letter
    std::transform(first_name.begin(), first_name.end(), first_name.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    first_name[0] = (char) std::toupper(first_name[0]);

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::types::b_date mongodb_current_date{current_timestamp};

    bsoncxx::builder::stream::document merge_document;
    merge_document
        << user_account_keys::FIRST_NAME_TIMESTAMP << mongodb_current_date
        << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << buildUpdateSingleOtherUserProjectionDoc<true>(mongodb_current_date)
        << user_account_keys::FIRST_NAME << open_document
            << "$literal" << bsoncxx::types::b_string{first_name}
        << close_document;

    const auto store_info_to_user_statistics = [&](mongocxx::client& mongo_cpp_client, mongocxx::database& accounts_db) {

        auto push_update_doc = document{}
            << user_account_statistics_keys::NAMES << open_document
                << user_account_statistics_keys::names::NAME << first_name
                << user_account_statistics_keys::names::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
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
                const bsoncxx::document::view& /*user_account_doc_view*/
        ) {
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

        const auto success_func = [&response, &current_timestamp](){
            response->set_return_status(SUCCESS);
            response->set_timestamp(current_timestamp.count());
        };

        const auto error_func = [&response](const std::string& error_str){
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



