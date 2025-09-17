//
// Created by jeremiah on 8/20/22.
//
#include <utility_general_functions.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include <chat_rooms_objects.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include <server_initialization_functions.h>

#include <create_chat_room_helper.h>
#include <chat_stream_container_object.h>
#include <send_messages_implementation.h>
#include <generate_random_messages.h>
#include <chat_stream_container.h>
#include <chat_room_message_keys.h>

#include "build_match_made_chat_room.h"
#include "change_stream_utility.h"
#include "generate_randoms.h"
#include "chat_change_stream_helper_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ChatChangeStreamHelperFunctions : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

class IterateAndSendMessagesToTargetUsersTests : public ::testing::Test {
protected:

    const std::string user_account_oid = bsoncxx::oid{}.to_string();
    const std::string chat_room_id = generateRandomChatRoomId();

    ChatStreamContainerObject user_chat_stream_container;

    grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>* sending_mock_rw =
            dynamic_cast<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(user_chat_stream_container.responder_.get());

    void SetUp() override {
        user_chat_stream_container.initialization_complete = true;
        user_chat_stream_container.current_user_account_oid_str = user_account_oid;

        user_open_chat_streams.insert(
                user_account_oid,
                &user_chat_stream_container
        );
    }

    void TearDown() override {
    }
};

class SendPreviouslyStoredMessagesWithDifferentUserJoinedTests : public ::testing::Test {
protected:

    const std::string user_account_oid = bsoncxx::oid{}.to_string();
    const std::string chat_room_id = generateRandomChatRoomId();

    ChatStreamContainerObject user_chat_stream_container;

    std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object;

    void SetUp() override {
        user_chat_stream_container.initialization_complete = true;
        user_chat_stream_container.current_user_account_oid_str = user_account_oid;

        user_open_chat_streams.insert(
                user_account_oid,
                &user_chat_stream_container
        );

        stream_container_object = user_open_chat_streams.find(user_account_oid);
    }

    void TearDown() override {
    }
};

TEST_F(ChatChangeStreamHelperFunctions, removeCachedMessagesOverMaxBytes_notOverMaxSize) {

    const std::string chat_room_id_one = generateRandomChatRoomId();
    const std::string chat_room_id_two = generateRandomChatRoomId();

    std::deque<MessageWaitingToBeSent> chat_room_one_messages;

    chat_room_one_messages.emplace_back(
        chat_room_id_one,
        generateUUID(),
        MessageSpecifics::MessageBodyCase::kUserKickedMessage,
        std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
        std::chrono::milliseconds{123},
        std::chrono::milliseconds{456},
        std::vector<MessageTarget>{}
        );

    std::deque<MessageWaitingToBeSent> chat_room_two_messages;

    chat_room_two_messages.emplace_back(
            chat_room_id_two,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{123},
            std::chrono::milliseconds{456},
            std::vector<MessageTarget>{}
    );

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    cached_messages.insert({std::string(chat_room_id_one), std::move(chat_room_one_messages)});
    cached_messages.insert({std::string(chat_room_id_two), std::move(chat_room_two_messages)});

    const size_t original_cached_messages_size_in_bytes = chat_change_stream_values::MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES;
    size_t cached_messages_size_in_bytes = original_cached_messages_size_in_bytes;

    removeCachedMessagesOverMaxBytes(
            cached_messages,
            cached_messages_size_in_bytes
    );

    EXPECT_EQ(original_cached_messages_size_in_bytes, cached_messages_size_in_bytes);

    ASSERT_EQ(cached_messages.size(), 2);
    for(const auto& x : cached_messages) {
        EXPECT_EQ(x.second.size(), 1);
    }
}

TEST_F(ChatChangeStreamHelperFunctions, removeCachedMessagesOverMaxBytes_popSingleMessage) {
    const std::string chat_room_id_one = generateRandomChatRoomId();
    const std::string chat_room_id_two = generateRandomChatRoomId();

    const std::chrono::milliseconds first_timestamp_stored{rand() % 1000 + 10};
    const std::chrono::milliseconds second_timestamp_stored = first_timestamp_stored + std::chrono::milliseconds{1};

    std::deque<MessageWaitingToBeSent> chat_room_one_messages;

    chat_room_one_messages.emplace_back(
            chat_room_id_one,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            first_timestamp_stored,
            std::chrono::milliseconds{456},
            std::vector<MessageTarget>{}
    );

    //size must be large enough that when removed it goes under the limit
    chat_room_one_messages.back().current_message_size = chat_change_stream_values::MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES * 1/10 + 1;

    std::deque<MessageWaitingToBeSent> chat_room_two_messages;

    chat_room_two_messages.emplace_back(
            chat_room_id_two,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            second_timestamp_stored,
            std::chrono::milliseconds{456},
            std::vector<MessageTarget>{}
    );

    const size_t original_cached_messages_size_in_bytes = chat_change_stream_values::MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES + 1;
    size_t generated_cached_messages_size_in_bytes = original_cached_messages_size_in_bytes - chat_room_one_messages.back().current_message_size - chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
    size_t cached_messages_size_in_bytes = original_cached_messages_size_in_bytes;

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    cached_messages.insert({std::string(chat_room_id_one), std::move(chat_room_one_messages)});
    cached_messages.insert({std::string(chat_room_id_two), std::move(chat_room_two_messages)});

    removeCachedMessagesOverMaxBytes(
            cached_messages,
            cached_messages_size_in_bytes
    );

    EXPECT_EQ(generated_cached_messages_size_in_bytes, cached_messages_size_in_bytes);

    ASSERT_EQ(cached_messages.size(), 1);
    for(const auto& x : cached_messages) {
        EXPECT_EQ(x.first, chat_room_id_two);
        EXPECT_EQ(x.second.size(), 1);
    }
}

