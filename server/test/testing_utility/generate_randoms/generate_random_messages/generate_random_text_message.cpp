//
// Created by jeremiah on 6/16/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_client_message_to_server.h"

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomTextMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& text,
        bool send_message_to_server
        ) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    std::string actual_text;

    if(text.empty()) {
        actual_text = gen_random_alpha_numeric_string((rand() % 1000) + 5);
    } else {
        actual_text = text;
    }

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientTextMessageRequest(
            account_oid,
            logged_in_token,
            installation_id,
            actual_message_uuid,
            chat_room_id,
            actual_text
    );

    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    if(send_message_to_server) {
        clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);
    }

    return std::make_pair(client_message_to_server_request, client_message_to_server_response);
}