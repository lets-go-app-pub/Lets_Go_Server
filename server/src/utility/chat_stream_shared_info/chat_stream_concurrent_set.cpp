//
// Created by jeremiah on 4/8/21.
//

#include "chat_stream_concurrent_set.h"
#include "user_open_chat_streams.h"
#include "chat_stream_container_object.h"

void ChatStreamConcurrentSet::upsert_internal(
        const std::string& user_oid,
        long object_index_value
) {
    auto result = map.insert(std::make_pair(user_oid, object_index_value));
    //If insert failed (object already existed) AND the passed index is larger, overwrite the
    // previous value.
    if (!result.second && object_index_value >= result.first->second) {
        map[user_oid] = object_index_value;
    }
}

void ChatStreamConcurrentSet::upsert(
        const std::string& user_oid,
        const long object_index_value
) {
    SpinlockWrapper lock(spin_lock);
    upsert_internal(user_oid, object_index_value);
}

ThreadPoolSuspend ChatStreamConcurrentSet::upsert_coroutine(
        const std::string& user_oid,
        const long object_index_value
) {
    SCOPED_SPIN_LOCK(spin_lock);
    upsert_internal(user_oid, object_index_value);
}

void ChatStreamConcurrentSet::erase_internal(
        const std::string& user_oid,
        long object_index_value
) {
    auto iter = map.find(user_oid);
    if (iter != map.end()
        && iter->second <= object_index_value) {
        map.erase(user_oid);
    }
}

void ChatStreamConcurrentSet::erase(
        const std::string& user_oid,
        const long object_index_value
) {
    SpinlockWrapper lock(spin_lock);
    erase_internal(user_oid, object_index_value);
}

ThreadPoolSuspend ChatStreamConcurrentSet::erase_coroutine(
        const std::string& user_oid,
        const long object_index_value
) {
    SCOPED_SPIN_LOCK(spin_lock);
    erase_internal(user_oid, object_index_value);
}

void ChatStreamConcurrentSet::iterateAndSendMessageUniqueMessageToSender(
        const std::string& sending_account_oid,
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& standard_function,
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& unique_function
) {
    SpinlockWrapper lock(spin_lock);
    for (const auto& chatRoomIterator : map) {

        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                chatRoomIterator.first);

        if (stream_container_object != nullptr) {

            if (chatRoomIterator.first != sending_account_oid) { //do not send this to the original sender
                stream_container_object->ptr()->injectStreamResponse(
                        standard_function,
                        stream_container_object
                );
            } else {
                stream_container_object->ptr()->injectStreamResponse(
                        unique_function,
                        stream_container_object
                );
            }
        }
        //else {
        //    //because of how these are removed, the chat room COULD exist inside this list
        //    // (being removed) while it does not exist inside the user_open_chat_streams
        //}
    }
}

void ChatStreamConcurrentSet::iterateAndSendMessageExcludeSender(
        const std::string& sending_account_oid,
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function
) {
    SpinlockWrapper lock(spin_lock);
    for (const auto& chatRoomIterator : map) {
        if (chatRoomIterator.first != sending_account_oid) { //do not send this to the original sender

            std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                    chatRoomIterator.first);

            if (stream_container_object != nullptr) {
                stream_container_object->ptr()->injectStreamResponse(
                        outside_function,
                        stream_container_object
                );
            }
            //else {
            //    //because of how these are removed, the chat room COULD exist inside this list
            //    // (being removed) while it does not exist inside the user_open_chat_streams
            //}
        }
    }
}

void ChatStreamConcurrentSet::sendSingleMessageToUserNoLock(
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function,
        const std::string& receiving_user_account_oid
        ) {
    auto user_ptr = map.find(receiving_user_account_oid);
    if(user_ptr != map.end()) { //user exists inside chat room
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> stream_container_object = user_open_chat_streams.find(
                receiving_user_account_oid);
        if (stream_container_object != nullptr) {
            stream_container_object->ptr()->injectStreamResponse(
                    outside_function,
                    stream_container_object
            );
        }
        //else {
        //    //because of how these are removed, the chat room COULD exist inside this list
        //    // (being removed) while it does not exist inside the stream_container_object
        //}
    }
}

void ChatStreamConcurrentSet::sendMessageToSpecificUser(
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function,
        const std::string& receiving_user_account_oid
) {
    SpinlockWrapper lock(spin_lock);
    sendSingleMessageToUserNoLock(outside_function, receiving_user_account_oid);
}

void ChatStreamConcurrentSet::sendMessagesToTwoUsers(
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& first_outside_function,
        const std::string& first_user_account_oid,
        const std::function<void(
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& second_outside_function,
        const std::string& second_user_account_oid
) {
    SpinlockWrapper lock(spin_lock);
    sendSingleMessageToUserNoLock(first_outside_function, first_user_account_oid);
    sendSingleMessageToUserNoLock(second_outside_function, second_user_account_oid);
}

