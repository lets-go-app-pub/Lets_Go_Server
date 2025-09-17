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
#include "setup_login_info.h"
#include "HandleErrors.pb.h"
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

class GetInitialFeedbackTesting : public ::testing::Test {
protected:
    handle_feedback::GetInitialFeedbackRequest request;
    handle_feedback::GetFeedbackResponse response;

    AdminAccountDoc admin_account_doc;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );
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
        if(admin_account_doc.current_object_oid.to_string() == "000000000000000000000000") {
            createTempAdminAccount(admin_level);
        }

        getInitialFeedback(
                &request, &response
        );
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<typename T>
    void generateRandomFeedback(std::vector<T>& feedback_list, int number_feedback = 3) {
        for (int i = 0; i < number_feedback; i++) {
            feedback_list.emplace_back();
            feedback_list.back().generateRandomValues(true);
            feedback_list.back().timestamp_stored = bsoncxx::types::b_date{
                    std::chrono::milliseconds{100 + i * 10}};
            feedback_list.back().setIntoCollection();
        }
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type, typename T>
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

    template<FeedbackType feedback_type>
    void setAdminTimestamp(
            const bsoncxx::types::b_date& timestamp_stored
    ) {
        createTempAdminAccount(AdminLevelEnum::FULL_ACCESS_ADMIN);

        admin_account_doc.getFromCollection(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);
        switch (feedback_type) {
            case FEEDBACK_TYPE_ACTIVITY_SUGGESTION:
                admin_account_doc.last_time_extracted_feedback_activity = timestamp_stored;
                break;
            case FEEDBACK_TYPE_BUG_REPORT:
                admin_account_doc.last_time_extracted_feedback_bug = timestamp_stored;
                break;
            case FEEDBACK_TYPE_OTHER_FEEDBACK:
                admin_account_doc.last_time_extracted_feedback_other = timestamp_stored;
                break;
            case FEEDBACK_TYPE_UNKNOWN:
            case FeedbackType_INT_MIN_SENTINEL_DO_NOT_USE_:
            case FeedbackType_INT_MAX_SENTINEL_DO_NOT_USE_:
                break;
        }
        admin_account_doc.setIntoCollection();
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type, typename T>
    std::vector<T> check_previousEnd_nextEnd() {
        std::vector<T> feedback_list;
        generateRandomFeedback<T>(feedback_list);

        setAdminTimestamp<feedback_type>(
                feedback_list[1].timestamp_stored
        );

        request.set_feedback_type(feedback_type);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        EXPECT_EQ(response.feedback_list_size(), 5);

        if(response.feedback_list_size() == 5) {
            EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());
            EXPECT_TRUE(response.feedback_list(response.feedback_list_size() - 1).has_end_of_feedback_element());

            EXPECT_EQ(response.timestamp_of_most_recently_viewed_feedback(),
                      feedback_list[1].timestamp_stored.value.count());
        }

        return feedback_list;
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type, typename T>
    std::vector<T> check_previousEnd_nextMore() {
        std::vector<T> feedback_list;
        generateRandomFeedback<T>(feedback_list, feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT + 1);

        setAdminTimestamp<feedback_type>(
                feedback_list[0].timestamp_stored
        );

        request.set_feedback_type(feedback_type);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        EXPECT_EQ(response.feedback_list_size(), feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT + 3);

        if(response.feedback_list_size() == feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT + 3) {
            EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());
            EXPECT_TRUE(response.feedback_list(response.feedback_list_size() - 1).has_more_feedback_elements_available());

            EXPECT_EQ(response.timestamp_of_most_recently_viewed_feedback(),
                      feedback_list[0].timestamp_stored.value.count());
        }

        return feedback_list;
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type, typename T>
    std::vector<T> check_previousMore_nextEnd() {
        std::vector<T> feedback_list;
        generateRandomFeedback<T>(feedback_list, feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT + 1);

        setAdminTimestamp<feedback_type>(
                feedback_list[feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT-1].timestamp_stored
        );

        request.set_feedback_type(feedback_type);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        EXPECT_EQ(response.feedback_list_size(), feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT + 3);

        if(response.feedback_list_size() == feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT + 3) {
            EXPECT_TRUE(response.feedback_list(0).has_more_feedback_elements_available());
            EXPECT_TRUE(response.feedback_list(response.feedback_list_size() - 1).has_end_of_feedback_element());

            EXPECT_EQ(response.timestamp_of_most_recently_viewed_feedback(),
                      feedback_list[feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT-1].timestamp_stored.value.count());
        }

        return feedback_list;
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type, typename T>
    std::vector<T> check_previousMore_nextMore() {
        std::vector<T> feedback_list;
        generateRandomFeedback<T>(feedback_list, feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT*2);

        setAdminTimestamp<feedback_type>(
                feedback_list[feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT-1].timestamp_stored
        );

        request.set_feedback_type(feedback_type);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        EXPECT_EQ(response.feedback_list_size(), feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT*2 + 2);

        if(response.feedback_list_size() == feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT*2 + 2) {
            EXPECT_TRUE(response.feedback_list(0).has_more_feedback_elements_available());
            EXPECT_TRUE(response.feedback_list(response.feedback_list_size() - 1).has_more_feedback_elements_available());

            EXPECT_EQ(response.timestamp_of_most_recently_viewed_feedback(),
                      feedback_list[feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT-1].timestamp_stored.value.count());
        }

        return feedback_list;
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type>
    void check_noFeedback() {

        request.set_feedback_type(feedback_type);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        EXPECT_EQ(response.feedback_list_size(), 2);

        if(response.feedback_list_size() == 2) {
            EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());
            EXPECT_TRUE(response.feedback_list(response.feedback_list_size() - 1).has_end_of_feedback_element());
        }
    }

    //The template is meant to be one of the following three variables.
    // ActivitySuggestionFeedbackDoc
    // BugReportFeedbackDoc
    // OtherSuggestionFeedbackDoc
    template<FeedbackType feedback_type, typename T>
    std::vector<T> check_neverBeforeRequested() {
        std::vector<T> feedback_list;
        generateRandomFeedback<T>(feedback_list, feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT*2);

        request.set_feedback_type(feedback_type);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        EXPECT_EQ(response.feedback_list_size(), feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT*2 + 2);

        if(response.feedback_list_size() == feedback_values::NUMBER_FEEDBACK_TO_REQUEST_ABOVE_AND_BELOW_CURRENT*2 + 2) {
            EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());
            EXPECT_TRUE(response.feedback_list(response.feedback_list_size() - 1).has_more_feedback_elements_available());

            EXPECT_EQ(response.timestamp_of_most_recently_viewed_feedback(), -1);
        }

        return feedback_list;
    }

};

TEST_F(GetInitialFeedbackTesting, invalidLoginInfo) {
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

TEST_F(GetInitialFeedbackTesting, invalidFeedbackType) {
    request.set_feedback_type(FeedbackType(-1));

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetInitialFeedbackTesting, feedbackType_unknown) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_UNKNOWN);

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_noAdminAccess) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION);

    runFunction(NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_noAdminAccess) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_BUG_REPORT);

    runFunction(NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_noAdminAccess) {
    request.set_feedback_type(FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK);

    runFunction(NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_previousEnd_nextEnd) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION;
    auto feedback_list = check_previousEnd_nextEnd<feedback_type, ActivitySuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, ActivitySuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1,
                feedback_list[i].activity_name
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_previousEnd_nextEnd) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_BUG_REPORT;
    auto feedback_list = check_previousEnd_nextEnd<feedback_type, BugReportFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, BugReportFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_previousEnd_nextEnd) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK;
    auto feedback_list = check_previousEnd_nextEnd<feedback_type, OtherSuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_previousEnd_nextMore) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION;
    auto feedback_list = check_previousEnd_nextMore<feedback_type, ActivitySuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, ActivitySuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1,
                feedback_list[i].activity_name
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_previousEnd_nextMore) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_BUG_REPORT;
    auto feedback_list = check_previousEnd_nextMore<feedback_type, BugReportFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, BugReportFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_previousEnd_nextMore) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK;
    auto feedback_list = check_previousEnd_nextMore<feedback_type, OtherSuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_previousMore_nextEnd) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION;
    auto feedback_list = check_previousMore_nextEnd<feedback_type, ActivitySuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, ActivitySuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1,
                feedback_list[i].activity_name
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_previousMore_nextEnd) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_BUG_REPORT;
    auto feedback_list = check_previousMore_nextEnd<feedback_type, BugReportFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, BugReportFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_previousMore_nextEnd) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK;
    auto feedback_list = check_previousMore_nextEnd<feedback_type, OtherSuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_previousMore_nextMore) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION;
    auto feedback_list = check_previousMore_nextMore<feedback_type, ActivitySuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, ActivitySuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1,
                feedback_list[i].activity_name
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_previousMore_nextMore) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_BUG_REPORT;
    auto feedback_list = check_previousMore_nextMore<feedback_type, BugReportFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, BugReportFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_previousMore_nextMore) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK;
    auto feedback_list = check_previousMore_nextMore<feedback_type, OtherSuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_noFeedback) {
    check_noFeedback<FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION>();
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_noFeedback) {
    check_noFeedback<FeedbackType::FEEDBACK_TYPE_BUG_REPORT>();
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_noFeedback) {
    check_noFeedback<FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK>();
}

