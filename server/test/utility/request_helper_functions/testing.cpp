//
// Created by jeremiah on 6/17/22.
//

#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <generate_multiple_random_accounts.h>
#include <utility_general_functions.h>
#include <reports_objects.h>
#include <report_values.h>
#include <generate_randoms.h>
#include <report_helper_functions.h>
#include <user_account_keys.h>
#include <request_helper_functions.h>
#include <general_values.h>
#include <LoginFunction.pb.h>
#include <activities_info_keys.h>
#include <mongocxx/exception/exception.hpp>
#include "gtest/gtest.h"

#include "report_helper_functions_test.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RequestHelperFunctions : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(RequestHelperFunctions, requestBirthdayHelper) {

    int year = 1986;
    int month = 10;
    int day_of_month = 23;
    int age = 35;

    bsoncxx::builder::stream::document builder;

    builder
            << user_account_keys::BIRTH_YEAR << year
            << user_account_keys::BIRTH_MONTH << month
            << user_account_keys::BIRTH_DAY_OF_MONTH << day_of_month
            << user_account_keys::AGE << age;

    BirthdayMessage response;

    bool return_val = requestBirthdayHelper(
            builder,
            &response
    );

    EXPECT_TRUE(return_val);
    EXPECT_EQ(response.birth_year(), year);
    EXPECT_EQ(response.birth_month(), month);
    EXPECT_EQ(response.birth_day_of_month(), day_of_month);
    EXPECT_EQ(response.age(), age);
}

