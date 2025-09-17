//
// Created by jeremiah on 8/2/22.
//

#pragma once

#include <bsoncxx/oid.hpp>

#include "ChatRoomCommands.pb.h"

void addTextReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid,
        const std::string& message_text
);

void addPictureReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid,
        const std::string& thumbnail_in_bytes
);

void addLocationReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid
);

void addInviteReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid
);

void addMimeTypeReplyToRequest(
        ActiveMessageInfo* active_message_info,
        const bsoncxx::oid& reply_is_sent_from_user_oid,
        const std::string& reply_is_to_message_uuid,
        const std::string& thumbnail_in_bytes,
        const std::string& mime_type
);