TEST_F(ChatChangeStreamHelperFunctions, removeCachedMessagesOverMaxBytes_popAllMessages) {
    const std::string chat_room_id_one = generateRandomChatRoomId();
    const std::string chat_room_id_two = generateRandomChatRoomId();

    const std::chrono::milliseconds first_timestamp_stored{rand() % 1000 + 10};
    const std::chrono::milliseconds second_timestamp_stored = first_timestamp_stored + std::chrono::milliseconds{1};

    std::deque<MessageWaitingToBeSent> chat_room_one_messages;

    chat_room_one_messages.emplace_back(
            chat_room_id_one,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            first_timestamp_stored,
            std::chrono::milliseconds{456},
            std::vector<MessageTarget>{}
    );

    std::deque<MessageWaitingToBeSent> chat_room_two_messages;

    chat_room_two_messages.emplace_back(
            chat_room_id_two,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            second_timestamp_stored,
            std::chrono::milliseconds{456},
            std::vector<MessageTarget>{}
    );

    const size_t original_cached_messages_size_in_bytes = chat_change_stream_values::MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES + 1;
    size_t generated_cached_messages_size_in_bytes = original_cached_messages_size_in_bytes - chat_room_one_messages.back().current_message_size - chat_room_two_messages.back().current_message_size - 2 * chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
    size_t cached_messages_size_in_bytes = original_cached_messages_size_in_bytes;

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    cached_messages.insert({std::string(chat_room_id_one), std::move(chat_room_one_messages)});
    cached_messages.insert({std::string(chat_room_id_two), std::move(chat_room_two_messages)});

    removeCachedMessagesOverMaxBytes(
            cached_messages,
            cached_messages_size_in_bytes
    );

    EXPECT_EQ(generated_cached_messages_size_in_bytes, cached_messages_size_in_bytes);

    EXPECT_TRUE(cached_messages.empty());
}

TEST_F(ChatChangeStreamHelperFunctions, removeCachedMessagesOverMaxTime_popNoMessages) {
    const std::string chat_room_id_one = generateRandomChatRoomId();
    const std::string chat_room_id_two = generateRandomChatRoomId();

    const std::chrono::milliseconds synthetic_current_timestamp = std::chrono::milliseconds{rand() % 10000  + 1} + chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES;

    const std::chrono::milliseconds first_time_received = synthetic_current_timestamp;
    const std::chrono::milliseconds second_time_received = synthetic_current_timestamp - chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES;

    std::deque<MessageWaitingToBeSent> chat_room_one_messages;

    chat_room_one_messages.emplace_back(
            chat_room_id_one,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{456},
            first_time_received,
            std::vector<MessageTarget>{}
    );

    std::deque<MessageWaitingToBeSent> chat_room_two_messages;

    chat_room_two_messages.emplace_back(
            chat_room_id_two,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{456},
            second_time_received,
            std::vector<MessageTarget>{}
    );

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;
    const size_t original_cached_messages_size_in_bytes = sizeof(cached_messages) + chat_room_one_messages.back().current_message_size + chat_room_two_messages.back().current_message_size + 2 * chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
    size_t cached_messages_size_in_bytes = original_cached_messages_size_in_bytes;

    cached_messages.insert({std::string(chat_room_id_one), std::move(chat_room_one_messages)});
    cached_messages.insert({std::string(chat_room_id_two), std::move(chat_room_two_messages)});

    removeCachedMessagesOverMaxTime(
            cached_messages,
            cached_messages_size_in_bytes,
            synthetic_current_timestamp
    );

    EXPECT_EQ(original_cached_messages_size_in_bytes, cached_messages_size_in_bytes);

    ASSERT_EQ(cached_messages.size(), 2);
    for(const auto& x : cached_messages) {
        EXPECT_EQ(x.second.size(), 1);
    }
}

