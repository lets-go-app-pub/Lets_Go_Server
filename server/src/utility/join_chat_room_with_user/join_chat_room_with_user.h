//
// Created by jeremiah on 3/4/23.
//

#pragma once

#include <mongocxx/client_session.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include "StatusEnum.pb.h"
#include "ChatRoomCommands.pb.h"

struct JoinChatRoomWithUserReturn {
    ReturnStatus return_status;
    grpc_chat_commands::ChatRoomStatus chat_room_status;

    JoinChatRoomWithUserReturn() = delete;

    JoinChatRoomWithUserReturn(
            ReturnStatus _return_status,
            grpc_chat_commands::ChatRoomStatus _chat_room_status
            ):
            return_status(_return_status), chat_room_status(_chat_room_status) {}
};

//The following parameters will be changed on successful function completion.
// different_user_joined_message_uuid
// different_user_joined_chat_room_value
// current_timestamp
//When successful this function will return {ReturnStatus::SUCCESS, ChatRoomStatus::SUCCESSFULLY_JOINED}.
//user_account_doc_view requires PICTURES, AGE and FIRST_NAME projected into it.
JoinChatRoomWithUserReturn joinChatRoomWithUser(
        std::string& different_user_joined_message_uuid,
        bsoncxx::stdx::optional<bsoncxx::document::value>& different_user_joined_chat_room_value,
        std::chrono::milliseconds& current_timestamp,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& chat_room_collection,
        mongocxx::client_session* session,
        const bsoncxx::document::view& user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        const std::string& chat_room_id,
        const std::string& chat_room_password,
        bool user_joined_event_from_swiping,
        const std::function<void(std::string& error_string)>& append_info_to_error_string = []([[maybe_unused]]std::string& error_string){}
);