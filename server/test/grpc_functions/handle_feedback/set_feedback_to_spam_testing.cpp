//
// Created by jeremiah on 9/17/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "setup_login_info.h"
#include "HandleErrors.pb.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "HandleFeedback.pb.h"
#include "handle_feedback.h"
#include "feedback_objects.h"
#include "feedback_testing_helpers.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetFeedbackToSpamTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    handle_feedback::SetFeedbackToSpamRequest request;
    handle_feedback::SetFeedbackToSpamResponse response;

    AdminAccountDoc admin_account_doc;

    ActivitySuggestionFeedbackDoc activity_feedback;
    BugReportFeedbackDoc bug_report_feedback;
    OtherSuggestionFeedbackDoc other_feedback;

    template <FeedbackType feedback_type>
    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_feedback_type(feedback_type);

        switch (feedback_type) {
            case FEEDBACK_TYPE_ACTIVITY_SUGGESTION: {
                request.set_feedback_oid(activity_feedback.current_object_oid.to_string());
                break;
            }
            case FEEDBACK_TYPE_BUG_REPORT: {
                request.set_feedback_oid(bug_report_feedback.current_object_oid.to_string());
                break;
            }
            case FEEDBACK_TYPE_OTHER_FEEDBACK: {
                request.set_feedback_oid(other_feedback.current_object_oid.to_string());
                break;
            }
            case FEEDBACK_TYPE_UNKNOWN:
            case FeedbackType_INT_MIN_SENTINEL_DO_NOT_USE_:
            case FeedbackType_INT_MAX_SENTINEL_DO_NOT_USE_:
                break;
        }
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        activity_feedback.generateRandomValues(false);
        activity_feedback.account_oid = user_account_oid;
        activity_feedback.setIntoCollection();

        bug_report_feedback.generateRandomValues(false);
        bug_report_feedback.account_oid = user_account_oid;
        bug_report_feedback.setIntoCollection();

        other_feedback.generateRandomValues(false);
        other_feedback.account_oid = user_account_oid;
        other_feedback.setIntoCollection();

        request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION);
        request.set_feedback_oid(activity_feedback.current_object_oid.to_string());
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if(admin_account_doc.current_object_oid.to_string() == "000000000000000000000000") {
            createTempAdminAccount(admin_level);
            admin_account_doc.getFromCollection(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);
        }

        setFeedbackToSpam(
                &request, &response
        );
    }

    void compareUserAccountDoc() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    void compareAdminAccountDoc() {
        AdminAccountDoc extracted_admin_account_doc(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);
        EXPECT_EQ(admin_account_doc, extracted_admin_account_doc);
    }

    void compareFeedbackAccountDocs() {
        ActivitySuggestionFeedbackDoc extracted_activity_feedback(activity_feedback.current_object_oid);
        EXPECT_EQ(activity_feedback, extracted_activity_feedback);

        BugReportFeedbackDoc extracted_bug_report_feedback(bug_report_feedback.current_object_oid);
        EXPECT_EQ(bug_report_feedback, extracted_bug_report_feedback);

        OtherSuggestionFeedbackDoc extracted_other_feedback(other_feedback.current_object_oid);
        EXPECT_EQ(other_feedback, extracted_other_feedback);
    }

    void compareAllInfo() {
        compareUserAccountDoc();

        compareAdminAccountDoc();

        compareFeedbackAccountDocs();
    }

    void compareInfo_failed() {
        EXPECT_FALSE(response.error_msg().empty());
        EXPECT_FALSE(response.success());

        compareAllInfo();
    }

    void compareInfo_success() {
        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        admin_account_doc.number_feedback_marked_as_spam++;
        user_account_doc.number_times_spam_feedback_sent++;

        compareAllInfo();
    }

};

TEST_F(SetFeedbackToSpamTesting, invalidLoginInfo) {
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

    compareAllInfo();
}

TEST_F(SetFeedbackToSpamTesting, invalidFeedbackType) {
    request.set_feedback_type(FeedbackType(-1));

    runFunction();

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_unknown) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_UNKNOWN);

    runFunction();

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, invalidFeedbackOid) {
    request.set_feedback_oid("1234");

    runFunction();

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_activity_noAdminAccess) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION>();

    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_bugReport_noAdminAccess) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_BUG_REPORT>();

    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_other_noAdminAccess) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK>();

    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_activity_successful) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION>();

    runFunction();

    activity_feedback.marked_as_spam = std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME);

    compareInfo_success();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_bugReport_successful) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_BUG_REPORT>();

    runFunction();

    bug_report_feedback.marked_as_spam = std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME);

    compareInfo_success();
}

TEST_F(SetFeedbackToSpamTesting, feedbackType_other_successful) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK>();

    runFunction();

    other_feedback.marked_as_spam = std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME);

    compareInfo_success();
}

TEST_F(SetFeedbackToSpamTesting, feedbackAlreadySetAsSpam) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK>();

    other_feedback.marked_as_spam = std::make_unique<std::string>(bsoncxx::oid{}.to_string());
    other_feedback.setIntoCollection();

    runFunction();

    compareInfo_failed();
}

TEST_F(SetFeedbackToSpamTesting, feedbackOidDoesNotExist) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK>();
    request.set_feedback_oid(bsoncxx::oid{}.to_string());

    runFunction();

    compareInfo_failed();
}
