//
// Created by jeremiah on 5/31/22.
//

#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <generate_multiple_random_accounts.h>
#include <utility_general_functions.h>
#include <reports_objects.h>
#include <report_values.h>
#include <generate_randoms.h>
#include <report_helper_functions.h>
#include "gtest/gtest.h"

#include "clear_database_for_testing.h"
#include "report_helper_functions_test.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ReportsHelperFunctions : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(ReportsHelperFunctions, saveNewReportToCollection_newDocument) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::oid reporting_user_account_oid;
    bsoncxx::oid reported_user_account_oid;
    ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_INFO;
    std::string report_message = "generic report message I guess";
    std::string chat_room_id = "12345678";
    std::string message_uuid = generateUUID();

    bool return_val = saveNewReportToCollection(
            mongo_cpp_client,
            current_timestamp,
            reporting_user_account_oid,
            reported_user_account_oid,
            report_reason,
            report_origin,
            report_message,
            chat_room_id,
            message_uuid
            );

    EXPECT_TRUE(return_val);

    OutstandingReports generated_outstanding_reports;

    generated_outstanding_reports.current_object_oid = reported_user_account_oid;
    if(report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED <= 1) {
        generated_outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{current_timestamp};
    } else {
        generated_outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    }

    generated_outstanding_reports.reports_log.emplace_back(
            reporting_user_account_oid,
            report_reason,
            report_message,
            report_origin,
            chat_room_id,
            message_uuid,
            bsoncxx::types::b_date{current_timestamp}
    );

    OutstandingReports extracted_outstanding_reports(reported_user_account_oid);

    EXPECT_EQ(generated_outstanding_reports, extracted_outstanding_reports);
}

TEST_F(ReportsHelperFunctions, saveNewReportToCollection_outstandingReportsUserDocumentAlreadyExists) {

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::oid reported_user_account_oid;
    bsoncxx::oid reporting_user_account_oid;

    OutstandingReports generated_outstanding_reports;

    generated_outstanding_reports.current_object_oid = reported_user_account_oid;
    generated_outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_outstanding_reports.reports_log.emplace_back(
            bsoncxx::oid{},
            ReportReason::REPORT_REASON_ADVERTISING,
            gen_random_alpha_numeric_string(rand() % 100 + 10),
            ReportOriginType::REPORT_ORIGIN_SWIPING,
            generateRandomChatRoomId(),
            generateUUID(),
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{10L * 1000L}}
    );
    generated_outstanding_reports.setIntoCollection();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_INFO;
    std::string report_message = "generic report message I guess";
    std::string chat_room_id = "12345678";
    std::string message_uuid = generateUUID();

    bool return_val = saveNewReportToCollection(
            mongo_cpp_client,
            current_timestamp,
            reporting_user_account_oid,
            reported_user_account_oid,
            report_reason,
            report_origin,
            report_message,
            chat_room_id,
            message_uuid
    );

    EXPECT_TRUE(return_val);

    generated_outstanding_reports.reports_log.emplace_back(
            reporting_user_account_oid,
            report_reason,
            report_message,
            report_origin,
            chat_room_id,
            message_uuid,
            bsoncxx::types::b_date{current_timestamp}
    );

    OutstandingReports extracted_outstanding_reports(reported_user_account_oid);

    EXPECT_EQ(generated_outstanding_reports, extracted_outstanding_reports);
}

TEST_F(ReportsHelperFunctions, saveNewReportToCollection_reportingUserAlreadyReported) {
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::oid reported_user_account_oid;
    bsoncxx::oid reporting_user_account_oid;

    OutstandingReports generated_outstanding_reports;

    generated_outstanding_reports.current_object_oid = reported_user_account_oid;
    generated_outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_outstanding_reports.reports_log.emplace_back(
            reporting_user_account_oid,
            ReportReason::REPORT_REASON_ADVERTISING,
            gen_random_alpha_numeric_string(rand() % 100 + 10),
            ReportOriginType::REPORT_ORIGIN_SWIPING,
            generateRandomChatRoomId(),
            generateUUID(),
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{10L * 1000L}}
    );
    generated_outstanding_reports.setIntoCollection();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_INFO;
    std::string report_message = "generic report message I guess";
    std::string chat_room_id = "12345678";
    std::string message_uuid = generateUUID();

    bool return_val = saveNewReportToCollection(
            mongo_cpp_client,
            current_timestamp,
            reporting_user_account_oid,
            reported_user_account_oid,
            report_reason,
            report_origin,
            report_message,
            chat_room_id,
            message_uuid
    );

    EXPECT_TRUE(return_val);

    //New element should NOT be added b/c it is the same user doing the reporting.

    OutstandingReports extracted_outstanding_reports(reported_user_account_oid);

    EXPECT_EQ(generated_outstanding_reports, extracted_outstanding_reports);
}

