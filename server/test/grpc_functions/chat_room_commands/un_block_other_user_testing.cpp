//
// Created by jeremiah on 8/24/22.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/collection.hpp>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UnBlockOtherUserTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;
    bsoncxx::oid first_blocked_user_oid{};
    bsoncxx::oid second_blocked_user_oid{};

    UserAccountDoc generated_account_doc;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);

        generated_account_doc.other_users_blocked.emplace_back(
                first_blocked_user_oid.to_string(),
                bsoncxx::types::b_date{std::chrono::milliseconds{10}}
        );

        generated_account_doc.other_users_blocked.emplace_back(
                second_blocked_user_oid.to_string(),
                bsoncxx::types::b_date{std::chrono::milliseconds{15}}
        );

        generated_account_doc.setIntoCollection();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(UnBlockOtherUserTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
        [&](const LoginToServerBasicInfo& login_info)-> ReturnStatus {
            grpc_chat_commands::UnblockOtherUserRequest un_block_other_user_request;
            grpc_chat_commands::UnblockOtherUserResponse un_block_other_user_response;

            un_block_other_user_request.mutable_login_info()->CopyFrom(login_info);
            un_block_other_user_request.set_user_to_unblock_account_id(bsoncxx::oid{}.to_string());

            unblockOtherUser(&un_block_other_user_request, &un_block_other_user_response);

            return un_block_other_user_response.return_status();
        }
    );

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
}

TEST_F(UnBlockOtherUserTesting, invalidTargetOid) {
    grpc_chat_commands::UnblockOtherUserRequest un_block_other_user_request;
    grpc_chat_commands::UnblockOtherUserResponse un_block_other_user_response;

    setupUserLoginInfo(
            un_block_other_user_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    un_block_other_user_request.set_user_to_unblock_account_id("bsoncxx::oid{}.to_string()");

    unblockOtherUser(&un_block_other_user_request, &un_block_other_user_response);

    EXPECT_EQ(un_block_other_user_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
}

TEST_F(UnBlockOtherUserTesting, targetUserIsSendingUser) {
    grpc_chat_commands::UnblockOtherUserRequest un_block_other_user_request;
    grpc_chat_commands::UnblockOtherUserResponse un_block_other_user_response;

    setupUserLoginInfo(
            un_block_other_user_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    un_block_other_user_request.set_user_to_unblock_account_id(generated_account_oid.to_string());

    unblockOtherUser(&un_block_other_user_request, &un_block_other_user_response);

    EXPECT_EQ(un_block_other_user_response.return_status(), ReturnStatus::SUCCESS);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
}

TEST_F(UnBlockOtherUserTesting, successful) {
    grpc_chat_commands::UnblockOtherUserRequest un_block_other_user_request;
    grpc_chat_commands::UnblockOtherUserResponse un_block_other_user_response;

    setupUserLoginInfo(
            un_block_other_user_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    un_block_other_user_request.set_user_to_unblock_account_id(first_blocked_user_oid.to_string());

    unblockOtherUser(&un_block_other_user_request, &un_block_other_user_response);

    EXPECT_EQ(un_block_other_user_response.return_status(), ReturnStatus::SUCCESS);

    for(auto it = generated_account_doc.other_users_blocked.begin(); it != generated_account_doc.other_users_blocked.end(); ++it) {
        if(it->oid_string == first_blocked_user_oid.to_string()) {
            generated_account_doc.other_users_blocked.erase(it);
            break;
        }
    }

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
}

TEST_F(UnBlockOtherUserTesting, targetUserDoesNotExist) {
    grpc_chat_commands::UnblockOtherUserRequest un_block_other_user_request;
    grpc_chat_commands::UnblockOtherUserResponse un_block_other_user_response;

    setupUserLoginInfo(
            un_block_other_user_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    un_block_other_user_request.set_user_to_unblock_account_id(bsoncxx::oid{}.to_string());

    unblockOtherUser(&un_block_other_user_request, &un_block_other_user_response);

    EXPECT_EQ(un_block_other_user_response.return_status(), ReturnStatus::SUCCESS);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
}
