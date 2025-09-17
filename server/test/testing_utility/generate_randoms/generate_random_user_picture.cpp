//
// Created by jeremiah on 6/1/22.
//

#include "generate_randoms.h"

UserPictureDoc generateRandomUserPicture(
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::oid&
) {
    UserPictureDoc user_picture;

    user_picture.current_object_oid = bsoncxx::oid{};
    user_picture.user_account_reference = bsoncxx::oid{};

    const size_t size = rand() % 20;
    for (size_t i = 0; i < size; i++) {
        user_picture.thumbnail_references.emplace_back(generateRandomChatRoomId());
    }

    std::string thumbnail_in_bytes = gen_random_alpha_numeric_string(rand() % 500);

    user_picture.timestamp_stored = bsoncxx::types::b_date{
            currentTimestamp - std::chrono::milliseconds{(rand() % 1000) * 60L * 1000L}};
    user_picture.picture_index = 0;
    user_picture.thumbnail_in_bytes = thumbnail_in_bytes;
    user_picture.thumbnail_size_in_bytes = (int) thumbnail_in_bytes.size();
    user_picture.picture_in_bytes = gen_random_alpha_numeric_string(thumbnail_in_bytes.size() + 1 + rand() % 500); //make pic longer than thumbnail
    user_picture.picture_size_in_bytes = (int) user_picture.picture_in_bytes.size();

    return user_picture;
}