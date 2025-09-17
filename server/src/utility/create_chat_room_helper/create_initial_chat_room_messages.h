//
// Created by jeremiah on 3/13/23.
//

#pragma once

#include <optional>

#include <mongocxx/database.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include "generate_new_chat_room_times.h"
#include "full_event_values.h"
#include "create_chat_room_location_struct.h"

#include "ChatRoomCommands.pb.h"

bool createInitialChatRoomMessages(
        const std::string& cap_message_uuid,
        const bsoncxx::oid& user_account_oid,
        const std::string& user_account_oid_str,
        const std::string& chat_room_id,
        const GenerateNewChatRoomTimes& chat_room_times,
        ChatMessageToClient* mutable_chat_room_cap_message,
        ChatMessageToClient* mutable_current_user_joined_chat_message,
        std::shared_ptr<bsoncxx::builder::stream::document>& different_user_joined_chat_room_message_doc,
        std::string& different_user_joined_message_uuid
);