TEST_F(ChatChangeStreamHelperFunctions, removeCachedMessagesOverMaxTime_popSingleMessage) {
    const std::string chat_room_id_one = generateRandomChatRoomId();
    const std::string chat_room_id_two = generateRandomChatRoomId();

    const std::chrono::milliseconds synthetic_current_timestamp = std::chrono::milliseconds{rand() % 10000  + 1} + chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES;

    const std::chrono::milliseconds first_time_received = synthetic_current_timestamp;
    const std::chrono::milliseconds second_time_received = synthetic_current_timestamp - chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES - std::chrono::milliseconds{1};

    std::deque<MessageWaitingToBeSent> chat_room_one_messages;

    chat_room_one_messages.emplace_back(
            chat_room_id_one,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{456},
            first_time_received,
            std::vector<MessageTarget>{}
    );

    std::deque<MessageWaitingToBeSent> chat_room_two_messages;

    chat_room_two_messages.emplace_back(
            chat_room_id_two,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{456},
            second_time_received,
            std::vector<MessageTarget>{}
    );

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;
    const size_t original_cached_messages_size_in_bytes = sizeof(cached_messages) + chat_room_one_messages.back().current_message_size + chat_room_two_messages.back().current_message_size + 2 * chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
    const size_t expected_messages_size_in_bytes = original_cached_messages_size_in_bytes - chat_room_one_messages.back().current_message_size - chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
    size_t cached_messages_size_in_bytes = original_cached_messages_size_in_bytes;

    cached_messages.insert({std::string(chat_room_id_one), std::move(chat_room_one_messages)});
    cached_messages.insert({std::string(chat_room_id_two), std::move(chat_room_two_messages)});

    removeCachedMessagesOverMaxTime(
            cached_messages,
            cached_messages_size_in_bytes,
            synthetic_current_timestamp
    );

    EXPECT_EQ(expected_messages_size_in_bytes, cached_messages_size_in_bytes);

    ASSERT_EQ(cached_messages.size(), 1);
    for(const auto& x : cached_messages) {
        EXPECT_EQ(x.first, chat_room_id_one);
        EXPECT_EQ(x.second.size(), 1);
    }
}

