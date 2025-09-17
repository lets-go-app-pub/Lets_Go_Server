//
// Created by jeremiah on 10/15/22.
//

#pragma once

#include <bsoncxx/builder/stream/document.hpp>

bsoncxx::document::value generateChatRoomCapMessage(
        const std::string& message_uuid,
        const bsoncxx::oid& chat_room_created_by_oid,
        const std::chrono::milliseconds& chat_room_cap_message_time
);