TEST_F(GetInitialFeedbackTesting, feedbackType_activity_neverBeforeRequested) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_ACTIVITY_SUGGESTION;
    auto feedback_list = check_previousMore_nextMore<feedback_type, ActivitySuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, ActivitySuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1,
                feedback_list[i].activity_name
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_bugReport_neverBeforeRequested) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_BUG_REPORT;
    auto feedback_list = check_previousMore_nextMore<feedback_type, BugReportFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, BugReportFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, feedbackType_other_neverBeforeRequested) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK;
    auto feedback_list = check_previousMore_nextMore<feedback_type, OtherSuggestionFeedbackDoc>();

    for(size_t i = 0; i < feedback_list.size(); ++i) {
        compareSingleFeedbackUnit<feedback_type, OtherSuggestionFeedbackDoc>(
                feedback_list[i],
                (int)i+1
        );
    }
}

TEST_F(GetInitialFeedbackTesting, identicalTimestampsRequested) {
    const FeedbackType feedback_type = FeedbackType::FEEDBACK_TYPE_OTHER_FEEDBACK;
    std::vector<OtherSuggestionFeedbackDoc> feedback_list;
    generateRandomFeedback<OtherSuggestionFeedbackDoc>(feedback_list);

    const bsoncxx::types::b_date timestamp_to_request{std::chrono::milliseconds{100}};

    feedback_list[0].timestamp_stored = timestamp_to_request;
    feedback_list[0].setIntoCollection();

    feedback_list[1].timestamp_stored = timestamp_to_request;
    feedback_list[1].setIntoCollection();

    feedback_list[2].timestamp_stored = timestamp_to_request;
    feedback_list[2].setIntoCollection();

    request.set_feedback_type(feedback_type);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.feedback_list_size(), 5);

    EXPECT_TRUE(response.feedback_list(0).has_end_of_feedback_element());
    EXPECT_TRUE(response.feedback_list(4).has_end_of_feedback_element());

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
                (int)i + 1
        );
    }

}
