//
// Created by jeremiah on 9/1/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <AdminLevelEnum.grpc.pb.h>
#include <handle_feedback_helper_functions/handle_feedback_helper_functions.h>

#include "handle_feedback.h"


#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "handle_function_operation_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "feedback_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void getInitialFeedbackImplementation(
        const handle_feedback::GetInitialFeedbackRequest* request,
        handle_feedback::GetFeedbackResponse* response
);

void getInitialFeedback(
        const handle_feedback::GetInitialFeedbackRequest* request,
        handle_feedback::GetFeedbackResponse* response
) {
    handleFunctionOperationException(
            [&] {
                getInitialFeedbackImplementation(request, response);
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

void getInitialFeedbackImplementation(
        const handle_feedback::GetInitialFeedbackRequest* request,
        handle_feedback::GetFeedbackResponse* response
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

    const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

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

    std::chrono::milliseconds last_time_feedback_extracted;
    std::string feedback_collection_name;

    //feedback fields
    std::string feedback_timestamp_field_name;
    std::string feedback_message_field_name;
    std::string feedback_sent_by_user_oid;
    std::string feedback_marked_as_spam_by_admin_name_field_name;

    if (!verifyRequestAndSetVariablesFeedback(
            request->feedback_type(),
            response,
            admin_info_doc_view,
            admin_level,
            last_time_feedback_extracted,
            feedback_collection_name,
            feedback_timestamp_field_name,
            feedback_message_field_name,
            feedback_sent_by_user_oid,
            feedback_marked_as_spam_by_admin_name_field_name)
            ) {
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database feedback_db = mongo_cpp_client[database_names::FEEDBACK_DATABASE_NAME];
    mongocxx::collection feedback_collection = feedback_db[feedback_collection_name];

    //NOTE: Technically if there are more than NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT feedback with the
    // same timestamp, then some of it can be missed.

    if (last_time_feedback_extracted.count() == -1) { //if user has never requested feedback before

        if (!extractAndSaveFeedbackToResponse(
                response,
                request->feedback_type(),
                NextPreviousFeedback::ALL_FEEDBACK_TYPE,
                feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT * 2,
                last_time_feedback_extracted,
                feedback_collection_name,
                feedback_timestamp_field_name,
                feedback_message_field_name,
                feedback_sent_by_user_oid,
                feedback_marked_as_spam_by_admin_name_field_name,
                feedback_collection)
                ) {
            //error already handled
            return;
        }

    } else { //if user has requested feedback before

        if (!extractAndSaveFeedbackToResponse(
                response,
                request->feedback_type(),
                NextPreviousFeedback::PREVIOUS_FEEDBACK_INCLUSIVE_REQUEST,
                feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT,
                last_time_feedback_extracted,
                feedback_collection_name,
                feedback_timestamp_field_name,
                feedback_message_field_name,
                feedback_sent_by_user_oid,
                feedback_marked_as_spam_by_admin_name_field_name,
                feedback_collection)
                ) {
            //error already handled
            return;
        }

        if (!extractAndSaveFeedbackToResponse(
                response,
                request->feedback_type(),
                NextPreviousFeedback::NEXT_FEEDBACK_EXCLUSIVE_TYPE,
                feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT,
                last_time_feedback_extracted,
                feedback_collection_name,
                feedback_timestamp_field_name,
                feedback_message_field_name,
                feedback_sent_by_user_oid,
                feedback_marked_as_spam_by_admin_name_field_name,
                feedback_collection)
                ) {
            //error already handled
            return;
        }
    }

    response->set_timestamp_of_most_recently_viewed_feedback(last_time_feedback_extracted.count());
    response->set_success(true);
}




