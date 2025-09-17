//
// Created by jeremiah on 9/19/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "HandleFeedback.pb.h"
#include "HandleReports.pb.h"
#include "handle_reports.h"
#include "reports_objects.h"
#include "generate_randoms.h"
#include "generate_multiple_random_accounts.h"
#include "report_values.h"
#include "user_event_commands.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class TimeOutUserTesting : public ::testing::Test {
protected:

    handle_reports::TimeOutUserRequest request;
    handle_reports::TimeOutUserResponse response;

    bsoncxx::oid user_account_oid;

    const std::chrono::milliseconds current_timestamp{100};

    UserAccountDoc user_account_doc;

    OutstandingReports outstanding_reports;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_user_oid(user_account_oid.to_string());

        using namespace server_parameter_restrictions;
        request.set_inactive_message(gen_random_alpha_numeric_string(
                rand() % (MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE-MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE)
                + MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE)
        );
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        outstanding_reports = generateRandomOutstandingReports(
                user_account_oid,
                current_timestamp
        );
        outstanding_reports.setIntoCollection();

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        timeOutUser(
                &request, &response
        );
    }

    void compareDocuments() {
        OutstandingReports extracted_outstanding_reports(outstanding_reports.current_object_oid);
        EXPECT_EQ(outstanding_reports, extracted_outstanding_reports);

        UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    void expectFailedReturn() {
        EXPECT_FALSE(response.error_message().empty());
        EXPECT_FALSE(response.successful());

        //Expect no changes
        compareDocuments();
    }

    template <bool compare_reports = true>
    void expectSuccessfulReturn(
        const DisciplinaryActionTypeEnum disciplinary_action_taken,
        const UserAccountStatus updated_user_account_status,
        const int index_in_time_out_times
    ) {

        EXPECT_TRUE(response.error_message().empty());
        EXPECT_TRUE(response.successful());

        //current_timestamp inside timeOutUser()
        const std::chrono::milliseconds timestamp_used{response.timestamp_last_updated()};

        const std::chrono::milliseconds suspension_expires =
                index_in_time_out_times < report_values::NUMBER_TIMES_TIMED_OUT_BEFORE_BAN ?
                timestamp_used + report_values::TIME_OUT_TIMES[index_in_time_out_times] :
                std::chrono::milliseconds{-1};

        EXPECT_GT(timestamp_used.count(), 0);

        EXPECT_EQ(response.updated_user_account_status(), updated_user_account_status);
        EXPECT_EQ(response.timestamp_suspension_expires(), suspension_expires.count());

        user_account_doc.status = updated_user_account_status;
        user_account_doc.inactive_message = request.inactive_message();
        user_account_doc.inactive_end_time = bsoncxx::types::b_date{suspension_expires};
        user_account_doc.number_of_times_timed_out++;

        user_account_doc.disciplinary_record.emplace_back(
                bsoncxx::types::b_date{timestamp_used},
                bsoncxx::types::b_date{suspension_expires},
                disciplinary_action_taken,
                request.inactive_message(),
                TEMP_ADMIN_ACCOUNT_NAME
        );

        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);

        if(compare_reports) {
            //Outstanding report no longer exists
            OutstandingReports extracted_outstanding_reports(user_account_oid);
            EXPECT_EQ(extracted_outstanding_reports.current_object_oid.to_string(), "000000000000000000000000");

            //Outstanding report was moved to handled reports collection
            HandledReports generated_handled_report;

            generated_handled_report.current_object_oid = user_account_oid;
            generated_handled_report.handled_reports_info.emplace_back(
                    HandledReports::ReportsArrayElement(
                            outstanding_reports,
                            TEMP_ADMIN_ACCOUNT_NAME,
                            bsoncxx::types::b_date{timestamp_used},
                            ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN,
                            disciplinary_action_taken
                    )
            );

            HandledReports extracted_handled_report(user_account_oid);
            EXPECT_EQ(generated_handled_report, extracted_handled_report);
        }
    }

    int setupNumberTimesTimedOut(int number_times_timed_out) {
        user_account_doc.number_of_times_timed_out = number_times_timed_out;
        user_account_doc.setIntoCollection();
        return number_times_timed_out;
    }

    void deleteOutstandingReport() {
        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
        mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];

        outstanding_reports_collection.delete_one(
                document{}
                        << "_id" << outstanding_reports.current_object_oid
                        << finalize
        );
    }
};

TEST_F(TimeOutUserTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_message().empty());
                return response.successful();
            }
    );

    compareDocuments();
}

TEST_F(TimeOutUserTesting, noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    expectFailedReturn();
}

TEST_F(TimeOutUserTesting, invalidUserOid) {
    request.set_user_oid("123");

    runFunction();

    expectFailedReturn();
}

TEST_F(TimeOutUserTesting, inactive_message_tooShort) {
    request.set_inactive_message(gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE));

    runFunction();

    expectFailedReturn();
}

TEST_F(TimeOutUserTesting, inactive_message_tooLong) {
    request.set_inactive_message(gen_random_alpha_numeric_string(rand() % 10 + server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE + 1));

    runFunction();

    expectFailedReturn();
}

TEST_F(TimeOutUserTesting, firstTimeout) {
    runFunction();

    expectSuccessfulReturn(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED,
            UserAccountStatus::STATUS_SUSPENDED,
            0
    );
}

TEST_F(TimeOutUserTesting, secondTimeout) {
    const int number_times_timed_out = setupNumberTimesTimedOut(1);

    runFunction();

    expectSuccessfulReturn(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED,
            UserAccountStatus::STATUS_SUSPENDED,
            number_times_timed_out
    );
}

