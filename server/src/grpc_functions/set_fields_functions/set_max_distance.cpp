//
// Created by jeremiah on 3/19/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <set_fields_helper_functions/set_fields_helper_functions.h>
#include <grpc_function_server_template.h>
#include <handle_function_operation_exception.h>
#include <admin_functions_for_set_values.h>
#include <store_info_to_user_statistics.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "server_parameter_restrictions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setMaxDistanceImplementation(
        const setfields::SetMaxDistanceRequest* request,
        setfields::SetFieldResponse* response
);

void setMaxDistance(
        const setfields::SetMaxDistanceRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setMaxDistanceImplementation(request, response);
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

void setMaxDistanceImplementation(
        const setfields::SetMaxDistanceRequest* request,
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

    int matching_distance = request->max_distance();

    //error check matching distance
    if (matching_distance < server_parameter_restrictions::MINIMUM_ALLOWED_DISTANCE) {
        matching_distance = server_parameter_restrictions::MINIMUM_ALLOWED_DISTANCE;
    } else if (matching_distance > server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE) {
        matching_distance = server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document merge_document;

    appendDocumentToClearByDistanceAggregation(
            merge_document,
            matching_distance
    );

    merge_document
            << user_account_keys::MAX_DISTANCE << bsoncxx::types::b_int32{matching_distance}
            << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << bsoncxx::types::b_date{current_timestamp};

    const auto store_info_to_user_statistics = [&](
            mongocxx::client& mongo_cpp_client,
            mongocxx::database& accounts_db
    ) {

        const bsoncxx::document::value push_update_doc = document{}
                << user_account_statistics_keys::MAX_DISTANCES << open_document
                        << user_account_statistics_keys::max_distances::MAX_DISTANCE << bsoncxx::types::b_int32{matching_distance}
                        << user_account_statistics_keys::max_distances::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
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

        auto setSuccess = [&response, &current_timestamp](
                const bsoncxx::document::view& /*user_account_doc_view*/) {
            response->set_timestamp(current_timestamp.count());
            response->set_return_status(ReturnStatus::SUCCESS);
        };

        grpcValidateLoginFunctionTemplate<false>(
                user_account_oid_str,
                login_token_str,
                installation_id,
                current_timestamp,
                merge_document.view(),
                set_return_status,
                setSuccess,
                nullptr,
                store_info_to_user_statistics
        );

    } else { //admin request

        auto success_func = [&response, &current_timestamp]() {
            response->set_return_status(SUCCESS);
            response->set_timestamp(current_timestamp.count());
        };

        auto error_func = [&response](const std::string& error_str) {
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
