//
// Created by jeremiah on 8/23/22.
//

#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
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
#include "chat_room_commands_helper_functions.h"
#include "grpc_mock_stream/mock_stream.h"
#include "generate_randoms.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RemoveFromChatRoomTesting : public ::testing::Test {
protected:

    bsoncxx::oid admin_account_oid;
    bsoncxx::oid target_account_oid;

    UserAccountDoc admin_account_doc;
    UserAccountDoc target_account_doc;

    std::string chat_room_id;
    std::string chat_room_password;
    const std::string message_uuid = generateUUID();

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        admin_account_oid = insertRandomAccounts(1, 0);
        target_account_oid = insertRandomAccounts(1, 0);

        admin_account_doc.getFromCollection(admin_account_oid);
        target_account_doc.getFromCollection(target_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                admin_account_oid,
                admin_account_doc.logged_in_token,
                admin_account_doc.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

        setupUserLoginInfo(
                join_chat_room_request.mutable_login_info(),
                target_account_oid,
                target_account_doc.logged_in_token,
                target_account_doc.installation_ids.front()
        );

        join_chat_room_request.set_chat_room_id(chat_room_id);
        join_chat_room_request.set_chat_room_password(chat_room_password);

        grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

        joinChatRoom(&join_chat_room_request, &mock_server_writer);

        EXPECT_EQ(mock_server_writer.write_params.size(), 2);
        if (mock_server_writer.write_params.size() >= 2) {
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
            if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
                EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
                EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            }
        }

        admin_account_doc.getFromCollection(admin_account_oid);
        target_account_doc.getFromCollection(target_account_oid);

        //Sleep to guarantee last active times will be different when running promoteNewAdmin().
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(RemoveFromChatRoomTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
        admin_account_oid,
        admin_account_doc.logged_in_token,
        admin_account_doc.installation_ids.front(),
        [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
            grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
            grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

            remove_from_chat_room_request.mutable_login_info()->CopyFrom(login_info);
            remove_from_chat_room_request.set_chat_room_id(chat_room_id);
            remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
            remove_from_chat_room_request.set_message_uuid(message_uuid);
            remove_from_chat_room_request.set_kick_or_ban(
                    grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
                    );

            removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

            return remove_from_chat_room_response.return_status();
        }
    );

    UserAccountDoc admin_account_doc_after(admin_account_oid);
    UserAccountDoc target_account_doc_after(target_account_oid);

    EXPECT_EQ(admin_account_doc, admin_account_doc_after);
    EXPECT_EQ(target_account_doc, target_account_doc_after);
}

TEST_F(RemoveFromChatRoomTesting, successfullKick) {

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(remove_from_chat_room_response.operation_failed(), false);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_GT(extracted_header_doc.chat_room_last_active_time, generated_header_doc.chat_room_last_active_time);
    generated_header_doc.chat_room_last_active_time = extracted_header_doc.chat_room_last_active_time;

    ASSERT_EQ(extracted_header_doc.accounts_in_chat_room.size(), generated_header_doc.accounts_in_chat_room.size());

    int admin_index = 0;
    int target_index = 0;

    for(int i = 0; i < (int)extracted_header_doc.accounts_in_chat_room.size(); ++i) {
        if(extracted_header_doc.accounts_in_chat_room[i].account_oid == admin_account_oid) {
            admin_index = i;
        } else if(extracted_header_doc.accounts_in_chat_room[i].account_oid == target_account_oid) {
            target_index = i;
        }
    }

    ASSERT_LT(admin_index, generated_header_doc.accounts_in_chat_room.size());
    ASSERT_LT(target_index, generated_header_doc.accounts_in_chat_room.size());

    ASSERT_EQ(generated_header_doc.accounts_in_chat_room[admin_index].account_oid, admin_account_oid);
    ASSERT_EQ(generated_header_doc.accounts_in_chat_room[target_index].account_oid, target_account_oid);

    ASSERT_FALSE(extracted_header_doc.accounts_in_chat_room[target_index].times_joined_left.empty());

    EXPECT_GT(extracted_header_doc.accounts_in_chat_room[admin_index].last_activity_time, generated_header_doc.accounts_in_chat_room[admin_index].last_activity_time);
    generated_header_doc.accounts_in_chat_room[admin_index].last_activity_time = extracted_header_doc.accounts_in_chat_room[admin_index].last_activity_time;

    EXPECT_GT(extracted_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp, generated_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp);
    generated_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp = extracted_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp;

    generated_header_doc.accounts_in_chat_room[target_index].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;

    generated_header_doc.accounts_in_chat_room[target_index].times_joined_left.emplace_back(
            extracted_header_doc.accounts_in_chat_room[target_index].times_joined_left.back()
            );

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    ASSERT_EQ(admin_account_doc.chat_rooms.size(), 1);
    ASSERT_EQ(extracted_admin_account_doc.chat_rooms.size(), 1);

    EXPECT_GT(extracted_admin_account_doc.chat_rooms.front().last_time_viewed, admin_account_doc.chat_rooms.front().last_time_viewed);
    admin_account_doc.chat_rooms.front().last_time_viewed = extracted_admin_account_doc.chat_rooms.front().last_time_viewed;

    ASSERT_EQ(target_account_doc.chat_rooms.size(), 1);
    target_account_doc.chat_rooms.pop_back();

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_FALSE(message_doc.id.empty());
    EXPECT_EQ(message_doc.message_type, MessageSpecifics::MessageBodyCase::kUserKickedMessage);
}

