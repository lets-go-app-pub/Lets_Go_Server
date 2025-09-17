//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"
#include "setup_client_message_to_server.h"

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomPictureMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& picture_in_bytes
        ) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    std::string actual_picture_in_bytes;

    if(picture_in_bytes.empty()) {
        actual_picture_in_bytes = gen_random_alpha_numeric_string((rand() % 1000) + 5);
    } else {
        actual_picture_in_bytes = picture_in_bytes;
    }

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientPictureMessageRequest(
            account_oid,
            logged_in_token,
            installation_id,
            actual_message_uuid,
            chat_room_id,
            actual_picture_in_bytes
    );

    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    return std::pair(client_message_to_server_request, client_message_to_server_response);
}