TEST_F(ChatChangeStreamHelperFunctions, removeCachedMessagesOverMaxTime_popAllMessages) {
    const std::string chat_room_id_one = generateRandomChatRoomId();
    const std::string chat_room_id_two = generateRandomChatRoomId();

    const std::chrono::milliseconds synthetic_current_timestamp = std::chrono::milliseconds{rand() % 10000  + 1} + chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES;

    const std::chrono::milliseconds first_time_received = synthetic_current_timestamp - chat_change_stream_values::MAX_TIME_TO_CACHE_MESSAGES - std::chrono::milliseconds{1};
    const std::chrono::milliseconds second_time_received = first_time_received - std::chrono::milliseconds{1000000};

    std::deque<MessageWaitingToBeSent> chat_room_one_messages;

    chat_room_one_messages.emplace_back(
            chat_room_id_one,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{456},
            first_time_received,
            std::vector<MessageTarget>{}
    );

    std::deque<MessageWaitingToBeSent> chat_room_two_messages;

    chat_room_two_messages.emplace_back(
            chat_room_id_two,
            generateUUID(),
            MessageSpecifics::MessageBodyCase::kUserKickedMessage,
            std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
            std::chrono::milliseconds{456},
            second_time_received,
            std::vector<MessageTarget>{}
    );

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;
    const size_t original_cached_messages_size_in_bytes = sizeof(cached_messages) + chat_room_one_messages.back().current_message_size + chat_room_two_messages.back().current_message_size + 2 * chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
    const size_t expected_messages_size_in_bytes =  sizeof(cached_messages);
    size_t cached_messages_size_in_bytes = original_cached_messages_size_in_bytes;

    cached_messages.insert({std::string(chat_room_id_one), std::move(chat_room_one_messages)});
    cached_messages.insert({std::string(chat_room_id_two), std::move(chat_room_two_messages)});

    removeCachedMessagesOverMaxTime(
            cached_messages,
            cached_messages_size_in_bytes,
            synthetic_current_timestamp
    );

    EXPECT_EQ(expected_messages_size_in_bytes, cached_messages_size_in_bytes);

    EXPECT_TRUE(cached_messages.empty());
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, noMessagesToRequest) {

    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if (!reply_vector.empty()) {
                EXPECT_TRUE(reply_vector.front()->has_return_new_chat_message());
                if (reply_vector.front()->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list().size(), 1);
                    if (reply_vector.front()->return_new_chat_message().messages_list().size() == 1) {
                        //sends back kDifferentUserJoined
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(0).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, requestNormalMessage) {
    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    //first will not be sent back, second & third will
    const long first_timestamp = response_timestamp - chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() - 1;
    const long second_timestamp = response_timestamp - chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count();
    const long third_timestamp = response_timestamp;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    auto insert_ptr = cached_messages.insert({chat_room_id, std::deque<MessageWaitingToBeSent>()});

    auto text_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto first_text_message = text_response->mutable_return_new_chat_message()->add_messages_list();
    first_text_message->set_sent_by_account_id(user_account_oid);
    first_text_message->set_message_uuid(generateUUID());
    first_text_message->set_timestamp_stored(first_timestamp);
    first_text_message->mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            gen_random_alpha_numeric_string(rand() % 100 + 10)
    );

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            first_text_message->message_uuid(),
            first_text_message->message().message_specifics().message_body_case(),
            std::move(text_response),
            std::chrono::milliseconds{first_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    text_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto second_text_message = text_response->mutable_return_new_chat_message()->add_messages_list();
    second_text_message->set_sent_by_account_id(user_account_oid);
    second_text_message->set_message_uuid(generateUUID());
    second_text_message->set_timestamp_stored(second_timestamp);
    second_text_message->mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            gen_random_alpha_numeric_string(rand() % 100 + 10)
    );

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            second_text_message->message_uuid(),
            second_text_message->message().message_specifics().message_body_case(),
            std::move(text_response),
            std::chrono::milliseconds{second_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    text_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto third_text_message = text_response->mutable_return_new_chat_message()->add_messages_list();
    third_text_message->set_sent_by_account_id(user_account_oid);
    third_text_message->set_message_uuid(generateUUID());
    third_text_message->set_timestamp_stored(third_timestamp);
    third_text_message->mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            gen_random_alpha_numeric_string(rand() % 100 + 10)
    );

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            third_text_message->message_uuid(),
            third_text_message->message().message_specifics().message_body_case(),
            std::move(text_response),
            std::chrono::milliseconds{third_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also, it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if (!reply_vector.empty()) {
                EXPECT_TRUE(reply_vector.front()->has_return_new_chat_message());
                if (reply_vector.front()->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list().size(), 3);
                    if (reply_vector.front()->return_new_chat_message().messages_list().size() == 3) {
                        //skips first text messages
                        //sends back second and third text messages
                        //sends back kDifferentUserJoined
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kTextMessage
                        );
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(0).message_uuid(),
                                second_text_message->message_uuid()
                        );
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(1).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kTextMessage
                        );
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(1).message_uuid(),
                                third_text_message->message_uuid()
                        );
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(2).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector.front()->return_new_chat_message().messages_list(2).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, requestkDifferentUserJoinedMessage) {

    bool clear_database_success = clearDatabaseAndGlobalsForTesting();
    ASSERT_EQ(clear_database_success, true);

    bsoncxx::oid new_generated_account_oid = insertRandomAccounts(1, 0);

    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    //both will be sent back
    const long join_message_timestamp = response_timestamp;
    const long text_message_timestamp = join_message_timestamp - 1;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    auto insert_ptr = cached_messages.insert({chat_room_id, std::deque<MessageWaitingToBeSent>()});

    auto text_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto text_message = text_response->mutable_return_new_chat_message()->add_messages_list();
    text_message->set_sent_by_account_id(new_generated_account_oid.to_string());
    text_message->set_message_uuid(generateUUID());
    text_message->set_timestamp_stored(text_message_timestamp);
    text_message->mutable_message()->mutable_message_specifics()->mutable_text_message()->set_message_text(
            gen_random_alpha_numeric_string(rand() % 100 + 10)
    );

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            text_message->message_uuid(),
            text_message->message().message_specifics().message_body_case(),
            std::move(text_response),
            std::chrono::milliseconds{text_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    auto user_joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto user_joined_message = user_joined_response->mutable_return_new_chat_message()->add_messages_list();
    user_joined_message->set_sent_by_account_id(new_generated_account_oid.to_string());
    user_joined_message->set_message_uuid(generateUUID());
    user_joined_message->set_timestamp_stored(join_message_timestamp);
    user_joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            user_joined_message->message_uuid(),
            user_joined_message->message().message_specifics().message_body_case(),
            std::move(user_joined_response),
            std::chrono::milliseconds{join_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_joined_message->sent_by_account_id()}}
    );

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also, it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    //NOTE: There are three elements here because the first value will contain all three. It will then
    // split the reply_vector into three even pieces. It will send the first value and store the next
    // two values inside writes_waiting_to_process to be sent afterwards. It will then pop the
    // first element.
    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 3);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 3) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 3);
            if (reply_vector.size() == 3) {
                //sends back kTextMessage
                //sends back other user kDifferentUserJoined
                //sends back current user kDifferentUserJoined

                EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
                if (reply_vector[0]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list().size(), 1);
                    if (!reply_vector[0]->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kTextMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message_uuid(),
                                text_message->message_uuid()
                        );
                    }
                }

                EXPECT_TRUE(reply_vector[1]->has_return_new_chat_message());
                if (reply_vector[1]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[1]->return_new_chat_message().messages_list().size(), 1);
                    if (!reply_vector[1]->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(
                                reply_vector[1]->return_new_chat_message().messages_list(0).message_uuid(),
                                user_joined_message->message_uuid()
                        );
                        ASSERT_EQ(
                                reply_vector[1]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );

                        //thumbnail and age should both be non-default
                        EXPECT_NE(
                                reply_vector[1]->return_new_chat_message().messages_list(0).message().message_specifics().different_user_joined_message().member_info().user_info().account_thumbnail(),
                                ""
                        );
                        EXPECT_NE(
                                reply_vector[1]->return_new_chat_message().messages_list(0).message().message_specifics().different_user_joined_message().member_info().user_info().age(),
                                0
                        );
                    }
                }

                EXPECT_TRUE(reply_vector[2]->has_return_new_chat_message());
                if (reply_vector[2]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[2]->return_new_chat_message().messages_list().size(), 1);
                    if (!reply_vector[2]->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(
                                reply_vector[2]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[2]->return_new_chat_message().messages_list(0).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }

    clearDatabaseAndGlobalsForTesting();
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, inviteForCurrentUserIsPassedThrough) {

    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    //will be sent back
    const long invite_message_timestamp = response_timestamp;
    const long second_invite_message_timestamp = response_timestamp + 1;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    auto insert_ptr = cached_messages.insert({chat_room_id, std::deque<MessageWaitingToBeSent>()});

    auto user_invited_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto user_invited_message = user_invited_response->mutable_return_new_chat_message()->add_messages_list();
    user_invited_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    user_invited_message->set_message_uuid(generateUUID());
    user_invited_message->set_timestamp_stored(invite_message_timestamp);
    user_invited_message->mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(user_account_oid);

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            user_invited_message->message_uuid(),
            user_invited_message->message().message_specifics().message_body_case(),
            std::move(user_invited_response),
            std::chrono::milliseconds{invite_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    auto second_user_invited_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto second_user_invited_message = second_user_invited_response->mutable_return_new_chat_message()->add_messages_list();
    second_user_invited_message->set_sent_by_account_id(user_account_oid);
    second_user_invited_message->set_message_uuid(generateUUID());
    second_user_invited_message->set_timestamp_stored(second_invite_message_timestamp);
    second_user_invited_message->mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(bsoncxx::oid{}.to_string());

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            second_user_invited_message->message_uuid(),
            second_user_invited_message->message().message_specifics().message_body_case(),
            std::move(second_user_invited_response),
            std::chrono::milliseconds{second_invite_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also, it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if (reply_vector.size() == 1) {
                //sends back kInviteMessage
                //sends back current user kDifferentUserJoined

                EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
                if (reply_vector[0]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list().size(), 3);
                    if (reply_vector[0]->return_new_chat_message().messages_list().size() == 3) {
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kInviteMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message_uuid(),
                                user_invited_message->message_uuid()
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(1).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kInviteMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(1).message_uuid(),
                                second_user_invited_message->message_uuid()
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(2).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(2).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, inviteForOtherUserDoesNotPassThrough) {
    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    //will be sent back
    const long invite_message_timestamp = response_timestamp;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    auto insert_ptr = cached_messages.insert({chat_room_id, std::deque<MessageWaitingToBeSent>()});

    auto user_invited_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto user_invited_message = user_invited_response->mutable_return_new_chat_message()->add_messages_list();
    user_invited_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    user_invited_message->set_message_uuid(generateUUID());
    user_invited_message->set_timestamp_stored(invite_message_timestamp);
    user_invited_message->mutable_message()->mutable_message_specifics()->mutable_invite_message()->set_invited_user_account_oid(bsoncxx::oid{}.to_string());

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            user_invited_message->message_uuid(),
            user_invited_message->message().message_specifics().message_body_case(),
            std::move(user_invited_response),
            std::chrono::milliseconds{invite_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also, it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if (reply_vector.size() == 1) {
                //sends back kInviteMessage
                //sends back current user kDifferentUserJoined

                EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
                if (reply_vector[0]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list().size(), 1);
                    if (reply_vector[0]->return_new_chat_message().messages_list().size() == 1) {
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForCurrentUserIsPassedThrough) {

    bool clear_database_success = clearDatabaseAndGlobalsForTesting();
    ASSERT_EQ(clear_database_success, true);

    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    //will be sent back
    const long delete_message_timestamp = response_timestamp;
    const long second_delete_message_timestamp = response_timestamp + 1;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    auto insert_ptr = cached_messages.insert({chat_room_id, std::deque<MessageWaitingToBeSent>()});

    auto user_deleted_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    //deleted for all users
    auto user_deleted_message = user_deleted_response->mutable_return_new_chat_message()->add_messages_list();
    user_deleted_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    user_deleted_message->set_message_uuid(generateUUID());
    user_deleted_message->set_timestamp_stored(delete_message_timestamp);
    user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_delete_type(DeleteType::DELETE_FOR_ALL_USERS);
    user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_message_uuid(generateUUID());

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            user_deleted_message->message_uuid(),
            user_deleted_message->message().message_specifics().message_body_case(),
            std::move(user_deleted_response),
            std::chrono::milliseconds{delete_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    auto second_user_deleted_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    //deleted for current user only
    auto second_user_deleted_message = second_user_deleted_response->mutable_return_new_chat_message()->add_messages_list();
    second_user_deleted_message->set_sent_by_account_id(user_account_oid);
    second_user_deleted_message->set_message_uuid(generateUUID());
    second_user_deleted_message->set_timestamp_stored(second_delete_message_timestamp);
    second_user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_delete_type(DeleteType::DELETE_FOR_SINGLE_USER);
    second_user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_message_uuid(generateUUID());

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            second_user_deleted_message->message_uuid(),
            second_user_deleted_message->message().message_specifics().message_body_case(),
            std::move(second_user_deleted_response),
            std::chrono::milliseconds{second_delete_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also, it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if (reply_vector.size() == 1) {
                EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
                if (reply_vector[0]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list().size(), 3);
                    if (reply_vector[0]->return_new_chat_message().messages_list().size() == 3) {
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDeletedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message_uuid(),
                                user_deleted_message->message_uuid()
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(1).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDeletedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(1).message_uuid(),
                                second_user_deleted_message->message_uuid()
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(2).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(2).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }

    clearDatabaseAndGlobalsForTesting();
}

TEST_F(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForOtherUserDoesNotPassThrough) {

    bool clear_database_success = clearDatabaseAndGlobalsForTesting();
    ASSERT_EQ(clear_database_success, true);

    const long response_timestamp = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count() + rand() % 1000;

    //will be sent back
    const long delete_message_timestamp = response_timestamp;
    const long second_delete_message_timestamp = response_timestamp + 1;

    ChatMessageToClient response_message;
    response_message.set_message_uuid(generateUUID());
    response_message.set_sent_by_account_id(user_account_oid);
    response_message.set_timestamp_stored(response_timestamp);

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;

    auto insert_ptr = cached_messages.insert({chat_room_id, std::deque<MessageWaitingToBeSent>()});

    auto user_deleted_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    //invalid delete type
    auto user_deleted_message = user_deleted_response->mutable_return_new_chat_message()->add_messages_list();
    user_deleted_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    user_deleted_message->set_message_uuid(generateUUID());
    user_deleted_message->set_timestamp_stored(delete_message_timestamp);
    user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_delete_type(DeleteType::DELETE_TYPE_NOT_SET);

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            user_deleted_message->message_uuid(),
            user_deleted_message->message().message_specifics().message_body_case(),
            std::move(user_deleted_response),
            std::chrono::milliseconds{delete_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    auto second_user_deleted_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    //deleted for different user
    auto second_user_deleted_message = second_user_deleted_response->mutable_return_new_chat_message()->add_messages_list();
    second_user_deleted_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    second_user_deleted_message->set_message_uuid(generateUUID());
    second_user_deleted_message->set_timestamp_stored(second_delete_message_timestamp);
    second_user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_delete_type(DeleteType::DELETE_FOR_SINGLE_USER);
    second_user_deleted_message->mutable_message()->mutable_message_specifics()->mutable_deleted_message()->set_message_uuid(generateUUID());

    insert_ptr.first->second.emplace_back(
            chat_room_id,
            second_user_deleted_message->message_uuid(),
            second_user_deleted_message->message().message_specifics().message_body_case(),
            std::move(second_user_deleted_response),
            std::chrono::milliseconds{second_delete_message_timestamp},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{}
    );

    sendPreviouslyStoredMessagesWithDifferentUserJoined(
            cached_messages,
            chat_room_id,
            &response_message,
            stream_container_object
    );

    //While the MockServerAsyncReaderWriter is not accessed, another thread will access
    // writes_waiting_to_process. Let the other threads access complete before moving
    // forward.
    //Also, it will attempt to continue after the object is deconstructed.
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    //NOTE: Write() command will not be repeatedly called w/o the async server running behind it. This means
    // the MockServerAsyncReaderWriter will only ever have a single value written to it so just checking
    // the writes_waiting_to_process vector.

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if (write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if (reply_vector.size() == 1) {
                EXPECT_TRUE(reply_vector[0]->has_return_new_chat_message());
                if (reply_vector[0]->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector[0]->return_new_chat_message().messages_list().size(), 1);
                    if (reply_vector[0]->return_new_chat_message().messages_list().size() == 1) {
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(),
                                MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage
                        );
                        EXPECT_EQ(
                                reply_vector[0]->return_new_chat_message().messages_list(0).message_uuid(),
                                response_message.message_uuid()
                        );
                    }
                }
            }
        }
    }

    clearDatabaseAndGlobalsForTesting();
}

TEST_F(IterateAndSendMessagesToTargetUsersTests, notALeaveOrJoinMessage) {
    auto joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto joined_message = joined_response->mutable_return_new_chat_message()->add_messages_list();
    joined_message->set_sent_by_account_id(user_account_oid);
    joined_message->set_message_uuid(generateUUID());
    joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    MessageWaitingToBeSent message_info{
            chat_room_id,
            joined_message->message_uuid(),
            joined_message->message().message_specifics().message_body_case(),
            std::move(joined_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    };

    std::deque<MessageWaitingToBeSent> messages_reference;

    auto kicked_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto kicked_message = kicked_response->mutable_return_new_chat_message()->add_messages_list();
    kicked_message->set_sent_by_account_id(user_account_oid);
    kicked_message->set_message_uuid(generateUUID());
    kicked_message->mutable_message()->mutable_message_specifics()->mutable_different_user_left_message();

    messages_reference.emplace_back(
        chat_room_id,
        kicked_message->message_uuid(),
        kicked_message->message().message_specifics().message_body_case(),
        std::move(kicked_response),
        std::chrono::milliseconds{456},
        std::chrono::milliseconds{123},
        std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_REMOVE, user_account_oid}}
    );

    auto text_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto text_message = text_response->mutable_return_new_chat_message()->add_messages_list();
    text_message->set_sent_by_account_id(user_account_oid);
    text_message->set_message_uuid(generateUUID());
    text_message->mutable_message()->mutable_message_specifics()->mutable_text_message();

    messages_reference.emplace_back(
        chat_room_id,
        text_message->message_uuid(),
        text_message->message().message_specifics().message_body_case(),
        std::move(text_response),
        std::chrono::milliseconds{456},
        std::chrono::milliseconds{123},
        std::vector<MessageTarget>{}
    );

    //This should be after the kDifferentUserLeftMessage and so it should not be checked.
    auto iterator_pos_to_insert_at = messages_reference.begin() + 1;

    iterateAndSendMessagesToTargetUsers(
            message_info,
            messages_reference,
            iterator_pos_to_insert_at
    );

    //Sleep while Write() occurs, the thread pool must run it (shouldn't be sent here, but just in case something breaks).
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 0);
    EXPECT_EQ(sending_mock_rw->write_params.size(), 0);
}

TEST_F(IterateAndSendMessagesToTargetUsersTests, joinMessageInFront) {

    auto joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto joined_message = joined_response->mutable_return_new_chat_message()->add_messages_list();
    joined_message->set_sent_by_account_id(user_account_oid);
    joined_message->set_message_uuid(generateUUID());
    joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    MessageWaitingToBeSent message_info{
            chat_room_id,
            joined_message->message_uuid(),
            joined_message->message().message_specifics().message_body_case(),
            std::move(joined_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    };

    std::deque<MessageWaitingToBeSent> messages_reference;

    auto kicked_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto kicked_message = kicked_response->mutable_return_new_chat_message()->add_messages_list();
    kicked_message->set_sent_by_account_id(user_account_oid);
    kicked_message->set_message_uuid(generateUUID());
    kicked_message->mutable_message()->mutable_message_specifics()->mutable_different_user_left_message();

    messages_reference.emplace_back(
            chat_room_id,
            kicked_message->message_uuid(),
            kicked_message->message().message_specifics().message_body_case(),
            std::move(kicked_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_REMOVE, user_account_oid}}
    );

    auto second_joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto second_joined_message = second_joined_response->mutable_return_new_chat_message()->add_messages_list();
    second_joined_message->set_sent_by_account_id(user_account_oid);
    second_joined_message->set_message_uuid(generateUUID());
    second_joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    messages_reference.emplace_back(
            chat_room_id,
            second_joined_message->message_uuid(),
            second_joined_message->message().message_specifics().message_body_case(),
            std::move(second_joined_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    );

    //This should be after the kDifferentUserLeftMessage and so it should not be checked.
    auto iterator_pos_to_insert_at = messages_reference.begin() + 1;

    iterateAndSendMessagesToTargetUsers(
            message_info,
            messages_reference,
            iterator_pos_to_insert_at
    );

    //Sleep while Write() occurs, the thread pool must run it (shouldn't be sent here, but just in case something breaks).
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    EXPECT_EQ(sending_mock_rw->write_params.size(), 1);

    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                    std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                    PushedToQueueFromLocation
                >
            >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if(write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if(!reply_vector.empty()) {
                EXPECT_TRUE(reply_vector.front()->has_return_new_chat_message());
                if(reply_vector.front()->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list().size(), 1);
                    if(!reply_vector.front()->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage);
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message_uuid(), second_joined_message->message_uuid());
                    }
                }
            }
        }

        //message will be converted from kDifferentUserJoinedMessage sent by the current user into a kNewUpdateTimeMessage
        EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
        if(!sending_mock_rw->write_params.empty()) {
            EXPECT_TRUE(sending_mock_rw->write_params[0].msg.has_return_new_chat_message());
            if(sending_mock_rw->write_params[0].msg.has_return_new_chat_message()) {
                EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().size(), 1);
                if(!sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kNewUpdateTimeMessage);
                    EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message_uuid(), "");
                }
            }
        }
    }
}

