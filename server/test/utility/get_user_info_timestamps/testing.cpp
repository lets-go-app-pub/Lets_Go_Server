//
// Created by jeremiah on 6/17/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <utility_general_functions.h>
#include <LoginFunction.pb.h>
#include <general_values.h>

#include "get_user_info_timestamps.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GetVerifiedInfoTimestamps : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_allSet) {

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = current_date;
    user_account.email_timestamp = current_date;
    user_account.gender_timestamp = current_date;
    user_account.first_name_timestamp = current_date;
    user_account.categories_timestamp = current_date;

    std::vector<long> generated_timestamps_vector;

    //fill pictures vector randomly
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        if(rand() % 2 > 0) {
            user_account.pictures.emplace_back(
                    bsoncxx::oid{},
                    current_date
                    );

            generated_timestamps_vector.emplace_back(current_date.value.count());
        } else {
            user_account.pictures.emplace_back();
            generated_timestamps_vector.emplace_back(-1L);
        }
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_FALSE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), current_timestamp.count());

    ASSERT_EQ(generated_timestamps_vector.size(), login_response.pictures_timestamps().size());

    for(int i = 0; i < (int)generated_timestamps_vector.size(); i++) {
        EXPECT_EQ(generated_timestamps_vector[i], login_response.pictures_timestamps()[i]);
    }
}

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_birthdayNotSet) {
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date value_not_set_timestamp{std::chrono::milliseconds{-1L}};
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = value_not_set_timestamp;
    user_account.email_timestamp = current_date;
    user_account.gender_timestamp = current_date;
    user_account.first_name_timestamp = current_date;
    user_account.categories_timestamp = current_date;

    std::vector<long> generated_timestamps_vector;

    //fill pictures vector randomly
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        if(rand() % 2 > 0) {
            user_account.pictures.emplace_back(
                    bsoncxx::oid{},
                    current_date
                    );

            generated_timestamps_vector.emplace_back(current_date.value.count());
        } else {
            user_account.pictures.emplace_back();
            generated_timestamps_vector.emplace_back(-1L);
        }
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), value_not_set_timestamp.value.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), current_timestamp.count());

    ASSERT_EQ(generated_timestamps_vector.size(), login_response.pictures_timestamps().size());

    for(int i = 0; i < (int)generated_timestamps_vector.size(); i++) {
        EXPECT_EQ(generated_timestamps_vector[i], login_response.pictures_timestamps()[i]);
    }
}

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_emailNotSet) {
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date value_not_set_timestamp{std::chrono::milliseconds{-1L}};
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = current_date;
    user_account.email_timestamp = value_not_set_timestamp;
    user_account.gender_timestamp = current_date;
    user_account.first_name_timestamp = current_date;
    user_account.categories_timestamp = current_date;

    std::vector<long> generated_timestamps_vector;

    //fill pictures vector randomly
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        if(rand() % 2 > 0) {
            user_account.pictures.emplace_back(
                    bsoncxx::oid{},
                    current_date
                    );

            generated_timestamps_vector.emplace_back(current_date.value.count());
        } else {
            user_account.pictures.emplace_back();
            generated_timestamps_vector.emplace_back(-1L);
        }
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), value_not_set_timestamp.value.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), current_timestamp.count());

    ASSERT_EQ(generated_timestamps_vector.size(), login_response.pictures_timestamps().size());

    for(int i = 0; i < (int)generated_timestamps_vector.size(); i++) {
        EXPECT_EQ(generated_timestamps_vector[i], login_response.pictures_timestamps()[i]);
    }
}

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_genderNotSet) {
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date value_not_set_timestamp{std::chrono::milliseconds{-1L}};
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = current_date;
    user_account.email_timestamp = current_date;
    user_account.gender_timestamp = value_not_set_timestamp;
    user_account.first_name_timestamp = current_date;
    user_account.categories_timestamp = current_date;

    std::vector<long> generated_timestamps_vector;

    //fill pictures vector randomly
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        if(rand() % 2 > 0) {
            user_account.pictures.emplace_back(
                    bsoncxx::oid{},
                    current_date
                    );

            generated_timestamps_vector.emplace_back(current_date.value.count());
        } else {
            user_account.pictures.emplace_back();
            generated_timestamps_vector.emplace_back(-1L);
        }
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), value_not_set_timestamp.value.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), current_timestamp.count());

    ASSERT_EQ(generated_timestamps_vector.size(), login_response.pictures_timestamps().size());

    for(int i = 0; i < (int)generated_timestamps_vector.size(); i++) {
        EXPECT_EQ(generated_timestamps_vector[i], login_response.pictures_timestamps()[i]);
    }
}

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_nameNotSet) {
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date value_not_set_timestamp{std::chrono::milliseconds{-1L}};
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = current_date;
    user_account.email_timestamp = current_date;
    user_account.gender_timestamp = current_date;
    user_account.first_name_timestamp = value_not_set_timestamp;
    user_account.categories_timestamp = current_date;

    std::vector<long> generated_timestamps_vector;

    //fill pictures vector randomly
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        if(rand() % 2 > 0) {
            user_account.pictures.emplace_back(
                    bsoncxx::oid{},
                    current_date
                    );

            generated_timestamps_vector.emplace_back(current_date.value.count());
        } else {
            user_account.pictures.emplace_back();
            generated_timestamps_vector.emplace_back(-1L);
        }
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), value_not_set_timestamp.value.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), current_timestamp.count());

    ASSERT_EQ(generated_timestamps_vector.size(), login_response.pictures_timestamps().size());

    for(int i = 0; i < (int)generated_timestamps_vector.size(); i++) {
        EXPECT_EQ(generated_timestamps_vector[i], login_response.pictures_timestamps()[i]);
    }
}

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_categoriesNotSet) {

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date value_not_set_timestamp{std::chrono::milliseconds{-1L}};
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = current_date;
    user_account.email_timestamp = current_date;
    user_account.gender_timestamp = current_date;
    user_account.first_name_timestamp = current_date;
    user_account.categories_timestamp = value_not_set_timestamp;

    std::vector<long> generated_timestamps_vector;

    //fill pictures vector randomly
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        if(rand() % 2 > 0) {
            user_account.pictures.emplace_back(
                    bsoncxx::oid{},
                    current_date
            );

            generated_timestamps_vector.emplace_back(current_date.value.count());
        } else {
            user_account.pictures.emplace_back();
            generated_timestamps_vector.emplace_back(-1L);
        }
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), value_not_set_timestamp.value.count());

    ASSERT_EQ(generated_timestamps_vector.size(), login_response.pictures_timestamps().size());

    for(int i = 0; i < (int)generated_timestamps_vector.size(); i++) {
        EXPECT_EQ(generated_timestamps_vector[i], login_response.pictures_timestamps()[i]);
    }

}

