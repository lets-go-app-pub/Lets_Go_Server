//
// Created by jeremiah on 9/5/21.
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
#include "user_account_keys.h"
#include "activity_suggestion_feedback_keys.h"
#include "other_suggestion_feedback_keys.h"
#include "bug_report_feedback_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setFeedbackToSpamImplementation(
        const handle_feedback::SetFeedbackToSpamRequest* request,
        handle_feedback::SetFeedbackToSpamResponse* response
);

void setFeedbackToSpam(
        const handle_feedback::SetFeedbackToSpamRequest* request,
        handle_feedback::SetFeedbackToSpamResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setFeedbackToSpamImplementation(request, response);
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

void setFeedbackToSpamImplementation(
        const handle_feedback::SetFeedbackToSpamRequest* request,
        handle_feedback::SetFeedbackToSpamResponse* response
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

    std::string feedback_collection_name;

    std::string feedback_sent_by_account_oid_field;
    std::string feedback_spam_field;

    switch (request->feedback_type()) {
        case FEEDBACK_TYPE_ACTIVITY_SUGGESTION:

            if (!admin_privileges[admin_level].view_activity_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                        " does not have 'view_activity_feedback' access.");
                return;
            }

            feedback_collection_name = collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME;

            feedback_sent_by_account_oid_field = activity_suggestion_feedback_keys::ACCOUNT_OID;
            feedback_spam_field = activity_suggestion_feedback_keys::MARKED_AS_SPAM;

            break;
        case FEEDBACK_TYPE_BUG_REPORT:

            if (!admin_privileges[admin_level].view_bugs_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                        " does not have 'view_bugs_feedback' access.");
                return;
            }

            feedback_collection_name = collection_names::BUG_REPORT_FEEDBACK_COLLECTION_NAME;

            feedback_sent_by_account_oid_field = bug_report_feedback_keys::ACCOUNT_OID;
            feedback_spam_field = bug_report_feedback_keys::MARKED_AS_SPAM;

            break;
        case FEEDBACK_TYPE_OTHER_FEEDBACK:

            if (!admin_privileges[admin_level].view_other_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                        " does not have 'view_other_feedback' access.");
                return;
            }

            feedback_collection_name = collection_names::OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME;

            feedback_sent_by_account_oid_field = other_suggestion_feedback_keys::ACCOUNT_OID;
            feedback_spam_field = other_suggestion_feedback_keys::MARKED_AS_SPAM;

            break;
        default:
            response->set_success(false);
            response->set_error_msg("Feedback type " + FeedbackType_Name(request->feedback_type()) +
                                    " was not found.");
            return;
    }

    if(isInvalidOIDString(request->feedback_oid())) {
        response->set_success(false);
        response->set_error_msg("Invalid feedback oid sent to server. Unable to mark as spam");
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database feedback_db = mongo_cpp_client[database_names::FEEDBACK_DATABASE_NAME];

    mongocxx::collection admin_accounts_collection = accounts_db[collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection feedback_collection = feedback_db[feedback_collection_name];

    static const std::string ERROR_STRING = "Error finding feedback on server. Unable to mark as spam.\n\n";

    try {

        bsoncxx::stdx::optional<bsoncxx::document::value> feedback_update_doc;

        mongocxx::options::find_one_and_update feedback_opts;

        feedback_opts.projection(
                document{}
                        << feedback_sent_by_account_oid_field << 1
                << finalize
        );

        feedback_update_doc = feedback_collection.find_one_and_update(
            document{}
                << "_id" << bsoncxx::oid{request->feedback_oid()}
                << feedback_spam_field << open_document
                    << "$exists" << false
                << close_document
            << finalize,
            document{}
                << "$set" << open_document
                    << feedback_spam_field << request->login_info().admin_name()
                << close_document
            << finalize,
            feedback_opts
        );

        if(!feedback_update_doc) {

            const std::string error_string = ERROR_STRING + "Error msg: !feedback_update_doc feedback document did not exist in database."
                                                      "Or it was already set as spam. Please restart the application.";

            response->set_success(false);
            response->set_error_msg(error_string);
            return;
        }

        bsoncxx::stdx::optional<mongocxx::result::update> admin_update;

        //NOTE: This was found above when login info was checked, it should exist (although
        // technically it COULD have been removed in between).
        admin_update = admin_accounts_collection.update_one(
            document{}
                << admin_account_key::NAME << request->login_info().admin_name()
            << finalize,
            document{}
                << "$inc" << open_document
                    << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << 1
                << close_document
            << finalize
        );

        if(!admin_update || admin_update->matched_count() == 0) {
            std::string error_string = ERROR_STRING;

            if(admin_update) {
                error_string += "Error msg: admin_update->matched_count() == 0";
            } else {
                error_string += "Error msg: !admin_update";
            }

            response->set_success(false);
            response->set_error_msg(error_string);
            return;
        }

        bsoncxx::oid sent_by_user_account_oid;
        {
            const bsoncxx::document::view feedback_update_doc_view = *feedback_update_doc;

            auto user_account_oid_element = feedback_update_doc_view[feedback_sent_by_account_oid_field];
            if (user_account_oid_element
                && user_account_oid_element.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
                sent_by_user_account_oid = user_account_oid_element.get_oid().value;
            } else { //if element does not exist or is not type oid
                logElementError(
                        __LINE__, __FILE__,
                        user_account_oid_element, feedback_update_doc_view,
                        bsoncxx::type::k_oid, feedback_sent_by_account_oid_field,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
                );

                response->set_success(false);
                response->set_error_msg("Error stored on server.");
                return;
            }
        }

        user_accounts_collection.update_one(
            document{}
                << "_id" << sent_by_user_account_oid
            << finalize,
            document{}
                << "$inc" << open_document
                    << user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT << 1
                << close_document
            << finalize
        );

        //NOTE: If the user account OID is found or not doesn't really matter, the account could easily have been
        // removed.

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME,
                "admin_account_name", request->login_info().admin_name()
        );

        response->set_error_msg("Error stored on server. Exception inside set_feedback_to_spam.cpp.");
        response->set_success(false);
        return;
    }

    response->set_success(true);
}