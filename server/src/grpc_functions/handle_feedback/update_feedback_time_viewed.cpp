//
// Created by jeremiah on 9/1/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <AdminLevelEnum.grpc.pb.h>
#include <store_mongoDB_error_and_exception.h>

#include "handle_feedback.h"


#include "utility_general_functions.h"
#include "admin_privileges_vector.h"
#include "connection_pool_global_variable.h"
#include "handle_function_operation_exception.h"
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

void updateFeedbackTimeViewedImplementation(
        const handle_feedback::UpdateFeedbackTimeViewedRequest* request,
        handle_feedback::UpdateFeedbackTimeViewedResponse* response
);

void updateFeedbackTimeViewed(
        const handle_feedback::UpdateFeedbackTimeViewedRequest* request,
        handle_feedback::UpdateFeedbackTimeViewedResponse* response
) {
    handleFunctionOperationException(
            [&] {
                updateFeedbackTimeViewedImplementation(request, response);
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request);
}

void updateFeedbackTimeViewedImplementation(
        const handle_feedback::UpdateFeedbackTimeViewedRequest* request,
        handle_feedback::UpdateFeedbackTimeViewedResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;

    {
        std::string error_message;
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

        if (basic_info_return_status != ReturnStatus::SUCCESS || !admin_info_doc_value) {
            response->set_success(false);
            response->set_error_msg(
                    "ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
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
        } else { //if element does not exist or is not type oid
            logElementError(
                    __LINE__, __FILE__,
                    admin_privilege_element, admin_info_doc_view,
                    bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
            );

            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return;
        }
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const std::chrono::milliseconds feedback_observed_time =
            request->timestamp_feedback_observed_time() > current_timestamp.count() ?
            current_timestamp :
            std::chrono::milliseconds{request->timestamp_feedback_observed_time()};

    std::string timestamp_field_key;

    switch(request->feedback_type()) {
        case FEEDBACK_TYPE_ACTIVITY_SUGGESTION:

            if (!admin_privileges[admin_level].view_activity_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                " does not have 'view_activity_feedback' access.");
                return;
            }

            timestamp_field_key = admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY;

            break;
        case FEEDBACK_TYPE_BUG_REPORT:

            if (!admin_privileges[admin_level].view_bugs_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                " does not have 'view_bugs_feedback' access.");
                return;
            }

            timestamp_field_key = admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG;

            break;
        case FEEDBACK_TYPE_OTHER_FEEDBACK:

            if (!admin_privileges[admin_level].view_other_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                " does not have 'view_other_feedback' access.");
                return;
            }

            timestamp_field_key = admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER;

            break;
        default:
            response->set_success(false);
            response->set_error_msg("Feedback type " + FeedbackType_Name(request->feedback_type()) +
            " was not found.");
            return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc;
    try {
        mongocxx::options::find_one_and_update opts;

        opts.projection(
                document{}
                        << timestamp_field_key << 1
                << finalize
        );

        opts.return_document(mongocxx::options::return_document::k_after);

        admin_info_doc = admin_accounts_collection.find_one_and_update(
                document{}
                        << admin_account_key::NAME << request->login_info().admin_name()
                        << admin_account_key::PASSWORD << request->login_info().admin_password()
                    << finalize,
                document{}
                    << "$max" << open_document
                        << timestamp_field_key << bsoncxx::types::b_date{feedback_observed_time}
                    << close_document
                << finalize,
                opts
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME,
                "admin_account_name", request->login_info().admin_name()
        );

        response->set_error_msg("Error stored on server. Exception inside update_feedback_time_viewed.cpp.");
        response->set_success(false);
        return;
    }

    if(!admin_info_doc) {
        const std::string error_string = "Admin account was found when login info was validated. Then NOT found shortly afterwards.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME,
                "admin_account_name", request->login_info().admin_name()
        );

        response->set_error_msg("Admin account does not exist.");
        response->set_success(false);
        return;
    }

    {
        const bsoncxx::document::view admin_info_view = *admin_info_doc;

        auto timestamp_element = admin_info_view[timestamp_field_key];
        if (timestamp_element
            && timestamp_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
            response->set_timestamp_feedback_observed_time(timestamp_element.get_date().value.count());
        } else { //if element does not exist or is not type date
            logElementError(
                    __LINE__, __FILE__,
                    timestamp_element, admin_info_view,
                    bsoncxx::type::k_date, timestamp_field_key,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
            );

            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return;
        }
    }

    response->set_success(true);
}


