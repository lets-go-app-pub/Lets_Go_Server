//
// Created by jeremiah on 4/6/21.
//

#include <store_mongoDB_error_and_exception.h>
#include "send_messages_implementation.h"
#include "database_names.h"
#include "collection_names.h"

bool insertUserOIDToChatRoomId(const std::string& chatRoomId,
                               const std::string& userAccountOIDStr,
                               const long object_index_value) {
    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
    if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
        chatRoomOIDs->second.upsert(userAccountOIDStr, object_index_value);
    } else {

        auto insert_result = map_of_chat_rooms_to_users.insert(std::make_pair(chatRoomId, ChatStreamConcurrentSet(chatRoomId)));

        if(insert_result.second) { //if upsert was successful
            insert_result.first->second.upsert(userAccountOIDStr, object_index_value);
        } else { //if upsert failed (possible if another thread inserted value first, should be rare)
            chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
            if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
                chatRoomOIDs->second.upsert(userAccountOIDStr, object_index_value);
            } else {
                std::string error_string = "The Chat Room Id was just inserted into the map and is not found.";

                std::optional<std::string> dummy_exception_string;
                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              dummy_exception_string, error_string,
                                              "database", database_names::ACCOUNTS_DATABASE_NAME,
                                              "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                              "oid_used", userAccountOIDStr,
                                              "chat_room_id", chatRoomId);
                return false;
            }
        }
    }

    return true;
}

ThreadPoolSuspend insertUserOIDToChatRoomId_coroutine(
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        const long object_index_value
        ) {
    auto chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
    if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
        RUN_COROUTINE(
            chatRoomOIDs->second.upsert_coroutine,
            userAccountOIDStr,
            object_index_value
        );
    } else {

        auto insert_result = map_of_chat_rooms_to_users.insert(std::make_pair(chatRoomId, ChatStreamConcurrentSet(chatRoomId)));

        if(insert_result.second) { //if upsert was successful
            RUN_COROUTINE(
                insert_result.first->second.upsert_coroutine,
                userAccountOIDStr,
                object_index_value
            );
        } else { //if upsert failed (possible if another thread inserted value first, should be rare)
            chatRoomOIDs = map_of_chat_rooms_to_users.find(chatRoomId);
            if (chatRoomOIDs != map_of_chat_rooms_to_users.end()) { //if the chat room exists
                RUN_COROUTINE(
                    chatRoomOIDs->second.upsert_coroutine,
                    userAccountOIDStr,
                    object_index_value
                );
            } else {
                std::string error_string = "The Chat Room Id was just inserted into the map and is not found.";

                std::optional<std::string> dummy_exception_string;
                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              dummy_exception_string, error_string,
                                              "database", database_names::ACCOUNTS_DATABASE_NAME,
                                              "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                              "oid_used", userAccountOIDStr,
                                              "chat_room_id", chatRoomId);
            }
        }
    }

    co_return;
}