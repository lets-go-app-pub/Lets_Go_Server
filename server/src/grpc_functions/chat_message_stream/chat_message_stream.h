//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include <ChatMessageStream.grpc.pb.h>

#include "user_open_chat_streams.h"
#include "store_and_send_messages.h"
#include "utility_chat_functions.h"
#include "server_initialization_functions.h"

enum UserAccountChatRoomComparison {
    CHAT_ROOM_COMPARISON_ERROR,
    CHAT_ROOM_COMPARISON_DO_NOT_MATCH,
    CHAT_ROOM_COMPARISON_MATCH
};

//initialization for a user chat stream
std::string checkLoginToken(
        const grpc_stream_chat::InitialLoginMessageRequest& request,
        ReturnStatus& returnStatus,
        std::chrono::milliseconds& currentTimestamp
);

UserAccountChatRoomComparison compareUserAccountChatRoomsStates(
        std::vector<std::string> pre_chat_room_ids_inside_user_account_doc,
        mongocxx::collection& user_account_collection,
        const bsoncxx::oid& user_account_oid
);

ThreadPoolSuspend extractChatRooms(
        const grpc_stream_chat::InitialLoginMessageRequest& request,
        StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE>& sendMessagesObject,
        std::set<std::string>& chat_room_ids_user_is_part_of,
        bool& successful,
        long calling_current_index_value,
        const std::string& userAccountOIDStr,
        const std::chrono::milliseconds& currentTimestamp
);

/** ---------------------------- **/
/** --- INCLUDED FOR TESTING --- **/
/** ---------------------------- **/
//essentially 'counts' in base 26, the 26 digits are the lower case letters
//NOTE: the 'most significant digit' would be the final char not the first
void generateNextElementName(std::string& previous_element_name);