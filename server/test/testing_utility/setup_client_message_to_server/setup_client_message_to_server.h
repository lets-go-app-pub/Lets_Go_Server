//
// Created by jeremiah on 8/1/22.
//

#pragma once

#include <bsoncxx/oid.hpp>

#include "ChatRoomCommands.pb.h"

grpc_chat_commands::ClientMessageToServerRequest setupClientTextMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& message_text
);

grpc_chat_commands::ClientMessageToServerRequest setupClientPictureMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& picture_in_bytes
);

grpc_chat_commands::ClientMessageToServerRequest setupClientLocationMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        double longitude,
        double latitude
);

grpc_chat_commands::ClientMessageToServerRequest setupClientMimeTypeMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& url_of_download,
        const std::string& mime_type
);

grpc_chat_commands::ClientMessageToServerRequest setupClientInviteMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& invited_user_account_oid,
        const std::string& invited_user_name,
        const std::string& invited_chat_room_id,
        const std::string& invited_chat_room_name,
        const std::string& invited_chat_room_password
);

grpc_chat_commands::ClientMessageToServerRequest setupClientEditedMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& edited_message_uuid,
        const std::string& edited_message_text
);

grpc_chat_commands::ClientMessageToServerRequest setupClientDeletedMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& deleted_message_uuid,
        const DeleteType& delete_type
);

grpc_chat_commands::ClientMessageToServerRequest setupClientObservedTimeMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id
);

grpc_chat_commands::ClientMessageToServerRequest setupClientUserActivityDetectedMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id
);