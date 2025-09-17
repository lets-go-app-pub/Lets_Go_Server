//
// Created by jeremiah on 6/3/22.
//

#pragma once

#include "chat_rooms_objects.h"

void checkLeaveChatRoomResult(
        const std::chrono::milliseconds& currentTimestamp,
        ChatRoomHeaderDoc& original_chat_room_header,
        const bsoncxx::oid& leaving_user_account_oid,
        const std::string& updated_thumbnail_reference,
        const std::string& updated_thumbnail,
        const std::string& chat_room_id,
        const ReturnStatus& return_status = ReturnStatus::SUCCESS,
        const std::chrono::milliseconds& timestamp_stored = std::chrono::milliseconds{-1L},
        const std::chrono::milliseconds& expected_timestamp_stored = std::chrono::milliseconds{-1L},
        bool return_val = true
);