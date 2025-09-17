//
// Created by jeremiah on 7/21/22.
//
#include <utility_general_functions.h>
#include <general_values.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/collection.hpp>
#include <database_names.h>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include <async_server.h>
#include <user_open_chat_streams.h>
#include <chat_stream_container_object.h>
#include <chat_message_stream.h>
#include <send_messages_implementation.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ChatMessageStreamTests : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

class ThreadSafeWritesWaitingToProcessVectorTests : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

class ExtractChatRoomsTests : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(ChatMessageStreamTests, generateNextElementName_emptyString) {
    std::set<std::string> previous_values;
    std::string value;

    generateNextElementName(value);

    EXPECT_EQ(value, "a");
}

TEST_F(ChatMessageStreamTests, generateNextElementName_genericMiddleCase) {

    std::set<std::string> previous_values;
    std::string value = "dg";

    generateNextElementName(value);

    EXPECT_EQ(value, "eg");
}

TEST_F(ChatMessageStreamTests, generateNextElementName_noDuplicates) {
    std::set<std::string> previous_values;
    std::string value;

    //4 'digits' and starts 5
    for(int i = 0; i < (26*26*26*26+1); ++i) {
        generateNextElementName(value);

        bool contains = previous_values.contains(value);
        EXPECT_FALSE(contains);
        if(contains) std::cout << value << '\n';

        previous_values.insert(value);
    }
}

