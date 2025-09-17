//
// Created by jeremiah on 9/19/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"
#include <google/protobuf/util/message_differencer.h>

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
#include "grpc_mock_stream/mock_stream.h"
#include "report_values.h"
#include "generate_multiple_random_accounts.h"
#include "generate_random_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestReportsTesting : public ::testing::Test {
protected:

    handle_reports::RequestReportsRequest request;
    grpc::testing::MockServerWriterVector<::handle_reports::RequestReportsResponse> response_writer;

    int number_user_to_request = 1;

    bsoncxx::oid dummy_user_account_oid;
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    OutstandingReports outstanding_reports = generateRandomOutstandingReports(
            dummy_user_account_oid,
            current_timestamp
    );

    std::vector<std::pair<bsoncxx::oid, std::unique_ptr<OutstandingReports>>> oid_and_outstanding_reports;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        outstanding_reports.checked_out_end_time_reached = bsoncxx::types::b_date{
                current_timestamp - std::chrono::milliseconds{60L * 1000L}};
        outstanding_reports.setIntoCollection();
        request.set_number_user_to_request(number_user_to_request);
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

        //guarantee different timestamp for function run
        std::this_thread::sleep_for(std::chrono::milliseconds{16});

        requestReports(
                &request, &response_writer
        );
    }

    void compareOutstandingReportUnchanged() {
        OutstandingReports extracted_outstanding_reports(outstanding_reports.current_object_oid);
        EXPECT_EQ(outstanding_reports, extracted_outstanding_reports);
    }

    static void compareOutstandingReportSetCheckoutTime(OutstandingReports& passed_outstanding_report) {
        OutstandingReports extracted_outstanding_reports(passed_outstanding_report.current_object_oid);
        passed_outstanding_report.checked_out_end_time_reached = extracted_outstanding_reports.checked_out_end_time_reached;

        EXPECT_EQ(passed_outstanding_report, extracted_outstanding_reports);
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

    void setupAccountAndOutstandingReport() {

        dummy_user_account_oid = insertRandomAccounts(1, 0);

        std::unique_ptr outstanding_report_ptr = generateRandomOutstandingReports(
                        dummy_user_account_oid,
                        current_timestamp,
                        false
                );

        outstanding_report_ptr->checked_out_end_time_reached = bsoncxx::types::b_date{
                current_timestamp - std::chrono::milliseconds{60L * 1000L}};
        outstanding_report_ptr->setIntoCollection();

        oid_and_outstanding_reports.emplace_back(
                std::pair<bsoncxx::oid, std::unique_ptr<OutstandingReports>>{
                        dummy_user_account_oid,
                        std::move(outstanding_report_ptr)
                }
        );
    }

    void setupMultipleAccountAndOutstandingReport(
            const int number_to_insert
            ) {
        for(int i = 0; i < number_to_insert; ++i) {
            setupAccountAndOutstandingReport();
        }

        //Sort for vector to match message return order.
        std::sort(oid_and_outstanding_reports.begin(), oid_and_outstanding_reports.end(), [](
                const std::pair<bsoncxx::oid, std::unique_ptr<OutstandingReports>>& l, const std::pair<bsoncxx::oid, std::unique_ptr<OutstandingReports>>& r
        ){
            return l.second->timestamp_limit_reached < r.second->timestamp_limit_reached;
        });
    }

    void static compareReportedUserInfo(
        const handle_reports::ReturnedInfo& returned_info,
        const OutstandingReports& outstanding_report
    ) {
        ASSERT_TRUE(returned_info.has_reported_user_info());

        const handle_reports::ReportedUserInfo& returned_reported_user_info = returned_info.reported_user_info();

        handle_reports::ReportedUserInfo reported_user_info = outstanding_report.convertToReportedUserInfo();

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                reported_user_info,
                returned_reported_user_info
        );

        if (!equivalent) {
            std::cout << "reported_user_info\n" << reported_user_info.DebugString() << '\n';
            std::cout << "returned_message\n" << returned_reported_user_info.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    static void compareReturnedUserAccount(
            const handle_reports::ReturnedInfo& returned_info,
            const bsoncxx::oid& account_oid
    ) {

        ASSERT_TRUE(returned_info.has_user_account_info());

        const CompleteUserAccountInfo& returned_user_account_info_msg = returned_info.user_account_info();

        UserAccountDoc user_account_doc(account_oid);

        CompleteUserAccountInfo user_account_info_msg = user_account_doc.convertToCompleteUserAccountInfo(
                std::chrono::milliseconds{returned_user_account_info_msg.timestamp_user_returned()}
        );

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                user_account_info_msg,
                returned_user_account_info_msg
        );

        if (!equivalent) {
            for (auto& pic: *user_account_info_msg.mutable_picture_list()) {
                pic.mutable_picture()->clear_file_in_bytes();
            }
            CompleteUserAccountInfo response_writer_copy = returned_user_account_info_msg;
            for (auto& pic: *response_writer_copy.mutable_picture_list()) {
                pic.mutable_picture()->clear_file_in_bytes();
            }
            std::cout << "user_account_info_msg\n" << user_account_info_msg.DebugString() << '\n';
            std::cout << "returned_message\n" << response_writer_copy.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    void static compareChatMessageToClient(
            const handle_reports::ReturnedInfo& returned_info,
            const bsoncxx::oid& account_oid,
            const std::string& chat_room_id,
            const std::string& message_uuid
    ) {

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
        mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

        ASSERT_TRUE(returned_info.has_chat_message());

        const ChatMessageToClient& returned_chat_message = returned_info.chat_message();

        ChatRoomMessageDoc chat_room_message_doc(message_uuid, chat_room_id);

        const ChatMessageToClient generated_chat_message = chat_room_message_doc.convertToChatMessageToClient(
                account_oid.to_string(),
                AmountOfMessage::COMPLETE_MESSAGE_INFO,
                false,
                false
        );

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                generated_chat_message,
                returned_chat_message
        );

        if (!equivalent) {
            std::cout << "generated_chat_message\n" << generated_chat_message.DebugString() << '\n';
            std::cout << "returned_message\n" << returned_chat_message.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    void static compareReportStreamCompleted(
            const handle_reports::ReturnedInfo& returned_info,
            const bsoncxx::oid& user_account_oid
    ) {

        ASSERT_TRUE(returned_info.has_completed());

        const handle_reports::ReportStreamCompleted& returned_report_stream_completed = returned_info.completed();

        handle_reports::ReportStreamCompleted report_stream_completed;
        report_stream_completed.set_user_account_oid(user_account_oid.to_string());

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                report_stream_completed,
                returned_report_stream_completed
        );

        if (!equivalent) {
            std::cout << "report_stream_completed\n" << report_stream_completed.DebugString() << '\n';
            std::cout << "returned_message\n" << returned_report_stream_completed.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    void static compareNoReportsToReturnMessage(
            const handle_reports::ReturnedInfo& returned_info
    ) {
        ASSERT_TRUE(returned_info.has_no_reports());
    }

    static void generateChatMessageForOutstandingReports(
            OutstandingReports& outstanding_report,
            const bsoncxx::oid& user_account_oid
            ) {
        ASSERT_TRUE(outstanding_report.reports_log.size());

        UserAccountDoc user_account_doc(user_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        auto [text_message_request, text_message_response] = generateRandomTextMessage(
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front(),
                create_chat_room_response.chat_room_id()
        );

        outstanding_report.reports_log[0].chat_room_id = create_chat_room_response.chat_room_id();
        outstanding_report.reports_log[0].message_uuid = text_message_request.message_uuid();
        outstanding_report.setIntoCollection();
    }

    template<bool include_chat_message = false>
    void compareSingleUserWithoutChatMessage(int starting_index = 0) {

        int calibrated_index = starting_index*3;

        compareReportedUserInfo(
                response_writer.write_params[calibrated_index].msg.returned_info(),
                *oid_and_outstanding_reports[starting_index].second
        );

        calibrated_index++;

        compareReturnedUserAccount(
                response_writer.write_params[calibrated_index].msg.returned_info(),
                oid_and_outstanding_reports[starting_index].first
        );

        calibrated_index++;

        if(include_chat_message) {
            compareChatMessageToClient(
                    response_writer.write_params[calibrated_index].msg.returned_info(),
                    oid_and_outstanding_reports[starting_index].first,
                    oid_and_outstanding_reports[starting_index].second->reports_log[0].chat_room_id,
                    oid_and_outstanding_reports[starting_index].second->reports_log[0].message_uuid
            );

            calibrated_index++;
        }

        compareReportStreamCompleted(
                response_writer.write_params[calibrated_index].msg.returned_info(),
                oid_and_outstanding_reports[starting_index].first
        );

        compareOutstandingReportSetCheckoutTime(*oid_and_outstanding_reports[starting_index].second);
    }
};

TEST_F(RequestReportsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {
                request.mutable_login_info()->CopyFrom(login_info);

                response_writer.write_params.clear();
                runFunction();

                EXPECT_EQ(response_writer.write_params.size(), 1);
                if (response_writer.write_params.size() == 1) {
                    EXPECT_FALSE(response_writer.write_params[0].msg.error_message().empty());
                    return response_writer.write_params[0].msg.successful();
                } else {
                    return false;
                }
            }
    );

    //guarantee nothing changed
    compareOutstandingReportUnchanged();
}

TEST_F(RequestReportsTesting, numberUserToRequest_0) {
    request.set_number_user_to_request(0);
    runFunction();

    ASSERT_EQ(response_writer.write_params.size(), 1);
    EXPECT_FALSE(response_writer.write_params[0].msg.error_message().empty());
    EXPECT_FALSE(response_writer.write_params[0].msg.successful());

    //guarantee nothing changed
    compareOutstandingReportUnchanged();
}

TEST_F(RequestReportsTesting, timestampLimitReached_notSet) {
    outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    outstanding_reports.setIntoCollection();

    runFunction();

    ASSERT_EQ(response_writer.write_params.size(), 1);
    EXPECT_TRUE(response_writer.write_params[0].msg.error_message().empty());
    EXPECT_TRUE(response_writer.write_params[0].msg.successful());
    EXPECT_TRUE(response_writer.write_params[0].msg.returned_info().has_no_reports());

    //guarantee nothing changed
    compareOutstandingReportUnchanged();
}

TEST_F(RequestReportsTesting, timestampLimitReached_set_reportStillCheckedOut) {

    outstanding_reports.checked_out_end_time_reached = bsoncxx::types::b_date{
            getCurrentTimestamp() + std::chrono::milliseconds{60L * 1000L}};
    outstanding_reports.setIntoCollection();

    runFunction();

    ASSERT_EQ(response_writer.write_params.size(), 1);
    EXPECT_TRUE(response_writer.write_params[0].msg.error_message().empty());
    EXPECT_TRUE(response_writer.write_params[0].msg.successful());
    EXPECT_TRUE(response_writer.write_params[0].msg.returned_info().has_no_reports());

    //guarantee nothing changed
    compareOutstandingReportUnchanged();

}

TEST_F(RequestReportsTesting, noReportsFound) {

    deleteOutstandingReport();

    runFunction();

    ASSERT_EQ(response_writer.write_params.size(), 1);
    EXPECT_TRUE(response_writer.write_params[0].msg.error_message().empty());
    EXPECT_TRUE(response_writer.write_params[0].msg.successful());
    EXPECT_TRUE(response_writer.write_params[0].msg.returned_info().has_no_reports());

    OutstandingReports extracted_outstanding_reports(outstanding_reports.current_object_oid);
    EXPECT_EQ(extracted_outstanding_reports.current_object_oid.to_string(), "000000000000000000000000");
}

TEST_F(RequestReportsTesting, singleReportRequestedAndReturned) {

    deleteOutstandingReport();

    setupAccountAndOutstandingReport();

    runFunction();

    ASSERT_EQ(response_writer.write_params.size(), 3);

    compareSingleUserWithoutChatMessage();
}

TEST_F(RequestReportsTesting, singleReportRequestedAndReturned_withTextMessage) {
    deleteOutstandingReport();

    setupAccountAndOutstandingReport();

    generateChatMessageForOutstandingReports(
            *oid_and_outstanding_reports[0].second,
            oid_and_outstanding_reports[0].first
    );

    runFunction();

    ASSERT_EQ(response_writer.write_params.size(), 4);

    compareSingleUserWithoutChatMessage<true>();

    compareOutstandingReportSetCheckoutTime(*oid_and_outstanding_reports[0].second);
}

TEST_F(RequestReportsTesting, multipleReportsRequestedAndReturned) {
    deleteOutstandingReport();

    number_user_to_request = report_values::MAX_NUMBER_OF_REPORTED_USERS_ADMIN_CAN_REQUEST_NOTIFIED;

    request.set_number_user_to_request(number_user_to_request);

    setupMultipleAccountAndOutstandingReport(
            number_user_to_request
    );

    runFunction();

    const int total_num_messages = number_user_to_request*3;

    ASSERT_EQ(response_writer.write_params.size(), total_num_messages);

    for(int i = 0; i < number_user_to_request; ++i) {
        compareSingleUserWithoutChatMessage(i);
    }
}

TEST_F(RequestReportsTesting, lessReportsReturnedThanRequested) {
    deleteOutstandingReport();

    number_user_to_request = report_values::MAX_NUMBER_OF_REPORTED_USERS_ADMIN_CAN_REQUEST_NOTIFIED;
    const int number_to_insert = number_user_to_request - 1;

    request.set_number_user_to_request(number_user_to_request);

    setupMultipleAccountAndOutstandingReport(
            number_to_insert
    );

    runFunction();

    const int total_num_messages = number_to_insert * 3 + 1;

    ASSERT_EQ(response_writer.write_params.size(), total_num_messages);

    for(int i = 0; i < number_to_insert; ++i) {
        compareSingleUserWithoutChatMessage(i);
    }

    compareNoReportsToReturnMessage(response_writer.write_params.back().msg.returned_info());
}
