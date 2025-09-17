//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"

grpc_chat_commands::UpdateChatRoomInfoRequest generateRandomChatRoomNameUpdatedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& new_chat_room_name
        ) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    grpc_chat_commands::UpdateChatRoomInfoRequest update_chat_room_info_request;
    grpc_chat_commands::UpdateChatRoomInfoResponse update_chat_room_info_response;

    setupUserLoginInfo(
            update_chat_room_info_request.mutable_login_info(),
            account_oid,
            logged_in_token,
            installation_id
            );

    std::string actual_new_chat_room_name;

    if(new_chat_room_name.empty()) {
        actual_new_chat_room_name = gen_random_alpha_numeric_string(rand() % 50);
    } else {
        actual_new_chat_room_name = new_chat_room_name;
    }

    update_chat_room_info_request.set_chat_room_id(chat_room_id);
    update_chat_room_info_request.set_type_of_info_to_update(grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME);
    update_chat_room_info_request.set_new_chat_room_info(actual_new_chat_room_name);
    update_chat_room_info_request.set_message_uuid(actual_message_uuid);

    updateChatRoomInfo(&update_chat_room_info_request, &update_chat_room_info_response);

    return update_chat_room_info_request;
}