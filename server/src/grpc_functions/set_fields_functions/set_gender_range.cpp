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
#include "general_values.h"
#include "specific_match_queries/specific_match_queries.h"
#include "run_initial_login_with_match_parameter.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;


void setGenderRangeImplementation(
        const setfields::SetGenderRangeRequest* request,
        setfields::SetFieldResponse* response
);

void setGenderRange(
        const setfields::SetGenderRangeRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setGenderRangeImplementation(request, response);
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

void setGenderRangeImplementation(
        const setfields::SetGenderRangeRequest* request,
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

    bsoncxx::builder::basic::array mongo_arr_gender_range_aggregation;
    bsoncxx::builder::basic::array mongo_arr_gender_range;
    bool user_matches_with_everyone = false;

    std::vector<std::string> gender_range_vector;

    extractRepeatedGenderProtoToVector(
        request->gender_range(),
            gender_range_vector,
            user_matches_with_everyone
    );

    if(gender_range_vector.empty()) {
        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("User gender range had no valid genders stored.");
        } else { //user
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        }
        return;
    }

    for (const auto& gender: gender_range_vector) {
        mongo_arr_gender_range.append(gender);

        mongo_arr_gender_range_aggregation.append(
                document{}
                        << "$literal" << gender
                        << finalize
        );
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bsoncxx::builder::stream::document match_parameters_to_query;
    matchGenderInsideUserGenderRangeOrEvent(
            match_parameters_to_query,
            mongo_arr_gender_range,
            user_matches_with_everyone,
            false
    );

    bsoncxx::builder::stream::document fields_to_update_document;
    fields_to_update_document
            << user_account_keys::GENDERS_RANGE << mongo_arr_gender_range_aggregation
            << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << bsoncxx::types::b_date{current_timestamp};

    const auto set_return_status = [&](
            const ReturnStatus& return_status,
            const std::string& error_message
    ){
        if(request->login_info().admin_info_used()){
            response->set_error_string(error_message);
        }
        response->set_return_status(return_status);
    };

    const auto set_success = [&]() {
        response->set_timestamp(current_timestamp.count());
        response->set_return_status(ReturnStatus::SUCCESS);
    };

    const auto save_statistics = [&](
            mongocxx::client& mongo_cpp_client,
            mongocxx::database& accounts_db
    ) {
        const bsoncxx::document::value push_update_doc = document{}
                << user_account_statistics_keys::GENDER_RANGES << open_document
                    << user_account_statistics_keys::gender_ranges::GENDER_RANGE << mongo_arr_gender_range
                    << user_account_statistics_keys::gender_ranges::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
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

    //The function will handle setting the return status through the lambda, no reason
    // to directly check for failure.
    grpcValidateLoginFunctionWithMatchParametersTemplate<false>(
            user_account_oid_str,
            login_token_str,
            installation_id,
            request->login_info().admin_info_used(),
            current_timestamp,
            match_parameters_to_query.view(),
            fields_to_update_document.view(),
            set_return_status,
            set_success,
            save_statistics
    );
}
