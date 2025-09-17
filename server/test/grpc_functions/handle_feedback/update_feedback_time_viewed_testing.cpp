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

#include "setup_login_info.h"
#include "HandleErrors.pb.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "HandleFeedback.pb.h"
#include "handle_feedback.h"
#include "feedback_testing_helpers.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UpdateFeedbackTimeViewedTesting : public ::testing::Test {
protected:

    handle_feedback::UpdateFeedbackTimeViewedRequest request;
    handle_feedback::UpdateFeedbackTimeViewedResponse response;

    AdminAccountDoc admin_account_doc;

    const bsoncxx::types::b_date passed_timestamp{std::chrono::milliseconds{100}};

    template <FeedbackType feedback_type>
    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_feedback_type(feedback_type);
        request.set_timestamp_feedback_observed_time(passed_timestamp.value.count());
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        setupValidRequest<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION>();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if(admin_account_doc.current_object_oid.to_string() == "000000000000000000000000") {
            createTempAdminAccount(admin_level);
            admin_account_doc.getFromCollection(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);
        }

        updateFeedbackTimeViewed(
                &request, &response
        );
    }

    void compareAdminAccountDoc() {
        AdminAccountDoc extracted_admin_account_doc(admin_account_doc.current_object_oid);
        EXPECT_EQ(admin_account_doc, extracted_admin_account_doc);
    }

    void compareInfo_failed() {
        EXPECT_FALSE(response.error_msg().empty());
        EXPECT_FALSE(response.success());

        compareAdminAccountDoc();
    }

    void compareInfo_success() {
        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        compareAdminAccountDoc();
    }

};

TEST_F(UpdateFeedbackTimeViewedTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {
                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                compareAdminAccountDoc();
                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(UpdateFeedbackTimeViewedTesting, invalidFeedbackInfo) {
    request.set_feedback_type(FeedbackType(-1));

    runFunction();

    compareInfo_failed();
}

TEST_F(UpdateFeedbackTimeViewedTesting, feedbackType_unknown) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_UNKNOWN);

    runFunction();

    compareInfo_failed();
}

TEST_F(UpdateFeedbackTimeViewedTesting, feedbackType_activity_successful) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION>();

    runFunction();

    admin_account_doc.last_time_extracted_feedback_activity = passed_timestamp;
    compareInfo_success();
}

TEST_F(UpdateFeedbackTimeViewedTesting, feedbackType_bugReport_successful) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_BUG_REPORT>();

    runFunction();

    admin_account_doc.last_time_extracted_feedback_bug = passed_timestamp;
    compareInfo_success();
}

TEST_F(UpdateFeedbackTimeViewedTesting, feedbackType_other_successful) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK>();

    runFunction();

    admin_account_doc.last_time_extracted_feedback_other = passed_timestamp;
    compareInfo_success();
}

TEST_F(UpdateFeedbackTimeViewedTesting, timestampFeedbackObservedTooLarge) {
    setupValidRequest<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION>();

    //A number larger than the unix timestamp will ever be for the test (this should be around the year 5100 in ms).
    request.set_timestamp_feedback_observed_time(100000000000000);
    runFunction();

    admin_account_doc.last_time_extracted_feedback_activity =
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp_feedback_observed_time()}};
    compareInfo_success();
}
