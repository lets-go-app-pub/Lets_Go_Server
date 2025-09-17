//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <optional>

#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include "generate_new_chat_room_times.h"
#include "full_event_values.h"
#include "create_chat_room_location_struct.h"

//generates a chat room iD and stores it inside chatRoomGeneratedId
//NOTE: No need to include this inside a transaction.
bool generateChatRoomId(mongocxx::database& chatRoomDB, std::string& chatRoomGeneratedId);

// creates a chat room from the passed chatRoomId
//There are three different times passed here.
// The chat_room_created_time is the time the header will get as 'chat_room_shared_keys::TIMESTAMP_CREATED'
// The chat_room_cap_message_time is the time the cap message will get as 'chat_room_shared_keys::TIMESTAMP_CREATED'
// The chat_room_last_active_time is the time the header will get as 'chat_room_shared_keys::CHAT_ROOM_LAST_ACTIVE_TIME'
//  this final value will be expected to match a different_user_joined message
// This timestamp system is set up to avoid duplicates in general, the expected order of messages is
//  chat room header
//  chat room cap message
//  different user joined message(s)
bool createChatRoomFromId(
        const bsoncxx::array::view& header_accounts_in_chat_room_array,
        const std::string& chat_room_name,
        std::string& cap_message_uuid,
        const bsoncxx::oid& chat_room_created_by_oid,
        const GenerateNewChatRoomTimes& chat_room_times,
        mongocxx::client_session* session,
        const std::function<void(const std::string&, const std::string&)>& set_return_status,
        const bsoncxx::array::view& matching_array,
        const mongocxx::database& chat_room_db,
        const std::string& chat_room_id,
        const std::optional<CreateChatRoomLocationStruct>& location_values = std::optional<CreateChatRoomLocationStruct>(),
        const std::optional<FullEventValues>& event_values = std::optional<FullEventValues>()
);