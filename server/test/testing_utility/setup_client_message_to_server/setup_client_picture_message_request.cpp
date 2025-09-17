//
// Created by jeremiah on 8/1/22.
//

#include "setup_login_info.h"
#include "utility_general_functions.h"

#include "setup_client_message_to_server.h"

grpc_chat_commands::ClientMessageToServerRequest setupClientPictureMessageRequest(
        const bsoncxx::oid& login_account_oid,
        const std::string& login_account_token,
        const std::string& login_installation_id,
        const std::string& message_uuid,
        const std::string& chat_room_id,
        const std::string& picture_in_bytes
) {

    grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request;

    setupUserLoginInfo(
            client_message_to_server_request.mutable_login_info(),
            login_account_oid,
            login_account_token,
            login_installation_id
    );

    client_message_to_server_request.set_message_uuid(message_uuid);
    client_message_to_server_request.set_timestamp_observed(getCurrentTimestamp().count());

    StandardChatMessageInfo* standard_message_info = client_message_to_server_request.mutable_message()->mutable_standard_message_info();
    standard_message_info->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
    standard_message_info->set_message_has_complete_info(true);
    standard_message_info->set_chat_room_id_message_sent_from(chat_room_id);

    MessageSpecifics* message_specifics = client_message_to_server_request.mutable_message()->mutable_message_specifics();
    message_specifics->mutable_picture_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_picture_message()->set_image_height(rand() % 500 + 25);
    message_specifics->mutable_picture_message()->set_image_width(rand() % 500 + 25);
    message_specifics->mutable_picture_message()->set_picture_file_in_bytes(picture_in_bytes);
    message_specifics->mutable_picture_message()->set_picture_file_size((int)picture_in_bytes.size());

    return client_message_to_server_request;
}