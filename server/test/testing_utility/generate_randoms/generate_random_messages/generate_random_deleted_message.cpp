//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"
#include "setup_client_message_to_server.h"

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse>
generateRandomDeletedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& deleted_message_uuid,
        const DeleteType delete_type
) {

    std::string actual_message_uuid;

    if (isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    DeleteType actual_delete_type;

    if (delete_type != DeleteType::DELETE_FOR_ALL_USERS
        && delete_type != DeleteType::DELETE_FOR_SINGLE_USER) {
        //This is the only delete type that will be received by other users.
        actual_delete_type = DeleteType::DELETE_FOR_ALL_USERS;
    } else {
        actual_delete_type = delete_type;
    }

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientDeletedMessageRequest(
            account_oid,
            logged_in_token,
            installation_id,
            actual_message_uuid,
            chat_room_id,
            deleted_message_uuid,
            actual_delete_type
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    return std::make_pair(client_message_to_server_request, client_message_to_server_response);
}