//
// Created by jeremiah on 4/9/21.
//
#pragma once

#include <ChatRoomCommands.grpc.pb.h>
#include <ChatMessageStream.grpc.pb.h>

#include <utility>

#include "thread_safe_writes_waiting_to_process_vector.h"
#include "pushed_from_queue_location_enum.h"

//this will store a list of ChatMessageToClient then send groups of them
// back at a time when either the list is getting near the 4MB limit, finalCleanup()
// is called or the Destructor is called
class StoreAndSendMessagesVirtual {
public:

    StoreAndSendMessagesVirtual() = delete;

    explicit StoreAndSendMessagesVirtual(
            google::protobuf::RepeatedPtrField<ChatMessageToClient>* _mutableList
    ) : mutableMessageList(_mutableList) {}

    //saves the message inside the mutable list to be sent later
    void sendMessage(ChatMessageToClient chatMessageToClient);

    //will send any 'leftover' messages
    void finalCleanup();

    //returns total number messages run through sendMessage() for this object
    [[nodiscard]] size_t getNumberMessagesSent() const {
        return number_messages_sent;
    }

//    ~StoreAndSendMessagesVirtual() {
//        //there are problems with calling a virtual function (look inside finalCleanup()) in the destructor
//        // of the base object
//        //finalCleanup();
//    }

protected:
    size_t number_messages_sent = 0;
    size_t currentByteSizeOfResponseMsg = 0;
    google::protobuf::RepeatedPtrField<ChatMessageToClient>* mutableMessageList = nullptr;

    //writes the message to the responseStream and clears the mutableMessageList
    void writeMessageAndClearList();

    virtual void clearMessageList() = 0;

    virtual void writeMessageToClient() = 0;
};

class StoreAndSendRequestedMessagesToClientBiDiStream : public StoreAndSendMessagesVirtual {
public:

    StoreAndSendRequestedMessagesToClientBiDiStream() = delete;

    explicit StoreAndSendRequestedMessagesToClientBiDiStream(
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& _reply_vector,
            const std::string& chat_room_id
    ) : StoreAndSendRequestedMessagesToClientBiDiStream(std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
                                                        _reply_vector, chat_room_id) {}

private:

    explicit StoreAndSendRequestedMessagesToClientBiDiStream(
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse>&& _responseMessage,
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& _reply_vector,
            const std::string& _chat_room_id
    ) : StoreAndSendMessagesVirtual(
            _responseMessage->mutable_request_full_message_info_response()->mutable_full_messages()->mutable_full_message_list()),
        responseMessage(std::move(_responseMessage)), reply_vector(_reply_vector), chat_room_id(_chat_room_id) {
        responseMessage->mutable_request_full_message_info_response()->set_chat_room_id(_chat_room_id);
        responseMessage->mutable_request_full_message_info_response()->set_request_status(
                grpc_stream_chat::RequestFullMessageInfoResponse::INTERMEDIATE_MESSAGE_LIST
        );
    }

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> responseMessage;
    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector;
    std::string chat_room_id;

    void clearMessageList() override {
        //Because this is not actually Written to the stream immediately like the other implementations of StoreAndSendMessagesVirtual
        // it must be recreated, otherwise it will overwrite previous messages
        responseMessage = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
        responseMessage->mutable_request_full_message_info_response()->set_chat_room_id(chat_room_id);
        responseMessage->mutable_request_full_message_info_response()->set_request_status(
                grpc_stream_chat::RequestFullMessageInfoResponse::INTERMEDIATE_MESSAGE_LIST
        );
        responseMessage->mutable_request_full_message_info_response()->mutable_full_messages()->clear_full_message_list();
        mutableMessageList = responseMessage->mutable_request_full_message_info_response()->mutable_full_messages()->mutable_full_message_list();
    }

    void writeMessageToClient() override {
        reply_vector.push_back(responseMessage);
    }
};

enum TypeOfChatToClientToUse {
    CHAT_TO_CLIENT_TYPE_NEW_MESSAGE,
    CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE
};

template <TypeOfChatToClientToUse enum_val>
class StoreAndSendMessagesToClientBiDiStream : public StoreAndSendMessagesVirtual {
public:

