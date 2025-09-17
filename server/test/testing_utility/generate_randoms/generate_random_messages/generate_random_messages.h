//
// Created by jeremiah on 6/20/22.
//

#pragma once

#include <reports_objects.h>
#include <string>
#include <utility_general_functions.h>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>

//generates a random text messages and runs it using sendMessageToClient, expects valid login info
std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomTextMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid = "",
        const std::string& text = "",
        bool send_message_to_server = true
);

//generates a random text messages and runs it using sendMessageToClient, expects valid login info
std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomPictureMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid = "",
        const std::string& picture_in_bytes = ""
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomLocationMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid = ""
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomMimeTypeMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid = "",
        const std::string& mime_type = "",
        const std::string& url_of_download = ""
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse> generateRandomInviteTypeMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& invited_user_account_oid = "",
        const std::string& invited_user_name = "",
        const std::string& invited_chat_room_id = "",
        const std::string& invited_chat_room_name = "",
        const std::string& invited_chat_room_password = ""
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse>
generateRandomEditedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& edited_message_uuid,
        const std::string& edited_message_text = ""
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse>
generateRandomDeletedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& deleted_message_uuid,
        DeleteType delete_type = DeleteType::DELETE_TYPE_NOT_SET
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse>
generateRandomUserActivityDetectedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid
);

std::pair<grpc_chat_commands::ClientMessageToServerRequest, grpc_chat_commands::ClientMessageToServerResponse>
        generateRandomUpdateObservedTimeMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid
);

grpc_chat_commands::RemoveFromChatRoomRequest generateRandomUserKickedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& account_oid_to_remove
);

grpc_chat_commands::RemoveFromChatRoomRequest generateRandomUserBannedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& account_oid_to_remove
);

grpc_chat_commands::UpdateChatRoomInfoRequest generateRandomChatRoomNameUpdatedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& new_chat_room_name = ""
);

grpc_chat_commands::UpdateChatRoomInfoRequest generateRandomChatRoomPasswordUpdatedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& new_chat_room_password = ""
);

grpc_chat_commands::PromoteNewAdminRequest generateRandomNewAdminPromotedUpdatedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& new_admin_account_oid
);

grpc_chat_commands::SetPinnedLocationRequest generateRandomNewPinnedLocationMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        double longitude = chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE,
        double latitude = chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE
);

grpc_chat_commands::UnMatchRequest generateRandomMatchCanceledMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& matched_account_oid
);

