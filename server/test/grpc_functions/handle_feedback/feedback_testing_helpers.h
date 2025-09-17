//
// Created by jeremiah on 9/19/22.
//

#pragma once

#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "FeedbackTypeEnum.grpc.pb.h"
#include "HandleFeedback.grpc.pb.h"

//The template is meant to be one of the following three variables.
// ActivitySuggestionFeedbackDoc
// BugReportFeedbackDoc
// OtherSuggestionFeedbackDoc
template<FeedbackType feedback_type, typename T>
void compareFeedbackUnitFromList(
        const T& expected_feedback_unit,
        const int response_feedback_list_index,
        const google::protobuf::RepeatedPtrField<handle_feedback::FeedbackElement>& feedback_list,
        const std::string& activity_name = ""
) {

    ASSERT_TRUE(feedback_list[response_feedback_list_index].has_single_feedback_unit());

    handle_feedback::SingleFeedbackUnit generated_feedback_unit;
    generated_feedback_unit.set_feedback_type(feedback_type);
    generated_feedback_unit.set_feedback_oid(expected_feedback_unit.current_object_oid.to_string());
    generated_feedback_unit.set_account_oid(expected_feedback_unit.account_oid.to_string());
    generated_feedback_unit.set_activity_name(activity_name);
    generated_feedback_unit.set_message(expected_feedback_unit.message);
    generated_feedback_unit.set_timestamp_stored_on_server(expected_feedback_unit.timestamp_stored.value.count());
    generated_feedback_unit.set_marked_as_spam_by_admin_name(
            expected_feedback_unit.marked_as_spam ? *expected_feedback_unit.marked_as_spam : "");

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_feedback_unit,
            feedback_list[response_feedback_list_index].single_feedback_unit()
    );

    if (!equivalent) {
        std::cout << "feedback_unit\n" << generated_feedback_unit.DebugString() << '\n';
        std::cout << "response.feedback_list\n" << feedback_list[response_feedback_list_index].single_feedback_unit().DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}