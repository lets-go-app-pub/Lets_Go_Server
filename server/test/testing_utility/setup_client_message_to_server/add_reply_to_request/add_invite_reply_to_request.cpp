//
// Created by jeremiah on 8/2/22.
//

#include "setup_login_info.h"

#include "add_reply_to_request.h"

void addInviteReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid
) {
    active_message_info->set_is_reply(true);
    active_message_info->mutable_reply_info()->set_reply_is_sent_from_user_oid(reply_is_sent_from_user_oid.to_string());
    active_message_info->mutable_reply_info()->set_reply_is_to_message_uuid(reply_is_to_message_uuid);
    active_message_info->mutable_reply_info()->mutable_reply_specifics()->mutable_invite_reply();
}