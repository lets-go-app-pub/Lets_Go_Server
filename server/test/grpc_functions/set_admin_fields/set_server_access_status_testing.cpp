//
// Created by jeremiah on 10/6/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "RequestStatistics.pb.h"
#include "compare_equivalent_messages.h"
#include "SetAdminFields.pb.h"
#include "set_admin_fields.h"
#include "report_values.h"
#include "generate_multiple_random_accounts.h"
#include "reports_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetServerAccessStatusTesting : public ::testing::Test {
protected:

    set_admin_fields::SetAccessStatusRequest request;
    set_admin_fields::SetAccessStatusResponse response;

    bsoncxx::oid user_account_oid;
    UserAccountDoc user_account;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );
        request.mutable_login_info()->set_current_account_id(user_account_oid.to_string());

        request.set_new_account_status(UserAccountStatus::STATUS_SUSPENDED);
        request.set_inactive_message(gen_random_alpha_numeric_string(server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE + 5));
        request.set_duration_in_millis(std::chrono::duration_cast<std::chrono::milliseconds>(report_values::MINIMUM_TIME_FOR_SUSPENSION).count() + rand() % (5L * 60L * 1000L));
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        setServerAccessStatus(&request, &response);
    }

    void checkReturnSuccessful() {
        EXPECT_TRUE(response.error_message().empty());
        EXPECT_TRUE(response.successful());
    }

    void checkFunctionFailed() {
        EXPECT_FALSE(response.error_message().empty());
        EXPECT_FALSE(response.successful());

        //no changes should have occurred
        compareUserAccountDoc();
    }

    void compareUserAccountDoc() {
        UserAccountDoc extracted_user_account(user_account_oid);
        EXPECT_EQ(user_account, extracted_user_account);
    }

    OutstandingReports buildOutstandingReports() {
        OutstandingReports outstanding_reports;
        outstanding_reports.reports_log.emplace_back(
                ReportsLog(
                        bsoncxx::oid{},
                        ReportReason::REPORT_REASON_ADVERTISING,
                        gen_random_alpha_numeric_string(rand() % 100 + 100),
                        ReportOriginType::REPORT_ORIGIN_SWIPING,
                        "",
                        "",
                        bsoncxx::types::b_date{std::chrono::milliseconds{getCurrentTimestamp() - std::chrono::milliseconds{rand() % 10000 + 1}}}
                )
        );
        outstanding_reports.current_object_oid = user_account_oid;
        outstanding_reports.setIntoCollection();
        return outstanding_reports;
    }

    void checkHandledReportsMoved(
            const OutstandingReports& outstanding_reports,
            DisciplinaryActionTypeEnum disciplinary_action_taken
            ) {
        HandledReports generated_handled_report;

        generated_handled_report.current_object_oid = user_account_oid;
        generated_handled_report.handled_reports_info.emplace_back(
                outstanding_reports,
                TEMP_ADMIN_ACCOUNT_NAME,
                bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
                ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN,
                disciplinary_action_taken
        );

        HandledReports extracted_handled_report(user_account_oid);

        ASSERT_EQ(extracted_handled_report.handled_reports_info.size(), 1);
        EXPECT_GT(extracted_handled_report.handled_reports_info[0].timestamp_handled, 0);
        generated_handled_report.handled_reports_info[0].timestamp_handled = extracted_handled_report.handled_reports_info[0].timestamp_handled;

        EXPECT_EQ(generated_handled_report, extracted_handled_report);
    }

    void modifyAndCompareUserAccountOnSuccess(
            UserAccountStatus user_account_status,
            DisciplinaryActionTypeEnum disciplinary_action
            ) {
        UserAccountDoc extracted_user_account(user_account_oid);

        user_account.status = user_account_status;
        user_account.inactive_message = request.inactive_message();

        if(user_account_status == UserAccountStatus::STATUS_SUSPENDED) {
            EXPECT_GT(extracted_user_account.inactive_end_time, 0);
            user_account.inactive_end_time = extracted_user_account.inactive_end_time;
        } else {
            user_account.inactive_end_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
        }

        user_account.disciplinary_record.emplace_back(
                UserAccountDoc::DisciplinaryRecord(
                        bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
                        bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
                        disciplinary_action,
                        request.inactive_message(),
                        TEMP_ADMIN_ACCOUNT_NAME
                )
        );

        ASSERT_EQ(extracted_user_account.disciplinary_record.size(), 1);
        EXPECT_GT(extracted_user_account.disciplinary_record[0].submitted_time, 0);

        if(user_account_status == UserAccountStatus::STATUS_SUSPENDED) {
            EXPECT_GT(extracted_user_account.disciplinary_record[0].end_time,
                      extracted_user_account.disciplinary_record[0].submitted_time);
        }

        user_account.disciplinary_record[0].submitted_time = extracted_user_account.disciplinary_record[0].submitted_time;
        user_account.disciplinary_record[0].end_time = extracted_user_account.disciplinary_record[0].end_time;

        compareUserAccountDoc();
    }
};