TEST_F(ChatMessageStreamTests, checkLoginToken_invalidInfo) {

    const bsoncxx::oid generated_account_oid;
    const std::string logged_in_token = bsoncxx::oid{}.to_string();
    const std::string installation_id = generateUUID();
    std::chrono::milliseconds current_timestamp{-1};

    checkLoginInfoClientOnly(
            generated_account_oid,
            logged_in_token,
            installation_id,
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {

                grpc_stream_chat::InitialLoginMessageRequest request;
                ReturnStatus return_status = ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_;

                request.mutable_login_info()->CopyFrom(login_info);

                std::string return_value = checkLoginToken(
                        request,
                        return_status,
                        current_timestamp
                );

                EXPECT_EQ(return_value, "");
                return return_status;
            }
    );

}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, concat_vector_to_front_without_lock) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;
    data_structure.vector.emplace_back(2);
    data_structure.vector.emplace_back(3);
    data_structure.vector.emplace_back(4);
    std::vector<int> v{0,1};

    data_structure.concat_vector_to_front_without_lock(v);

    ASSERT_EQ(data_structure.vector.size(), 5);
    for(int i = 0; i < 5; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, concat_vector_to_index_one_without_lock) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;
    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(3);
    data_structure.vector.emplace_back(4);
    std::vector<int> v{1,2};

    data_structure.concat_vector_to_index_one_without_lock(v);

    ASSERT_EQ(data_structure.vector.size(), 5);
    for(int i = 0; i < 5; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, push_and_size_no_coroutine) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;
    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(1);
    data_structure.vector.emplace_back(2);

    bool function_ran = false;

    size_t size = data_structure.push_and_size_no_coroutine(
            3,
            [&](){
                function_ran = true;
            }
    );

    EXPECT_TRUE(function_ran);
    ASSERT_EQ(data_structure.vector.size(), 4);
    ASSERT_EQ(size, 4);
    for(int i = 0; i < 4; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, push_and_size_no_lock) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;
    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(1);
    data_structure.vector.emplace_back(2);

    size_t size = data_structure.push_and_size_no_lock(3);

    ASSERT_EQ(data_structure.vector.size(), 4);
    ASSERT_EQ(size, 4);
    for(int i = 0; i < 4; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, pop_front) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;
    data_structure.vector.emplace_back(5);
    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(1);
    data_structure.vector.emplace_back(2);

    bool successful = false;

    auto handle = data_structure.pop_front(successful).handle;

    handle.resume();
    handle.destroy();

    EXPECT_TRUE(successful);
    ASSERT_EQ(data_structure.vector.size(), 3);
    for(int i = 0; i < 3; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_empty_lastElement) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    data_structure.vector.emplace_back(5);

    bool empty = false;

    auto handle = data_structure.pop_front_and_empty(empty).handle;

    handle.resume();
    handle.destroy();

    EXPECT_TRUE(empty);
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_empty_middleCase) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    data_structure.vector.emplace_back(5);
    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(1);
    data_structure.vector.emplace_back(2);

    bool empty = true;

    auto handle = data_structure.pop_front_and_empty(empty).handle;

    handle.resume();
    handle.destroy();

    EXPECT_FALSE(empty);
    ASSERT_EQ(data_structure.vector.size(), 3);
    for(int i = 0; i < 3; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_get_next_empty) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    bool empty = true;
    std::shared_ptr<int> ptr;

    auto handle = data_structure.pop_front_and_get_next(empty, ptr).handle;

    handle.resume();
    handle.destroy();

    EXPECT_TRUE(empty);
    EXPECT_FALSE(ptr);
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_get_next_lastElement) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    data_structure.vector.emplace_back(0);

    bool empty = true;
    std::shared_ptr<int> ptr;

    auto handle = data_structure.pop_front_and_get_next(empty, ptr).handle;

    handle.resume();
    handle.destroy();

    EXPECT_TRUE(empty);
    EXPECT_FALSE(ptr);
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, pop_front_and_get_next_middleCase) {
    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    data_structure.vector.emplace_back(5);
    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(1);
    data_structure.vector.emplace_back(2);

    bool empty = true;
    std::shared_ptr<int> ptr;

    auto handle = data_structure.pop_front_and_get_next(empty, ptr).handle;

    handle.resume();
    handle.destroy();

    EXPECT_FALSE(empty);
    EXPECT_EQ(*ptr, 0);
    ASSERT_EQ(data_structure.vector.size(), 3);
    for(int i = 0; i < 3; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, front_if_not_empty_emptyVector) {

    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    std::shared_ptr<int> ptr;

    auto handle = data_structure.front_if_not_empty(ptr).handle;

    handle.resume();
    handle.destroy();

    EXPECT_FALSE(ptr);
    EXPECT_EQ(data_structure.vector.size(), 0);
}

TEST_F(ThreadSafeWritesWaitingToProcessVectorTests, front_if_not_empty_notEmpty) {

    ThreadSafeWritesWaitingToProcessVector<int> data_structure;

    data_structure.vector.emplace_back(0);
    data_structure.vector.emplace_back(1);

    std::shared_ptr<int> ptr;

    auto handle = data_structure.front_if_not_empty(ptr).handle;

    handle.resume();
    handle.destroy();

    EXPECT_EQ(*ptr, 0);
    EXPECT_EQ(data_structure.vector.size(), 2);
    for(int i = 0; i < 2; ++i) {
        EXPECT_EQ(data_structure.vector[i], i);
    }
}

TEST_F(ExtractChatRoomsTests, invalidChatRoomId) {

    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    const std::string generated_account_oid_str = generated_account_oid.to_string();

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bool successful = false;

    ChatStreamContainerObject chat_stream_container_object;

    auto handle = user_open_chat_streams.upsert(
            generated_account_oid_str,
        &chat_stream_container_object,
        successful,
        [&](ChatStreamContainerObject*) {
            chat_stream_container_object.current_index_value = ChatStreamContainerObject::chat_stream_container_index.fetch_add(1);
        }
    ).handle;

    handle.resume();
    handle.destroy();

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> send_messages_object(reply_vector);

    grpc_stream_chat::InitialLoginMessageRequest initial_login_message_request;

    auto chat_room_values = initial_login_message_request.add_chat_room_values();

    chat_room_values->set_chat_room_id("123");
    chat_room_values->set_chat_room_last_time_updated(current_timestamp.count());
    chat_room_values->set_chat_room_last_time_viewed(current_timestamp.count());

    std::set<std::string> chat_room_ids_user_is_part_of;

    handle = extractChatRooms(
            initial_login_message_request,
            send_messages_object,
            chat_room_ids_user_is_part_of,
            successful,
            chat_stream_container_object.current_index_value,
            generated_account_oid_str,
            current_timestamp
            ).handle;

    handle.resume();
    handle.destroy();

    send_messages_object.finalCleanup();

    EXPECT_TRUE(successful);

    EXPECT_TRUE(chat_room_ids_user_is_part_of.empty());

    EXPECT_TRUE(map_of_chat_rooms_to_users.empty());

    EXPECT_TRUE(reply_vector.empty());
}

TEST_F(ExtractChatRoomsTests, userNotInsideAnyChatRooms) {
    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    const std::string generated_account_oid_str = generated_account_oid.to_string();

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bool successful = false;

    ChatStreamContainerObject chat_stream_container_object;

    auto handle = user_open_chat_streams.upsert(
            generated_account_oid_str,
            &chat_stream_container_object,
            successful,
            [&](ChatStreamContainerObject*) {
                chat_stream_container_object.current_index_value = ChatStreamContainerObject::chat_stream_container_index.fetch_add(1);
            }
    ).handle;

    handle.resume();
    handle.destroy();

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> send_messages_object(reply_vector);

    grpc_stream_chat::InitialLoginMessageRequest initial_login_message_request;

    std::set<std::string> chat_room_ids_user_is_part_of;

    handle = extractChatRooms(
            initial_login_message_request,
            send_messages_object,
            chat_room_ids_user_is_part_of,
            successful,
            chat_stream_container_object.current_index_value,
            generated_account_oid_str,
            current_timestamp
    ).handle;

    handle.resume();
    handle.destroy();

    send_messages_object.finalCleanup();

    EXPECT_TRUE(successful);

    EXPECT_TRUE(chat_room_ids_user_is_part_of.empty());

    EXPECT_TRUE(map_of_chat_rooms_to_users.empty());

    EXPECT_TRUE(reply_vector.empty());
}

TEST_F(ExtractChatRoomsTests, chatRoomExcludedFromRequest) {

    //NOTE: Leave out a chat room from request and make sure it is added.
    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    const std::string generated_account_oid_str = generated_account_oid.to_string();
    UserAccountDoc generated_user_account(generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    //sleep to make sure all messages are stored in chat room and current_timestamp is past
    // those times
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    const std::string chat_room_id = create_chat_room_response.chat_room_id();
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bool successful = false;

    ChatStreamContainerObject chat_stream_container_object;

    auto handle = user_open_chat_streams.upsert(
            generated_account_oid_str,
            &chat_stream_container_object,
            successful,
            [&](ChatStreamContainerObject*) {
                chat_stream_container_object.current_index_value = ChatStreamContainerObject::chat_stream_container_index.fetch_add(1);
            }
    ).handle;

    handle.resume();
    handle.destroy();

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> send_messages_object(reply);

    grpc_stream_chat::InitialLoginMessageRequest initial_login_message_request;

    std::set<std::string> chat_room_ids_user_is_part_of;

    handle = extractChatRooms(
            initial_login_message_request,
            send_messages_object,
            chat_room_ids_user_is_part_of,
            successful,
            chat_stream_container_object.current_index_value,
            generated_account_oid_str,
            current_timestamp
    ).handle;

    handle.resume();
    handle.destroy();

    send_messages_object.finalCleanup();

    EXPECT_TRUE(successful);

    EXPECT_EQ(chat_room_ids_user_is_part_of.size(), 1);
    EXPECT_TRUE(chat_room_ids_user_is_part_of.contains(chat_room_id));

    auto chat_room_map_ptr = map_of_chat_rooms_to_users.find(chat_room_id);
    EXPECT_NE(chat_room_map_ptr, map_of_chat_rooms_to_users.end());
    if(chat_room_map_ptr != map_of_chat_rooms_to_users.end()) {
        auto user_ptr = chat_room_map_ptr->second.map.find(generated_account_oid_str);
        EXPECT_NE(user_ptr, chat_room_map_ptr->second.map.end());
        if(user_ptr != chat_room_map_ptr->second.map.end()) {
            EXPECT_EQ(user_ptr->second, chat_stream_container_object.current_index_value);
        }
    }

    ASSERT_EQ(reply.size(), 1);

    //should contain
    //this_user_joined_chat_room_start_message
    //chat_room_cap_message
    //different_user_joined_message
    //this_user_joined_chat_room_finished_message
    ASSERT_EQ(reply[0]->initial_connection_messages_response().messages_list_size(), 4);

    //NOTE: The code blocks to add, remove and update chat rooms each call functions that are individually
    // tested. There is no need to test all the specifics for each one here.
    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(0).message().standard_message_info().chat_room_id_message_sent_from(),
            chat_room_id
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(0).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomStartMessage
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(1).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kChatRoomCapMessage
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(2).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(3).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomFinishedMessage
    );

}

TEST_F(ExtractChatRoomsTests, extraChatRoomAddedToRequest) {

    //NOTE: Leave out a chat room from request and make sure it is added.
    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    const std::string generated_account_oid_str = generated_account_oid.to_string();

    const std::string chat_room_id = "12345678";
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bool successful = false;

    ChatStreamContainerObject chat_stream_container_object;

    auto handle = user_open_chat_streams.upsert(
            generated_account_oid_str,
            &chat_stream_container_object,
            successful,
            [&](ChatStreamContainerObject*) {
                chat_stream_container_object.current_index_value = ChatStreamContainerObject::chat_stream_container_index.fetch_add(1);
            }
    ).handle;

    handle.resume();
    handle.destroy();

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> send_messages_object(reply_vector);

    grpc_stream_chat::InitialLoginMessageRequest initial_login_message_request;

    auto chat_room_values = initial_login_message_request.add_chat_room_values();

    chat_room_values->set_chat_room_id(chat_room_id);
    chat_room_values->set_chat_room_last_time_updated(current_timestamp.count());
    chat_room_values->set_chat_room_last_time_viewed(current_timestamp.count());

    std::set<std::string> chat_room_ids_user_is_part_of;

    handle = extractChatRooms(
            initial_login_message_request,
            send_messages_object,
            chat_room_ids_user_is_part_of,
            successful,
            chat_stream_container_object.current_index_value,
            generated_account_oid_str,
            current_timestamp
    ).handle;

    handle.resume();
    handle.destroy();

    send_messages_object.finalCleanup();

    EXPECT_TRUE(successful);

    EXPECT_TRUE(chat_room_ids_user_is_part_of.empty());

    EXPECT_TRUE(map_of_chat_rooms_to_users.empty());

    ASSERT_EQ(reply_vector.size(), 1);

    //should contain
    //this_user_left_chat_room_message
    ASSERT_EQ(reply_vector[0]->initial_connection_messages_response().messages_list_size(), 1);

    //NOTE: The code blocks to add, remove and update chat rooms each call functions that are individually
    // tested. There is no need to test all the specifics for each one here.
    EXPECT_EQ(
            reply_vector[0]->initial_connection_messages_response().messages_list(0).message().standard_message_info().chat_room_id_message_sent_from(),
            chat_room_id
    );

    EXPECT_EQ(
            reply_vector[0]->initial_connection_messages_response().messages_list(0).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kThisUserLeftChatRoomMessage
    );

}

TEST_F(ExtractChatRoomsTests, chatRoomProperlyUpdated) {

    //NOTE: Leave out a chat room from request and make sure it is added.
    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    const std::string generated_account_oid_str = generated_account_oid.to_string();
    UserAccountDoc extracted_user_account_before(generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            extracted_user_account_before.logged_in_token,
            extracted_user_account_before.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    //make sure that the current_timestamp is AFTER the timestamp stored inside createChatRoom (so at least +2)
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    extracted_user_account_before.getFromCollection(generated_account_oid);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bool successful = false;

    ChatStreamContainerObject chat_stream_container_object;

    auto handle = user_open_chat_streams.upsert(
            generated_account_oid_str,
            &chat_stream_container_object,
            successful,
            [&](ChatStreamContainerObject*) {
                chat_stream_container_object.current_index_value = ChatStreamContainerObject::chat_stream_container_index.fetch_add(1);
            }
    ).handle;

    handle.resume();
    handle.destroy();

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> send_messages_object(reply);

    grpc_stream_chat::InitialLoginMessageRequest initial_login_message_request;

    auto chat_room_values = initial_login_message_request.add_chat_room_values();

    chat_room_values->set_chat_room_id(chat_room_id);
    chat_room_values->set_chat_room_last_time_updated(-1);
    chat_room_values->set_chat_room_last_time_viewed(-1);

    std::set<std::string> chat_room_ids_user_is_part_of;

    handle = extractChatRooms(
            initial_login_message_request,
            send_messages_object,
            chat_room_ids_user_is_part_of,
            successful,
            chat_stream_container_object.current_index_value,
            generated_account_oid_str,
            current_timestamp
    ).handle;

    handle.resume();
    handle.destroy();

    send_messages_object.finalCleanup();

    EXPECT_TRUE(successful);

    UserAccountDoc extracted_user_account_after(generated_account_oid);

    EXPECT_EQ(extracted_user_account_after, extracted_user_account_before);

    EXPECT_EQ(chat_room_ids_user_is_part_of.size(), 1);
    EXPECT_TRUE(chat_room_ids_user_is_part_of.contains(chat_room_id));

    auto chat_room_map_ptr = map_of_chat_rooms_to_users.find(chat_room_id);
    EXPECT_NE(chat_room_map_ptr, map_of_chat_rooms_to_users.end());
    if(chat_room_map_ptr != map_of_chat_rooms_to_users.end()) {
        auto user_ptr = chat_room_map_ptr->second.map.find(generated_account_oid_str);
        EXPECT_NE(user_ptr, chat_room_map_ptr->second.map.end());
        if(user_ptr != chat_room_map_ptr->second.map.end()) {
            EXPECT_EQ(user_ptr->second, chat_stream_container_object.current_index_value);
        }
    }

    ASSERT_EQ(reply.size(), 1);

    //Should contain
    //update_observed_time_message
    //chat_room_cap_message
    //different_user_joined_message
    ASSERT_EQ(reply[0]->initial_connection_messages_response().messages_list_size(), 3);

    //NOTE: The code blocks to add, remove and update chat rooms each call functions that are individually
    // tested. There is no need to test all the specifics for each one here.
    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(0).message().standard_message_info().chat_room_id_message_sent_from(),
            chat_room_id
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(0).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kUpdateObservedTimeMessage
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(0).message().message_specifics().update_observed_time_message().chat_room_last_observed_time(),
            extracted_user_account_after.chat_rooms[0].last_time_viewed
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(1).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kChatRoomCapMessage
    );

    EXPECT_EQ(
            reply[0]->initial_connection_messages_response().messages_list(2).message().message_specifics().message_body_case(),
            MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
    );
}

TEST_F(ExtractChatRoomsTests, updateObservedTimeOnServer) {

    //NOTE: Leave out a chat room from request and make sure it is added.
    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);
    const std::string generated_account_oid_str = generated_account_oid.to_string();
    UserAccountDoc extracted_user_account_before(generated_account_oid);

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            extracted_user_account_before.logged_in_token,
            extracted_user_account_before.installation_ids.front()
    );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

    extracted_user_account_before.getFromCollection(generated_account_oid);

    const std::string chat_room_id = create_chat_room_response.chat_room_id();

    bool successful = false;

    ChatStreamContainerObject chat_stream_container_object;

    auto handle = user_open_chat_streams.upsert(
            generated_account_oid_str,
            &chat_stream_container_object,
            successful,
            [&](ChatStreamContainerObject*) {
                chat_stream_container_object.current_index_value = ChatStreamContainerObject::chat_stream_container_index.fetch_add(1);
            }
    ).handle;

    handle.resume();
    handle.destroy();

    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> send_messages_object(reply_vector);

    //make sure that the current_timestamp is AFTER the timestamp stored inside createChatRoom (so at least +2)
    std::this_thread::sleep_for(std::chrono::milliseconds{10});

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    grpc_stream_chat::InitialLoginMessageRequest initial_login_message_request;

    auto chat_room_values = initial_login_message_request.add_chat_room_values();

    chat_room_values->set_chat_room_id(chat_room_id);
    chat_room_values->set_chat_room_last_time_updated(current_timestamp.count() + chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING.count());
    chat_room_values->set_chat_room_last_time_viewed(current_timestamp.count());

    std::set<std::string> chat_room_ids_user_is_part_of;

    handle = extractChatRooms(
            initial_login_message_request,
            send_messages_object,
            chat_room_ids_user_is_part_of,
            successful,
            chat_stream_container_object.current_index_value,
            generated_account_oid_str,
            current_timestamp
    ).handle;

    handle.resume();
    handle.destroy();

    send_messages_object.finalCleanup();

    EXPECT_TRUE(successful);

    //last_time_viewed should be updated by extractChatRooms()
    extracted_user_account_before.chat_rooms[0].last_time_viewed = bsoncxx::types::b_date{current_timestamp};
    UserAccountDoc extracted_user_account_after(generated_account_oid);

    EXPECT_EQ(extracted_user_account_after, extracted_user_account_before);

    EXPECT_EQ(chat_room_ids_user_is_part_of.size(), 1);
    EXPECT_TRUE(chat_room_ids_user_is_part_of.contains(chat_room_id));

    auto chat_room_map_ptr = map_of_chat_rooms_to_users.find(chat_room_id);
    EXPECT_NE(chat_room_map_ptr, map_of_chat_rooms_to_users.end());
    if(chat_room_map_ptr != map_of_chat_rooms_to_users.end()) {
        auto user_ptr = chat_room_map_ptr->second.map.find(generated_account_oid_str);
        EXPECT_NE(user_ptr, chat_room_map_ptr->second.map.end());
        if(user_ptr != chat_room_map_ptr->second.map.end()) {
            EXPECT_EQ(user_ptr->second, chat_stream_container_object.current_index_value);
        }
    }

    EXPECT_EQ(reply_vector.size(), 1);
}

