//
// Created by jeremiah on 8/15/22.
//

#pragma once

#include <string>
#include <functional>
#include "ChatMessageToClientMessage.pb.h"
#include "messages_waiting_to_be_sent.h"
#include "utility_chat_functions.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_stream_container_object.h"
#include "chat_room_values.h"

//Will send the previous messages inside cached_messages over the last chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.
// Will then add on a kDifferentUserJoinedMessage for the current user. This function is only meant to be called for the user
// that originally sent the kDifferentUserJoinedMessage message.
void sendPreviouslyStoredMessagesWithDifferentUserJoined(
        const std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        const std::string& chatRoomId,
        ChatMessageToClient* responseMsg,
        const std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>& stream_container_object
);

void sendMessageToUsers(
        const std::string& chatRoomId,
        const std::shared_ptr<grpc_stream_chat::ChatToClientResponse>& chat_to_client_response,
        ChatMessageToClient* responseMsg,
        const std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages
);

//NOTE: This function was meant to be used with iterateAndSendMessagesToTargetUsers(). kDifferentUserJoined is not
// set up properly for general purpose when compared to sendMessageToUsers(). Currently, this function can handle
// any message type, however the message types that join or leave a chat room are the only ones currently sent to
// it.
void sendTargetMessageToSpecificUser(
        const std::string& chatRoomId,
        const std::shared_ptr<grpc_stream_chat::ChatToClientResponse>& chat_to_client_response,
        ChatMessageToClient* responseMsg,
        const std::string& user_account_oid,
        const std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>& chat_stream_container
);

void removeCachedMessagesOverMaxTime(
        std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        size_t& cached_messages_size_in_bytes,
        const std::chrono::milliseconds& current_timestamp
);

void removeCachedMessagesOverMaxBytes(
        std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        size_t& cached_messages_size_in_bytes
);

//Messages that are targeted at a specific user (the user is added or removed from a chat room)
// need special considerations if the message is out of order. This function will handle them.
// It will essentially send any messages that also target the same user to the users again to
// avoid unwanted conditions such as leave->join turning into join->leave.
void iterateAndSendMessagesToTargetUsers(
        const MessageWaitingToBeSent& message_info,
        const std::deque<MessageWaitingToBeSent>& messages_reference,
        const std::_Deque_iterator<MessageWaitingToBeSent, MessageWaitingToBeSent &, MessageWaitingToBeSent *>& iterator_pos_to_be_inserted_at
);

//NOTE: This will lock a mutex on initialize, be aware of that for multi threading.
struct RequiredInitializationNewChatMessageWrapper {

    RequiredInitializationNewChatMessageWrapper() = delete;

    RequiredInitializationNewChatMessageWrapper(
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse>&& responseMessage,
            ChatMessageToClient* _message_ptr
            ):
            chat_to_client_response(std::move(responseMessage)),
            message_ptr(_message_ptr)
            {}

    bool initialized = false;
    bool initialize_successful = false;
    std::mutex initializationMutex;
    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> chat_to_client_response;
    ChatMessageToClient* const message_ptr = nullptr;

    void initialize() {
        if(message_ptr == nullptr) {
            return;
        }

        std::scoped_lock<std::mutex> lock(initializationMutex);
        if (!initialized) {

            initialized = true;

            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

            mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
            mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

            //NOTE: Do not change anything at a 'higher' level than for example chat_room_id or sent_from_user_account_oid. This is because
            // the message can be read from other places.
            auto userMemberInfo = message_ptr->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info();
            initialize_successful = getUserAccountInfoForUserJoinedChatRoom(
                    mongoCppClient, accountsDB, userAccountsCollection,
                    message_ptr->message().standard_message_info().chat_room_id_message_sent_from(),
                    message_ptr->sent_by_account_id(),
                    HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
                    userMemberInfo
            );
        }
    }
};