TEST_F(IterateAndSendMessagesToTargetUsersTests, leaveMessageInFront) {
    auto joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto joined_message = joined_response->mutable_return_new_chat_message()->add_messages_list();
    joined_message->set_sent_by_account_id(user_account_oid);
    joined_message->set_message_uuid(generateUUID());
    joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    MessageWaitingToBeSent message_info{
            chat_room_id,
            joined_message->message_uuid(),
            joined_message->message().message_specifics().message_body_case(),
            std::move(joined_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    };

    std::deque<MessageWaitingToBeSent> messages_reference;

    auto kicked_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto kicked_message = kicked_response->mutable_return_new_chat_message()->add_messages_list();
    kicked_message->set_sent_by_account_id(user_account_oid);
    kicked_message->set_message_uuid(generateUUID());
    kicked_message->mutable_message()->mutable_message_specifics()->mutable_different_user_left_message();

    messages_reference.emplace_back(
            chat_room_id,
            kicked_message->message_uuid(),
            kicked_message->message().message_specifics().message_body_case(),
            std::move(kicked_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_REMOVE, user_account_oid}}
    );

    auto banned_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto banned_message = banned_response->mutable_return_new_chat_message()->add_messages_list();
    banned_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    banned_message->set_message_uuid(generateUUID());
    banned_message->mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(user_account_oid);

    messages_reference.emplace_back(
            chat_room_id,
            banned_message->message_uuid(),
            banned_message->message().message_specifics().message_body_case(),
            std::move(banned_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    );

    //This should be after the kDifferentUserLeftMessage and so it should not be checked.
    auto iterator_pos_to_insert_at = messages_reference.begin() + 1;

    iterateAndSendMessagesToTargetUsers(
            message_info,
            messages_reference,
            iterator_pos_to_insert_at
    );

    //Sleep while Write() occurs, the thread pool must run it (shouldn't be sent here, but just in case something breaks).
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 1);
    EXPECT_EQ(sending_mock_rw->write_params.size(), 1);

    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 1) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if(write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if(!reply_vector.empty()) {
                EXPECT_TRUE(reply_vector.front()->has_return_new_chat_message());
                if(reply_vector.front()->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list().size(), 1);
                    if(!reply_vector.front()->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kUserBannedMessage);
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message_uuid(), banned_message->message_uuid());
                    }
                }
            }
        }

        //message will be converted from kDifferentUserJoinedMessage sent by the current user into a kNewUpdateTimeMessage
        EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
        if(!sending_mock_rw->write_params.empty()) {
            EXPECT_TRUE(sending_mock_rw->write_params[0].msg.has_return_new_chat_message());
            if(sending_mock_rw->write_params[0].msg.has_return_new_chat_message()) {
                EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().size(), 1);
                if(!sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kUserBannedMessage);
                    EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message_uuid(), banned_message->message_uuid());
                }
            }
        }
    }
}