TEST_F(ReportsHelperFunctions, saveNewReportToCollection_numberReportsReached) {
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::oid reported_user_account_oid;
    bsoncxx::oid reporting_user_account_oid;

    OutstandingReports generated_outstanding_reports;

    generated_outstanding_reports.current_object_oid = reported_user_account_oid;
    generated_outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

    if(report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED > 1) {
        for(int i = 0; i < report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED-1; ++i) {
            generated_outstanding_reports.reports_log.emplace_back(
                    bsoncxx::oid{},
                    ReportReason::REPORT_REASON_ADVERTISING,
                    gen_random_alpha_numeric_string(rand() % 100 + 10),
                    ReportOriginType::REPORT_ORIGIN_SWIPING,
                    generateRandomChatRoomId(),
                    generateUUID(),
                    bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{10L * 1000L}}
            );
        }

        generated_outstanding_reports.setIntoCollection();
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    ReportReason report_reason = ReportReason::REPORT_REASON_LANGUAGE;
    ReportOriginType report_origin = ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_INFO;
    std::string report_message = "generic report message I guess";
    std::string chat_room_id = "12345678";
    std::string message_uuid = generateUUID();

    bool return_val = saveNewReportToCollection(
            mongo_cpp_client,
            current_timestamp,
            reporting_user_account_oid,
            reported_user_account_oid,
            report_reason,
            report_origin,
            report_message,
            chat_room_id,
            message_uuid
    );

    EXPECT_TRUE(return_val);

    generated_outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{current_timestamp};
    generated_outstanding_reports.reports_log.emplace_back(
            reporting_user_account_oid,
            report_reason,
            report_message,
            report_origin,
            chat_room_id,
            message_uuid,
            bsoncxx::types::b_date{current_timestamp}
    );

    OutstandingReports extracted_outstanding_reports(reported_user_account_oid);

    EXPECT_EQ(generated_outstanding_reports, extracted_outstanding_reports);
}

TEST_F(ReportsHelperFunctions, moveOutstandingReportsToHandled) {

    auto generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const std::string admin_name = "Test_name";

    auto handled_move_reason = ReportHandledMoveReason::REPORT_HANDLED_REASON_REPORTS_DISMISSED;
    auto disciplinary_action_taken = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_BANNED;

    {
        HandledReports handled_reports(generated_account_oid);

        bool return_value = moveOutstandingReportsToHandled(
                mongo_cpp_client,
                current_timestamp,
                generated_account_oid,
                admin_name,
                handled_move_reason,
                nullptr,
                disciplinary_action_taken
                );

        //TEST: No existing reports to move from outstanding reports collection.
        checkMoveOutstandingReportsToHandledResult(
                current_timestamp,
                generated_account_oid,
                admin_name,
                handled_move_reason,
                disciplinary_action_taken,
                OutstandingReports(),
                return_value,
                handled_reports
                );
    }

    {
        OutstandingReports outstanding_reports = generateRandomOutstandingReports(
                generated_account_oid,
                current_timestamp
                );

        outstanding_reports.setIntoCollection();

        HandledReports handled_reports(generated_account_oid);

        bool return_value = moveOutstandingReportsToHandled(
                mongo_cpp_client,
                current_timestamp,
                generated_account_oid,
                admin_name,
                handled_move_reason,
                nullptr,
                disciplinary_action_taken
                );

        //TEST: Outstanding reports exist to be moved. Handled reports do not exist.
        checkMoveOutstandingReportsToHandledResult(
                current_timestamp,
                generated_account_oid,
                admin_name,
                handled_move_reason,
                disciplinary_action_taken,
                outstanding_reports,
                return_value,
                handled_reports
                );
    }

    {
        OutstandingReports outstanding_reports = generateRandomOutstandingReports(
                generated_account_oid,
                current_timestamp
                );

        outstanding_reports.setIntoCollection();

        handled_move_reason = ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN;
        disciplinary_action_taken = DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_BANNED;

        HandledReports handled_reports(generated_account_oid);

        bool return_value = moveOutstandingReportsToHandled(
                mongo_cpp_client,
                current_timestamp,
                generated_account_oid,
                admin_name,
                handled_move_reason,
                nullptr,
                disciplinary_action_taken
                );

        //TEST: Outstanding reports exist to be moved. Handled reports already exists.
        //TEST: DisciplinaryActionTypeEnum stored when REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN is set.
        checkMoveOutstandingReportsToHandledResult(
                current_timestamp,
                generated_account_oid,
                admin_name,
                handled_move_reason,
                disciplinary_action_taken,
                outstanding_reports,
                return_value,
                handled_reports
                );
    }
}