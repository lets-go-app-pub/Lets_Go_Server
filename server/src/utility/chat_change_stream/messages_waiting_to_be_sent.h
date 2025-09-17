//
// Created by jeremiah on 8/15/22.
//

#pragma once

#include <chrono>
#include <memory>
#include <vector>
#include "TypeOfChatMessage.pb.h"
#include "ChatMessageStream.pb.h"

enum AddOrRemove {
    MESSAGE_TARGET_NOT_SET,
    MESSAGE_TARGET_ADD,
    MESSAGE_TARGET_REMOVE
};

//This function is a bit redundant in a lot of places. The purpose of it is that if a new
// add or remove message type is added in the future, all places where it is relevant can
// be easily tracked down.
inline AddOrRemove addOrRemoveUserBasedOnMessageType(MessageSpecifics::MessageBodyCase message_type) {

    AddOrRemove return_val;

    if(message_type == MessageSpecifics::kUserKickedMessage
        || message_type == MessageSpecifics::kUserBannedMessage
        || message_type == MessageSpecifics::kDifferentUserLeftMessage
        || message_type == MessageSpecifics::kMatchCanceledMessage
            ) {
        return_val = AddOrRemove::MESSAGE_TARGET_REMOVE;
    } else if(message_type == MessageSpecifics::kDifferentUserJoinedMessage) {
        return_val = AddOrRemove::MESSAGE_TARGET_ADD;
    } else {
        return_val = AddOrRemove::MESSAGE_TARGET_NOT_SET;
    }

    return return_val;
}

struct MessageTarget {
    AddOrRemove add_or_remove = AddOrRemove::MESSAGE_TARGET_NOT_SET;
    std::string account_oid;

    MessageTarget(
            AddOrRemove _add_or_remove,
            std::string _account_oid
    ) :
            add_or_remove(_add_or_remove),
            account_oid(std::move(_account_oid)) {}

    MessageTarget() = default;
    MessageTarget(const MessageTarget& copy) = default;
    MessageTarget(MessageTarget&& copy) = default;

    MessageTarget& operator=(const MessageTarget& rhs) = default;
    ~MessageTarget() = default;
};

struct RequiredInitializationNewChatMessageWrapper;

struct MessageWaitingToBeSent {
    //None of these members are const so that a move constructor can (and will be) used.
    std::string chat_room_id;
    std::string message_uuid;
    MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::MESSAGE_BODY_NOT_SET;

    //NOTE: When kDifferentUserJoined it will modify the message (technically it will not affect any fundamentals
    // such as chat_room_id, message_type etc...). In order to avoid risking a race condition
    // it is better to not access anything inside the message directly. Use the other members of this struct for such
    // values.
    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> chat_to_client_response = nullptr;
    std::chrono::milliseconds time_message_stored{-1};
    std::chrono::milliseconds time_received{-1};

    size_t current_message_size = sizeof(MessageWaitingToBeSent);

    //targets to be added or removed for this message
    std::vector<MessageTarget> message_targets;

    MessageWaitingToBeSent() = default;

    MessageWaitingToBeSent(const MessageWaitingToBeSent& copy) = delete;
    MessageWaitingToBeSent(MessageWaitingToBeSent&& move) = default;

    ~MessageWaitingToBeSent() = default;

    MessageWaitingToBeSent& operator=(const MessageWaitingToBeSent& rhs) = default;

    MessageWaitingToBeSent(
            std::string _chat_room_id,
            std::string _message_uuid,
            MessageSpecifics::MessageBodyCase _message_type,
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse>&& _chat_to_client_response,
            std::chrono::milliseconds _time_message_stored,
            std::chrono::milliseconds _time_received,
            std::vector<MessageTarget> _message_targets
    ) : chat_room_id(std::move(_chat_room_id)),
        message_uuid(std::move(_message_uuid)),
        message_type(_message_type),
        chat_to_client_response(std::move(_chat_to_client_response)),
        time_message_stored(_time_message_stored),
        time_received(_time_received),
        message_targets(std::move(_message_targets))
    {
        current_message_size += chat_room_id.size()+message_uuid.size()+chat_to_client_response->ByteSizeLong();
    }

};
