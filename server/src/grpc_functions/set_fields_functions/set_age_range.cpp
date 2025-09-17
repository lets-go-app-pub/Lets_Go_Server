//
// Created by jeremiah on 3/19/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <global_bsoncxx_docs.h>
#include <grpc_function_server_template.h>
#include <handle_function_operation_exception.h>
#include <admin_functions_for_set_values.h>
#include <store_mongoDB_error_and_exception.h>
#include <store_info_to_user_statistics.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"

#include "set_fields_helper_functions/set_fields_helper_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "server_parameter_restrictions.h"
#include "extract_data_from_bsoncxx.h"
#include "specific_match_queries/specific_match_queries.h"
#include "get_login_document.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setAgeRangeImplementation(
        const setfields::SetAgeRangeRequest* request,
        setfields::SetFieldResponse* response
);

void setAgeRange(
        const setfields::SetAgeRangeRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setAgeRangeImplementation(request, response);
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

void setAgeRangeImplementation(
        const setfields::SetAgeRangeRequest* request,
        setfields::SetFieldResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;

        auto store_error_message = [&](
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
        } else if (request->login_info().admin_info_used()
                   && !checkForUpdateUserPrivilege(response->mutable_error_string(), admin_info_doc_value)
                ) {
            response->set_return_status(LG_ERROR);
            return;
        }

    }

    int min_age = request->min_age();
    int max_age = request->max_age();

    //NOTE: the rest of the verification requires user age, so it is done below
    if (min_age < server_parameter_restrictions::LOWEST_ALLOWED_AGE) {
        min_age = server_parameter_restrictions::LOWEST_ALLOWED_AGE;
    } else if (min_age > server_parameter_restrictions::HIGHEST_ALLOWED_AGE) {
        min_age = server_parameter_restrictions::HIGHEST_ALLOWED_AGE;
    }

    if (max_age < server_parameter_restrictions::LOWEST_ALLOWED_AGE) {
        max_age = server_parameter_restrictions::LOWEST_ALLOWED_AGE;
    } else if (max_age > server_parameter_restrictions::HIGHEST_ALLOWED_AGE) {
        max_age = server_parameter_restrictions::HIGHEST_ALLOWED_AGE;
    }

    if (min_age > max_age) {
        min_age = max_age;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    mongocxx::stdx::optional<bsoncxx::document::value> login_user_account_doc;

    bsoncxx::builder::stream::document merge_document;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    if (!request->login_info().admin_info_used()) { //user request

        const bsoncxx::document::value login_document = getLoginDocument<false>(
                login_token_str,
                installation_id,
                merge_document,
                current_timestamp
        );

        using namespace user_account_keys;

        //Allowing id to be extracted so error messages will show it.
        const bsoncxx::document::value projection_document = document{}
                    << "_id" << 1
                    << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                    << user_account_keys::AGE << 1
                    << std::string(OTHER_USERS_MATCHED_ACCOUNTS_LIST).append(".").append(accounts_list::OID) << 1
                    << std::string(ALGORITHM_MATCHED_ACCOUNTS_LIST).append(".").append(accounts_list::OID) << 1
                << finalize;

        if (!runInitialLoginOperation(
                login_user_account_doc,
                user_accounts_collection,
                bsoncxx::oid{user_account_oid_str},
                login_document,
                projection_document.view())
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        const ReturnStatus login_return_status = checkForValidLoginToken(
                login_user_account_doc,
                user_account_oid_str
        );

        if (login_return_status != ReturnStatus::SUCCESS) {
            response->set_return_status(login_return_status);
            return;
        }
    } else { //admin request

        if(isInvalidOIDString(user_account_oid_str)) {
            response->set_return_status(INVALID_USER_OID);
            return;
        }

        auto error_func = [&response](const std::string& error_str) {
            response->set_return_status(LG_ERROR);
            response->set_error_string(error_str);
        };

        try {

            mongocxx::options::find opts;

            using namespace user_account_keys;

            opts.projection(
                document{}
                    << "_id" << 1
                    << user_account_keys::AGE << 1
                    << std::string(OTHER_USERS_MATCHED_ACCOUNTS_LIST).append(".").append(accounts_list::OID) << 1
                    << std::string(ALGORITHM_MATCHED_ACCOUNTS_LIST).append(".").append(accounts_list::OID) << 1
                << finalize
            );

            //find user account document
            login_user_account_doc = user_accounts_collection.find_one(
                    document{}
                            << "_id" << bsoncxx::oid{user_account_oid_str}
                    << finalize,
                    opts
            );

        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", user_account_oid_str,
                    "Document_passed", merge_document.view()
            );

            error_func("Error exception thrown when setting user info.");
            return;
        }

        if (!login_user_account_doc) {
            error_func("Failed to update user info.\nUser account document was not found.");
            return ;
        }
    }

    mongocxx::stdx::optional<bsoncxx::document::value> final_user_account_doc;
    try {

        const bsoncxx::document::view login_user_account_view = *login_user_account_doc;
        int user_age;

        user_age = extractFromBsoncxx_k_int32(
                login_user_account_view,
                user_account_keys::AGE
        );

        bsoncxx::builder::stream::document match_age_range;

        int temp_min_age = min_age;
        int temp_max_age = max_age;

        buildAgeRangeChecker(user_age, temp_min_age, temp_max_age);
        matchAgeInsideUserAgeRangeOrEvent(
                match_age_range,
                temp_min_age,
                temp_max_age,
                false
        );

        bsoncxx::builder::stream::document fields_to_update_document;
        fields_to_update_document
                << user_account_keys::AGE_RANGE << buildAgeRangeChecker(min_age, max_age)
                << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << bsoncxx::types::b_date{current_timestamp};

        bsoncxx::builder::stream::document fields_to_project;
        fields_to_project
                << user_account_keys::AGE_RANGE << 1;

        if(!removeInvalidElementsForUpdatedMatchingParameters(
                bsoncxx::oid{user_account_oid_str},
                login_user_account_view,
                user_accounts_collection,
                final_user_account_doc,
                match_age_range.view(),
                fields_to_update_document.view(),
                fields_to_project.view())
        ) {
            //Error already stored
            if(request->login_info().admin_info_used()){
                response->set_error_string("Error when running removeInvalidElementsForUpdatedMatchingParameters().");
            }
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        response->set_timestamp(current_timestamp.count());
        response->set_return_status(ReturnStatus::SUCCESS);
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Error is already stored
        if(request->login_info().admin_info_used()){
            response->set_error_string(std::string(e.what()));
        }
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    if(response->return_status() == SUCCESS && final_user_account_doc) {

        const bsoncxx::document::view user_account_doc_view = *final_user_account_doc;
        int updated_min_age_range = 0, updated_max_age_range = 0;

        if (extractMinMaxAgeRange(
                user_account_doc_view,
                updated_min_age_range,
                updated_max_age_range
        )) {

            auto push_update_doc = document{}
                << user_account_statistics_keys::AGE_RANGES << open_document
                    << user_account_statistics_keys::age_ranges::AGE_RANGE << open_array
                        << updated_min_age_range
                        << updated_max_age_range
                    << close_array
                    << user_account_statistics_keys::age_ranges::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                << close_document
                << finalize;

            storeInfoToUserStatistics(
                    mongo_cpp_client,
                    accounts_db,
                    bsoncxx::oid{user_account_oid_str},
                    push_update_doc,
                    current_timestamp
                    );
        }
    }

}
