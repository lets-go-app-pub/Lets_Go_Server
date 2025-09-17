//
// Created by jeremiah on 8/2/22.
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
#include "chat_rooms_objects.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "chat_room_commands_helper_functions.h"
#include "grpc_mock_stream/mock_stream.h"
#include "generate_randoms.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class JoinChatRoomTesting : public ::testing::Test {
protected:

    bsoncxx::oid chat_room_creator_account_oid;
    bsoncxx::oid user_account_oid;

    UserAccountDoc original_user_account;
    UserAccountDoc chat_room_creator_user_account;

    ChatRoomHeaderDoc original_chat_room_header;

    std::string chat_room_id;
    std::string chat_room_password;

    bsoncxx::oid user_thumbnail_oid;
    UserPictureDoc original_user_thumbnail;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        chat_room_creator_account_oid = insertRandomAccounts(1, 0);

        chat_room_creator_user_account.getFromCollection(chat_room_creator_account_oid);

        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                chat_room_creator_account_oid,
                chat_room_creator_user_account.logged_in_token,
                chat_room_creator_user_account.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        ASSERT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        chat_room_id = create_chat_room_response.chat_room_id();
        chat_room_password = create_chat_room_response.chat_room_password();

        original_chat_room_header.getFromCollection(chat_room_id);

        //The situation where a user joins when only they have been in before should be rare. Not making that the base case.
        user_account_oid = insertRandomAccounts(1, 0);
        original_user_account.getFromCollection(user_account_oid);

        for (const auto& pic: original_user_account.pictures) {
            if (pic.pictureStored()) {

                bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

                pic.getPictureReference(
                        user_thumbnail_oid,
                        timestamp_stored
                );

                original_user_thumbnail.getFromCollection(user_thumbnail_oid);
                break;
            }
        }
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void setUserAge(int new_age) {
        int birth_year;
        int birth_day_year;
        generateBirthYearForPassedAge(
                new_age,
                birth_year,
                original_user_account.birth_month,
                original_user_account.birth_day_of_month,
                birth_day_year
        );

        original_user_account.age = new_age;
        original_user_account.birth_year = birth_year;
        original_user_account.birth_day_of_year = birth_day_year;
        original_user_account.setIntoCollection();
    }
};

TEST_F(JoinChatRoomTesting, successfullyJoined) {

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    bsoncxx::types::b_date timestamp_message_stored{std::chrono::milliseconds{-1}};

    EXPECT_EQ(mock_server_writer.write_params.size(), 2);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }

        //The specifics of sendNewChatRoomAndMessages() are tested elsewhere. Just that it is sending
        // back all the messages is enough.
        EXPECT_EQ(mock_server_writer.write_params[1].msg.messages_list_size(), 6);
        if (mock_server_writer.write_params[1].msg.messages_list_size() >= 2) {
            //This will be the 'current timestamp' value after the message was sent from inside joinChatRoom().
            timestamp_message_stored =
                    bsoncxx::types::b_date{
                            std::chrono::milliseconds{
                                    mock_server_writer.write_params[1].msg.messages_list(
                                            1).message().message_specifics().this_user_joined_chat_room_member_message().member_info().user_info().current_timestamp()
                            }
                    };
        }

    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
    if (extracted_chat_room_header.accounts_in_chat_room.size() >= 2) {

        original_chat_room_header.chat_room_last_active_time = timestamp_message_stored;
        original_chat_room_header.accounts_in_chat_room.emplace_back(
                user_account_oid,
                AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
                original_user_account.first_name,
                user_thumbnail_oid.to_string(),
                extracted_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp,
                original_user_thumbnail.thumbnail_size_in_bytes,
                timestamp_message_stored,
                extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left
        );

        EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);
    }

    original_user_thumbnail.thumbnail_references.emplace_back(chat_room_id);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if (!extracted_user_account.chat_rooms.empty()) {
        original_user_account.chat_rooms.emplace_back(
                chat_room_id,
                extracted_user_account.chat_rooms[0].last_time_viewed
        );
        EXPECT_EQ(extracted_user_account, original_user_account);
    }

}

