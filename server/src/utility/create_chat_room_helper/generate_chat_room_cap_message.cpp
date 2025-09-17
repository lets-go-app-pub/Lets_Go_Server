//
// Created by jeremiah on 10/15/22.
//

#include "generate_chat_room_cap_message.h"

#include <TypeOfChatMessage.grpc.pb.h>

#include "chat_room_message_keys.h"
#include "chat_room_shared_keys.h"

bsoncxx::document::value generateChatRoomCapMessage(
        const std::string& message_uuid,
        const bsoncxx::oid& chat_room_created_by_oid,
        const std::chrono::milliseconds& chat_room_cap_message_time
        ) {
    return bsoncxx::builder::stream::document{}
            << "_id" << message_uuid
            << chat_room_message_keys::MESSAGE_SENT_BY << chat_room_created_by_oid
            << chat_room_message_keys::MESSAGE_TYPE << bsoncxx::types::b_int32{MessageSpecifics::MessageBodyCase::kChatRoomCapMessage}
            << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{chat_room_cap_message_time}
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << bsoncxx::builder::stream::open_document
            << bsoncxx::builder::stream::close_document
    << bsoncxx::builder::stream::finalize;
}