TEST_F(IterateAndSendMessagesToTargetUsersTests, multipleMessageInFront) {

    auto joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto joined_message = joined_response->mutable_return_new_chat_message()->add_messages_list();
    joined_message->set_sent_by_account_id(user_account_oid);
    joined_message->set_message_uuid(generateUUID());
    joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    MessageWaitingToBeSent message_info{
            chat_room_id,
            joined_message->message_uuid(),
            joined_message->message().message_specifics().message_body_case(),
            std::move(joined_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    };

    std::deque<MessageWaitingToBeSent> messages_reference;

    auto kicked_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto kicked_message = kicked_response->mutable_return_new_chat_message()->add_messages_list();
    kicked_message->set_sent_by_account_id(bsoncxx::oid{}.to_string());
    kicked_message->set_message_uuid(generateUUID());
    kicked_message->mutable_message()->mutable_message_specifics()->mutable_user_kicked_message()->set_kicked_account_oid(user_account_oid);

    messages_reference.emplace_back(
            chat_room_id,
            kicked_message->message_uuid(),
            kicked_message->message().message_specifics().message_body_case(),
            std::move(kicked_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_REMOVE, user_account_oid}}
    );

    auto second_joined_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    auto second_joined_message = second_joined_response->mutable_return_new_chat_message()->add_messages_list();
    second_joined_message->set_sent_by_account_id(user_account_oid);
    second_joined_message->set_message_uuid(generateUUID());
    second_joined_message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

    messages_reference.emplace_back(
            chat_room_id,
            second_joined_message->message_uuid(),
            second_joined_message->message().message_specifics().message_body_case(),
            std::move(second_joined_response),
            std::chrono::milliseconds{456},
            std::chrono::milliseconds{123},
            std::vector<MessageTarget>{MessageTarget{AddOrRemove::MESSAGE_TARGET_ADD, user_account_oid}}
    );

    auto iterator_pos_to_insert_at = messages_reference.begin();

    iterateAndSendMessagesToTargetUsers(
            message_info,
            messages_reference,
            iterator_pos_to_insert_at
    );

    //Sleep while Write() occurs, the thread pool must run it (shouldn't be sent here, but just in case something breaks).
    std::this_thread::sleep_for(std::chrono::milliseconds{200});

    EXPECT_EQ(user_chat_stream_container.writes_waiting_to_process.size_for_testing(), 2);
    EXPECT_EQ(sending_mock_rw->write_params.size(), 1);

    if(user_chat_stream_container.writes_waiting_to_process.size_for_testing() == 2) {
        auto write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector.front());

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if(write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if(!reply_vector.empty()) {
                EXPECT_TRUE(reply_vector.front()->has_return_new_chat_message());
                if(reply_vector.front()->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list().size(), 1);
                    if(!reply_vector.front()->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kUserKickedMessage);
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message_uuid(), kicked_message->message_uuid());
                    }
                }
            }
        }

        write_from_queue = std::make_shared<
                std::pair<
                        std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation
                >
        >(user_chat_stream_container.writes_waiting_to_process.vector[1]);

        ASSERT_TRUE(write_from_queue);
        ASSERT_TRUE(write_from_queue->first);
        if(write_from_queue && write_from_queue->first) {
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

            write_from_queue->first(reply_vector);

            EXPECT_EQ(reply_vector.size(), 1);
            if(!reply_vector.empty()) {
                EXPECT_TRUE(reply_vector.front()->has_return_new_chat_message());
                if(reply_vector.front()->has_return_new_chat_message()) {
                    EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list().size(), 1);
                    if(!reply_vector.front()->return_new_chat_message().messages_list().empty()) {
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage);
                        EXPECT_EQ(reply_vector.front()->return_new_chat_message().messages_list(0).message_uuid(), second_joined_message->message_uuid());
                    }
                }
            }
        }

        EXPECT_EQ(sending_mock_rw->write_params.size(), 1);
        if(!sending_mock_rw->write_params.empty()) {
            EXPECT_TRUE(sending_mock_rw->write_params[0].msg.has_return_new_chat_message());
            if(sending_mock_rw->write_params[0].msg.has_return_new_chat_message()) {
                EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().size(), 1);
                if(!sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list().empty()) {
                    EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message().message_specifics().message_body_case(), MessageSpecifics::MessageBodyCase::kUserKickedMessage);
                    EXPECT_EQ(sending_mock_rw->write_params[0].msg.return_new_chat_message().messages_list(0).message_uuid(), kicked_message->message_uuid());
                }
            }
        }
    }
}

