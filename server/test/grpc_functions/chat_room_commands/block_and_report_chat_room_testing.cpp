//
// Created by jeremiah on 7/31/22.
//

#include <utility_general_functions.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <database_names.h>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "build_match_made_chat_room.h"
#include "report_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class BlockAndReportChatRoomTests : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp{-1};

    bsoncxx::oid generated_account_oid;
    bsoncxx::oid second_generated_account_oid;

    UserAccountDoc user_account;
    UserAccountDoc second_user_account;

    ChatRoomHeaderDoc original_chat_room_header;

    std::string matching_chat_room_id;

    //Set up a match.
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);
        second_generated_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(generated_account_oid);
        second_user_account.getFromCollection(second_generated_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(BlockAndReportChatRoomTests, successful) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_OTHER;
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
        request.mutable_match_options_request()->mutable_login_info(),
        generated_account_oid,
        user_account.logged_in_token,
        user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.number_times_this_user_reported_from_chat_room++;
    user_account.other_users_blocked.emplace_back(
        second_generated_account_oid.to_string(),
        bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    second_user_account.number_times_reported_by_others_in_chat_room++;

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);

    OutstandingReports generated_report;
    generated_report.current_object_oid = second_generated_account_oid;

    if(report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED <= 1)  {
        generated_report.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};
    }

    generated_report.reports_log.emplace_back(
        generated_account_oid,
        report_reason,
        other_info,
        report_origin,
        chat_room_id,
        message_uuid,
        bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    EXPECT_EQ(extracted_report, generated_report);
}

TEST_F(BlockAndReportChatRoomTests, successful_withUnmatch) {

    current_timestamp = getCurrentTimestamp();

    matching_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            generated_account_oid,
            second_generated_account_oid,
            user_account,
            second_user_account
    );

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    const std::string other_info;
    const std::string chat_room_id = matching_chat_room_id;
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.set_chat_room_id(chat_room_id);
    request.set_un_match(true);

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.number_times_this_user_reported_from_chat_room++;
    user_account.other_users_blocked.emplace_back(
            second_generated_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    second_user_account.number_times_reported_by_others_in_chat_room++;

    EXPECT_FALSE(user_account.other_accounts_matched_with.empty());
    EXPECT_FALSE(user_account.chat_rooms.empty());
    if(!user_account.other_accounts_matched_with.empty()
        && !user_account.chat_rooms.empty()) {
        user_account.other_accounts_matched_with.pop_back();
        user_account.chat_rooms.pop_back();
    }

    EXPECT_FALSE(second_user_account.other_accounts_matched_with.empty());
    EXPECT_FALSE(second_user_account.chat_rooms.empty());
    if(!second_user_account.other_accounts_matched_with.empty()
       && !second_user_account.chat_rooms.empty()) {
        second_user_account.other_accounts_matched_with.pop_back();
        second_user_account.chat_rooms.pop_back();
    }

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);

    OutstandingReports generated_report;
    generated_report.current_object_oid = second_generated_account_oid;

    if(report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED <= 1)  {
        generated_report.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};
    }

    generated_report.reports_log.emplace_back(
            generated_account_oid,
            report_reason,
            other_info,
            report_origin,
            chat_room_id,
            message_uuid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    EXPECT_EQ(extracted_report, generated_report);
}

TEST_F(BlockAndReportChatRoomTests, successful_withUnmatch_invalidChatRoomId) {

    current_timestamp = getCurrentTimestamp();

    matching_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            generated_account_oid,
            second_generated_account_oid,
            user_account,
            second_user_account
    );

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    const std::string other_info;
    const std::string invalid_chat_room_id = "1234";
    const std::string chat_room_id = matching_chat_room_id;
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.set_chat_room_id(invalid_chat_room_id);
    request.set_un_match(true);

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    user_account.number_times_this_user_reported_from_chat_room++;
    user_account.other_users_blocked.emplace_back(
            second_generated_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    second_user_account.number_times_reported_by_others_in_chat_room++;

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(extracted_user_account.other_users_blocked.size(), 1);
    if(!extracted_user_account.other_users_blocked.empty()) {
        user_account.other_users_blocked.back().timestamp_blocked = extracted_user_account.other_users_blocked.back().timestamp_blocked;
    }

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);

    OutstandingReports generated_report;
    generated_report.current_object_oid = second_generated_account_oid;

    generated_report.reports_log.emplace_back(
            generated_account_oid,
            report_reason,
            other_info,
            report_origin,
            chat_room_id,
            message_uuid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    EXPECT_EQ(extracted_report.reports_log.size(), 1);
    if(!extracted_report.reports_log.empty()) {
        generated_report.reports_log.back().timestamp_submitted = extracted_report.reports_log.back().timestamp_submitted;
        if(report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED <= 1)  {
            generated_report.timestamp_limit_reached = extracted_report.reports_log.back().timestamp_submitted;
        }
    }

    EXPECT_EQ(extracted_report, generated_report);
}

