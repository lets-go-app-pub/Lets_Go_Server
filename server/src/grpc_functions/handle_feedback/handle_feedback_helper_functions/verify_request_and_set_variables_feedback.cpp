//
// Created by jeremiah on 9/3/21.
//

#include <bsoncxx/types.hpp>

#include "utility_general_functions.h"
#include "admin_privileges_vector.h"

#include "handle_feedback_helper_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "activity_suggestion_feedback_keys.h"
#include "other_suggestion_feedback_keys.h"
#include "bug_report_feedback_keys.h"

bool verifyRequestAndSetVariablesFeedback(
        FeedbackType feedback_type,
        handle_feedback::GetFeedbackResponse* response,
        const bsoncxx::document::view& admin_info_doc_view,
        AdminLevelEnum& admin_level,
        std::chrono::milliseconds& last_time_feedback_extracted,
        std::string& feedback_collection_name,
        std::string& feedback_timestamp_field_name,
        std::string& feedback_message_field_name,
        std::string& feedback_sent_by_user_oid,
        std::string& feedback_marked_as_spam_by_admin_name_field_name_element
) {

    switch (feedback_type) {
        case FEEDBACK_TYPE_ACTIVITY_SUGGESTION: {
            if (!admin_privileges[admin_level].view_activity_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                        " does not have 'view_activity_feedback' access.");
                return false;
            }

            auto last_time_feedback_extracted_element = admin_info_doc_view[admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY];
            if (last_time_feedback_extracted_element &&
                last_time_feedback_extracted_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                last_time_feedback_extracted = last_time_feedback_extracted_element.get_date().value;
            } else { //if element does not exist or is not type date
                logElementError(
                        __LINE__, __FILE__,
                        last_time_feedback_extracted_element, admin_info_doc_view,
                        bsoncxx::type::k_date, admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
                );

                response->set_success(false);
                response->set_error_msg("Error stored on server.");
                return false;
            }

            feedback_collection_name = collection_names::ACTIVITY_SUGGESTION_FEEDBACK_COLLECTION_NAME;

            feedback_timestamp_field_name = activity_suggestion_feedback_keys::TIMESTAMP_STORED;
            feedback_sent_by_user_oid = activity_suggestion_feedback_keys::ACCOUNT_OID;
            feedback_message_field_name = activity_suggestion_feedback_keys::MESSAGE;
            feedback_marked_as_spam_by_admin_name_field_name_element = activity_suggestion_feedback_keys::MARKED_AS_SPAM;
            break;
        }
        case FEEDBACK_TYPE_BUG_REPORT: {
            if (!admin_privileges[admin_level].view_bugs_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                        " does not have 'view_bugs_feedback' access.");
                return false;
            }

            auto last_time_feedback_extracted_element = admin_info_doc_view[admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG];
            if (last_time_feedback_extracted_element &&
                last_time_feedback_extracted_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                last_time_feedback_extracted = last_time_feedback_extracted_element.get_date().value;
            } else { //if element does not exist or is not type date
                logElementError(
                        __LINE__, __FILE__,
                        last_time_feedback_extracted_element, admin_info_doc_view,
                        bsoncxx::type::k_date, admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
                );

                response->set_success(false);
                response->set_error_msg("Error stored on server.");
                return false;
            }

            feedback_collection_name = collection_names::BUG_REPORT_FEEDBACK_COLLECTION_NAME;

            feedback_timestamp_field_name = bug_report_feedback_keys::TIMESTAMP_STORED;
            feedback_sent_by_user_oid = bug_report_feedback_keys::ACCOUNT_OID;
            feedback_message_field_name = bug_report_feedback_keys::MESSAGE;
            feedback_marked_as_spam_by_admin_name_field_name_element = other_suggestion_feedback_keys::MARKED_AS_SPAM;

            break;
        }
        case FEEDBACK_TYPE_OTHER_FEEDBACK: {
            if (!admin_privileges[admin_level].view_other_feedback()) {
                response->set_success(false);
                response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                        " does not have 'view_other_feedback' access.");
                return false;
            }

            auto last_time_feedback_extracted_element = admin_info_doc_view[admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER];
            if (last_time_feedback_extracted_element &&
                last_time_feedback_extracted_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                last_time_feedback_extracted = last_time_feedback_extracted_element.get_date().value;
            } else { //if element does not exist or is not type date
                logElementError(
                        __LINE__, __FILE__,
                        last_time_feedback_extracted_element, admin_info_doc_view,
                        bsoncxx::type::k_date, admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
                );

                response->set_success(false);
                response->set_error_msg("Error stored on server.");
                return false;
            }

            feedback_collection_name = collection_names::OTHER_SUGGESTION_FEEDBACK_COLLECTION_NAME;

            feedback_timestamp_field_name = other_suggestion_feedback_keys::TIMESTAMP_STORED;
            feedback_sent_by_user_oid = other_suggestion_feedback_keys::ACCOUNT_OID;
            feedback_message_field_name = other_suggestion_feedback_keys::MESSAGE;
            feedback_marked_as_spam_by_admin_name_field_name_element = bug_report_feedback_keys::MARKED_AS_SPAM;

            break;
        }
        default:
            response->set_success(false);
            response->set_error_msg(std::string("Feedback type ").append(FeedbackType_Name(feedback_type)).append(" was not found."));
            return false;
    }

    return true;
}