TEST_F(SetServerAccessStatusTesting, invalidLoginInfo) {
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
}

TEST_F(SetServerAccessStatusTesting, noAdminPriveledge) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, invalidUserAccountOid) {
    request.mutable_login_info()->set_current_account_id("invalid_user_account_oid");

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, invalidNewAccountStatus) {
    request.set_new_account_status(UserAccountStatus(-1));

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, invalidInactiveMessage_tooShort) {
    request.set_inactive_message(gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE));

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, invalidInactiveMessage_tooLong) {
    request.set_inactive_message(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE + 1 + rand() % 100));

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, invalidDurationInMillis_tooShort) {
    const std::chrono::milliseconds min_time_for_suspension = std::chrono::duration_cast<std::chrono::milliseconds>(report_values::MINIMUM_TIME_FOR_SUSPENSION);
    //because std::chrono::hours seems to (?) round, using /2 - 1
    request.set_duration_in_millis(min_time_for_suspension.count()/2 - 1);

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, invalidDurationInMillis_tooLong) {
    const std::chrono::milliseconds max_time_for_suspension = std::chrono::duration_cast<std::chrono::milliseconds>(report_values::MAXIMUM_TIME_FOR_SUSPENSION);
    //because std::chrono::hours seems to (?) round, adding 1 hour
    request.set_duration_in_millis((max_time_for_suspension + std::chrono::hours{1}).count() + 1);

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, attemptUpdateWhenAccountRequiresMoreInfo) {
    user_account.status = UserAccountStatus::STATUS_REQUIRES_MORE_INFO;
    user_account.setIntoCollection();

    runFunction();

    checkFunctionFailed();
}

TEST_F(SetServerAccessStatusTesting, successful_statusSuspended) {

    OutstandingReports outstanding_reports = buildOutstandingReports();

    runFunction();

    checkReturnSuccessful();

    modifyAndCompareUserAccountOnSuccess(
            UserAccountStatus::STATUS_SUSPENDED,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_SUSPENDED
    );

    checkHandledReportsMoved(
            outstanding_reports,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_SUSPENDED
    );
}

TEST_F(SetServerAccessStatusTesting, successful_statusBanned) {
    request.set_new_account_status(UserAccountStatus::STATUS_BANNED);

    OutstandingReports outstanding_reports = buildOutstandingReports();

    runFunction();

    checkReturnSuccessful();

    modifyAndCompareUserAccountOnSuccess(
            UserAccountStatus::STATUS_BANNED,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_BANNED
    );

    checkHandledReportsMoved(
            outstanding_reports,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_BANNED
    );
}

TEST_F(SetServerAccessStatusTesting, successful_statusActive) {
    user_account.status = UserAccountStatus::STATUS_SUSPENDED;
    user_account.setIntoCollection();

    request.set_new_account_status(UserAccountStatus::STATUS_ACTIVE);

    OutstandingReports outstanding_reports = buildOutstandingReports();

    runFunction();

    checkReturnSuccessful();

    modifyAndCompareUserAccountOnSuccess(
            UserAccountStatus::STATUS_ACTIVE,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_ACTIVE
    );

    //report should NOT have been set to handled
    HandledReports extracted_handled_report(user_account_oid);
    EXPECT_EQ(extracted_handled_report.current_object_oid.to_string(), "000000000000000000000000");
}

TEST_F(SetServerAccessStatusTesting, successful_statusNeedsMoreInfo) {
    user_account.status = UserAccountStatus::STATUS_SUSPENDED;
    user_account.setIntoCollection();

    request.set_new_account_status(UserAccountStatus::STATUS_REQUIRES_MORE_INFO);

    OutstandingReports outstanding_reports = buildOutstandingReports();

    runFunction();

    checkReturnSuccessful();

    modifyAndCompareUserAccountOnSuccess(
            UserAccountStatus::STATUS_REQUIRES_MORE_INFO,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_REQUIRES_MORE_INFO
    );

    //report should NOT have been set to handled
    HandledReports extracted_handled_report(user_account_oid);
    EXPECT_EQ(extracted_handled_report.current_object_oid.to_string(), "000000000000000000000000");
}
