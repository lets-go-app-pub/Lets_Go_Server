//
// Created by jeremiah on 3/28/23.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "generate_multiple_random_accounts.h"
#include "set_fields_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetOptedInToPromotionalEmailTesting : public testing::Test {
protected:

    setfields::SetOptedInToPromotionalEmailRequest request;
    setfields::SetFieldResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    UserAccountStatisticsDoc user_account_statistics;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        request.set_opted_in_to_promotional_email(!user_account.opted_in_to_promotional_email);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);
        user_account_statistics.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        setOptedInToPromotionalEmail(&request, &response);
    }

    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);
        EXPECT_EQ(user_account, extracted_user_account);
    }

    void compareUserStatisticsAccounts() {
        UserAccountStatisticsDoc extracted_user_account_statistics(user_account_oid);
        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }
};

TEST_F(SetOptedInToPromotionalEmailTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();

                runFunction();

                return response.return_status();
            }
    );

    //should be no changes
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetOptedInToPromotionalEmailTesting, successful) {
    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    user_account.opted_in_to_promotional_email = request.opted_in_to_promotional_email();

    user_account_statistics.opted_in_to_promotional_email.emplace_back(
            request.opted_in_to_promotional_email(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    compareUserAccounts();
    compareUserStatisticsAccounts();
}