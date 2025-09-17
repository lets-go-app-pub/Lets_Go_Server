//
// Created by jeremiah on 6/16/22.
//

#include <utility_general_functions.h>
#include <general_values.h>
#include <fstream>
#include <mongocxx/pool.hpp>
#include <reports_objects.h>
#include <generate_randoms.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>
#include <user_pictures_keys.h>

#include <send_messages_implementation.h>
#include <chat_stream_container_object.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ChatStreamSharedInfo : public ::testing::Test {
protected:
    void SetUp() override {
        map_of_chat_rooms_to_users.clear();
    }

    void TearDown() override {
        map_of_chat_rooms_to_users.clear();
    }
};

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_upsert) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.current_index_value = 2;
    ChatStreamContainerObject third_chat_stream_container_object;
    third_chat_stream_container_object.current_index_value = 3;

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
        chat_room_id,
        first_user_account_oid.to_string(),
        first_chat_stream_container_object.getCurrentIndexValue()
   );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chatRoomOIDs != map_of_chat_rooms_to_users.end());

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);

    chatRoomOIDs->second.upsert(first_user_account_oid.to_string(), second_chat_stream_container_object.current_index_value);

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);
    EXPECT_EQ(chatRoomOIDs->second.map[first_user_account_oid.to_string()], second_chat_stream_container_object.current_index_value);

    chatRoomOIDs->second.upsert(first_user_account_oid.to_string(), second_chat_stream_container_object.current_index_value-1);

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);
    EXPECT_EQ(chatRoomOIDs->second.map[first_user_account_oid.to_string()], second_chat_stream_container_object.current_index_value);

    chatRoomOIDs->second.upsert(second_user_account_oid.to_string(), third_chat_stream_container_object.current_index_value);

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);
    EXPECT_EQ(chatRoomOIDs->second.map[first_user_account_oid.to_string()], second_chat_stream_container_object.current_index_value);
    EXPECT_EQ(chatRoomOIDs->second.map[second_user_account_oid.to_string()], third_chat_stream_container_object.current_index_value);
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_upsert_coroutine) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};

    const std::string first_user_account_oid_str = first_user_account_oid.to_string();
    const std::string second_user_account_oid_str = second_user_account_oid.to_string();

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.current_index_value = 2;
    ChatStreamContainerObject third_chat_stream_container_object;
    third_chat_stream_container_object.current_index_value = 3;

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chatRoomOIDs != map_of_chat_rooms_to_users.end());

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);

    auto handle = chatRoomOIDs->second.upsert_coroutine(first_user_account_oid_str, second_chat_stream_container_object.current_index_value).handle;
    handle.resume();
    handle.destroy();

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);
    EXPECT_EQ(chatRoomOIDs->second.map[first_user_account_oid_str], second_chat_stream_container_object.current_index_value);

    handle = chatRoomOIDs->second.upsert_coroutine(first_user_account_oid_str, second_chat_stream_container_object.current_index_value-1).handle;
    handle.resume();
    handle.destroy();

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);
    EXPECT_EQ(chatRoomOIDs->second.map[first_user_account_oid.to_string()], second_chat_stream_container_object.current_index_value);

    handle = chatRoomOIDs->second.upsert_coroutine(second_user_account_oid_str, third_chat_stream_container_object.current_index_value).handle;
    handle.resume();
    handle.destroy();

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);
    EXPECT_EQ(chatRoomOIDs->second.map[first_user_account_oid.to_string()], second_chat_stream_container_object.current_index_value);
    EXPECT_EQ(chatRoomOIDs->second.map[second_user_account_oid.to_string()], third_chat_stream_container_object.current_index_value);
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_erase) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.current_index_value = 2;

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            second_user_account_oid.to_string(),
            second_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chatRoomOIDs != map_of_chat_rooms_to_users.end());

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);

    chatRoomOIDs->second.erase(bsoncxx::oid{}.to_string(), first_chat_stream_container_object.getCurrentIndexValue());

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);

    chatRoomOIDs->second.erase(first_user_account_oid.to_string(), first_chat_stream_container_object.getCurrentIndexValue()-1);

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);

    chatRoomOIDs->second.erase(first_user_account_oid.to_string(), first_chat_stream_container_object.getCurrentIndexValue());

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);
    EXPECT_EQ(chatRoomOIDs->second.map[second_user_account_oid.to_string()], second_chat_stream_container_object.getCurrentIndexValue());

    chatRoomOIDs->second.erase(second_user_account_oid.to_string(), second_chat_stream_container_object.getCurrentIndexValue()+1);

    EXPECT_TRUE(chatRoomOIDs->second.map.empty());
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_erase_coroutine) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};

    const std::string first_user_account_oid_str = first_user_account_oid.to_string();
    const std::string second_user_account_oid_str = second_user_account_oid.to_string();

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.current_index_value = 2;

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            second_user_account_oid.to_string(),
            second_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chatRoomOIDs != map_of_chat_rooms_to_users.end());

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);

    std::string oid_str = bsoncxx::oid{}.to_string();
    auto handle = chatRoomOIDs->second.erase_coroutine(oid_str, first_chat_stream_container_object.getCurrentIndexValue()).handle;
    handle.resume();
    handle.destroy();

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);

    handle = chatRoomOIDs->second.erase_coroutine(first_user_account_oid_str, first_chat_stream_container_object.getCurrentIndexValue()-1).handle;
    handle.resume();
    handle.destroy();

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 2);

    handle = chatRoomOIDs->second.erase_coroutine(first_user_account_oid_str, first_chat_stream_container_object.getCurrentIndexValue()).handle;
    handle.resume();
    handle.destroy();

    EXPECT_EQ(chatRoomOIDs->second.map.size(), 1);
    EXPECT_EQ(chatRoomOIDs->second.map[second_user_account_oid.to_string()], second_chat_stream_container_object.getCurrentIndexValue());

    handle = chatRoomOIDs->second.erase_coroutine(second_user_account_oid_str, second_chat_stream_container_object.getCurrentIndexValue()+1).handle;
    handle.resume();
    handle.destroy();

    EXPECT_TRUE(chatRoomOIDs->second.map.empty());
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_iterateAndSendMessageUniqueMessageToSender) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};
    bsoncxx::oid third_user_account_oid{};

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.initialization_complete = true;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.initialization_complete = true;
    second_chat_stream_container_object.current_index_value = 2;
    ChatStreamContainerObject third_chat_stream_container_object;
    third_chat_stream_container_object.initialization_complete = true;
    third_chat_stream_container_object.current_index_value = 3;

    user_open_chat_streams.clearAll();

    user_open_chat_streams.insert(
            first_user_account_oid.to_string(),
            &first_chat_stream_container_object
            );

    user_open_chat_streams.insert(
            second_user_account_oid.to_string(),
            &second_chat_stream_container_object
            );

    user_open_chat_streams.insert(
            third_user_account_oid.to_string(),
            &third_chat_stream_container_object
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            second_user_account_oid.to_string(),
            second_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            third_user_account_oid.to_string(),
            third_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chat_room_oids = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chat_room_oids != map_of_chat_rooms_to_users.end());

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> standard_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto standard_message = standard_response->mutable_return_new_chat_message()->add_messages_list();
    standard_message->set_message_uuid(generateUUID());

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> unique_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto unique_message = unique_response->mutable_return_new_chat_message()->add_messages_list();
    unique_message->set_message_uuid(generateUUID());

    chat_room_oids->second.iterateAndSendMessageUniqueMessageToSender(
            first_user_account_oid.to_string(),
            [standard_response](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                reply_vector.push_back(standard_response);
            },
            [unique_response](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                reply_vector.push_back(unique_response);
            }
        );

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    auto first_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(first_chat_stream_container_object.responder_.get());
    auto second_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(second_chat_stream_container_object.responder_.get());
    auto third_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(third_chat_stream_container_object.responder_.get());

    ASSERT_EQ(first_mock_rw->write_params.size(), 1);
    ASSERT_EQ(first_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(first_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), unique_message->message_uuid());

    ASSERT_EQ(second_mock_rw->write_params.size(), 1);
    ASSERT_EQ(second_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(second_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), standard_message->message_uuid());

    ASSERT_EQ(third_mock_rw->write_params.size(), 1);
    ASSERT_EQ(third_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(third_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), standard_message->message_uuid());

    user_open_chat_streams.clearAll();
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_iterateAndSendMessageExcludeSender) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};
    bsoncxx::oid third_user_account_oid{};

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.initialization_complete = true;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.initialization_complete = true;
    second_chat_stream_container_object.current_index_value = 2;
    ChatStreamContainerObject third_chat_stream_container_object;
    third_chat_stream_container_object.initialization_complete = true;
    third_chat_stream_container_object.current_index_value = 3;

    user_open_chat_streams.clearAll();

    user_open_chat_streams.insert(
            first_user_account_oid.to_string(),
            &first_chat_stream_container_object
            );

    user_open_chat_streams.insert(
            second_user_account_oid.to_string(),
            &second_chat_stream_container_object
            );

    user_open_chat_streams.insert(
            third_user_account_oid.to_string(),
            &third_chat_stream_container_object
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            second_user_account_oid.to_string(),
            second_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            third_user_account_oid.to_string(),
            third_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chat_room_oids = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chat_room_oids != map_of_chat_rooms_to_users.end());

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto message = response->mutable_return_new_chat_message()->add_messages_list();
    message->set_message_uuid(generateUUID());

    chat_room_oids->second.iterateAndSendMessageExcludeSender(
        first_user_account_oid.to_string(),
        [response](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
            reply_vector.push_back(response);
        }
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    auto first_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(first_chat_stream_container_object.responder_.get());
    auto second_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(second_chat_stream_container_object.responder_.get());
    auto third_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(third_chat_stream_container_object.responder_.get());

    EXPECT_TRUE(first_mock_rw->write_params.empty());

    ASSERT_EQ(second_mock_rw->write_params.size(), 1);
    ASSERT_EQ(second_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(second_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), message->message_uuid());

    ASSERT_EQ(third_mock_rw->write_params.size(), 1);
    ASSERT_EQ(third_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(third_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), message->message_uuid());

    user_open_chat_streams.clearAll();
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_sendMessageToSpecificUser) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};
    bsoncxx::oid third_user_account_oid{};

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.initialization_complete = true;
    first_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.initialization_complete = true;
    second_chat_stream_container_object.current_index_value = 1;
    ChatStreamContainerObject third_chat_stream_container_object;
    third_chat_stream_container_object.initialization_complete = true;
    third_chat_stream_container_object.current_index_value = 1;

    user_open_chat_streams.clearAll();

    user_open_chat_streams.insert(
            first_user_account_oid.to_string(),
            &first_chat_stream_container_object
            );

    user_open_chat_streams.insert(
            second_user_account_oid.to_string(),
            &second_chat_stream_container_object
            );

    user_open_chat_streams.insert(
            third_user_account_oid.to_string(),
            &third_chat_stream_container_object
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            second_user_account_oid.to_string(),
            second_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            third_user_account_oid.to_string(),
            third_chat_stream_container_object.getCurrentIndexValue()
            );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chat_room_oids = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chat_room_oids != map_of_chat_rooms_to_users.end());

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto message = response->mutable_return_new_chat_message()->add_messages_list();
    message->set_message_uuid(generateUUID());

    chat_room_oids->second.sendMessageToSpecificUser(
            [response](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                reply_vector.push_back(response);
            },
            first_user_account_oid.to_string()
            );

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    auto first_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(first_chat_stream_container_object.responder_.get());
    auto second_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(second_chat_stream_container_object.responder_.get());
    auto third_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(third_chat_stream_container_object.responder_.get());

    ASSERT_EQ(first_mock_rw->write_params.size(), 1);
    ASSERT_EQ(first_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(first_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), message->message_uuid());

    ASSERT_TRUE(second_mock_rw->write_params.empty());
    ASSERT_TRUE(third_mock_rw->write_params.empty());

    user_open_chat_streams.clearAll();
}