TEST_F(RequestHelperFunctions, requestEmailHelper) {
    std::string email = "email@email.com";
    bool email_requires_verification = false;

    bsoncxx::builder::stream::document builder;

    builder
            << user_account_keys::EMAIL_ADDRESS << email
            << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << email_requires_verification;

    EmailMessage response;

    bool return_val = requestEmailHelper(
            builder,
            &response
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(response.email(), email);
    EXPECT_EQ(response.requires_email_verification(), email_requires_verification);
}

TEST_F(RequestHelperFunctions, requestGenderHelper) {
    std::string gender = "gender_555";

    bsoncxx::builder::stream::document builder;

    builder
            << user_account_keys::GENDER << gender;

    std::string response;

    bool return_val = requestGenderHelper(
            builder,
            &response
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(response, gender);
}

TEST_F(RequestHelperFunctions, requestNameHelper) {
    std::string name = "Name";

    bsoncxx::builder::stream::document builder;

    builder
            << user_account_keys::FIRST_NAME << name;

    std::string response;

    bool return_val = requestNameHelper(
            builder,
            &response
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(response, name);

}

TEST_F(RequestHelperFunctions, requestPostLoginInfoHelper) {

    std::string user_bio = "this is a bio";
    std::string user_city = "city name";
    int min_age = 21;
    int max_age = 34;
    int max_distance = 32;
    std::vector<std::string> gender_range_vector{general_values::MALE_GENDER_VALUE, "gender_444"};

    bsoncxx::builder::basic::array gender_range_arr;

    for (const std::string& gender : gender_range_vector) {
        gender_range_arr.append(gender);
    }

    bsoncxx::builder::stream::document builder;

    builder
            << user_account_keys::BIO << user_bio
            << user_account_keys::CITY << user_city
            << user_account_keys::AGE_RANGE << open_document
            << user_account_keys::age_range::MIN << min_age
            << user_account_keys::age_range::MAX << max_age
            << close_document
            << user_account_keys::MAX_DISTANCE << max_distance
            << user_account_keys::GENDERS_RANGE << gender_range_arr;

    PostLoginMessage response;

    bool return_val = requestPostLoginInfoHelper(
            builder,
            &response
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(response.user_bio(), user_bio);
    EXPECT_EQ(response.user_city(), user_city);
    EXPECT_EQ(response.min_age(), min_age);
    EXPECT_EQ(response.max_age(), max_age);
    EXPECT_EQ(response.max_distance(), max_distance);

    ASSERT_EQ(gender_range_vector.size(), response.gender_range_size());

    for (int i = 0; i < response.gender_range_size(); i++) {
        EXPECT_EQ(gender_range_vector[i], response.gender_range()[i]);
    }
}

TEST_F(RequestHelperFunctions, requestCategoriesHelper) {

    bsoncxx::builder::basic::array categories_arr;

    categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 1
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << close_array
                    << finalize
    );

    categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 4
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{500}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{600}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{900}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << close_array
                    << finalize
    );

    categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 7
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{1000}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{2000}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << close_array
                    << finalize
    );

    //theoretically includes activity index value 1
    categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 1
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << close_array
                    << finalize
    );

    //theoretically includes activity index values 5 and 7
    categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 2
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{500}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{600}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{900}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{1000}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document
                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{2000}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document
                    << close_array
                    << finalize
    );

    bsoncxx::builder::stream::document builder;

    builder
            << user_account_keys::CATEGORIES << categories_arr;

    request_fields::CategoriesResponse response;

    bool return_val = requestCategoriesHelper(
            builder,
            response.mutable_categories_array()
    );

    EXPECT_TRUE(return_val);

    int current_index = 0;

    for (const auto& category : categories_arr.view()) {
        if (AccountCategoryType(category[user_account_keys::categories::TYPE].get_int32().value)
            == AccountCategoryType::ACTIVITY_TYPE) {

            ASSERT_TRUE(current_index < response.categories_array_size());
            EXPECT_EQ(response.categories_array()[current_index].activity_index(),
                      category[user_account_keys::categories::INDEX_VALUE].get_int32().value);

            bsoncxx::array::view time_frames_arr = category[user_account_keys::categories::TIMEFRAMES].get_array().value;
            int size = (int) std::distance(time_frames_arr.begin(), time_frames_arr.end());

            int current_time_frame_index = 0;

            for (int i = 0; i < size; i++) {
                bsoncxx::document::view time_frame_doc = time_frames_arr[i].get_document().value;

                long time = time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value;
                int start_stop_val = time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value;

                if (start_stop_val == -1) {
                    CategoryTimeFrameMessage time_frame_obj = response.categories_array()[current_index].time_frame_array()[current_time_frame_index];
                    EXPECT_EQ(time_frame_obj.start_time_frame(), -1);
                    EXPECT_EQ(time_frame_obj.stop_time_frame(), time);
                } else { //start time
                    CategoryTimeFrameMessage time_frame_obj = response.categories_array()[current_index].time_frame_array()[current_time_frame_index];
                    EXPECT_EQ(time_frame_obj.start_time_frame(), time);

                    i++;
                    time_frame_doc = time_frames_arr[i].get_document().value;
                    time = time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value;

                    EXPECT_EQ(time_frame_obj.stop_time_frame(), time);
                }

                current_time_frame_index++;
            }

            current_index++;
        }
    }
}

TEST_F(RequestHelperFunctions, requestOutOfDateIconIndexValues) {
    //NOTE: This is just a wrapper for buildFindOutdatedIconIndexes(), it will be tested elsewhere.
}

