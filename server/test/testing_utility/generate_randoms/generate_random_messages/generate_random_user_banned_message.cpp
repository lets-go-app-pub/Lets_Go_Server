//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"

grpc_chat_commands::RemoveFromChatRoomRequest generateRandomUserBannedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& account_oid_to_remove
        ) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    grpc_chat_commands::RemoveFromChatRoomRequest remove_from_chat_room_request;
    grpc_chat_commands::RemoveFromChatRoomResponse remove_from_chat_room_response;

    setupUserLoginInfo(
        remove_from_chat_room_request.mutable_login_info(),
        account_oid,
        logged_in_token,
        installation_id
    );

    remove_from_chat_room_request.set_chat_room_id(chat_room_id);
    remove_from_chat_room_request.set_kick_or_ban(grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_BAN);
    remove_from_chat_room_request.set_account_id_to_remove(account_oid_to_remove);
    remove_from_chat_room_request.set_message_uuid(actual_message_uuid);

    removeFromChatRoom(&remove_from_chat_room_request, &remove_from_chat_room_response);

    return remove_from_chat_room_request;
}