TEST_F(ExtractChatRoomsTests, compareUserAccountChatRoomsStates_userStatesUnchanged) {

    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account(generated_account_oid);

    std::vector<std::string> pre_chat_room_ids_inside_user_account_doc{
        generateUUID(),
        generateUUID()
    };

    user_account.chat_rooms.emplace_back(
            pre_chat_room_ids_inside_user_account_doc[1],
            bsoncxx::types::b_date{std::chrono::milliseconds{5}}
            );

    user_account.chat_rooms.emplace_back(
            pre_chat_room_ids_inside_user_account_doc[0],
            bsoncxx::types::b_date{std::chrono::milliseconds{5}}
    );

    user_account.setIntoCollection();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_account_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountChatRoomComparison return_value = compareUserAccountChatRoomsStates(
            pre_chat_room_ids_inside_user_account_doc,
            user_account_collection,
            generated_account_oid
    );

    EXPECT_EQ(return_value, UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_MATCH);

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account, user_account);
}

TEST_F(ExtractChatRoomsTests, compareUserAccountChatRoomsStates_userStatesChanged) {

    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account(generated_account_oid);

    std::vector<std::string> pre_chat_room_ids_inside_user_account_doc{
            generateUUID(),
            generateUUID()
    };

    user_account.chat_rooms.emplace_back(
            pre_chat_room_ids_inside_user_account_doc[1],
            bsoncxx::types::b_date{std::chrono::milliseconds{5}}
    );

    user_account.setIntoCollection();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_account_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountChatRoomComparison return_value = compareUserAccountChatRoomsStates(
            pre_chat_room_ids_inside_user_account_doc,
            user_account_collection,
            generated_account_oid
    );

    EXPECT_EQ(return_value, UserAccountChatRoomComparison::CHAT_ROOM_COMPARISON_DO_NOT_MATCH);

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account, user_account);

}

