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
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetReportsToSpamTesting : public ::testing::Test {
protected:

    handle_reports::ReportUnaryCallRequest request;
    handle_reports::ReportResponseUnaryCallResponse response;

    bsoncxx::oid user_account_oid;

    const std::chrono::milliseconds current_timestamp{100};

    AdminAccountDoc admin_account_doc;

    UserAccountDoc user_account_doc;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_user_oid(user_account_oid.to_string());
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if(admin_account_doc.current_object_oid.to_string() == "000000000000000000000000") {
            bsoncxx::oid admin_account_oid{createTempAdminAccount(admin_level)};
            admin_account_doc.getFromCollection(admin_account_oid);
        }

        setReportToSpam(
                &request, &response
        );
    }

    void compareDocuments() {
        AdminAccountDoc extracted_admin_account_doc(admin_account_doc.current_object_oid);
        EXPECT_EQ(admin_account_doc, extracted_admin_account_doc);

        UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    void expectFailedReturn() {
        EXPECT_FALSE(response.error_message().empty());
        EXPECT_FALSE(response.successful());

        //Expect no changes
        compareDocuments();
    }

    void expectSuccessfulReturn() {
        EXPECT_TRUE(response.error_message().empty());
        EXPECT_TRUE(response.successful());

        admin_account_doc.number_reports_marked_as_spam++;
        user_account_doc.number_times_spam_reports_sent++;

        compareDocuments();
    }

};

TEST_F(SetReportsToSpamTesting, invalidLoginInfo) {
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

    //make sure nothing changed
    compareDocuments();
}

TEST_F(SetReportsToSpamTesting, noAdminAccess) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    expectFailedReturn();
}

TEST_F(SetReportsToSpamTesting, invalidUserOid) {
    request.set_user_oid(gen_random_alpha_numeric_string(rand() % 50 + 36));

    runFunction();

    expectFailedReturn();
}

TEST_F(SetReportsToSpamTesting, successful) {
    runFunction();

    expectSuccessfulReturn();
}

