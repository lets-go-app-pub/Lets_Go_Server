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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class DismissReportTesting : public ::testing::Test {
protected:

    handle_reports::ReportUnaryCallRequest request;
    handle_reports::ReportResponseUnaryCallResponse response;

    bsoncxx::oid user_account_oid;

    const std::chrono::milliseconds current_timestamp{100};

    OutstandingReports outstanding_reports = generateRandomOutstandingReports(
            user_account_oid,
            current_timestamp
    );

    HandledReports handled_reports;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        outstanding_reports.setIntoCollection();
        request.set_user_oid(user_account_oid.to_string());
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

        dismissReport(
                &request, &response
        );
    }

    void compareReports(
            bool outstanding_report_exists,
            bool handled_report_exists
            ) {

        OutstandingReports extracted_outstanding_reports(outstanding_reports.current_object_oid);

        if(outstanding_report_exists) {
            EXPECT_EQ(outstanding_reports, extracted_outstanding_reports);
        } else {
            EXPECT_EQ(extracted_outstanding_reports.current_object_oid.to_string(), "000000000000000000000000");
        }

        HandledReports extracted_handled_reports(outstanding_reports.current_object_oid);

        if(handled_report_exists) {

            //No way to get the timestamp from inside dismissReport().
            if(extracted_handled_reports.handled_reports_info.size() == handled_reports.handled_reports_info.size()) {
                for(size_t i = 0; i < handled_reports.handled_reports_info.size(); ++i) {
                    handled_reports.handled_reports_info[i] = extracted_handled_reports.handled_reports_info[i];
                }
            }

            EXPECT_EQ(handled_reports, extracted_handled_reports);
        } else {
            EXPECT_EQ(extracted_handled_reports.current_object_oid.to_string(), "000000000000000000000000");
        }
    }

    void expectFailedReturn() {
        EXPECT_FALSE(response.error_message().empty());
        EXPECT_FALSE(response.successful());

        //Expect no changes
        compareReports(true, false);
    }

    void expectSuccessfulReturn(
            bool outstanding_report_exists,
            bool handled_report_exists
            ) {
        EXPECT_TRUE(response.error_message().empty());
        EXPECT_TRUE(response.successful());

        compareReports(outstanding_report_exists, handled_report_exists);
    }

    void addOutstandingReportToHandledReport() {
        handled_reports.handled_reports_info.emplace_back(
                outstanding_reports,
                TEMP_ADMIN_ACCOUNT_NAME,
                bsoncxx::types::b_date{std::chrono::milliseconds{-1}}, //will be set inside compareReports()
                ReportHandledMoveReason::REPORT_HANDLED_REASON_REPORTS_DISMISSED,
                DisciplinaryActionTypeEnum(-1)
        );
    }

};

TEST_F(DismissReportTesting, invalidLoginInfo) {
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

    compareReports(true, false);
}

TEST_F(DismissReportTesting, invalid_userOid) {
    request.set_user_oid(gen_random_alpha_numeric_string(rand() % 10 + 5));

    runFunction();

    expectFailedReturn();
}

TEST_F(DismissReportTesting, noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    expectFailedReturn();
}

TEST_F(DismissReportTesting, outstandingReportsDoNotExist) {
    request.set_user_oid(bsoncxx::oid{}.to_string());

    runFunction();

    expectSuccessfulReturn(true, false);
}

TEST_F(DismissReportTesting, outstandingReportsExist_handledReportsDoNotExist) {
    runFunction();

    handled_reports.current_object_oid = user_account_oid;
    addOutstandingReportToHandledReport();

    expectSuccessfulReturn(false, true);
}

TEST_F(DismissReportTesting, outstandingReportsExist_handledReportsExist) {

    handled_reports.current_object_oid = user_account_oid;
    handled_reports.handled_reports_info.emplace_back(
            gen_random_alpha_numeric_string(rand() % 20 + 10),
            bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 1000 + 500}},
            bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 1000 + 500}},
            ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN,
            DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_MANUALLY_SET_TO_SUSPENDED,
            std::vector<ReportsLog>{}
    );
    handled_reports.setIntoCollection();

    runFunction();

    addOutstandingReportToHandledReport();

    expectSuccessfulReturn(false, true);
}
