//
// Created by jeremiah on 10/7/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "generate_multiple_random_accounts.h"
#include "set_fields_functions.h"
#include "feedback_objects.h"
#include "feedback_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetFeedbackTesting : public testing::Test {
protected:

    setfields::SetFeedbackRequest request;
    setfields::SetFeedbackResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    UserAccountStatisticsDoc user_account_statistics;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_BUG_REPORT);
        request.set_info(gen_random_alpha_numeric_string(rand() % (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_FEEDBACK-1) + 1));
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);
        user_account_statistics.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        setFeedback(&request, &response);
    }

    void compareFailedAccounts(ReturnStatus return_status) {
        EXPECT_EQ(response.return_status(), return_status);

        //should be no changes
        compareUserAccounts();
        compareUserStatisticsAccounts();

        ActivitySuggestionFeedbackDoc extracted_activity_suggestion_feedback;
        extracted_activity_suggestion_feedback.getFromCollection();

        EXPECT_EQ(extracted_activity_suggestion_feedback.current_object_oid.to_string(), "000000000000000000000000");

        BugReportFeedbackDoc extracted_bug_report_feedback;
        extracted_bug_report_feedback.getFromCollection();

        EXPECT_EQ(extracted_bug_report_feedback.current_object_oid.to_string(), "000000000000000000000000");

        OtherSuggestionFeedbackDoc extracted_other_feedback;
        extracted_other_feedback.getFromCollection();

        EXPECT_EQ(extracted_other_feedback.current_object_oid.to_string(), "000000000000000000000000");
    }

    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);
        EXPECT_EQ(user_account, extracted_user_account);
    }

    void compareUserStatisticsAccounts() {
        UserAccountStatisticsDoc extracted_user_account_statistics(user_account_oid);
        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }
};

TEST_F(SetFeedbackTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();

                runFunction();

                return response.return_status();
            }
    );

    //should be no changes
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetFeedbackTesting, invalidFeedbackType) {
    request.set_feedback_type(FeedbackType(-1));

    runFunction();

    compareFailedAccounts(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetFeedbackTesting, infoTooLong) {
    request.set_info(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_FEEDBACK + 1));

    runFunction();

    compareFailedAccounts(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetFeedbackTesting, activityNameTooLong) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION);
    request.set_activity_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runFunction();

    compareFailedAccounts(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetFeedbackTesting, userFlaggedAsSpammer) {
    user_account.number_times_spam_feedback_sent = feedback_values::MAX_NUMBER_OF_TIMES_CAN_SPAM_FEEDBACK + 1;
    user_account.setIntoCollection();

    runFunction();

    user_account.number_times_sent_bug_report++;

    compareFailedAccounts(ReturnStatus::SUCCESS);
}

TEST_F(SetFeedbackTesting, successful_activitySuggestionFeedback) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION);
    request.set_activity_name(gen_random_alpha_numeric_string(rand() % 100 + 5));

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.number_times_sent_activity_suggestion++;

    compareUserAccounts();
    compareUserStatisticsAccounts();

    ActivitySuggestionFeedbackDoc generated_feedback;

    generated_feedback.account_oid = user_account_oid;
    generated_feedback.activity_name = request.activity_name();
    generated_feedback.message = request.info();

    ActivitySuggestionFeedbackDoc extracted_feedback;
    extracted_feedback.getFromCollection();

    EXPECT_GT(extracted_feedback.timestamp_stored, 0);
    generated_feedback.timestamp_stored = extracted_feedback.timestamp_stored;
    generated_feedback.current_object_oid = extracted_feedback.current_object_oid;

    EXPECT_EQ(generated_feedback, extracted_feedback);
}

TEST_F(SetFeedbackTesting, successful_bugReportFeedback) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_BUG_REPORT);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.number_times_sent_bug_report++;

    compareUserAccounts();
    compareUserStatisticsAccounts();

    BugReportFeedbackDoc generated_feedback;

    generated_feedback.account_oid = user_account_oid;
    generated_feedback.message = request.info();

    BugReportFeedbackDoc extracted_feedback;
    extracted_feedback.getFromCollection();

    EXPECT_GT(extracted_feedback.timestamp_stored, 0);
    generated_feedback.timestamp_stored = extracted_feedback.timestamp_stored;
    generated_feedback.current_object_oid = extracted_feedback.current_object_oid;

    EXPECT_EQ(generated_feedback, extracted_feedback);
}

TEST_F(SetFeedbackTesting, successful_otherFeedback) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.number_times_sent_other_suggestion++;

    compareUserAccounts();
    compareUserStatisticsAccounts();

    OtherSuggestionFeedbackDoc generated_feedback;

    generated_feedback.account_oid = user_account_oid;
    generated_feedback.message = request.info();

    OtherSuggestionFeedbackDoc extracted_feedback;
    extracted_feedback.getFromCollection();

    EXPECT_GT(extracted_feedback.timestamp_stored, 0);
    generated_feedback.timestamp_stored = extracted_feedback.timestamp_stored;
    generated_feedback.current_object_oid = extracted_feedback.current_object_oid;

    EXPECT_EQ(generated_feedback, extracted_feedback);
}
