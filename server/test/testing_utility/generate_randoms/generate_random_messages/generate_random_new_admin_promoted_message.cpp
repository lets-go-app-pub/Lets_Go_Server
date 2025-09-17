//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"

grpc_chat_commands::PromoteNewAdminRequest generateRandomNewAdminPromotedUpdatedMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const std::string& new_admin_account_oid
        ) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    grpc_chat_commands::PromoteNewAdminRequest promote_new_admin_request;
    grpc_chat_commands::PromoteNewAdminResponse promote_new_admin_response;

    setupUserLoginInfo(
            promote_new_admin_request.mutable_login_info(),
            account_oid,
            logged_in_token,
            installation_id
            );

    promote_new_admin_request.set_chat_room_id(chat_room_id);
    promote_new_admin_request.set_new_admin_account_oid(new_admin_account_oid);
    promote_new_admin_request.set_message_uuid(actual_message_uuid);

    promoteNewAdmin(&promote_new_admin_request, &promote_new_admin_response);

    return promote_new_admin_request;
}