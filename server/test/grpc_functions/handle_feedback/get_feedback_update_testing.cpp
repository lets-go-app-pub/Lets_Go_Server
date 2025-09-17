//
// Created by jeremiah on 9/17/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "extract_data_from_bsoncxx.h"
#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "HandleFeedback.pb.h"
#include "handle_feedback.h"
#include "feedback_objects.h"
#include "feedback_values.h"
#include "feedback_testing_helpers.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GetFeedbackUpdateTesting : public ::testing::Test {
protected:

    handle_feedback::GetFeedbackUpdateRequest request;
    handle_feedback::GetFeedbackResponse response;

    const std::chrono::milliseconds timestamp_to_request{100};
    const std::chrono::milliseconds timestamp_before_request{timestamp_to_request.count() - 1};
    const std::chrono::milliseconds timestamp_after_request{timestamp_to_request.count() + 1};

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_feedback_type(FeedbackType(rand() % (FeedbackType_MAX-1) + 1));
        request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
        request.set_request_before_timestamp(rand() % 2);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        getFeedbackUpdate(
                &request, &response
        );
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template <typename T>
    void generateRandomFeedback(std::vector<T>& feedback_list, int number_feedback = 3) {

        for(int i = 0; i < number_feedback; i++) {

            feedback_list.emplace_back();
            feedback_list.back().generateRandomValues(true);

            if(i == 0) {
                feedback_list.back().timestamp_stored = bsoncxx::types::b_date{
                        std::chrono::milliseconds{timestamp_before_request.count()}};
            } else if(i == 1) {
                feedback_list.back().timestamp_stored = bsoncxx::types::b_date{
                        std::chrono::milliseconds{timestamp_to_request.count()}};
            } else if (i == 2) {
                feedback_list.back().timestamp_stored = bsoncxx::types::b_date{
                        std::chrono::milliseconds{timestamp_after_request.count()}};
            } else {
                feedback_list.back().timestamp_stored = bsoncxx::types::b_date{
                        std::chrono::milliseconds{timestamp_after_request.count() + i*10}};
            }

            feedback_list.back().setIntoCollection();

        }
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template <FeedbackType feedback_type, typename T>
    void compareSingleFeedbackUnit(
            const T& expected_feedback_unit,
            const int response_feedback_list_index,
            const std::string& activity_name = ""
            ) {
        compareFeedbackUnitFromList<feedback_type, T>(
                expected_feedback_unit,
                response_feedback_list_index,
                response.feedback_list(),
                activity_name
        );
    }

};

TEST_F(GetFeedbackUpdateTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(GetFeedbackUpdateTesting, invalidFeedbackType) {
    request.set_feedback_type(FeedbackType(-1));

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_unknown) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_UNKNOWN);

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_activity_noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_bugReport_noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_other_noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_activity_before) {
    std::vector<ActivitySuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<ActivitySuggestionFeedbackDoc>(feedback_list);

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
    request.set_request_before_timestamp(true);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 2);

    EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());

    compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION, ActivitySuggestionFeedbackDoc>(
            feedback_list.front(),
            1,
            feedback_list[0].activity_name
    );
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_bugReport_before) {
    std::vector<BugReportFeedbackDoc> feedback_list;
    generateRandomFeedback<BugReportFeedbackDoc>(feedback_list);

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_BUG_REPORT);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
    request.set_request_before_timestamp(true);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 2);

    EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());

    compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_BUG_REPORT, BugReportFeedbackDoc>(
            feedback_list.front(),
            1
    );
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_other_before) {
    std::vector<OtherSuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<OtherSuggestionFeedbackDoc>(feedback_list);

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
    request.set_request_before_timestamp(true);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 2);

    EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());

    compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK, OtherSuggestionFeedbackDoc>(
            feedback_list.front(),
            1
    );
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_activity_after) {
    std::vector<ActivitySuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<ActivitySuggestionFeedbackDoc>(feedback_list);

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
    request.set_request_before_timestamp(false);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 2);

    EXPECT_TRUE(response.feedback_list(1).has_end_of_feedback_element());

    compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION, ActivitySuggestionFeedbackDoc>(
            feedback_list.back(),
            0,
            feedback_list.back().activity_name
    );
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_bugReport_after) {
    std::vector<BugReportFeedbackDoc> feedback_list;
    generateRandomFeedback<BugReportFeedbackDoc>(feedback_list);

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_BUG_REPORT);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
    request.set_request_before_timestamp(false);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 2);

    EXPECT_TRUE(response.feedback_list(1).has_end_of_feedback_element());

    compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_BUG_REPORT, BugReportFeedbackDoc>(
            feedback_list.back(),
            0
    );
}

