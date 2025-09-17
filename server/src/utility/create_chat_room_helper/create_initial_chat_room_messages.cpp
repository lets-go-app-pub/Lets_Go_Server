//
// Created by jeremiah on 3/13/23.
//

#include "create_initial_chat_room_messages.h"

#include <mongocxx/uri.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <create_chat_room_helper.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <store_mongoDB_error_and_exception.h>
#include <thread_pool_global_variable.h>

#include "chat_room_commands.h"
#include "database_names.h"
#include "generate_chat_room_cap_message.h"

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
) {

    const bsoncxx::document::value chat_room_cap_message = generateChatRoomCapMessage(
            cap_message_uuid,
            user_account_oid,
            chat_room_times.chat_room_cap_message_time
    );

    if(!convertChatMessageDocumentToChatMessageToClient(
            chat_room_cap_message,
            chat_room_id,
            user_account_oid_str,
            true,
            mutable_chat_room_cap_message,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr)
            ) {
        return false;
    }

    different_user_joined_chat_room_message_doc = std::make_unique<bsoncxx::builder::stream::document>();

    different_user_joined_message_uuid = generateDifferentUserJoinedChatRoomMessage(
            *different_user_joined_chat_room_message_doc,
            user_account_oid,
            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
            chat_room_values::EVENT_ID_DEFAULT,
            chat_room_times.chat_room_last_active_time
    );

    if(!convertChatMessageDocumentToChatMessageToClient(
            different_user_joined_chat_room_message_doc->view(),
            chat_room_id,
            user_account_oid_str,
            true,
            mutable_current_user_joined_chat_message,
            AmountOfMessage::ONLY_SKELETON,
            DifferentUserJoinedChatRoomAmount::SKELETON,
            false,
            nullptr)
            ) {
        return false;
    }

    return true;
}