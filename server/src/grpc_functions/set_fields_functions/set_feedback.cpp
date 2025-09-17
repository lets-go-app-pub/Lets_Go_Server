//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <handle_function_operation_exception.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "activity_suggestion_feedback_keys.h"
#include "other_suggestion_feedback_keys.h"
#include "bug_report_feedback_keys.h"
#include "server_parameter_restrictions.h"
#include "feedback_values.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setFeedbackImplementation(
        const setfields::SetFeedbackRequest* request,
        setfields::SetFeedbackResponse* response
);

void setFeedback(
        const setfields::SetFeedbackRequest* request,
        setfields::SetFeedbackResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setFeedbackImplementation(request, response);
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

void setFeedbackImplementation(
        const setfields::SetFeedbackRequest* request,
        setfields::SetFeedbackResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    const std::string& feedback_info = request->info();
    if (feedback_info.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_FEEDBACK) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    std::string feedback_collection_name;
    std::string activity_name;
    if (!FeedbackType_IsValid(request->feedback_type())
            || request->feedback_type() == FeedbackType::FEEDBACK_TYPE_UNKNOWN) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    } else if (request->feedback_type() == FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION) {

        feedback_collection_name = collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME;

        activity_name = request->activity_name();

        //user activity name must be no larger than MAXIMUM_NUMBER_ALLOWED_BYTES
        if (activity_name.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
        } else if (feedback_info.empty() &&
                   activity_name.empty()) { //if the strings are both empty no reason to store this
            response->set_return_status(ReturnStatus::SUCCESS);
            return;
        }

    } else {
        //if the info string is empty no reason to store this
        if (feedback_info.empty()) {
            response->set_return_status(ReturnStatus::SUCCESS);
            return;
        }

        if (request->feedback_type() ==
            FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK) { //this could be else but being explicit in case anything is added in future
            feedback_collection_name = collection_names::OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME;
        } else if (request->feedback_type() == FeedbackType::FEEDBACK_TYPE_BUG_REPORT) {
            feedback_collection_name = collection_names::BUG_REPORT_FEEDBACK_COLLECTION_NAME;
        } else {
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
        }
    }

    std::string field_to_update;
    switch(request->feedback_type()) {
        case FEEDBACK_TYPE_ACTIVITY_SUGGESTION:
            field_to_update = user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION;
            break;
        case FEEDBACK_TYPE_BUG_REPORT:
            field_to_update = user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT;
            break;
        case FEEDBACK_TYPE_OTHER_FEEDBACK:
            field_to_update = user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION;
            break;
        case FeedbackType_INT_MIN_SENTINEL_DO_NOT_USE_:
        case FeedbackType_INT_MAX_SENTINEL_DO_NOT_USE_:
        case FEEDBACK_TYPE_UNKNOWN: {
            const std::string error_string = "Unrecognized feedback type from request.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "feedback type", std::to_string(request->feedback_type())
            );
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
        }
    }

    response->set_return_status(ReturnStatus::UNKNOWN);

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const bsoncxx::oid user_account_oid = bsoncxx::oid{user_account_oid_str};

    const bsoncxx::document::value merge_document = document{}
            << field_to_update << open_document
                << "$add" << open_array
                    << "$" + field_to_update << 1
                << close_array
            << close_document
        << finalize;

    const bsoncxx::document::value projection_document = document{}
                << "_id" << 1
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;
    if (!runInitialLoginOperation(
            find_and_update_user_account,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    //if document found and no exception occurred
    if (find_and_update_user_account) {

        const bsoncxx::document::view user_account_doc_view = find_and_update_user_account->view();
        int number_times_spam_feedback_sent;

        auto account_status_element = user_account_doc_view[user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT];
        if (account_status_element
            && account_status_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            number_times_spam_feedback_sent = account_status_element.get_int32().value;
        } else { //if element does not exist or is not type int32
            logElementError(
                    __LINE__, __FILE__,
                    account_status_element, user_account_doc_view,
                    bsoncxx::type::k_int32, user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        if (number_times_spam_feedback_sent > feedback_values::MAX_NUMBER_OF_TIMES_CAN_SPAM_FEEDBACK) { //if user was flagged as a spammer
            response->set_return_status(ReturnStatus::SUCCESS);
            return;
        }

        const ReturnStatus return_status = checkForValidLoginToken(
                find_and_update_user_account,
                user_account_oid_str
        );

        if (return_status != SUCCESS) { //if login failed
            response->set_return_status(return_status);
        } else { //if login succeeded

            bsoncxx::builder::stream::document insert_document;
            switch (request->feedback_type() ) {
                case FEEDBACK_TYPE_ACTIVITY_SUGGESTION:
                    insert_document
                            << activity_suggestion_feedback_keys::ACCOUNT_OID << user_account_oid
                            << activity_suggestion_feedback_keys::ACTIVITY_NAME << activity_name
                            << activity_suggestion_feedback_keys::MESSAGE << feedback_info
                            << activity_suggestion_feedback_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp};
                    break;
                case FEEDBACK_TYPE_BUG_REPORT:
                    insert_document
                            << bug_report_feedback_keys::ACCOUNT_OID << user_account_oid
                            << bug_report_feedback_keys::MESSAGE << feedback_info
                            << bug_report_feedback_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp};
                    break;
                case FEEDBACK_TYPE_OTHER_FEEDBACK:
                    insert_document
                            << other_suggestion_feedback_keys::ACCOUNT_OID << user_account_oid
                            << other_suggestion_feedback_keys::MESSAGE << feedback_info
                            << other_suggestion_feedback_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp};
                    break;
                case FeedbackType_INT_MIN_SENTINEL_DO_NOT_USE_:
                case FeedbackType_INT_MAX_SENTINEL_DO_NOT_USE_:
                case FEEDBACK_TYPE_UNKNOWN: {
                    const std::string error_string = "unrecognized feedback type from request";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "feedback type", std::to_string(request->feedback_type())
                    );
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    return;
                    break;
                }
            }

            mongocxx::database feedback_db = mongo_cpp_client[database_names::FEEDBACK_DATABASE_NAME];
            mongocxx::collection feedback_collection = feedback_db[feedback_collection_name];

            std::optional<std::string> feedback_exception_string;
            bsoncxx::stdx::optional<mongocxx::result::insert_one> insert_to_feedback_account_results;
            try {

                //update feedback account document
                insert_to_feedback_account_results = feedback_collection.insert_one(
                        insert_document.view()
                );
            }
            catch (const mongocxx::logic_error& e) {
                feedback_exception_string = std::string(e.what());
            }

            if (!insert_to_feedback_account_results) { //if upsert failed
                const std::string error_string = "failed to upsert feedback document to collection '" +
                                                 database_names::FEEDBACK_DATABASE_NAME + "' '" + feedback_collection_name + "'";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        feedback_exception_string, error_string,
                        "database", database_names::FEEDBACK_DATABASE_NAME,
                        "collection", feedback_collection_name,
                        "ObjectID_used", user_account_oid_str,
                        "message", feedback_info,
                        "activity", activity_name);

                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            } else {
                response->set_return_status(ReturnStatus::SUCCESS);
            }
        }
    } else {
        response->set_return_status(ReturnStatus::NO_VERIFIED_ACCOUNT);
    }

}