TEST_F(RequestHelperFunctions, requestAllServerCategoriesActivitiesHelper) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database fake_accounts_db = mongo_cpp_client["GENERATED_DATABASE"];
    fake_accounts_db.drop();

    mongocxx::collection activitiesInfoCollection = fake_accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

    std::string category_display_name = "first_category";
    std::string category_icon_display_name = "first_category_icon";
    double category_order_number = 21;
    int category_min_age = 13;
    std::string category_color = "#123456";

    std::string activity_one_display_name = "first";
    std::string activity_one_icon_display_name = "first_icon";
    int activity_one_min_age = 13;
    int activity_one_category_index = 1;
    int activity_one_icon_index = 4;

    std::string activity_two_display_name = "sec";
    std::string activity_two_icon_display_name = "sec_icon";
    int activity_two_min_age = 22;
    int activity_two_category_index = 1;
    int activity_two_icon_index = 55;

    //NOTE: this is perfectly fine to fail here (it will throw an exception if it exists already), they simply must exist
    // for the server to run properly
    try {

        activitiesInfoCollection.insert_one(document{}
                                                    << "_id" << activities_info_keys::ID
                                                    << activities_info_keys::CATEGORIES << open_array
                                                    << open_document
                                                    << activities_info_keys::categories::DISPLAY_NAME << category_display_name
                                                    << activities_info_keys::categories::ICON_DISPLAY_NAME << category_icon_display_name
                                                    << activities_info_keys::categories::ORDER_NUMBER << bsoncxx::types::b_double{
                category_order_number}
                                                    << activities_info_keys::categories::MIN_AGE << category_min_age
                                                    << activities_info_keys::categories::COLOR << category_color
                                                    << close_document
                                                    << close_array
                                                    << activities_info_keys::ACTIVITIES << open_array
                                                    << open_document
                                                    << activities_info_keys::activities::DISPLAY_NAME << activity_one_display_name
                                                    << activities_info_keys::activities::ICON_DISPLAY_NAME << activity_one_icon_display_name
                                                    << activities_info_keys::activities::MIN_AGE << activity_one_min_age
                                                    << activities_info_keys::activities::CATEGORY_INDEX << activity_one_category_index
                                                    << activities_info_keys::activities::ICON_INDEX << activity_one_icon_index
                                                    << close_document
                                                    << open_document
                                                    << activities_info_keys::activities::DISPLAY_NAME << activity_two_display_name
                                                    << activities_info_keys::activities::ICON_DISPLAY_NAME << activity_two_icon_display_name
                                                    << activities_info_keys::activities::MIN_AGE << activity_two_min_age
                                                    << activities_info_keys::activities::CATEGORY_INDEX << activity_two_category_index
                                                    << activities_info_keys::activities::ICON_INDEX << activity_two_icon_index
                                                    << close_document
                                                    << close_array
                                                    << finalize);
    }
    catch (const mongocxx::exception& e) {
        EXPECT_EQ("", std::string(e.what()));
        ASSERT_TRUE(false);
    }

    LoginValuesToReturnToClient response;

    bool return_val = requestAllServerCategoriesActivitiesHelper(
            response.mutable_server_categories(),
            response.mutable_server_activities(),
            fake_accounts_db
    );

    EXPECT_TRUE(return_val);
    ASSERT_EQ(response.server_categories_size(), 1);
    ASSERT_EQ(response.server_activities_size(), 2);

    EXPECT_EQ(response.server_categories()[0].display_name(), category_display_name);
    EXPECT_EQ(response.server_categories()[0].icon_display_name(), category_icon_display_name);
    EXPECT_EQ(response.server_categories()[0].order_number(), category_order_number);
    EXPECT_EQ(response.server_categories()[0].min_age(), category_min_age);
    EXPECT_EQ(response.server_categories()[0].color(), category_color);

    EXPECT_EQ(response.server_activities()[0].display_name(), activity_one_display_name);
    EXPECT_EQ(response.server_activities()[0].icon_display_name(), activity_one_icon_display_name);
    EXPECT_EQ(response.server_activities()[0].min_age(), activity_one_min_age);
    EXPECT_EQ(response.server_activities()[0].category_index(), activity_one_category_index);
    EXPECT_EQ(response.server_activities()[0].icon_index(), activity_one_icon_index);

    EXPECT_EQ(response.server_activities()[1].display_name(), activity_two_display_name);
    EXPECT_EQ(response.server_activities()[1].icon_display_name(), activity_two_icon_display_name);
    EXPECT_EQ(response.server_activities()[1].min_age(), activity_two_min_age);
    EXPECT_EQ(response.server_activities()[1].category_index(), activity_two_category_index);
    EXPECT_EQ(response.server_activities()[1].icon_index(), activity_two_icon_index);

    fake_accounts_db.drop();
}

