//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include <accepted_mime_types.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"
#include "setup_client_message_to_server.h"

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomMimeTypeMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& mime_type,
        const std::string& url_of_download
        ) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    std::string actual_mime_type;

    if(mime_type.empty()) {
        std::vector<std::string> accepted_mime_types_list;

        for(const std::string& extracted_mime_type : accepted_mime_types) {
            accepted_mime_types_list.emplace_back(extracted_mime_type);
        }

        //randomly choose an accepted mime type (make a copy)
        actual_mime_type = std::string(accepted_mime_types_list[rand() % accepted_mime_types_list.size()]);
    } else {
        actual_mime_type = mime_type;
    }

    std::string actual_url_of_download;

    if(mime_type.empty()) {
        actual_url_of_download = gen_random_alpha_numeric_string((rand() % 1000) + 5);
    } else {
        actual_url_of_download = url_of_download;
    }

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientMimeTypeMessageRequest(
            account_oid,
            logged_in_token,
            installation_id,
            actual_message_uuid,
            chat_room_id,
            actual_url_of_download,
            actual_mime_type
    );

    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    return std::pair(client_message_to_server_request, client_message_to_server_response);
}