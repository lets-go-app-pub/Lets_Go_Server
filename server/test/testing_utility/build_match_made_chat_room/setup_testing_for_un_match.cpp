//
// Created by jeremiah on 8/24/22.
//

#include "build_match_made_chat_room.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"

void SetupTestingForUnMatch::initialize() {
    generated_account_oid = insertRandomAccounts(1, 0);
    second_generated_account_oid = insertRandomAccounts(1, 0);

    user_account.getFromCollection(generated_account_oid);
    second_user_account.getFromCollection(second_generated_account_oid);

    current_timestamp = getCurrentTimestamp();

    matching_chat_room_id = buildMatchMadeChatRoom(
            current_timestamp,
            generated_account_oid,
            second_generated_account_oid,
            user_account,
            second_user_account
    );

    original_chat_room_header.getFromCollection(matching_chat_room_id);

    //get UserPictureDoc object for the generated user's thumbnail
    for (const auto& pic : user_account.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            thumbnail_picture.getFromCollection(picture_reference);

            break;
        }
    }

    //get UserPictureDoc object for the generated user's thumbnail
    for (const auto& pic : second_user_account.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            second_thumbnail_picture.getFromCollection(picture_reference);

            break;
        }
    }

    //sleep to make sure 'current_timestamp' is later than kDifferentUserJoined message sent by createChatRoom()
    std::this_thread::sleep_for(std::chrono::milliseconds{20});

    current_timestamp = getCurrentTimestamp();
}