//calls the add or remove function based on the message type
inline void addOrRemoveChatRoomIdByMessageType(
        const ChatMessageToClient& responseMsg,
        const std::function<void(const std::string&)>& addChatRoomId,
        const std::function<void(const std::string&)>& removeChatRoomId
) {

    const MessageSpecifics::MessageBodyCase& messageType = responseMsg.message().message_specifics().message_body_case();
    AddOrRemove add_or_remove_user = addOrRemoveUserBasedOnMessageType(messageType);

    switch (add_or_remove_user) {
        case MESSAGE_TARGET_NOT_SET:
            break;
        case MESSAGE_TARGET_ADD: {
            addChatRoomId(responseMsg.sent_by_account_id());
            break;
        }
        case MESSAGE_TARGET_REMOVE: {
            switch (messageType) {
                case MessageSpecifics::kUserKickedMessage:
                    removeChatRoomId(responseMsg.message().message_specifics().user_kicked_message().kicked_account_oid());
                    break;
                case MessageSpecifics::kUserBannedMessage:
                    removeChatRoomId(responseMsg.message().message_specifics().user_banned_message().banned_account_oid());
                    break;
                case MessageSpecifics::kDifferentUserLeftMessage:
                    removeChatRoomId(responseMsg.sent_by_account_id());
                    break;
                case MessageSpecifics::kMatchCanceledMessage:
                    removeChatRoomId(responseMsg.message().message_specifics().match_canceled_message().matched_account_oid());
                    removeChatRoomId(responseMsg.sent_by_account_id());
                    break;
                default:
                    break;
            }
            break;
        }
    }
}

inline void buildNewUpdateTimeMessageResponse(
        ChatMessageToClient* message_to_client,
        const std::string& chat_room_id,
        long timestamp_updated,
        const std::string& message_uuid
) {
    message_to_client->set_timestamp_stored(timestamp_updated);
    message_to_client->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(chat_room_id);
    message_to_client->mutable_message()->mutable_message_specifics()->mutable_new_update_time_message()->set_represented_message_uuid(message_uuid);
    message_to_client->mutable_message()->mutable_message_specifics()->mutable_new_update_time_message()->set_message_timestamp_stored(timestamp_updated);
}

inline auto buildNewUpdateTimeMessageLambda(
        ChatMessageToClient* response_message,
        const std::string& chat_room_id
) {

    return [
            chat_room_id = chat_room_id,
            timestamp_updated = response_message->timestamp_stored(),
            message_uuid = response_message->message_uuid()
    ] (std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
        auto new_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

        buildNewUpdateTimeMessageResponse(
                new_response->mutable_return_new_chat_message()->add_messages_list(),
                chat_room_id,
                timestamp_updated,
                message_uuid
        );

        reply_vector.emplace_back(std::move(new_response));
    };
}

inline auto buildMessageLambdaToBeInjected(
        const std::shared_ptr<grpc_stream_chat::ChatToClientResponse>& chat_to_client_response
) {

    return [chat_to_client_response_cpy = chat_to_client_response](
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) mutable {
        reply_vector.emplace_back(std::move(chat_to_client_response_cpy));
    };
}

inline void buildDifferentUserChatMessageToClientForSender(
        ChatMessageToClient* message,
        const std::string& account_oid,
        const std::string& message_uuid,
        long timestamp_stored,
        const std::string& chat_room_id
) {
    message->set_sent_by_account_id(account_oid);
    message->set_message_uuid(message_uuid);
    message->set_timestamp_stored(timestamp_stored);

    message->mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id);
    message->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message()->mutable_member_info()->mutable_user_info()->set_account_oid(
            account_oid);
}

inline auto buildDifferentUserMessageForSender(
        ChatMessageToClient* response_message,
        const std::string& chat_room_id
) {

    //This message is needed in certain situations because a iterateAndSendMessageExcludeSender() will not pass
    // to the sending_user ChatStreamContainerObject. This means that the user could miss this chat room
    // being added.
    //NOTE: The same is not true for removed values, this is because removing a value that does not exist
    // does not have any negative effect. While not having a value that WAS added can leave a value in
    // map_of_chat_rooms_to_users.
    return [
            account_oid = response_message->sent_by_account_id(),
            message_uuid = response_message->message_uuid(),
            timestamp_stored = response_message->timestamp_stored(),
            chat_room_id = chat_room_id
    ] (
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

        auto response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

        auto message = response->mutable_return_new_chat_message()->add_messages_list();

        buildDifferentUserChatMessageToClientForSender(
                message,
                account_oid,
                message_uuid,
                timestamp_stored,
                chat_room_id
        );

        reply_vector.emplace_back(std::move(response));
    };
}