TEST_F(GetFeedbackUpdateTesting, feedbackType_other_after) {
    std::vector<OtherSuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<OtherSuggestionFeedbackDoc>(feedback_list);

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count());
    request.set_request_before_timestamp(false);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 2);

    EXPECT_TRUE(response.feedback_list(1).has_end_of_feedback_element());

    compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK, OtherSuggestionFeedbackDoc>(
            feedback_list.back(),
            0
    );
}

TEST_F(GetFeedbackUpdateTesting, moreFeedbackElementsAvailable_beforeRequested) {
    std::vector<OtherSuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<OtherSuggestionFeedbackDoc>(
            feedback_list,
            feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ON_UPDATE
    );

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);
    request.set_timestamp_of_message_at_end_of_list(feedback_list.back().timestamp_stored.value.count() + 1);
    request.set_request_before_timestamp(true);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ON_UPDATE + 1);

    EXPECT_TRUE(response.feedback_list(0).has_more_feedback_elements_available());

    for(int i = 1; i < response.feedback_list_size(); ++i) {
        compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK, OtherSuggestionFeedbackDoc>(
                feedback_list[i-1],
                i
        );
    }
}

TEST_F(GetFeedbackUpdateTesting, moreFeedbackElementsAvailable_afterRequested) {
    std::vector<OtherSuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<OtherSuggestionFeedbackDoc>(
            feedback_list,
            feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ON_UPDATE
    );

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);
    request.set_timestamp_of_message_at_end_of_list(feedback_list.front().timestamp_stored.value.count() - 1);
    request.set_request_before_timestamp(false);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ON_UPDATE + 1);

    EXPECT_TRUE(response.feedback_list(response.feedback_list_size()-1).has_more_feedback_elements_available());

    for(int i = 0; i < response.feedback_list_size()-1; ++i) {
        compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                i
        );
    }
}

TEST_F(GetFeedbackUpdateTesting, noFeedbackAvailable) {
    //Not inserting any feedback.

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);
    request.set_timestamp_of_message_at_end_of_list(100);
    request.set_request_before_timestamp(false);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 1);

    EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());
}

TEST_F(GetFeedbackUpdateTesting, identicalTimestampsRequested) {
    std::vector<OtherSuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<OtherSuggestionFeedbackDoc>(feedback_list);

    const bsoncxx::types::b_date b_date_to_request{std::chrono::milliseconds{100}};

    feedback_list[0].timestamp_stored = b_date_to_request;
    feedback_list[0].setIntoCollection();

    feedback_list[1].timestamp_stored = b_date_to_request;
    feedback_list[1].setIntoCollection();

    feedback_list[2].timestamp_stored = b_date_to_request;
    feedback_list[2].setIntoCollection();

    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);
    request.set_timestamp_of_message_at_end_of_list(timestamp_to_request.count() - 1);
    request.set_request_before_timestamp(false);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 4);

    EXPECT_TRUE(response.feedback_list(3).has_end_of_feedback_element());

    //These have identical timestamps, so sort them by _id as a secondary (the function
    // will return them in this order).
    std::sort(feedback_list.begin(), feedback_list.end(), [](
            const OtherSuggestionFeedbackDoc& l, const OtherSuggestionFeedbackDoc& r
            ){
        return l.current_object_oid < r.current_object_oid;
    });

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i
        );
    }
}