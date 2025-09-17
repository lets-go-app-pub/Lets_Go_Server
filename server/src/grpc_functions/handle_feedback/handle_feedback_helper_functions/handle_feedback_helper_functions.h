//
// Created by jeremiah on 9/3/21.
//

#pragma once

#include <bsoncxx/document/view_or_value.hpp>
#include <HandleFeedback.grpc.pb.h>
#include <AdminLevelEnum.grpc.pb.h>

//exclusive excludes the timestamp in results
//inclusive includes the timestamp in results
enum NextPreviousFeedback {
    ALL_FEEDBACK_TYPE,
    NEXT_FEEDBACK_EXCLUSIVE_TYPE,
    PREVIOUS_FEEDBACK_EXCLUSIVE_REQUEST,
    PREVIOUS_FEEDBACK_INCLUSIVE_REQUEST
};

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
);

bool saveFeedbackInfo(
        const bsoncxx::document::view& feedback_doc,
        const std::string& feedback_collection_name,
        const FeedbackType& feedback_type,
        const std::string& feedback_timestamp_field_name,
        const std::string& feedback_message_field_name,
        const std::string& feedback_sent_by_user_oid,
        const std::string& feedback_marked_as_spam_by_admin_name_field_name,
        std::vector<handle_feedback::FeedbackElement*>& feedback_vector
);

bool extractAndSaveFeedbackToResponse(
        handle_feedback::GetFeedbackResponse* response,
        FeedbackType feedback_type,
        NextPreviousFeedback feedback_next_previous,
        int number_to_request,
        const std::chrono::milliseconds& last_time_feedback_extracted,
        const std::string& feedback_collection_name,
        const std::string& feedback_timestamp_field_name,
        const std::string& feedback_message_field_name,
        const std::string& feedback_sent_by_user_oid,
        const std::string& feedback_marked_as_spam_by_admin_name_field_name,
        mongocxx::collection& match_results_collection
);

