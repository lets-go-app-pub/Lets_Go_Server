//
// Created by jeremiah on 3/8/23.
//

#pragma once

#include <optional>

#include <mongocxx/database.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include "generate_new_chat_room_times.h"
#include "full_event_values.h"
#include "create_chat_room_location_struct.h"

#include "ChatRoomCommands.pb.h"

//Creates a chat room with the passed user as admin.
//The final two parameters will be set.
//NOTE: This will not initialize the chat room with the chat stream to the device. That is done at the end of
// createChatRoom().
//User doc requires FIRST_NAME and PICTURES to be projected out.
bool createCompleteChatRoomWithUser(
        mongocxx::database& accounts_db,
        mongocxx::database& chat_room_db,
        mongocxx::client_session* session,
        const GenerateNewChatRoomTimes& chat_room_times,
        const bsoncxx::oid& admin_user_account_oid,
        const bsoncxx::document::view& admin_user_account_doc,
        const std::string& chat_room_id,
        const std::optional<FullEventValues>& event_values,
        const std::optional<CreateChatRoomLocationStruct>& location_values,
        std::string& chat_room_name,
        std::string& chat_room_password,
        std::string& cap_message_uuid
);

