//
// Created by jeremiah on 4/6/21.
//

#include "send_messages_implementation.h"

void eraseUserOIDFromChatRoomId(
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        const long object_index_value
        ) {
    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
    if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
        chatRoomOIDs->second.erase(userAccountOIDStr, object_index_value);
    }
}

ThreadPoolSuspend eraseUserOIDFromChatRoomId_coroutine(
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        const long object_index_value
        ) {
    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
    if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
        RUN_COROUTINE(
            chatRoomOIDs->second.erase_coroutine,
            userAccountOIDStr,
            object_index_value
        );
    }

    co_return;
}