TEST_F(GetVerifiedInfoTimestamps, getUserInfoTimestamps_noPicturesSet) {

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::types::b_date current_date{current_timestamp};

    UserAccountDoc user_account;
    user_account.birthday_timestamp = current_date;
    user_account.email_timestamp = current_date;
    user_account.gender_timestamp = current_date;
    user_account.first_name_timestamp = current_date;
    user_account.categories_timestamp = current_date;

    //leave pictures empty
    for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
        user_account.pictures.emplace_back();
    }

    bsoncxx::builder::stream::document account_doc;
    user_account.convertToDocument(account_doc);

    loginfunction::LoginResponse login_response;

    bool return_val = getUserInfoTimestamps(
            account_doc,
            login_response.mutable_pre_login_timestamps(),
            login_response.mutable_pictures_timestamps()
            );

    EXPECT_FALSE(return_val);

    EXPECT_EQ(login_response.pre_login_timestamps().birthday_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().email_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().gender_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().name_timestamp(), current_timestamp.count());
    EXPECT_EQ(login_response.pre_login_timestamps().categories_timestamp(), current_timestamp.count());

    ASSERT_EQ(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT, login_response.pictures_timestamps().size());

    for(const long timestamp : login_response.pictures_timestamps()) {
        EXPECT_EQ(timestamp, -1L);
    }

}