//
// Created by jeremiah on 8/1/22.
//

#include "client_message_to_server_helper_functions.h"
#include "chat_room_header_keys.h"

#include <bsoncxx/builder/stream/document.hpp>

bsoncxx::document::value buildFilterForCheckingInviteHeader(
        grpc_chat_commands::ClientMessageToServerRequest *request
        ) {
    return bsoncxx::builder::stream::document{}
            << "_id" << chat_room_header_keys::ID
            << chat_room_header_keys::CHAT_ROOM_NAME << request->message().message_specifics().invite_message().chat_room_name()
            << chat_room_header_keys::CHAT_ROOM_PASSWORD << request->message().message_specifics().invite_message().chat_room_password()
        << bsoncxx::builder::stream::finalize;
}