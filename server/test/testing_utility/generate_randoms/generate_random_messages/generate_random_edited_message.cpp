//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"
#include "setup_client_message_to_server.h"

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse>
generateRandomEditedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& edited_message_uuid,
        const std::string& edited_message_text
) {

    std::string actual_message_uuid;

    if (isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    std::string actual_edited_message_text;

    if (edited_message_text.empty()) {
        actual_edited_message_text = gen_random_alpha_numeric_string((rand() % 1000) + 5);
    } else {
        actual_edited_message_text = edited_message_text;
    }

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientEditedMessageRequest(
            account_oid,
            logged_in_token,
            installation_id,
            actual_message_uuid,
            chat_room_id,
            edited_message_uuid,
            actual_edited_message_text
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    return std::make_pair(client_message_to_server_request, client_message_to_server_response);
}