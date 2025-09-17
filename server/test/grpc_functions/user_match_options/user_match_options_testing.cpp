//
// Created by jeremiah on 9/8/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "find_matches.h"
#include "setup_login_info.h"
#include "generate_randoms.h"
#include "user_match_options.h"
#include "info_for_statistics_objects.h"
#include "report_values.h"
#include "../find_matches/helper_functions/generate_matching_users.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UserMatchOptionsTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserMatchOptionsRequest request;
    UserMatchOptionsResponse response;

    MatchingElement matching_element;

    IndividualMatchStatisticsDoc individual_match_statistics;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        match_account_oid = insertRandomAccounts(1, 0);
        match_account_doc.getFromCollection(match_account_oid);

        ASSERT_TRUE(
                buildMatchingUserForPassedAccount(
                        user_account_doc,
                        match_account_doc,
                        user_account_oid,
                        match_account_oid
                )
        );

        individual_match_statistics.generateRandomValues(match_account_oid);
        individual_match_statistics.setIntoCollection();

        //Make this a matching element that is not expired.
        matching_element.generateRandomValues();
        matching_element.oid = match_account_oid;
        matching_element.distance = 0;
        matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};
        matching_element.from_match_algorithm_list = false;
        matching_element.saved_statistics_oid = individual_match_statistics.current_object_oid;

        user_account_doc.has_been_extracted_accounts_list.emplace_back(
                matching_element
                );
        user_account_doc.setIntoCollection();

        //setup with simple valid info
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        request.set_match_account_id(match_account_oid.to_string());
        request.set_response_type(ResponseType::USER_MATCH_OPTION_NO);
        request.set_report_origin(ReportOriginType::REPORT_ORIGIN_SWIPING);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void compareUserAccounts() {
        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    void compareMatchAccounts() {
        UserAccountDoc extracted_match_account_doc(match_account_oid);
        EXPECT_EQ(match_account_doc, extracted_match_account_doc);
    }

    void compareAccounts() {
        compareUserAccounts();

        compareMatchAccounts();
    }

    void runNoFunction() {
        receiveMatchNo(&request, &response);
    };

    void runYesFunction() {
        receiveMatchYes(&request, &response);
    };

    void compareMatchStatistics() {
        IndividualMatchStatisticsDoc extracted_individual_match_statistics(individual_match_statistics.current_object_oid);
        EXPECT_EQ(individual_match_statistics, extracted_individual_match_statistics);
    }
};

TEST_F(UserMatchOptionsTesting, no_invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) {
                request.mutable_login_info()->CopyFrom(login_info);

                runNoFunction();

                return response.return_status();
            }
    );

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidMatchAccountOid) {
    request.set_match_account_id(match_account_oid.to_string() + 'a');

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidResponseType) {
    request.set_response_type(ResponseType(-1));

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidReportReason) {
    request.set_response_type(ResponseType::USER_MATCH_OPTION_REPORT);
    request.set_report_reason(ReportReason(-1));

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidOtherInfo) {
    request.set_response_type(ResponseType::USER_MATCH_OPTION_REPORT);
    request.set_report_reason(ReportReason::REPORT_REASON_OTHER);
    request.set_other_info(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidChatRoomId) {
    request.set_chat_room_id("123");

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidMessageUuid) {
    request.set_message_uuid("456");

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_invalidReportOriginType) {
    request.set_report_origin(ReportOriginType(-1));

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, no_responseType_no) {
    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_no++;

    user_account_doc.has_been_extracted_accounts_list.pop_back();

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    match_account_doc.number_times_others_swiped_no++;

    compareAccounts();

    individual_match_statistics.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::NO,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
            );

    compareMatchStatistics();
}

