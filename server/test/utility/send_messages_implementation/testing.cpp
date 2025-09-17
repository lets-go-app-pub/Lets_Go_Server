//
// Created by jeremiah on 6/18/22.
//

#include <fstream>
#include <mongocxx/pool.hpp>
#include <reports_objects.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>
#include <user_pictures_keys.h>

#include <send_messages_implementation.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SendMessagesImplementation : public ::testing::Test {
protected:
    void SetUp() override {
        map_of_chat_rooms_to_users.clear();
    }

    void TearDown() override {
        map_of_chat_rooms_to_users.clear();
    }
};

TEST_F(SendMessagesImplementation, insertUserOIDToChatRoomId) {

    const std::string chat_room_id = "12345678";
    const std::string user_account_oid = bsoncxx::oid{}.to_string();
    const long index_num = 1;

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid,
            index_num
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid,
            index_num
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);

    const std::string user_account_oid_two = bsoncxx::oid{}.to_string();

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid_two,
            index_num
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 2);

}

TEST_F(SendMessagesImplementation, insertUserOIDToChatRoomId_coroutine) {

    const std::string chat_room_id = "12345678";
    const std::string user_account_oid = bsoncxx::oid{}.to_string();
    const long index_num = 1;

    auto handle = insertUserOIDToChatRoomId_coroutine(
            chat_room_id,
            user_account_oid,
            index_num
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);

    handle = insertUserOIDToChatRoomId_coroutine(
            chat_room_id,
            user_account_oid,
            index_num
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);

    const std::string user_account_oid_two = bsoncxx::oid{}.to_string();

    handle = insertUserOIDToChatRoomId_coroutine(
            chat_room_id,
            user_account_oid_two,
            index_num
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 2);

}

TEST_F(SendMessagesImplementation, eraseUserOIDFromChatRoomId) {
    const std::string chat_room_id = "12345678";
    const std::string chat_room_id_two = "01234567";
    const std::string user_account_oid = bsoncxx::oid{}.to_string();
    const std::string user_account_oid_two = bsoncxx::oid{}.to_string();
    const long index_num= 1;
    const long index_num_two = 2;

    insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid,
            index_num
    );

    insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid_two,
            index_num_two
            );

    insertUserOIDToChatRoomId(
            chat_room_id_two,
            user_account_oid,
            index_num
    );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    //passing in wrong object pointer
    eraseUserOIDFromChatRoomId(
            chat_room_id,
            user_account_oid_two,
            index_num_two-1
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    eraseUserOIDFromChatRoomId(
            chat_room_id,
            user_account_oid_two,
            index_num_two
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    //does not exist
    eraseUserOIDFromChatRoomId(
            chat_room_id_two,
            user_account_oid_two,
            index_num_two
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    eraseUserOIDFromChatRoomId(
            chat_room_id_two,
            user_account_oid,
            index_num
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 0);
}

TEST_F(SendMessagesImplementation, eraseUserOIDFromChatRoomId_coroutine) {
    const std::string chat_room_id = "12345678";
    const std::string chat_room_id_two = "01234567";
    const std::string user_account_oid = bsoncxx::oid{}.to_string();
    const std::string user_account_oid_two = bsoncxx::oid{}.to_string();
    const long index_num= 1;
    const long index_num_two = 2;

    insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid,
            index_num
            );

    insertUserOIDToChatRoomId(
            chat_room_id,
            user_account_oid_two,
            index_num_two
            );

    insertUserOIDToChatRoomId(
            chat_room_id_two,
            user_account_oid,
            index_num
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    //passing in wrong object pointer
    auto handle = eraseUserOIDFromChatRoomId_coroutine(
            chat_room_id,
            user_account_oid_two,
            index_num_two-1
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    handle = eraseUserOIDFromChatRoomId_coroutine(
            chat_room_id,
            user_account_oid_two,
            index_num_two
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    //does not exist
    handle = eraseUserOIDFromChatRoomId_coroutine(
            chat_room_id_two,
            user_account_oid_two,
            index_num_two
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 1);

    handle = eraseUserOIDFromChatRoomId_coroutine(
            chat_room_id_two,
            user_account_oid,
            index_num
            ).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 2);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id].map.size(), 1);
    EXPECT_EQ(map_of_chat_rooms_to_users[chat_room_id_two].map.size(), 0);
}