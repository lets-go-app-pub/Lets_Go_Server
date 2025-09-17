//
// Created by jeremiah on 9/3/21.
//

#include <bsoncxx/types.hpp>


#include "utility_general_functions.h"
#include "handle_feedback_helper_functions.h"
#include "database_names.h"
#include "activity_suggestion_feedback_keys.h"
#include "extract_data_from_bsoncxx.h"

bool saveFeedbackInfo(
        const bsoncxx::document::view& feedback_doc,
        const std::string& feedback_collection_name,
        const FeedbackType& feedback_type,
        const std::string& feedback_timestamp_field_name,
        const std::string& feedback_message_field_name,
        const std::string& feedback_sent_by_user_oid,
        const std::string& feedback_marked_as_spam_by_admin_name_field_name,
        std::vector<handle_feedback::FeedbackElement*>& feedback_vector
) {

    try {

        feedback_vector.emplace_back(
                new handle_feedback::FeedbackElement
        );

        auto feedback_unit = feedback_vector.back()->mutable_single_feedback_unit();

        feedback_unit->set_feedback_type(feedback_type);

        feedback_unit->set_feedback_oid(
                extractFromBsoncxx_k_oid(
                        feedback_doc,
                        "_id"
                ).to_string()
        );

        feedback_unit->set_timestamp_stored_on_server(
                extractFromBsoncxx_k_date(
                        feedback_doc,
                        feedback_timestamp_field_name
                ).value.count()
        );

        feedback_unit->set_account_oid(
                extractFromBsoncxx_k_oid(
                        feedback_doc,
                        feedback_sent_by_user_oid
                ).to_string()
        );

        feedback_unit->set_message(
                extractFromBsoncxx_k_utf8(
                        feedback_doc,
                        feedback_message_field_name
                )
        );

        auto feedback_marked_as_spam_by_admin_name_field_name_element = feedback_doc[feedback_marked_as_spam_by_admin_name_field_name];
        if (feedback_marked_as_spam_by_admin_name_field_name_element) { //if element exists

            if (feedback_marked_as_spam_by_admin_name_field_name_element.type() == bsoncxx::type::k_utf8) { //if element does not exist or is not type utf8
                feedback_unit->set_marked_as_spam_by_admin_name(
                        feedback_marked_as_spam_by_admin_name_field_name_element.get_string().value.to_string());
            } else {
                logElementError(
                        __LINE__, __FILE__,
                        feedback_marked_as_spam_by_admin_name_field_name_element, feedback_doc,
                        bsoncxx::type::k_utf8, feedback_marked_as_spam_by_admin_name_field_name,
                        database_names::FEEDBACK_DATABASE_NAME, feedback_collection_name
                );
                return false;
            }

        } //else {} //the element NOT existing is perfectly ok, it simply means the feedback was not set as spam

        if (feedback_type == FEEDBACK_TYPE_ACTIVITY_SUGGESTION) {
            feedback_unit->set_activity_name(
                    extractFromBsoncxx_k_utf8(
                            feedback_doc,
                            activity_suggestion_feedback_keys::ACTIVITY_NAME
                    )
            );
        }

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored here
        return false;
    }


    return true;
}