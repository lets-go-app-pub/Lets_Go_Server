//
// Created by jeremiah on 8/20/22.
//


#include "generate_randoms.h"
#include "general_values.h"

void generateRandomLargestPossibleUserAccount(
        UserAccountDoc& user_account_doc,
        const std::chrono::milliseconds& category_time_frame_start_time
        ) {

    user_account_doc.first_name = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME);
    user_account_doc.gender = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES);
    user_account_doc.city = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES);
    user_account_doc.bio = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO);

    user_account_doc.categories.clear();

    for(int i = 0; i < server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT; ++i) {
        user_account_doc.categories.emplace_back(
                TestCategory(AccountCategoryType::ACTIVITY_TYPE, i)
        );
        user_account_doc.categories.emplace_back(
                TestCategory(AccountCategoryType::CATEGORY_TYPE, i)
        );
        for(long j = 0; j < server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY; ++j) {

            TestTimeframe start_time(j * 10 + category_time_frame_start_time.count(),1);
            TestTimeframe stop_time(j * 10 + 5 + category_time_frame_start_time.count(),-1);

            user_account_doc.categories.back().time_frames.emplace_back(start_time);
            user_account_doc.categories.back().time_frames.emplace_back(stop_time);

            user_account_doc.categories[user_account_doc.categories.size()-2].time_frames.emplace_back(start_time);
            user_account_doc.categories[user_account_doc.categories.size()-2].time_frames.emplace_back(stop_time);
        }
    }

    std::sort(user_account_doc.categories.begin(), user_account_doc.categories.end(), [](
            const TestCategory& lhs, const TestCategory& rhs
    ){
        return lhs.type < rhs.type;
    });

    user_account_doc.pictures.clear();

    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; ++i) {
        UserPictureDoc picture_doc;

        std::string thumbnail = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES);
        std::string picture = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES);
        auto timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 100 + 5}};

        picture_doc.user_account_reference = user_account_doc.current_object_oid;
        picture_doc.timestamp_stored = timestamp_stored;
        picture_doc.picture_index = i;
        picture_doc.thumbnail_in_bytes = thumbnail;
        picture_doc.thumbnail_size_in_bytes = (int)thumbnail.size();
        picture_doc.picture_in_bytes = picture;
        picture_doc.picture_size_in_bytes = (int)picture.size();

        picture_doc.setIntoCollection();

        user_account_doc.pictures.emplace_back(
                picture_doc.current_object_oid,
                timestamp_stored
        );
    }

    user_account_doc.setIntoCollection();
}