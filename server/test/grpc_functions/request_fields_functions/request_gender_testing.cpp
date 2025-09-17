//
// Created by jeremiah on 10/4/22.
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
#include "request_fields_functions.h"
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestGenderTesting : public ::testing::Test {
protected:

    request_fields::InfoFieldRequest request;
    request_fields::InfoFieldResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );
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

    void runFunction() {
        requestGender(&request, &response);
    }

};

TEST_F(RequestGenderTesting, invalidLoginInfo) {
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
}

TEST_F(RequestGenderTesting, successful) {
    runFunction();

    request_fields::InfoFieldResponse generated_response;

    generated_response.set_return_status(ReturnStatus::SUCCESS);

    EXPECT_GT(response.timestamp(), 0);
    generated_response.set_timestamp(response.timestamp());

    generated_response.set_return_string(user_account.gender);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            generated_response,
            response
    );

    if(!equivalent) {
        std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
        std::cout << "response\n" << response.DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}