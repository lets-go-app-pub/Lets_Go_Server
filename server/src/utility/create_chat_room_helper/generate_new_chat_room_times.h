//
// Created by jeremiah on 3/8/23.
//

#pragma once

struct GenerateNewChatRoomTimes {
    const std::chrono::milliseconds chat_room_created_time{};
    const std::chrono::milliseconds chat_room_cap_message_time{};
    const std::chrono::milliseconds chat_room_last_active_time{};

    GenerateNewChatRoomTimes() = delete;

    explicit GenerateNewChatRoomTimes(
            const std::chrono::milliseconds& current_timestamp
    ) :
            chat_room_created_time(current_timestamp),
            chat_room_cap_message_time(current_timestamp + std::chrono::milliseconds{1}),
            chat_room_last_active_time(current_timestamp + std::chrono::milliseconds{2}) {}
};