TEST_F(JoinChatRoomTesting, chatRoomNotFound) {

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    std::string new_chat_room;

    while (new_chat_room.empty() || new_chat_room == chat_room_id) {
        new_chat_room = generateRandomChatRoomId();
    }

    join_chat_room_request.set_chat_room_id(new_chat_room);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (!mock_server_writer.write_params.empty()) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            EXPECT_EQ(mock_server_writer.write_params[0].msg.chat_room_status(),
                      grpc_chat_commands::ChatRoomStatus::INVALID_CHAT_ROOM_ID);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);

}

TEST_F(JoinChatRoomTesting, incorrectPassword) {

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    std::string new_chat_room_password;

    while (new_chat_room_password.empty() || new_chat_room_password == chat_room_password) {
        new_chat_room_password = gen_random_alpha_numeric_string(3);
    }

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(new_chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (!mock_server_writer.write_params.empty()) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            EXPECT_EQ(mock_server_writer.write_params[0].msg.chat_room_status(),
                      grpc_chat_commands::ChatRoomStatus::INVALID_CHAT_ROOM_PASSWORD);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);

}

TEST_F(JoinChatRoomTesting, accountBanned) {

    grpc_chat_commands::JoinChatRoomRequest initial_join_chat_room_request;

    setupUserLoginInfo(
            initial_join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    initial_join_chat_room_request.set_chat_room_id(chat_room_id);
    initial_join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> initial_mock_server_writer;

    joinChatRoom(&initial_join_chat_room_request, &initial_mock_server_writer);

    EXPECT_EQ(initial_mock_server_writer.write_params.size(), 2);
    if (!initial_mock_server_writer.write_params.empty()) {
        EXPECT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!initial_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            ASSERT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            chat_room_creator_account_oid,
            chat_room_creator_user_account.logged_in_token,
            chat_room_creator_user_account.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_kick_or_ban(grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_BAN);
    remove_from_chat_room_request.set_account_id_to_remove(user_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(generateUUID());

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    ASSERT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    original_chat_room_header.getFromCollection(chat_room_id);
    original_user_thumbnail.getFromCollection(user_thumbnail_oid);
    original_user_account.getFromCollection(user_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (!mock_server_writer.write_params.empty()) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            EXPECT_EQ(mock_server_writer.write_params[0].msg.chat_room_status(),
                      grpc_chat_commands::ChatRoomStatus::ACCOUNT_WAS_BANNED);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);
}

TEST_F(JoinChatRoomTesting, accountAlreadyInsideChatRoom) {
    grpc_chat_commands::JoinChatRoomRequest initial_join_chat_room_request;

    setupUserLoginInfo(
            initial_join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    initial_join_chat_room_request.set_chat_room_id(chat_room_id);
    initial_join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> initial_mock_server_writer;

    joinChatRoom(&initial_join_chat_room_request, &initial_mock_server_writer);

    EXPECT_EQ(initial_mock_server_writer.write_params.size(), 2);
    if (!initial_mock_server_writer.write_params.empty()) {
        EXPECT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!initial_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            ASSERT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    original_chat_room_header.getFromCollection(chat_room_id);
    original_user_thumbnail.getFromCollection(user_thumbnail_oid);
    original_user_account.getFromCollection(user_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (!mock_server_writer.write_params.empty()) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            EXPECT_EQ(mock_server_writer.write_params[0].msg.chat_room_status(),
                      grpc_chat_commands::ChatRoomStatus::ALREADY_IN_CHAT_ROOM);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);
}

TEST_F(JoinChatRoomTesting, minAgeFieldExists_userTooYoung) {
    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    setUserAge(server_parameter_restrictions::LOWEST_ALLOWED_AGE);

    original_chat_room_header.min_age = server_parameter_restrictions::LOWEST_ALLOWED_AGE + 1;
    original_chat_room_header.setIntoCollection();

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (!mock_server_writer.write_params.empty()) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
            EXPECT_EQ(mock_server_writer.write_params[0].msg.chat_room_status(),
                      grpc_chat_commands::ChatRoomStatus::USER_TOO_YOUNG_FOR_CHAT_ROOM);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);
}

TEST_F(JoinChatRoomTesting, minAgeFieldExists_userAgeValid) {
    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    setUserAge(server_parameter_restrictions::LOWEST_ALLOWED_AGE);

    original_chat_room_header.min_age = server_parameter_restrictions::LOWEST_ALLOWED_AGE;
    original_chat_room_header.setIntoCollection();

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    bsoncxx::types::b_date timestamp_message_stored{std::chrono::milliseconds{-1}};

    EXPECT_EQ(mock_server_writer.write_params.size(), 2);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }

        //The specifics of sendNewChatRoomAndMessages() are tested elsewhere. Just that it is sending
        // back all the messages is enough.
        EXPECT_EQ(mock_server_writer.write_params[1].msg.messages_list_size(), 6);
        if (mock_server_writer.write_params[1].msg.messages_list_size() >= 2) {
            //This will be the 'current timestamp' value after the message was sent from inside joinChatRoom().
            timestamp_message_stored =
                    bsoncxx::types::b_date{
                            std::chrono::milliseconds{
                                    mock_server_writer.write_params[1].msg.messages_list(
                                            1).message().message_specifics().this_user_joined_chat_room_member_message().member_info().user_info().current_timestamp()
                            }
                    };
        }

    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
    if (extracted_chat_room_header.accounts_in_chat_room.size() >= 2) {

        original_chat_room_header.chat_room_last_active_time = timestamp_message_stored;
        original_chat_room_header.accounts_in_chat_room.emplace_back(
                user_account_oid,
                AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
                original_user_account.first_name,
                user_thumbnail_oid.to_string(),
                extracted_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp,
                original_user_thumbnail.thumbnail_size_in_bytes,
                timestamp_message_stored,
                extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left
        );

        EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);
    }

    original_user_thumbnail.thumbnail_references.emplace_back(chat_room_id);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if (!extracted_user_account.chat_rooms.empty()) {
        original_user_account.chat_rooms.emplace_back(
                chat_room_id,
                extracted_user_account.chat_rooms[0].last_time_viewed
        );
        EXPECT_EQ(extracted_user_account, original_user_account);
    }
}

TEST_F(JoinChatRoomTesting, accountHasBeenInChatRoomBefore) {

    grpc_chat_commands::JoinChatRoomRequest initial_join_chat_room_request;

    setupUserLoginInfo(
            initial_join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    initial_join_chat_room_request.set_chat_room_id(chat_room_id);
    initial_join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> initial_mock_server_writer;

    joinChatRoom(&initial_join_chat_room_request, &initial_mock_server_writer);

    EXPECT_EQ(initial_mock_server_writer.write_params.size(), 2);
    if (!initial_mock_server_writer.write_params.empty()) {
        EXPECT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!initial_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            ASSERT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            chat_room_creator_account_oid,
            chat_room_creator_user_account.logged_in_token,
            chat_room_creator_user_account.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_kick_or_ban(grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK);
    remove_from_chat_room_request.set_account_id_to_remove(user_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(generateUUID());

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    ASSERT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    original_chat_room_header.getFromCollection(chat_room_id);
    original_user_thumbnail.getFromCollection(user_thumbnail_oid);
    original_user_account.getFromCollection(user_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    bsoncxx::types::b_date timestamp_message_stored{std::chrono::milliseconds{-1}};

    EXPECT_EQ(mock_server_writer.write_params.size(), 2);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }

        //The specifics of sendNewChatRoomAndMessages() are tested elsewhere. Just that it is sending
        // back all the messages is enough.
        EXPECT_EQ(mock_server_writer.write_params[1].msg.messages_list_size(), 8);
        if (mock_server_writer.write_params[1].msg.messages_list_size() >= 2) {
            //This will be the 'current timestamp' value after the message was sent from inside joinChatRoom().
            timestamp_message_stored =
                    bsoncxx::types::b_date{
                            std::chrono::milliseconds{
                                    mock_server_writer.write_params[1].msg.messages_list(
                                            1).message().message_specifics().this_user_joined_chat_room_member_message().member_info().user_info().current_timestamp()
                            }
                    };
        }

    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
    EXPECT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 2);
    EXPECT_FALSE(extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left.empty());
    if (extracted_chat_room_header.accounts_in_chat_room.size() >= 2
        && original_chat_room_header.accounts_in_chat_room.size() >= 2
        && !extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left.empty()) {

        original_chat_room_header.chat_room_last_active_time = timestamp_message_stored;
        original_chat_room_header.accounts_in_chat_room[1].account_oid = user_account_oid;
        original_chat_room_header.accounts_in_chat_room[1].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
        original_chat_room_header.accounts_in_chat_room[1].first_name = original_user_account.first_name;
        original_chat_room_header.accounts_in_chat_room[1].thumbnail_reference = user_thumbnail_oid.to_string();
        original_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp = extracted_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp;
        original_chat_room_header.accounts_in_chat_room[1].thumbnail_size = original_user_thumbnail.thumbnail_size_in_bytes;
        original_chat_room_header.accounts_in_chat_room[1].last_activity_time = timestamp_message_stored;
        original_chat_room_header.accounts_in_chat_room[1].times_joined_left.emplace_back(
                extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left.back()
        );

        EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);
    }

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if (!extracted_user_account.chat_rooms.empty()) {
        original_user_account.chat_rooms.emplace_back(
                chat_room_id,
                extracted_user_account.chat_rooms[0].last_time_viewed
        );
        EXPECT_EQ(extracted_user_account, original_user_account);
    }
}

//Attempting this with two user accounts, a single one will be very rare.
TEST_F(JoinChatRoomTesting, noAdminExists) {

    grpc_chat_commands::JoinChatRoomRequest initial_join_chat_room_request;

    setupUserLoginInfo(
            initial_join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    initial_join_chat_room_request.set_chat_room_id(chat_room_id);
    initial_join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> initial_mock_server_writer;

    joinChatRoom(&initial_join_chat_room_request, &initial_mock_server_writer);

    EXPECT_EQ(initial_mock_server_writer.write_params.size(), 2);
    if (!initial_mock_server_writer.write_params.empty()) {
        EXPECT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!initial_mock_server_writer.write_params[0].msg.messages_list().empty()) {
            ASSERT_EQ(initial_mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::SUCCESS);
        }
    }

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
            remove_from_chat_room_request.mutable_login_info(),
            chat_room_creator_account_oid,
            chat_room_creator_user_account.logged_in_token,
            chat_room_creator_user_account.installation_ids.front()
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_kick_or_ban(grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK);
    remove_from_chat_room_request.set_account_id_to_remove(user_account_oid.to_string());
    remove_from_chat_room_request.set_message_uuid(generateUUID());

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    ASSERT_EQ(remove_from_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    grpc_chat_commands::LeaveChatRoomRequest leave_chat_room_request;
    grpc_chat_commands::LeaveChatRoomResponse leave_chat_room_response;

    setupUserLoginInfo(
            leave_chat_room_request.mutable_login_info(),
            chat_room_creator_account_oid,
            chat_room_creator_user_account.logged_in_token,
            chat_room_creator_user_account.installation_ids.front()
    );

    leave_chat_room_request.set_chat_room_id(chat_room_id);

    leaveChatRoom(&leave_chat_room_request, &leave_chat_room_response);

    ASSERT_EQ(leave_chat_room_response.return_status(), ReturnStatus::SUCCESS);

    //sleep to let timestamps catch up
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    //Chat room is now empty
    original_chat_room_header.getFromCollection(chat_room_id);
    original_user_thumbnail.getFromCollection(user_thumbnail_oid);
    original_user_account.getFromCollection(user_account_oid);

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
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

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
    EXPECT_EQ(original_chat_room_header.accounts_in_chat_room.size(), 2);
    EXPECT_FALSE(extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left.empty());
    if (extracted_chat_room_header.accounts_in_chat_room.size() >= 2
        && original_chat_room_header.accounts_in_chat_room.size() >= 2
        && !extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left.empty()
            ) {

        //cannot get timestamp back
        original_chat_room_header.chat_room_last_active_time = extracted_chat_room_header.chat_room_last_active_time;
        original_chat_room_header.accounts_in_chat_room[1].account_oid = user_account_oid;
        original_chat_room_header.accounts_in_chat_room[1].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN;
        original_chat_room_header.accounts_in_chat_room[1].first_name = original_user_account.first_name;
        original_chat_room_header.accounts_in_chat_room[1].thumbnail_reference = user_thumbnail_oid.to_string();
        original_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp = extracted_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp;
        original_chat_room_header.accounts_in_chat_room[1].thumbnail_size = original_user_thumbnail.thumbnail_size_in_bytes;
        original_chat_room_header.accounts_in_chat_room[1].last_activity_time = extracted_chat_room_header.accounts_in_chat_room[1].last_activity_time;
        original_chat_room_header.accounts_in_chat_room[1].times_joined_left.emplace_back(
                extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left.back()
        );

        EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);
    }

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if (!extracted_user_account.chat_rooms.empty()) {
        original_user_account.chat_rooms.emplace_back(
                chat_room_id,
                extracted_user_account.chat_rooms[0].last_time_viewed
        );
        EXPECT_EQ(extracted_user_account, original_user_account);
    }
}

TEST_F(JoinChatRoomTesting, invalidLoginInfo) {

    checkLoginInfoClientOnly(
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

                join_chat_room_request.mutable_login_info()->CopyFrom(login_info);

                join_chat_room_request.set_chat_room_id(chat_room_id);
                join_chat_room_request.set_chat_room_password(chat_room_password);

                grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

                joinChatRoom(&join_chat_room_request, &mock_server_writer);

                EXPECT_EQ(mock_server_writer.write_params.size(), 1);
                if (!mock_server_writer.write_params.empty()) {
                    EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
                    if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
                        EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
                        return mock_server_writer.write_params[0].msg.messages_list(0).return_status();
                    } else {
                        return ReturnStatus::LG_ERROR;
                    }
                } else {
                    return ReturnStatus::LG_ERROR;
                }
            }
    );

    //Make sure nothing changed
    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);
}