TEST_F(RequestHelperFunctions, requestPicturesHelper) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    struct PictureReturnValues{
        bsoncxx::oid picture_oid{};
        std::string pictureByteString;
        int pictureSize = 0;
        int indexNumber = 0;
        std::chrono::milliseconds picture_timestamp{-1L};
        bool extracted_from_deleted_pictures = false;
        bool references_removed_after_delete = false;

        PictureReturnValues() = default;
    };

    std::vector<int> picture_empty_return_values;
    std::vector<PictureReturnValues> picture_return_values;

    std::vector<int> picture_empty_generated;
    std::vector<PictureReturnValues> picture_generated;

    std::function<void(int)> set_picture_empty_response =
            [&](int indexNumber) {
        picture_empty_return_values.emplace_back(indexNumber);
    };

    auto set_picture_to_response =
            [&](
                    const bsoncxx::oid& picture_oid,
                    std::string&& pictureByteString,
                    int pictureSize,
                    int indexNumber,
                    const std::chrono::milliseconds& picture_timestamp,
                    bool extracted_from_deleted_pictures,
                    bool references_removed_after_delete
            ) {
        PictureReturnValues return_value;

        return_value.picture_oid = picture_oid;
        return_value.pictureByteString = pictureByteString;
        return_value.pictureSize = pictureSize;
        return_value.indexNumber = indexNumber;
        return_value.picture_timestamp = picture_timestamp;
        return_value.extracted_from_deleted_pictures = extracted_from_deleted_pictures;
        return_value.references_removed_after_delete = references_removed_after_delete;

        picture_return_values.emplace_back(return_value);
    };

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::builder::stream::document user_account_builder;
    user_account.convertToDocument(user_account_builder);

    std::set<int> all_picture_indexes;

    for (int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        all_picture_indexes.insert(i);
    }

    for(size_t i = 0; i < user_account.pictures.size(); i++) {
        if(user_account.pictures[i].pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1L}};

            user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            UserPictureDoc user_pic(picture_reference);

            PictureReturnValues return_value;

            return_value.picture_oid = picture_reference;
            return_value.pictureByteString = user_pic.picture_in_bytes;
            return_value.pictureSize = user_pic.picture_size_in_bytes;
            return_value.indexNumber = (int)i;
            return_value.picture_timestamp = user_pic.timestamp_stored.value;
            return_value.extracted_from_deleted_pictures = false;
            return_value.references_removed_after_delete = false;

            picture_generated.emplace_back(return_value);
        } else {
            picture_empty_generated.emplace_back(i);
        }
    }

    requestPicturesHelper(
            all_picture_indexes,
            accounts_db,
            generated_account_oid,
            user_account_builder,
            mongo_cpp_client,
            user_accounts_collection,
            current_timestamp,
            set_picture_empty_response,
            set_picture_to_response,
            nullptr
    );

    ASSERT_EQ(picture_empty_generated.size(), picture_empty_generated.size());

    std::sort(picture_empty_generated.begin(), picture_empty_generated.end());
    std::sort(picture_empty_return_values.begin(), picture_empty_return_values.end());

    for(int i = 0; i < (int)picture_empty_generated.size(); i++) {
        EXPECT_EQ(picture_empty_generated[i], picture_empty_return_values[i]);
    }

    ASSERT_EQ(picture_generated.size(), picture_return_values.size());

    std::sort(picture_generated.begin(), picture_generated.end(), [](const PictureReturnValues& lhs, const PictureReturnValues& rhs)->bool{
        return lhs.indexNumber < rhs.indexNumber;
    });
    std::sort(picture_return_values.begin(), picture_return_values.end(), [](const PictureReturnValues& lhs, const PictureReturnValues& rhs)->bool{
        return lhs.indexNumber < rhs.indexNumber;
    });

    for(int i = 0; i < (int)picture_return_values.size(); i++) {
        EXPECT_EQ(picture_return_values[i].picture_oid, picture_generated[i].picture_oid);
        EXPECT_EQ(picture_return_values[i].pictureByteString, picture_generated[i].pictureByteString);
        EXPECT_EQ(picture_return_values[i].pictureSize, picture_generated[i].pictureSize);
        EXPECT_EQ(picture_return_values[i].indexNumber, picture_generated[i].indexNumber);
        EXPECT_EQ(picture_return_values[i].picture_timestamp, picture_generated[i].picture_timestamp);
        EXPECT_EQ(picture_return_values[i].extracted_from_deleted_pictures, picture_generated[i].extracted_from_deleted_pictures);
        EXPECT_EQ(picture_return_values[i].references_removed_after_delete, picture_generated[i].references_removed_after_delete);
    }
}