TEST_F(BlockAndReportChatRoomTests, invalidMatchingAccountOID) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    const std::string invalid_matching_account_oid = "4234";

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(invalid_matching_account_oid);
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);
    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, invalidReportReason) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const auto report_reason = ReportReason(-1);
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);
    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, invalidOtherUser) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_OTHER;
    const std::string other_info = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + rand() % 1000);
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);
    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, invalidChatRoomId) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12378";
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);
    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, invalidMessageUuid) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = "invalid_message_uuid";
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);
    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, invalidReportOrigin) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_OTHER;
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = generateUUID();
    const auto report_origin = ReportOriginType(-1);

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);
    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, userMarkedAsSpammer) {

    grpc_chat_commands::BlockAndReportChatRoomRequest request;
    UserMatchOptionsResponse response;

    const ReportReason report_reason = ReportReason::REPORT_REASON_OTHER;
    const std::string other_info = "other_info_set";
    const std::string chat_room_id = "12345678";
    const std::string message_uuid = generateUUID();
    const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

    setupUserLoginInfo(
            request.mutable_match_options_request()->mutable_login_info(),
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front()
    );

    user_account.number_times_spam_reports_sent = report_values::MAX_NUMBER_OF_TIMES_CAN_SPAM_REPORTS;
    user_account.setIntoCollection();

    request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
    request.mutable_match_options_request()->set_report_reason(report_reason);

    request.mutable_match_options_request()->set_other_info(other_info);

    request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
    request.mutable_match_options_request()->set_message_uuid(message_uuid);

    request.mutable_match_options_request()->set_report_origin(report_origin);

    blockAndReportChatRoom(&request, &response);

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account.number_times_this_user_reported_from_chat_room++;
    user_account.other_users_blocked.emplace_back(
            second_generated_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    second_user_account.number_times_reported_by_others_in_chat_room++;

    UserAccountDoc extracted_user_account(generated_account_oid);
    UserAccountDoc extracted_second_user_account(second_generated_account_oid);

    EXPECT_EQ(user_account, extracted_user_account);
    EXPECT_EQ(second_user_account, extracted_second_user_account);

    OutstandingReports extracted_report(second_generated_account_oid);

    EXPECT_TRUE(extracted_report.reports_log.empty());
}

TEST_F(BlockAndReportChatRoomTests, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {

                grpc_chat_commands::BlockAndReportChatRoomRequest request;
                UserMatchOptionsResponse response;

                const ReportReason report_reason = ReportReason::REPORT_REASON_OTHER;
                const std::string other_info = "other_info_set";
                const std::string chat_room_id = "12345678";
                const std::string message_uuid = generateUUID();
                const ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MEMBER;

                request.mutable_match_options_request()->mutable_login_info()->CopyFrom(login_info);

                request.mutable_match_options_request()->set_match_account_id(second_generated_account_oid.to_string());
                request.mutable_match_options_request()->set_report_reason(report_reason);

                request.mutable_match_options_request()->set_other_info(other_info);

                request.mutable_match_options_request()->set_chat_room_id(chat_room_id);
                request.mutable_match_options_request()->set_message_uuid(message_uuid);

                request.mutable_match_options_request()->set_report_origin(report_origin);

                blockAndReportChatRoom(&request, &response);

                return response.return_status();
            }
    );
}