TEST_F(JoinChatRoomTesting, invalidChatRoomId) {

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id("123");
    join_chat_room_request.set_chat_room_password(chat_room_password);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::INVALID_PARAMETER_PASSED);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);
}

TEST_F(JoinChatRoomTesting, invalidChatRoomPassword) {

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.mutable_login_info()->set_lets_go_version(0);

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(gen_random_alpha_numeric_string(
                                                          rand() % 100 + 1 + server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES
                                                  )
    );

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    EXPECT_EQ(mock_server_writer.write_params.size(), 1);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(),
                      ReturnStatus::INVALID_PARAMETER_PASSED);
        }
    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
    EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    EXPECT_EQ(extracted_user_account, original_user_account);
}

void removeMatchingEventElement(
        std::vector<MatchingElement>& matching_array,
        const bsoncxx::oid& event_oid
) {
    matching_array.erase(
            std::remove_if(
                    matching_array.begin(),
                    matching_array.end(),
                    [&](const MatchingElement& m) {
                        return m.oid.to_string() == event_oid.to_string();
                    }
            ),
            matching_array.end()
    );
}

TEST_F(JoinChatRoomTesting, joinedEventChatRoom) {
    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);

    createAndStoreEventAdminAccount();

    const EventChatRoomAdminInfo chat_room_admin_info(
            UserAccountType::ADMIN_GENERATED_EVENT_TYPE
    );

    const auto current_timestamp = getCurrentTimestamp();

    auto event_return_info = generateRandomEventAndEventInfo(
            chat_room_admin_info,
            TEMP_ADMIN_ACCOUNT_NAME,
            current_timestamp
    );

    chat_room_id = event_return_info.chat_room_return_info.chat_room_id();
    chat_room_password = event_return_info.chat_room_return_info.chat_room_password();

    const bsoncxx::oid event_oid{event_return_info.event_account_oid};
    //joinChatRoom should remove the event match but NOT the other match.
    MatchingElement event_match;
    event_match.generateRandomValues();
    event_match.oid = event_oid;
    MatchingElement random_matching_element;
    random_matching_element.generateRandomValues();

    original_user_account.has_been_extracted_accounts_list.emplace_back(
            event_match
    );
    original_user_account.has_been_extracted_accounts_list.emplace_back(
            random_matching_element
    );

    original_user_account.algorithm_matched_accounts_list.emplace_back(
            event_match
    );
    original_user_account.algorithm_matched_accounts_list.emplace_back(
            random_matching_element
    );

    original_user_account.other_users_matched_accounts_list.emplace_back(
            event_match
    );
    original_user_account.other_users_matched_accounts_list.emplace_back(
            random_matching_element
    );

    original_user_account.setIntoCollection();

    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

    setupUserLoginInfo(
            join_chat_room_request.mutable_login_info(),
            user_account_oid,
            original_user_account.logged_in_token,
            original_user_account.installation_ids.front()
    );

    join_chat_room_request.set_chat_room_id(chat_room_id);
    join_chat_room_request.set_chat_room_password(chat_room_password);

    original_chat_room_header.getFromCollection(chat_room_id);

    grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse> mock_server_writer;

    joinChatRoom(&join_chat_room_request, &mock_server_writer);

    bsoncxx::types::b_date timestamp_message_stored{std::chrono::milliseconds{-1}};

    EXPECT_EQ(mock_server_writer.write_params.size(), 2);
    if (mock_server_writer.write_params.size() >= 2) {
        EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list_size(), 1);
        if (!mock_server_writer.write_params[0].msg.messages_list().empty()) {
            EXPECT_TRUE(mock_server_writer.write_params[0].msg.messages_list(0).primer());
            EXPECT_EQ(mock_server_writer.write_params[0].msg.messages_list(0).return_status(), ReturnStatus::SUCCESS);
        }

        //The specifics of sendNewChatRoomAndMessages() are tested elsewhere. Just that it is sending
        // back all the messages is enough.
        EXPECT_EQ(mock_server_writer.write_params[1].msg.messages_list_size(), 6);
        if (mock_server_writer.write_params[1].msg.messages_list_size() >= 2) {
            //This will be the 'current timestamp' value after the message was sent from inside joinChatRoom().
            timestamp_message_stored =
                    bsoncxx::types::b_date{
                            std::chrono::milliseconds{
                                    mock_server_writer.write_params[1].msg.messages_list(
                                            1).message().message_specifics().this_user_joined_chat_room_member_message().member_info().user_info().current_timestamp()
                            }
                    };
        }

    }

    ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);

    EXPECT_EQ(extracted_chat_room_header.accounts_in_chat_room.size(), 2);
    if (extracted_chat_room_header.accounts_in_chat_room.size() >= 2) {

        original_chat_room_header.chat_room_last_active_time = timestamp_message_stored;
        original_chat_room_header.accounts_in_chat_room.emplace_back(
                user_account_oid,
                AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
                original_user_account.first_name,
                user_thumbnail_oid.to_string(),
                extracted_chat_room_header.accounts_in_chat_room[1].thumbnail_timestamp,
                original_user_thumbnail.thumbnail_size_in_bytes,
                timestamp_message_stored,
                extracted_chat_room_header.accounts_in_chat_room[1].times_joined_left
        );

        EXPECT_EQ(extracted_chat_room_header, original_chat_room_header);
    }

    original_user_thumbnail.thumbnail_references.emplace_back(chat_room_id);

    UserPictureDoc extracted_user_thumbnail(user_thumbnail_oid);
    EXPECT_EQ(original_user_thumbnail, extracted_user_thumbnail);

    UserAccountDoc extracted_user_account(user_account_oid);
    removeMatchingEventElement(
            original_user_account.has_been_extracted_accounts_list,
            event_oid
    );
    removeMatchingEventElement(
            original_user_account.algorithm_matched_accounts_list,
            event_oid
    );
    removeMatchingEventElement(
            original_user_account.other_users_matched_accounts_list,
            event_oid
    );

    EXPECT_EQ(extracted_user_account.chat_rooms.size(), 1);
    if (!extracted_user_account.chat_rooms.empty()) {
        original_user_account.chat_rooms.emplace_back(
                chat_room_id,
                extracted_user_account.chat_rooms[0].last_time_viewed,
                std::optional<bsoncxx::oid>(bsoncxx::oid{event_return_info.event_account_oid})
        );
        EXPECT_EQ(extracted_user_account, original_user_account);
    }
}
