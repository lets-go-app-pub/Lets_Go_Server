//
// Created by jeremiah on 4/4/21.
//
#pragma once

#include "chat_stream_concurrent_set.h"
#include "tbb/concurrent_unordered_map.h"
#include "tbb/concurrent_vector.h"

#include "user_open_chat_streams.h"

//NOTE: the chatRoomIds are never removed from this map, however the userAccountOIDs that are stored
// inside each chat room are removed.
//key = chatRoomId
inline tbb::concurrent_unordered_map<std::string, ChatStreamConcurrentSet> map_of_chat_rooms_to_users;

//erases the user account OID from the specified chat room
void eraseUserOIDFromChatRoomId(const std::string& chatRoomId, const std::string& userAccountOIDStr, long object_index_value);

//erases the user account OID from the specified chat room
ThreadPoolSuspend eraseUserOIDFromChatRoomId_coroutine(
    const std::string& chatRoomId,
    const std::string& userAccountOIDStr,
    long object_index_value
);

//inserts the user account OID into the desired chat room Id, if the chat room Id does not exist, will
// create it
//NOTE: This does not follow the convention used by STL. If a value exists inside ChatStreamConcurrentSet, it will overwrite it.
bool insertUserOIDToChatRoomId(const std::string& chatRoomId, const std::string& userAccountOIDStr, long object_index_value);

//inserts the user account OID into the desired chat room Id, if the chat room Id does not exist, will
// create it
//NOTE: This does not follow the convention used by STL. If a value exists inside ChatStreamConcurrentSet, it will overwrite it.
ThreadPoolSuspend insertUserOIDToChatRoomId_coroutine(
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        long object_index_value
        );