TEST_F(ChatStreamSharedInfo, ChatStreamConcurrentSet_sendMessagesToTwoUsers) {

    const std::string chat_room_id = generateRandomChatRoomId();
    bsoncxx::oid first_user_account_oid{};
    bsoncxx::oid second_user_account_oid{};
    bsoncxx::oid third_user_account_oid{};

    std::cout << "first_user_account_oid : " << first_user_account_oid.to_string() << '\n';
    std::cout << "second_user_account_oid: " << second_user_account_oid.to_string() << '\n';
    std::cout << "third_user_account_oid : " << third_user_account_oid.to_string() << '\n';

    ChatStreamContainerObject first_chat_stream_container_object;
    first_chat_stream_container_object.initialization_complete = true;
    first_chat_stream_container_object.current_index_value = 1;
    first_chat_stream_container_object.current_user_account_oid_str = first_user_account_oid.to_string();
    ChatStreamContainerObject second_chat_stream_container_object;
    second_chat_stream_container_object.initialization_complete = true;
    second_chat_stream_container_object.current_index_value = 1;
    second_chat_stream_container_object.current_user_account_oid_str = second_user_account_oid.to_string();
    ChatStreamContainerObject third_chat_stream_container_object;
    third_chat_stream_container_object.initialization_complete = true;
    third_chat_stream_container_object.current_index_value = 1;
    third_chat_stream_container_object.current_user_account_oid_str = third_user_account_oid.to_string();

    user_open_chat_streams.clearAll();

    user_open_chat_streams.insert(
            first_user_account_oid.to_string(),
            &first_chat_stream_container_object
    );

    user_open_chat_streams.insert(
            second_user_account_oid.to_string(),
            &second_chat_stream_container_object
    );

    user_open_chat_streams.insert(
            third_user_account_oid.to_string(),
            &third_chat_stream_container_object
    );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 0);

    bool return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            first_user_account_oid.to_string(),
            first_chat_stream_container_object.getCurrentIndexValue()
    );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            second_user_account_oid.to_string(),
            second_chat_stream_container_object.getCurrentIndexValue()
    );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    return_val = insertUserOIDToChatRoomId(
            chat_room_id,
            third_user_account_oid.to_string(),
            third_chat_stream_container_object.getCurrentIndexValue()
    );

    EXPECT_EQ(map_of_chat_rooms_to_users.size(), 1);
    EXPECT_TRUE(return_val);

    auto chat_room_oids = map_of_chat_rooms_to_users.find(chat_room_id);
    ASSERT_TRUE(chat_room_oids != map_of_chat_rooms_to_users.end());

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto message = response->mutable_return_new_chat_message()->add_messages_list();
    message->set_message_uuid(generateUUID());

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response_two = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto message_two = response_two->mutable_return_new_chat_message()->add_messages_list();
    message_two->set_message_uuid(generateUUID());

    chat_room_oids->second.sendMessagesToTwoUsers(
            [response](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                reply_vector.push_back(response);
            },
            first_user_account_oid.to_string(),
            [response_two](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                reply_vector.push_back(response_two);
            },
            second_user_account_oid.to_string()
    );

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    auto first_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(first_chat_stream_container_object.responder_.get());
    auto second_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(second_chat_stream_container_object.responder_.get());
    auto third_mock_rw = dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(third_chat_stream_container_object.responder_.get());

    ASSERT_EQ(first_mock_rw->write_params.size(), 1);
    ASSERT_EQ(first_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(first_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), message->message_uuid());

    ASSERT_EQ(second_mock_rw->write_params.size(), 1);
    ASSERT_EQ(second_mock_rw->write_params[0].msg.return_new_chat_message().messages_list_size(), 1);
    EXPECT_EQ(second_mock_rw->write_params[0].msg.return_new_chat_message().messages_list()[0].message_uuid(), message_two->message_uuid());

    ASSERT_TRUE(third_mock_rw->write_params.empty());

    user_open_chat_streams.clearAll();
}