TEST_F(RemoveFromChatRoomTesting, successfullBan) {

    UserPictureDoc updated_user_thumbnail = generateRandomUserPicture();
    updated_user_thumbnail.setIntoCollection();

    //update thumbnail
    target_account_doc.pictures[0].setPictureReference(
            updated_user_thumbnail.current_object_oid,
            updated_user_thumbnail.timestamp_stored
            );
    target_account_doc.setIntoCollection();

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_BAN
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(remove_from_chat_room_response.operation_failed(), false);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_GT(extracted_header_doc.chat_room_last_active_time, generated_header_doc.chat_room_last_active_time);
    generated_header_doc.chat_room_last_active_time = extracted_header_doc.chat_room_last_active_time;

    ASSERT_EQ(extracted_header_doc.accounts_in_chat_room.size(), generated_header_doc.accounts_in_chat_room.size());

    int admin_index = 0;
    int target_index = 0;

    for(int i = 0; i < (int)extracted_header_doc.accounts_in_chat_room.size(); ++i) {
        if(extracted_header_doc.accounts_in_chat_room[i].account_oid == admin_account_oid) {
            admin_index = i;
        } else if(extracted_header_doc.accounts_in_chat_room[i].account_oid == target_account_oid) {
            target_index = i;
        }
    }

    ASSERT_LT(admin_index, generated_header_doc.accounts_in_chat_room.size());
    ASSERT_LT(target_index, generated_header_doc.accounts_in_chat_room.size());

    ASSERT_EQ(generated_header_doc.accounts_in_chat_room[admin_index].account_oid, admin_account_oid);
    ASSERT_EQ(generated_header_doc.accounts_in_chat_room[target_index].account_oid, target_account_oid);

    ASSERT_FALSE(extracted_header_doc.accounts_in_chat_room[target_index].times_joined_left.empty());

    EXPECT_GT(extracted_header_doc.accounts_in_chat_room[admin_index].last_activity_time, generated_header_doc.accounts_in_chat_room[admin_index].last_activity_time);
    generated_header_doc.accounts_in_chat_room[admin_index].last_activity_time = extracted_header_doc.accounts_in_chat_room[admin_index].last_activity_time;

    EXPECT_GT(extracted_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp, generated_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp);
    generated_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp = extracted_header_doc.accounts_in_chat_room[target_index].thumbnail_timestamp;
    generated_header_doc.accounts_in_chat_room[target_index].thumbnail_size = updated_user_thumbnail.thumbnail_size_in_bytes;
    generated_header_doc.accounts_in_chat_room[target_index].thumbnail_reference = updated_user_thumbnail.current_object_oid.to_string();

    generated_header_doc.accounts_in_chat_room[target_index].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_BANNED;

    generated_header_doc.accounts_in_chat_room[target_index].times_joined_left.emplace_back(
            extracted_header_doc.accounts_in_chat_room[target_index].times_joined_left.back()
    );

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    ASSERT_EQ(admin_account_doc.chat_rooms.size(), 1);
    ASSERT_EQ(extracted_admin_account_doc.chat_rooms.size(), 1);

    EXPECT_GT(extracted_admin_account_doc.chat_rooms.front().last_time_viewed, admin_account_doc.chat_rooms.front().last_time_viewed);
    admin_account_doc.chat_rooms.front().last_time_viewed = extracted_admin_account_doc.chat_rooms.front().last_time_viewed;

    ASSERT_EQ(target_account_doc.chat_rooms.size(), 1);
    target_account_doc.chat_rooms.pop_back();

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_FALSE(message_doc.id.empty());
    EXPECT_EQ(message_doc.message_type, MessageSpecifics::MessageBodyCase::kUserBannedMessage);
}

