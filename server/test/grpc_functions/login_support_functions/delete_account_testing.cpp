//
// Created by jeremiah on 9/22/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "generate_multiple_random_accounts.h"
#include "LoginSupportFunctions.pb.h"
#include "setup_login_info.h"
#include "login_support_functions.h"
#include "connection_pool_global_variable.h"
#include "utility_general_functions_test.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class DeleteAccountTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    UserAccountDoc generated_account_doc;

    loginsupport::LoginSupportRequest request;
    loginsupport::LoginSupportResponse response;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
        );
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        deleteAccount(&request, &response);
    }
};

TEST_F(DeleteAccountTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                request.Clear();
                response.Clear();

                request.mutable_login_info()->CopyFrom(login_info);

                runFunction();

                return response.return_status();
            }
    );

    UserAccountDoc extracted_generated_account_doc(generated_account_oid);
    EXPECT_EQ(generated_account_doc, extracted_generated_account_doc);
}

TEST_F(DeleteAccountTesting, properlyDeletesAccount) {
    buildAndCheckDeleteAccount(
            generated_account_oid,
            [&](std::chrono::milliseconds& current_timestamp){
                runFunction();

                current_timestamp = std::chrono::milliseconds{response.timestamp()};
                return response.return_status() == ReturnStatus::SUCCESS;
            }
    );
}