    StoreAndSendMessagesToClientBiDiStream() = delete;

    explicit StoreAndSendMessagesToClientBiDiStream(
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& _reply_vector
    ) : StoreAndSendMessagesToClientBiDiStream(std::make_shared<grpc_stream_chat::ChatToClientResponse>(),
                                               _reply_vector) {}

    void clearAllMessages() {
        clearMessageList();
        reply_vector.clear();
    }

private:

    explicit StoreAndSendMessagesToClientBiDiStream(
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse>&& _responseMessage,
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& _reply_vector
    ) : StoreAndSendMessagesVirtual(
            (enum_val == TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE) ?
                _responseMessage->mutable_return_new_chat_message()->mutable_messages_list() :
                _responseMessage->mutable_initial_connection_messages_response()->mutable_messages_list()
            ),
        responseMessage(std::move(_responseMessage)), reply_vector(_reply_vector) {
    }

    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> responseMessage;
    std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector;

    void clearMessageList() override {
        //Because this is not actually Written to the stream immediately like the other implementations of StoreAndSendMessagesVirtual
        // it must be recreated, otherwise it will overwrite previous messages.
        if(enum_val == TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE) {
            responseMessage = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
            responseMessage->mutable_return_new_chat_message()->clear_messages_list();
            mutableMessageList = responseMessage->mutable_return_new_chat_message()->mutable_messages_list();
        } else {
            responseMessage = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
            responseMessage->mutable_initial_connection_messages_response()->clear_messages_list();
            mutableMessageList = responseMessage->mutable_initial_connection_messages_response()->mutable_messages_list();
        }
    }

    void writeMessageToClient() override {
        reply_vector.push_back(responseMessage);
    }
};

class StoreAndSendMessagesToJoinChatRoom : public StoreAndSendMessagesVirtual {
public:

    StoreAndSendMessagesToJoinChatRoom() = delete;

    explicit StoreAndSendMessagesToJoinChatRoom(
            grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* _responseStream
    ) : StoreAndSendMessagesToJoinChatRoom(std::make_unique<grpc_chat_commands::JoinChatRoomResponse>(),
                                           _responseStream) {}

private:

    explicit StoreAndSendMessagesToJoinChatRoom(
            std::unique_ptr<grpc_chat_commands::JoinChatRoomResponse>&& _responseMessage,
            grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* _responseStream
    ) : StoreAndSendMessagesVirtual(_responseMessage->mutable_messages_list()),
        responseMessage(std::move(_responseMessage)), responseStream(_responseStream) {}

    std::unique_ptr<grpc_chat_commands::JoinChatRoomResponse> responseMessage = nullptr;
    grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* responseStream = nullptr;

    void clearMessageList() override {
        responseMessage->clear_messages_list();
    }

    void writeMessageToClient() override {
        responseStream->Write(*responseMessage);
    }

};

class StoreAndSendMessagesToUpdateChatRoom : public StoreAndSendMessagesVirtual {
public:

    StoreAndSendMessagesToUpdateChatRoom() = delete;

    explicit StoreAndSendMessagesToUpdateChatRoom(
            grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* _responseStream
    ) : StoreAndSendMessagesToUpdateChatRoom(std::make_unique<grpc_chat_commands::UpdateChatRoomResponse>(),
                                             _responseStream) {}

private:

    explicit StoreAndSendMessagesToUpdateChatRoom(
            std::unique_ptr<grpc_chat_commands::UpdateChatRoomResponse>&& _responseMessage,
            grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* _responseStream
    ) : StoreAndSendMessagesVirtual(_responseMessage->mutable_chat_messages_list()->mutable_messages_list()),
        responseMessage(std::move(_responseMessage)), responseStream(_responseStream) {}

    std::unique_ptr<grpc_chat_commands::UpdateChatRoomResponse> responseMessage = nullptr;
    grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* responseStream = nullptr;

    void clearMessageList() override {
        responseMessage->mutable_chat_messages_list()->clear_messages_list();
    }

    void writeMessageToClient() override {
        responseStream->Write(*responseMessage);
    }

};