TEST_F(RemoveFromChatRoomTesting, invalidChatRoomId) {

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id("chat_room_id");
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, invalidTargetOid) {

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove("target_account_oid.to_string()");
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, invalidMessageUuid) {

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid("message_uuid");
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, invalidKickOrBanParameter) {

    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest::KickOrBan(-1)
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, targetOidIsSendingOid) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(admin_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(remove_from_chat_room_response.operation_failed(), false);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, targetOidIsEventAdminOid) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    createAndStoreEventAdminAccount();

    UserAccountDoc event_admin_doc(event_admin_values::OID);
    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
    for (const auto& pic: event_admin_doc.pictures) {
        if (pic.pictureStored()) {
            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );
            break;
        }
    }

    ASSERT_NE(timestamp_stored.value.count(), -1L);

    const::std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    //This situation can never happen, that is because the event admin is always admin. However, being admin also
    // means that they can not be kicked/banned to begin with. So this will properly test the functions restriction.
    UserPictureDoc event_admin_picture(picture_reference);
    generated_header_doc.accounts_in_chat_room.emplace_back(
            event_admin_values::OID,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            event_admin_values::FIRST_NAME,
            picture_reference.to_string(),
            timestamp_stored,
            event_admin_picture.thumbnail_size_in_bytes,
            bsoncxx::types::b_date{std::chrono::milliseconds{current_timestamp.count()}},
            std::vector<bsoncxx::types::b_date> {bsoncxx::types::b_date{std::chrono::milliseconds{current_timestamp.count() - 2}}}
    );

    generated_header_doc.setIntoCollection();
    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(event_admin_values::OID.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(remove_from_chat_room_response.operation_failed(), true);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);
    UserAccountDoc extracted_event_admin_account_doc(event_admin_values::OID);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);
    EXPECT_EQ(extracted_event_admin_account_doc, event_admin_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, sendingAccountIsNotAdminState) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == admin_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
            break;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(remove_from_chat_room_response.operation_failed(), true);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}

TEST_F(RemoveFromChatRoomTesting, targetAccountIsNotInChatRoomState) {
    ChatRoomHeaderDoc generated_header_doc(chat_room_id);

    for(auto& account : generated_header_doc.accounts_in_chat_room) {
        if(account.account_oid == target_account_oid) {
            account.state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
            break;
        }
    }

    generated_header_doc.setIntoCollection();

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            admin_account_oid,
            admin_account_doc.logged_in_token,
            admin_account_doc.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_account_id_to_remove(target_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(message_uuid);
    remove_from_chat_room_request.set_kick_or_ban(
            grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK
    );

    //Sleep to guarantee removeFromChatRoom() uses a unique timestamp for last_activity time variables.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    EXPECT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(remove_from_chat_room_response.operation_failed(), true);

    ChatRoomHeaderDoc extracted_header_doc(chat_room_id);

    EXPECT_EQ(extracted_header_doc, generated_header_doc);

    UserAccountDoc extracted_admin_account_doc(admin_account_oid);
    UserAccountDoc extracted_target_account_doc(target_account_oid);

    EXPECT_EQ(extracted_admin_account_doc, admin_account_doc);
    EXPECT_EQ(extracted_target_account_doc, target_account_doc);

    ChatRoomMessageDoc message_doc(message_uuid, chat_room_id);

    EXPECT_TRUE(message_doc.id.empty());
}
