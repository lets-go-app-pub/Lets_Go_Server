//
// Created by jeremiah on 8/2/22.
//

#include "setup_login_info.h"

#include "add_reply_to_request.h"

void addPictureReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid,
        const std::string& thumbnail_in_bytes
) {
    active_message_info->set_is_reply(true);
    active_message_info->mutable_reply_info()->set_reply_is_sent_from_user_oid(reply_is_sent_from_user_oid.to_string());
    active_message_info->mutable_reply_info()->set_reply_is_to_message_uuid(reply_is_to_message_uuid);

    auto reply_msg = active_message_info->mutable_reply_info()->mutable_reply_specifics()->mutable_picture_reply();
    reply_msg->set_thumbnail_file_size(thumbnail_in_bytes.size());
    reply_msg->set_thumbnail_in_bytes(thumbnail_in_bytes);
}