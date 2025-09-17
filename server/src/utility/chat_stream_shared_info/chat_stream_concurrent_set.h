//
// Created by jeremiah on 4/7/21.
//
#pragma once

#include <shared_mutex>
#include <utility>

#ifdef LG_TESTING
#include <gtest/gtest_prod.h>
#endif

#include "utility_chat_functions.h"

//NOTE: this needs to be defined inside a separate file because there is an included loop here
// user_open_chat_streams -> chat_room_object -> send_messages_implementation -> chat_stream_shared_info -> user_open_chat_streams

class ChatStreamConcurrentSet {
public:

    //Will upsert a value
    void upsert(const std::string& user_oid, long object_index_value);

    ThreadPoolSuspend upsert_coroutine(const std::string& user_oid, long object_index_value);

    void erase(const std::string& user_oid, long object_index_value);

    ThreadPoolSuspend erase_coroutine(const std::string& user_oid, long object_index_value);

    void iterateAndSendMessageUniqueMessageToSender(
            const std::string& sending_account_oid,
            const std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& standard_function,
            const std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& unique_function
            );

    void iterateAndSendMessageExcludeSender(
            const std::string& sending_account_oid,
            const std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function
            );

    void sendMessageToSpecificUser(
            const std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function,
            const std::string& receiving_user_account_oid
    );

    void sendMessagesToTwoUsers(
            const std::function<void(
                    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& first_outside_function,
            const std::string& first_user_account_oid,
            const std::function<void(
                    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& second_outside_function,
            const std::string& second_user_account_oid
    );

    ChatStreamConcurrentSet() = default;

    explicit ChatStreamConcurrentSet(
            std::string _chat_room_id
            ) : chat_room_id(std::move(_chat_room_id)) {
    }

    ChatStreamConcurrentSet(const ChatStreamConcurrentSet& other)  noexcept {
        map = other.map;
        chat_room_id = other.chat_room_id;
    }

private:

    void upsert_internal(const std::string& user_oid, long object_index_value);

    void erase_internal(const std::string& user_oid, long object_index_value);

    std::string chat_room_id;

    //this map will store the userAccountOID as well as the identifier for the
    // DataObject that was active. The stored long is expected to be larger
    // for objects added later, and smaller for objects added sooner. Can look
    // at user_open_chat_streams.upsert inside chat_stream_container_object.cpp to
    // see how this is handled.
    //key = userAccountOID, value = unique id (ChatStreamObjectContainer.current_index_value is used)
    std::unordered_map<std::string, long> map;

    //The shared mutex has very little value here. The two operations that may take a little while
    // (where the thread must iterate the map) are only called by the chat change stream thread. The
    // other operations where the standard lock (as opposed to the shared lock) must be locked instead
    // are the only ones called from different threads. This means that there are no threads to 'share'
    // the read only functions. So because most of these operations will be quick using a spin lock instead
    // of the standard mutex or shared mutex.
    CoroutineSpinlock spin_lock;

    void sendSingleMessageToUserNoLock(
            const std::function<void(
                    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function,
            const std::string& receiving_user_account_oid
    );

#ifdef LG_TESTING
    FRIEND_TEST(SendMessagesImplementation, insertUserOIDToChatRoomId);
    FRIEND_TEST(SendMessagesImplementation, insertUserOIDToChatRoomId_coroutine);
    FRIEND_TEST(SendMessagesImplementation, eraseUserOIDFromChatRoomId);
    FRIEND_TEST(SendMessagesImplementation, eraseUserOIDFromChatRoomId_coroutine);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_upsert);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_upsert_coroutine);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_erase);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_erase_coroutine);
    FRIEND_TEST(ServerStressTest, small_stress_test);
    FRIEND_TEST(ServerStressTest, joined_left_chat_room_test);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanUpObject);

    FRIEND_TEST(ExtractChatRoomsTests, chatRoomExcludedFromRequest);
    FRIEND_TEST(ExtractChatRoomsTests, extraChatRoomAddedToRequest);
    FRIEND_TEST(ExtractChatRoomsTests, chatRoomProperlyUpdated);
    FRIEND_TEST(ExtractChatRoomsTests, updateObservedTimeOnServer);
#endif
};

