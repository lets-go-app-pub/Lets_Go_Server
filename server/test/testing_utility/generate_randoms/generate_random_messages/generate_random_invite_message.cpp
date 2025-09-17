//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"
#include "setup_client_message_to_server.h"

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomInviteTypeMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& invited_user_account_oid,
        const std::string& invited_user_name,
        const std::string& invited_chat_room_id,
        const std::string& invited_chat_room_name,
        const std::string& invited_chat_room_password
) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    std::string actual_invited_user_account_oid;

    if(invited_user_account_oid.empty()) {
        actual_invited_user_account_oid = bsoncxx::oid{}.to_string();
    } else {
        actual_invited_user_account_oid = invited_user_account_oid;
    }

    std::string actual_invited_user_name;

    if(invited_user_name.empty()) {
        actual_invited_user_name = gen_random_alpha_numeric_string(rand() % 50 + 20);

        //remove anything that is not a char, lowercase everything that is a char
        for(auto it = actual_invited_user_name.begin(); it != actual_invited_user_name.end();) {
            if(std::isalpha(*it)) {
                *it = (char)tolower(*it);
                it++;
            } else {
                it = actual_invited_user_name.erase(it);
            }
        }

        //capitalize first letter (the check inside clientMessageToServer() allows an empty name)
        if(!actual_invited_user_name.empty()) {
            actual_invited_user_name[0] = (char)toupper(actual_invited_user_name[0]);
        }

    } else {
        actual_invited_user_name = invited_user_name;
    }

    std::string actual_invited_chat_room_id;

    if(invited_chat_room_id.empty()) {
        actual_invited_chat_room_id = gen_random_alpha_numeric_string(7 + rand() % 2);

        for(char& c : actual_invited_chat_room_id) {
            c = (char)tolower(c);
        }
    } else {
        actual_invited_chat_room_id = invited_chat_room_id;
    }

    std::string actual_invited_chat_room_password;

    if(invited_chat_room_password.empty()) {
        actual_invited_chat_room_password = gen_random_alpha_numeric_string(rand() % 50);
    } else {
        actual_invited_chat_room_password = invited_chat_room_password;
    }

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request = setupClientInviteMessageRequest(
            account_oid,
            logged_in_token,
            installation_id,
            actual_message_uuid,
            chat_room_id,
            actual_invited_user_account_oid,
            actual_invited_user_name,
            actual_invited_chat_room_id,
            invited_chat_room_name,
            actual_invited_chat_room_password
    );
    grpc_chat_commands::ClientMessageToServerResponse client_message_to_server_response;

    clientMessageToServer(&client_message_to_server_request, &client_message_to_server_response);

    std::cout << "return_status: " << convertReturnStatusToString(client_message_to_server_response.return_status()) << '\n';

    return std::pair(client_message_to_server_request, client_message_to_server_response);
}