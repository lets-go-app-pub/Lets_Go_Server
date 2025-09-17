//
// Created by jeremiah on 8/25/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "build_match_made_chat_room.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GrpcUpdateChatRoomInfoTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    UserAccountDoc generated_account_doc;

    std::string chat_room_id;
    std::string chat_room_password;
    const std::string message_uuid = generateUUID();

    ChatRoomHeaderDoc original_chat_room_header;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        original_chat_room_header.getFromCollection(chat_room_id);
        generated_account_doc.getFromCollection(generated_account_oid);

        //sleep to guarantee any new timestamps generated are unique
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(GrpcUpdateChatRoomInfoTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {
                grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
                grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

                update_chat_room_info_request.mutable_login_info()->CopyFrom(login_info);
                update_chat_room_info_request.set_chat_room_id(chat_room_id);
                update_chat_room_info_request.set_type_of_info_to_update(
                        grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME
                );
                update_chat_room_info_request.set_new_chat_room_info(gen_random_alpha_numeric_string(rand() % 10 + 10));
                update_chat_room_info_request.set_message_uuid(message_uuid);

                updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

                return update_chat_room_info_response.return_status();
            }
    );

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);

}

TEST_F(GrpcUpdateChatRoomInfoTesting, invalidChatRoomId) {
    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id("chat_room_id");
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME
    );
    update_chat_room_info_request.set_new_chat_room_info(gen_random_alpha_numeric_string(rand() % 10 + 10));
    update_chat_room_info_request.set_message_uuid(message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), false);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomInfoTesting, invalidTypeOfInfoToUpdate) {

    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_INT_MAX_SENTINEL_DO_NOT_USE_
    );
    update_chat_room_info_request.set_new_chat_room_info(gen_random_alpha_numeric_string(rand() % 10 + 10));
    update_chat_room_info_request.set_message_uuid(message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), false);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomInfoTesting, invalidNewChatRoomInfo) {
    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME
    );
    update_chat_room_info_request.set_new_chat_room_info(gen_random_alpha_numeric_string(
            rand() % 100 + server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES));
    update_chat_room_info_request.set_message_uuid(message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), false);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomInfoTesting, invalidMessageUUID) {
    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME
    );
    update_chat_room_info_request.set_new_chat_room_info(gen_random_alpha_numeric_string(rand() % 10 + 10));
    update_chat_room_info_request.set_message_uuid("message_uuid");

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), false);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomInfoTesting, accountStateNotAdmin) {

    //set user to not be admin
    for(auto& account : original_chat_room_header.accounts_in_chat_room) {
        if(account.account_oid == generated_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
            break;
        }
    }
    original_chat_room_header.setIntoCollection();

    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME
    );
    update_chat_room_info_request.set_new_chat_room_info(gen_random_alpha_numeric_string(rand() % 10 + 10));
    update_chat_room_info_request.set_message_uuid(message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), true);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomInfoTesting, successfullyUpdatedName) {

    const std::string new_chat_room_info = gen_random_alpha_numeric_string(rand() % 10 + 10);

    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME
    );
    update_chat_room_info_request.set_new_chat_room_info(new_chat_room_info);
    update_chat_room_info_request.set_message_uuid(message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), false);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ASSERT_EQ(generated_account_doc_after.chat_rooms.size(), 1);
    ASSERT_EQ(generated_account_doc.chat_rooms.size(), 1);

    EXPECT_GT(generated_account_doc_after.chat_rooms[0].last_time_viewed, generated_account_doc.chat_rooms[0].last_time_viewed);
    generated_account_doc.chat_rooms[0].last_time_viewed = generated_account_doc_after.chat_rooms[0].last_time_viewed;

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_GT(original_chat_room_header_after.chat_room_last_active_time, original_chat_room_header.chat_room_last_active_time);
    original_chat_room_header.chat_room_last_active_time = original_chat_room_header_after.chat_room_last_active_time;

    ASSERT_EQ(original_chat_room_header_after.accounts_in_chat_room.size(), 1);
    ASSERT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 1);

    EXPECT_GT(original_chat_room_header_after.accounts_in_chat_room[0].last_activity_time, original_chat_room_header.accounts_in_chat_room[0].last_activity_time);
    original_chat_room_header.accounts_in_chat_room[0].last_activity_time = original_chat_room_header_after.accounts_in_chat_room[0].last_activity_time;

    original_chat_room_header.chat_room_name = new_chat_room_info;

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}

TEST_F(GrpcUpdateChatRoomInfoTesting, successfullyUpdatedPassword) {

    const std::string new_chat_room_info = gen_random_alpha_numeric_string(rand() % 10 + 10);

    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(
            grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_PASSWORD
    );
    update_chat_room_info_request.set_new_chat_room_info(new_chat_room_info);
    update_chat_room_info_request.set_message_uuid(message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    EXPECT_EQ(update_chat_room_info_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(update_chat_room_info_response.operation_failed(), false);

    UserAccountDoc generated_account_doc_after(generated_account_oid);

    ASSERT_EQ(generated_account_doc_after.chat_rooms.size(), 1);
    ASSERT_EQ(generated_account_doc.chat_rooms.size(), 1);

    EXPECT_GT(generated_account_doc_after.chat_rooms[0].last_time_viewed, generated_account_doc.chat_rooms[0].last_time_viewed);
    generated_account_doc.chat_rooms[0].last_time_viewed = generated_account_doc_after.chat_rooms[0].last_time_viewed;

    ChatRoomHeaderDoc original_chat_room_header_after(chat_room_id);

    EXPECT_GT(original_chat_room_header_after.chat_room_last_active_time, original_chat_room_header.chat_room_last_active_time);
    original_chat_room_header.chat_room_last_active_time = original_chat_room_header_after.chat_room_last_active_time;

    ASSERT_EQ(original_chat_room_header_after.accounts_in_chat_room.size(), 1);
    ASSERT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 1);

    EXPECT_GT(original_chat_room_header_after.accounts_in_chat_room[0].last_activity_time, original_chat_room_header.accounts_in_chat_room[0].last_activity_time);
    original_chat_room_header.accounts_in_chat_room[0].last_activity_time = original_chat_room_header_after.accounts_in_chat_room[0].last_activity_time;

    original_chat_room_header.chat_room_password = new_chat_room_info;

    EXPECT_EQ(generated_account_doc_after, generated_account_doc);
    EXPECT_EQ(original_chat_room_header_after, original_chat_room_header);
}
