//
// Created by jeremiah on 6/1/22.
//

#include "generate_randoms.h"

ChatMessagePictureDoc generateRandomChatPicture(
        const std::string& chat_room_id,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid&
        ) {

    ChatMessagePictureDoc chat_picture;

    chat_picture.chat_room_id = chat_room_id;
    chat_picture.current_object_oid = bsoncxx::oid{};

    chat_picture.picture_in_bytes = gen_random_alpha_numeric_string(rand() % 500);
    chat_picture.picture_size_in_bytes = chat_picture.picture_in_bytes.size();

    chat_picture.width = rand() % 2000 + 50;
    chat_picture.height = rand() % 2000 + 50;

    chat_picture.timestamp_stored = bsoncxx::types::b_date{
        current_timestamp - std::chrono::milliseconds{(rand() % 1000) * 60L * 1000L}
    };

    return chat_picture;
}