//NOTE: A mutex is locked when RequiredInitializationNewChatMessageWrapper is used below, read below for more info.
inline std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)> buildDifferentUserMessageForReceivingUser(
        const std::shared_ptr<grpc_stream_chat::ChatToClientResponse>& chat_to_client_response,
        const bool do_not_update_user_state
) {

    //There are several considerations for how this works.
    // 1) If the same message is used as inside messages_cache, the size will be variable and so keeping track of the
    // size of the deque will become harder.
    // 2) Race conditions can occur if the chat_to_client_response inside messages_cache is accessed.
    // 3) For some kDifferentUserJoinedMessage set_do_not_update_user_state must be set to true, for others it cannot
    // be.
    // 4) If the 'wrapper' was stored it would need to be inside the MessageWaitingToBeSent. This would mean allowing this
    // function to access a reference to the message_info object.
    // 5) Don't want the change stream threads doing database calls in order to keep them at a good speed. This means the
    // database access should be left to the thread acting on the ChatStreamContainerObject.

    //It is important to make a copy of this the first time it is sent, this is because the message itself
    // will be changed by RequiredInitializationNewChatMessageWrapper. This can cause a race condition if
    // the message continues to exist inside cached_messages.
    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> chat_to_client_response_copy =
            std::make_shared<grpc_stream_chat::ChatToClientResponse>();

    chat_to_client_response_copy->CopyFrom(*chat_to_client_response);

    ChatMessageToClient* response_msg_copy;

    if (!chat_to_client_response_copy->has_return_new_chat_message()
        || chat_to_client_response_copy->return_new_chat_message().messages_list().empty()) {

        std::string error_string = "A message that was supposed to be kDifferentUserJoined was incorrectly formatted.";
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), error_string,
            "has_return_new_chat_message", (chat_to_client_response_copy->has_return_new_chat_message()?"true":"false"),
            "messages_list().empty()", (chat_to_client_response_copy->return_new_chat_message().messages_list().empty()?"true":"false"),
            "chat_to_client_response_copy", chat_to_client_response_copy->DebugString()
        );

        return [chat_to_client_response_copy] (
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
            reply_vector.emplace_back(chat_to_client_response_copy);
        };
    } else {
        response_msg_copy = chat_to_client_response_copy->mutable_return_new_chat_message()->mutable_messages_list(
                0);
    }

    response_msg_copy->mutable_message()->mutable_standard_message_info()->set_do_not_update_user_state(do_not_update_user_state);

    if (response_msg_copy->message().message_specifics().message_body_case() !=
        MessageSpecifics::kDifferentUserJoinedMessage) {

        std::string error_string = "A message that was supposed to be kDifferentUserJoined was incorrectly formatted.";
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional <std::string>(), error_string,
            "has_return_new_chat_message", (chat_to_client_response_copy->has_return_new_chat_message()?"true":"false"),
            "messages_list().empty()", (chat_to_client_response_copy->return_new_chat_message().messages_list().empty()?"true":"false"),
            "chat_to_client_response_copy", chat_to_client_response_copy->DebugString()
        );

        return [chat_to_client_response_copy] (
                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
            reply_vector.emplace_back(chat_to_client_response_copy);
        };
    }

    //NOTE: This will lock a mutex when initialize() is called. In this situation there will never be deadlock because
    // this function will always be run by the global thread_pool variable.
    std::shared_ptr<RequiredInitializationNewChatMessageWrapper> wrapper =
            std::make_shared<RequiredInitializationNewChatMessageWrapper>(
                    std::move(chat_to_client_response_copy),
                    response_msg_copy
            );

    return [wrapper] (
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
        wrapper->initialize();
        if(wrapper->initialize_successful) {
            reply_vector.emplace_back(wrapper->chat_to_client_response);
        }
    };
}