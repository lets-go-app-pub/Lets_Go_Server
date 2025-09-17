//
// Created by jeremiah on 8/1/22.
//
#include "setup_login_info.h"
#include "utility_general_functions.h"

#include "setup_client_message_to_server.h"

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
    message_specifics->mutable_invite_message()->mutable_active_message_info()->set_is_reply(false);
    message_specifics->mutable_invite_message()->set_invited_user_account_oid(invited_user_account_oid);
    message_specifics->mutable_invite_message()->set_invited_user_name(invited_user_name);
    message_specifics->mutable_invite_message()->set_chat_room_id(invited_chat_room_id);
    message_specifics->mutable_invite_message()->set_chat_room_name(invited_chat_room_name);
    message_specifics->mutable_invite_message()->set_chat_room_password(invited_chat_room_password);

    return client_message_to_server_request;
}