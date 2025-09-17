//
// Created by jeremiah on 6/17/22.
//

#include <general_values.h>
#include <fstream>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <account_objects.h>
#include <generate_randoms.h>
#include <generate_multiple_random_accounts.h>

#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "extract_thumbnail_from_verified_doc.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ExtractThumbnailFromVerifiedDoc : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(ExtractThumbnailFromVerifiedDoc, extractThumbnailFromVerifiedDoc_doNothing) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::oid actual_thumbnail_reference;
    bsoncxx::types::b_date actual_thumbnail_timestamp{std::chrono::milliseconds{-1L}};
    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {

            pic.getPictureReference(
                actual_thumbnail_reference,
                actual_thumbnail_timestamp
            );

            break;
        }
    }

    ASSERT_NE(actual_thumbnail_timestamp.value.count(), -1L);

    UserPictureDoc user_thumbnail_doc(actual_thumbnail_reference);

    bsoncxx::builder::stream::document user_account_doc;
    generated_user_account.convertToDocument(user_account_doc);

    std::string returned_thumbnail;
    int returned_thumbnail_size = 0;
    std::string returned_reference_oid;
    int returned_index = -1;
    std::chrono::milliseconds returned_thumbnail_timestamp{-1L};

    auto set_thumbnail_values = [&](
            std::string& _thumbnail,
            int thumbnail_size,
            const std::string& _thumbnail_reference_oid,
            const int _index,
            const std::chrono::milliseconds& _thumbnail_timestamp
            ) {
        returned_thumbnail = _thumbnail;
        returned_thumbnail_size = thumbnail_size;
        returned_reference_oid = _thumbnail_reference_oid;
        returned_index = _index;
        returned_thumbnail_timestamp = _thumbnail_timestamp;
    };

    bool return_val = extractThumbnailFromUserAccountDoc(
            accounts_db, user_account_doc, generated_account_oid,
            nullptr, set_thumbnail_values
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(user_thumbnail_doc.thumbnail_in_bytes, returned_thumbnail);
    EXPECT_EQ(user_thumbnail_doc.thumbnail_size_in_bytes, returned_thumbnail_size);
    EXPECT_EQ(user_thumbnail_doc.current_object_oid.to_string(), returned_reference_oid);
    EXPECT_EQ(user_thumbnail_doc.picture_index, returned_index);
    EXPECT_EQ(user_thumbnail_doc.timestamp_stored.value.count(), returned_thumbnail_timestamp.count());

    //make sure picture doc has not changed
    UserPictureDoc after_user_thumbnail_doc(actual_thumbnail_reference);
    EXPECT_EQ(after_user_thumbnail_doc, user_thumbnail_doc);

}

TEST_F(ExtractThumbnailFromVerifiedDoc, extractThumbnailFromVerifiedDoc_add) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::oid actual_thumbnail_reference;
    bsoncxx::types::b_date actual_thumbnail_timestamp{std::chrono::milliseconds{-1L}};
    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {

            pic.getPictureReference(
                    actual_thumbnail_reference,
                    actual_thumbnail_timestamp
                    );

            break;
        }
    }

    ASSERT_NE(actual_thumbnail_timestamp.value.count(), -1L);

    UserPictureDoc user_thumbnail_doc(actual_thumbnail_reference);

    bsoncxx::builder::stream::document user_account_doc;
    generated_user_account.convertToDocument(user_account_doc);

    std::string returned_thumbnail;
    int returned_thumbnail_size = 0;
    std::string returned_reference_oid;
    int returned_index = -1;
    std::chrono::milliseconds returned_thumbnail_timestamp{-1L};

    auto set_thumbnail_values = [&](
            std::string& _thumbnail,
            int thumbnail_size,
            const std::string& _thumbnail_reference_oid,
            const int _index,
            const std::chrono::milliseconds& _thumbnail_timestamp
            ) {
        returned_thumbnail = _thumbnail;
        returned_thumbnail_size = thumbnail_size;
        returned_reference_oid = _thumbnail_reference_oid;
        returned_index = _index;
        returned_thumbnail_timestamp = _thumbnail_timestamp;
    };

    const std::string chat_room_id = "12345678";

    ExtractThumbnailAdditionalCommands additional_commands;
    additional_commands.setupForAddToSet(chat_room_id);

    bool return_val = extractThumbnailFromUserAccountDoc(
            accounts_db, user_account_doc, generated_account_oid,
            nullptr, set_thumbnail_values,
            additional_commands
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(user_thumbnail_doc.thumbnail_in_bytes, returned_thumbnail);
    EXPECT_EQ(user_thumbnail_doc.thumbnail_size_in_bytes, returned_thumbnail_size);
    EXPECT_EQ(user_thumbnail_doc.current_object_oid.to_string(), returned_reference_oid);
    EXPECT_EQ(user_thumbnail_doc.picture_index, returned_index);
    EXPECT_EQ(user_thumbnail_doc.timestamp_stored.value.count(), returned_thumbnail_timestamp.count());

    user_thumbnail_doc.thumbnail_references.emplace_back(chat_room_id);

    UserPictureDoc after_user_thumbnail_doc(actual_thumbnail_reference);
    EXPECT_EQ(after_user_thumbnail_doc, user_thumbnail_doc);

}