TEST_F(TimeOutUserTesting, thirdTimeout) {
    const int number_times_timed_out = setupNumberTimesTimedOut(2);

    runFunction();

    expectSuccessfulReturn(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED,
            UserAccountStatus::STATUS_SUSPENDED,
            number_times_timed_out
    );
}

TEST_F(TimeOutUserTesting, fourthTimeout) {
    const int number_times_timed_out = setupNumberTimesTimedOut(3);

    runFunction();

    expectSuccessfulReturn(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED,
            UserAccountStatus::STATUS_SUSPENDED,
            number_times_timed_out
    );
}

TEST_F(TimeOutUserTesting, fifthTimeout) {
    const int number_times_timed_out = setupNumberTimesTimedOut(4);

    runFunction();

    expectSuccessfulReturn(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_BANNED,
            UserAccountStatus::STATUS_BANNED,
            number_times_timed_out
    );
}

TEST_F(TimeOutUserTesting, userAlreadyTimedOut) {
    //This should return true, however change anything. Can not time out an account
    // that was already timed out.

    user_account_doc.status = UserAccountStatus::STATUS_SUSPENDED;
    user_account_doc.setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    compareDocuments();
}

TEST_F(TimeOutUserTesting, noOutstandingReportExists) {
    //This allows an admin to look up a user and time them out even without an outstanding report
    // existing for the user.

    deleteOutstandingReport();

    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    expectSuccessfulReturn<false>(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED,
            UserAccountStatus::STATUS_SUSPENDED,
            0
    );
}

TEST_F(TimeOutUserTesting, userCreatedEvent) {

    //User must be subscribed to create events.
    user_account_doc.subscription_status = UserSubscriptionStatus::BASIC_SUBSCRIPTION;
    user_account_doc.setIntoCollection();

    const EventChatRoomAdminInfo chat_room_admin_info{
            UserAccountType::USER_ACCOUNT_TYPE,
            user_account_oid.to_string(),
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    };

    EventRequestMessage event_request = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    const std::chrono::milliseconds& current_timestamp = getCurrentTimestamp();

    EXPECT_FALSE(event_request.activity().time_frame_array().empty());
    if(!event_request.activity().time_frame_array().empty()
        && event_request.activity().time_frame_array(0).start_time_frame() < current_timestamp.count()) {
        const int diff = (int)event_request.activity().time_frame_array(0).stop_time_frame() - (int)event_request.activity().time_frame_array(0).start_time_frame();

        event_request.mutable_activity()->mutable_time_frame_array(0)->set_start_time_frame(current_timestamp.count() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
        event_request.mutable_activity()->mutable_time_frame_array(0)->set_stop_time_frame(event_request.activity().time_frame_array(0).start_time_frame() + diff);
    }

    user_event_commands::AddUserEventRequest add_user_event_request;
    user_event_commands::AddUserEventResponse add_user_event_response;

    setupUserLoginInfo(
            add_user_event_request.mutable_login_info(),
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front()
    );

    add_user_event_request.mutable_event_request()->CopyFrom(event_request);

    addUserEvent(&add_user_event_request, &add_user_event_response);

    ASSERT_EQ(add_user_event_response.return_status(), ReturnStatus::SUCCESS);

    const std::string event_account_oid = add_user_event_response.event_oid();

    user_account_doc.getFromCollection(user_account_oid);

    outstanding_reports = generateRandomOutstandingReports(
            bsoncxx::oid{event_account_oid},
            current_timestamp
    );
    outstanding_reports.setIntoCollection();

    request.set_user_oid(event_account_oid);

    UserAccountDoc event_account_doc(bsoncxx::oid{event_account_oid});

    runFunction();

    EXPECT_TRUE(response.error_message().empty());
    EXPECT_TRUE(response.successful());

    event_account_doc.event_expiration_time = bsoncxx::types::b_date{general_values::event_expiration_time_values::EVENT_CANCELED};
    event_account_doc.matching_activated = false;
    for(auto& category : event_account_doc.categories) {
        category.time_frames.clear();
    }

    UserAccountDoc extracted_event_account_doc(bsoncxx::oid{event_account_oid});
    EXPECT_EQ(event_account_doc, extracted_event_account_doc);

    expectSuccessfulReturn<false>(
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED,
            UserAccountStatus::STATUS_SUSPENDED,
            0
    );
}

TEST_F(TimeOutUserTesting, adminCreatedEvent) {
    createAndStoreEventAdminAccount();

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    auto event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    //Add an admin created event
    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    const bsoncxx::oid event_account_oid{event_created_return_values.event_account_oid};
    UserAccountDoc event_account_doc(event_account_oid);

    outstanding_reports = generateRandomOutstandingReports(
            event_account_oid,
            current_timestamp
    );
    outstanding_reports.setIntoCollection();

    request.set_user_oid(event_account_oid.to_string());

    runFunction();

    EXPECT_FALSE(response.error_message().empty());
    EXPECT_FALSE(response.successful());

    OutstandingReports extracted_outstanding_reports(outstanding_reports.current_object_oid);
    EXPECT_EQ(extracted_outstanding_reports.current_object_oid, bsoncxx::oid{"000000000000000000000000"});

    UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    UserAccountDoc extracted_event_account_doc(bsoncxx::oid{event_account_oid});
    EXPECT_EQ(event_account_doc, extracted_event_account_doc);
}