TEST_F(UserMatchOptionsTesting, no_responseType_block) {

    request.set_response_type(ResponseType::USER_MATCH_OPTION_BLOCK);

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_block++;

    user_account_doc.has_been_extracted_accounts_list.pop_back();

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    user_account_doc.other_users_blocked.emplace_back(
            match_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    match_account_doc.number_times_others_swiped_block++;

    compareAccounts();

    individual_match_statistics.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::BLOCK,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    compareMatchStatistics();
}

TEST_F(UserMatchOptionsTesting, no_responseType_report) {
    request.set_response_type(ResponseType::USER_MATCH_OPTION_REPORT);
    request.set_report_reason(ReportReason::REPORT_REASON_OTHER);
    request.set_other_info(gen_random_alpha_numeric_string(rand() % 50 + 5));

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_report++;

    user_account_doc.has_been_extracted_accounts_list.pop_back();

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    user_account_doc.other_users_blocked.emplace_back(
            match_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    match_account_doc.number_times_others_swiped_report++;

    compareAccounts();

    individual_match_statistics.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::REPORT,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    compareMatchStatistics();

    OutstandingReports outstanding_report(match_account_oid);

    EXPECT_EQ(outstanding_report.current_object_oid.to_string(), match_account_oid.to_string());
    ASSERT_EQ(outstanding_report.reports_log.size(), 1);

    //The reset of the values are tested when the function to store the statistics is tested.
    EXPECT_EQ(outstanding_report.reports_log[0].account_oid, user_account_oid);
}

TEST_F(UserMatchOptionsTesting, no_userFlaggedAsSpammer) {

    user_account_doc.number_times_spam_reports_sent = report_values::MAX_NUMBER_OF_TIMES_CAN_SPAM_REPORTS;
    user_account_doc.setIntoCollection();

    request.set_response_type(ResponseType::USER_MATCH_OPTION_REPORT);
    request.set_report_reason(ReportReason::REPORT_REASON_LANGUAGE);

    runNoFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_report++;

    user_account_doc.has_been_extracted_accounts_list.pop_back();

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    user_account_doc.other_users_blocked.emplace_back(
            match_account_oid.to_string(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    match_account_doc.number_times_others_swiped_report++;

    compareAccounts();

    individual_match_statistics.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::REPORT,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    compareMatchStatistics();

    OutstandingReports outstanding_report;
    outstanding_report.getFromCollection();

    EXPECT_EQ(outstanding_report.current_object_oid.to_string(), "000000000000000000000000");
}

TEST_F(UserMatchOptionsTesting, yes_invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) {
                request.mutable_login_info()->CopyFrom(login_info);

                runYesFunction();

                return response.return_status();
            }
    );

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, yes_matchAccountId) {
    request.set_match_account_id(match_account_oid.to_string() + 'b');

    runYesFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    compareAccounts();
}

TEST_F(UserMatchOptionsTesting, yes_extractedDocumentDoesNotExist) {
    user_account_doc.has_been_extracted_accounts_list.pop_back();
    user_account_doc.setIntoCollection();

    runYesFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_yes++;

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    match_account_doc.number_times_others_swiped_yes++;

    compareAccounts();

    compareMatchStatistics();
}

TEST_F(UserMatchOptionsTesting, yes_extractedDocumentExists) {
    runYesFunction();

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::vector<std::string> collection_names = chat_rooms_db.list_collection_names();

    std::string chat_room_id;

    //extract the chat room id
    for(const auto& collection_name : collection_names) {
        //do not drop the info collection
        if(collection_name != collection_names::CHAT_ROOM_INFO) {
            chat_room_id = collection_name.substr(collection_names::CHAT_ROOM_ID_.size());
        }
    }

    ASSERT_FALSE(isInvalidChatRoomId(chat_room_id));

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_yes++;

    user_account_doc.has_been_extracted_accounts_list.pop_back();

    user_account_doc.previously_matched_accounts.emplace_back(
            match_account_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    bsoncxx::types::b_date last_active_timestamp{std::chrono::milliseconds{response.timestamp()} + std::chrono::milliseconds{2}};

    match_account_doc.chat_rooms.emplace_back(
            chat_room_id,
            last_active_timestamp
    );

    match_account_doc.other_accounts_matched_with.emplace_back(
            user_account_oid.to_string(),
            last_active_timestamp
    );

    user_account_doc.chat_rooms.emplace_back(
            chat_room_id,
            last_active_timestamp
    );

    user_account_doc.other_accounts_matched_with.emplace_back(
            match_account_oid.to_string(),
            last_active_timestamp
    );

    match_account_doc.number_times_others_swiped_yes++;

    compareAccounts();

    individual_match_statistics.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::YES,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    compareMatchStatistics();

    //NOTE: This is checked more thoroughly in other places such as move_extracted_element_to_other_account_testing.cpp.
    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_NE(extracted_chat_room_header.matching_oid_strings, nullptr);
}

TEST_F(UserMatchOptionsTesting, yes_eventMatched) {

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    EventRequestMessage event_info = generateRandomEventRequestMessage(
            chat_room_admin_info,
            current_timestamp
    );

    //Make sure event can be matched by user.
    event_info.clear_allowed_genders();
    event_info.add_allowed_genders(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);

    event_info.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    event_info.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

    auto event_created_return_values = generateRandomEvent(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp,
            event_info
    );

    const std::string chat_room_id = event_created_return_values.chat_room_return_info.chat_room_id();

    request.set_response_type(ResponseType::USER_MATCH_OPTION_YES);
    request.set_match_account_id(event_created_return_values.event_account_oid);

    UserAccountDoc event_account_doc(bsoncxx::oid{event_created_return_values.event_account_oid});

    IndividualMatchStatisticsDoc event_match_statistics;
    event_match_statistics.generateRandomValues(match_account_oid);
    event_match_statistics.setIntoCollection();

    MatchingElement event_matching_element;

    //Make this a matching element that is not expired.
    event_matching_element.generateRandomValues();
    event_matching_element.oid = bsoncxx::oid{event_created_return_values.event_account_oid};
    event_matching_element.distance = 0;
    event_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};
    event_matching_element.from_match_algorithm_list = true;
    event_matching_element.saved_statistics_oid = event_match_statistics.current_object_oid;

    user_account_doc.has_been_extracted_accounts_list.clear();
    user_account_doc.has_been_extracted_accounts_list.emplace_back(
            event_matching_element
    );
    user_account_doc.setIntoCollection();

    runYesFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

    user_account_doc.number_times_swiped_yes++;

    user_account_doc.previously_matched_accounts.emplace_back(
            bsoncxx::oid{event_created_return_values.event_account_oid},
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
            1
    );

    UserAccountDoc extracted_user_account_doc(user_account_oid);
    EXPECT_EQ(extracted_user_account_doc.chat_rooms.size(), 1);
    if(extracted_user_account_doc.chat_rooms.size() == 1) {
        user_account_doc.chat_rooms.emplace_back(
                chat_room_id,
                extracted_user_account_doc.chat_rooms[0].last_time_viewed,
                bsoncxx::oid{event_created_return_values.event_account_oid}
        );
    }

    user_account_doc.has_been_extracted_accounts_list.pop_back();

    event_account_doc.number_times_others_swiped_yes++;

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    UserAccountDoc extracted_event_account_doc(bsoncxx::oid{event_created_return_values.event_account_oid});
    EXPECT_EQ(event_account_doc, extracted_event_account_doc);

    event_match_statistics.status_array.emplace_back(
            individual_match_statistics_keys::status_array::status_enum::YES,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    IndividualMatchStatisticsDoc extracted_individual_match_statistics(event_match_statistics.current_object_oid);
    EXPECT_EQ(event_match_statistics, extracted_individual_match_statistics);

    //NOTE: This is checked more thoroughly in other places such as move_extracted_element_to_other_account_testing.cpp.
    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_TRUE(extracted_chat_room_header.event_id.operator bool());
}