TEST_F(ExtractThumbnailFromVerifiedDoc, extractThumbnailFromVerifiedDoc_addList) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::oid actual_thumbnail_reference;
    bsoncxx::types::b_date actual_thumbnail_timestamp{std::chrono::milliseconds{-1L}};
    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {

            pic.getPictureReference(
                    actual_thumbnail_reference,
                    actual_thumbnail_timestamp
                    );

            break;
        }
    }

    ASSERT_NE(actual_thumbnail_timestamp.value.count(), -1L);

    UserPictureDoc user_thumbnail_doc(actual_thumbnail_reference);

    bsoncxx::builder::stream::document user_account_doc;
    generated_user_account.convertToDocument(user_account_doc);

    std::string returned_thumbnail;
    int returned_thumbnail_size = 0;
    std::string returned_reference_oid;
    int returned_index = -1;
    std::chrono::milliseconds returned_thumbnail_timestamp{-1L};

    auto set_thumbnail_values = [&](
            std::string& _thumbnail,
            int thumbnail_size,
            const std::string& _thumbnail_reference_oid,
            const int _index,
            const std::chrono::milliseconds& _thumbnail_timestamp
            ) {
        returned_thumbnail = _thumbnail;
        returned_thumbnail_size = thumbnail_size;
        returned_reference_oid = _thumbnail_reference_oid;
        returned_index = _index;
        returned_thumbnail_timestamp = _thumbnail_timestamp;
    };

    const std::vector<std::string> chat_room_id_list{"12345678", "abcdefgh", "chatRoom"};

    bsoncxx::builder::basic::array chat_room_id_list_arr;

    for(const std::string& chat_room_id : chat_room_id_list) {
        chat_room_id_list_arr.append(chat_room_id);
    }

    ExtractThumbnailAdditionalCommands additional_commands;
    additional_commands.setupForAddArrayValuesToSet(chat_room_id_list_arr);

    bool return_val = extractThumbnailFromUserAccountDoc(
            accounts_db, user_account_doc, generated_account_oid,
            nullptr, set_thumbnail_values,
            additional_commands
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(user_thumbnail_doc.thumbnail_in_bytes, returned_thumbnail);
    EXPECT_EQ(user_thumbnail_doc.thumbnail_size_in_bytes, returned_thumbnail_size);
    EXPECT_EQ(user_thumbnail_doc.current_object_oid.to_string(), returned_reference_oid);
    EXPECT_EQ(user_thumbnail_doc.picture_index, returned_index);
    EXPECT_EQ(user_thumbnail_doc.timestamp_stored.value.count(), returned_thumbnail_timestamp.count());

    user_thumbnail_doc.thumbnail_references.insert(
            user_thumbnail_doc.thumbnail_references.end(),
            chat_room_id_list.begin(),
            chat_room_id_list.end()
    );

    UserPictureDoc after_user_thumbnail_doc(actual_thumbnail_reference);
    EXPECT_EQ(after_user_thumbnail_doc, user_thumbnail_doc);
}

TEST_F(ExtractThumbnailFromVerifiedDoc, extractThumbnailFromVerifiedDoc_update) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::oid actual_thumbnail_reference;
    bsoncxx::types::b_date actual_thumbnail_timestamp{std::chrono::milliseconds{-1L}};
    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {

            pic.getPictureReference(
                    actual_thumbnail_reference,
                    actual_thumbnail_timestamp
                    );

            break;
        }
    }

    ASSERT_NE(actual_thumbnail_timestamp.value.count(), -1L);

    UserPictureDoc user_thumbnail_doc(actual_thumbnail_reference);

    bsoncxx::builder::stream::document user_account_doc;
    generated_user_account.convertToDocument(user_account_doc);

    std::string returned_thumbnail;
    int returned_thumbnail_size = 0;
    std::string returned_reference_oid;
    int returned_index = -1;
    std::chrono::milliseconds returned_thumbnail_timestamp{-1L};

    auto set_thumbnail_values = [&](
            std::string& _thumbnail,
            int _thumbnail_size,
            const std::string& _thumbnail_reference_oid,
            const int _index,
            const std::chrono::milliseconds& _thumbnail_timestamp
            ) {
        returned_thumbnail = _thumbnail;
        returned_thumbnail_size = _thumbnail_size;
        returned_reference_oid = _thumbnail_reference_oid;
        returned_index = _index;
        returned_thumbnail_timestamp = _thumbnail_timestamp;
    };

    const std::string chat_room_id{"12345678"};

    UserPictureDoc previous_picture_doc = generateRandomUserPicture();
    previous_picture_doc.picture_index = 0;
    previous_picture_doc.thumbnail_references.emplace_back(chat_room_id);
    previous_picture_doc.setIntoCollection();

    ExtractThumbnailAdditionalCommands additional_commands;
    additional_commands.setupForUpdate(
            chat_room_id,
            previous_picture_doc.current_object_oid.to_string()
    );

    bool return_val = extractThumbnailFromUserAccountDoc(
            accounts_db, user_account_doc, generated_account_oid,
            nullptr, set_thumbnail_values,
            additional_commands
    );

    EXPECT_TRUE(return_val);

    EXPECT_EQ(user_thumbnail_doc.thumbnail_in_bytes, returned_thumbnail);
    EXPECT_EQ(user_thumbnail_doc.thumbnail_size_in_bytes, returned_thumbnail_size);
    EXPECT_EQ(user_thumbnail_doc.current_object_oid.to_string(), returned_reference_oid);
    EXPECT_EQ(user_thumbnail_doc.picture_index, returned_index);
    EXPECT_EQ(user_thumbnail_doc.timestamp_stored.value.count(), returned_thumbnail_timestamp.count());

    user_thumbnail_doc.thumbnail_references.emplace_back(chat_room_id);

    UserPictureDoc after_user_thumbnail_doc(actual_thumbnail_reference);
    EXPECT_EQ(after_user_thumbnail_doc, user_thumbnail_doc);

    previous_picture_doc.thumbnail_references.pop_back();
    UserPictureDoc after_previous_picture_doc(previous_picture_doc.current_object_oid);
    EXPECT_EQ(previous_picture_doc, previous_picture_doc);
}