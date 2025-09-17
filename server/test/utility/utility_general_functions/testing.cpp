//
// Created by jeremiah on 5/28/22.
//
#include <utility_general_functions.h>
#include <server_parameter_restrictions.h>
#include <android_specific_values.h>
#include <general_values.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <account_objects.h>
#include <reports_objects.h>
#include <generate_randoms.h>
#include <mongocxx/exception/logic_error.hpp>
#include <deleted_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include <chat_rooms_objects.h>
#include <user_account_keys.h>
#include <admin_account_keys.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "utility_general_functions_test.h"
#include <google/protobuf/util/message_differencer.h>
#include <user_pictures_keys.h>

//mongo_db
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GeneralUtilityTesting : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(GeneralUtilityTesting, isInvalidGender) {
    EXPECT_EQ(isInvalidGender(""), true);
    std::string temp(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES, 'a');
    EXPECT_EQ(isInvalidGender(temp), false);
    temp.push_back('a');
    EXPECT_EQ(isInvalidGender(temp), true);
}

TEST_F(GeneralUtilityTesting, isInvalidLocation) {
    double lon = 0, lat = 0;

    EXPECT_EQ(isInvalidLocation(lon, lat), false);
    lon = -181;
    EXPECT_EQ(isInvalidLocation(lon, lat), true);
    lon = 181;
    EXPECT_EQ(isInvalidLocation(lon, lat), true);
    lon = 55;
    lat = -90;
    EXPECT_EQ(isInvalidLocation(lon, lat), true);
    lat = 90;
    EXPECT_EQ(isInvalidLocation(lon, lat), true);
    lat = 12.5;
    EXPECT_EQ(isInvalidLocation(lon, lat), false);
}

TEST_F(GeneralUtilityTesting, isValidPhoneNumber) {
    std::string phone_number;

    EXPECT_EQ(isValidPhoneNumber(phone_number), false);
    phone_number = std::string(13, '2');
    EXPECT_EQ(isValidPhoneNumber(phone_number), false);
    phone_number.pop_back();
    phone_number[0] = '+';
    EXPECT_EQ(isValidPhoneNumber(phone_number), false);
    phone_number[1] = '1';
    EXPECT_EQ(isValidPhoneNumber(phone_number), true);
    phone_number = std::string(12, '2');
    phone_number[0] = '+';
    phone_number[5] = 'a';
    EXPECT_EQ(isValidPhoneNumber(phone_number), false);
    phone_number = std::string(12, '2');
    phone_number[0] = '+';
    phone_number[7] = '+';
    EXPECT_EQ(isValidPhoneNumber(phone_number), false);
    phone_number = std::string(12, '2');
    phone_number[0] = '+';
    phone_number[1] = '1';
    phone_number[2] = '5';
    phone_number[3] = '5';
    phone_number[4] = '5';
    EXPECT_EQ(isValidPhoneNumber(phone_number), false);
    phone_number[4] = '2';
    EXPECT_EQ(isValidPhoneNumber(phone_number), true);
}

TEST_F(GeneralUtilityTesting, isInvalidBirthday) {
    std::chrono::milliseconds synthetic_time{1666483200000}; //10/23/2022 (0:0:0) GMT
    int birth_year = 1950, birth_month = 4, birth_day_of_month = 16;

    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), false);
    birth_year = 1800;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), true);
    birth_year = 2025;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), true);
    birth_year = 1950;
    birth_month = 0;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), true);
    birth_month = 13;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), true);
    birth_month = 4;
    birth_day_of_month = 0;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), true);
    birth_day_of_month = 32;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), true);
    birth_year = 2000;
    birth_month = 6;
    birth_day_of_month = 27;
    EXPECT_EQ(isInvalidBirthday(synthetic_time, birth_year, birth_month, birth_day_of_month), false);
}

TEST_F(GeneralUtilityTesting, documentViewToJson) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::builder::stream::document document_builder;

    std::string result = documentViewToJson(document_builder.view());
    EXPECT_EQ(result, "{ }");

    std::ifstream input_stream(
            "/home/jeremiah/CLionProjects/LetsGoServer/server/test/utility/utility_general_functions/picture_thumbnail.txt");

    std::string thumbnail;

    for (char c; input_stream.get(c);) {
        thumbnail += c;
    }

    document_builder
            << "key" << thumbnail;

    result = documentViewToJson(document_builder.view());
    EXPECT_EQ(result, "{\"Error\": \"Error when converting to json\"}");
}

TEST_F(GeneralUtilityTesting, calculateAgeRangeFromUserAge) {
    int age = 13;

    AgeRangeDataObject result = calculateAgeRangeFromUserAge(age);
    AgeRangeDataObject expected{13, 15};
    EXPECT_EQ(result, expected);
    age = 14;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{13, 16};
    EXPECT_EQ(result, expected);
    age = 15;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{13, 17};
    EXPECT_EQ(result, expected);
    age = 16;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{14, 18};
    EXPECT_EQ(result, expected);
    age = 17;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{15, 19};
    EXPECT_EQ(result, expected);
    age = 18;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{16, 20};
    EXPECT_EQ(result, expected);
    age = 19;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{17, 21};
    EXPECT_EQ(result, expected);
    age = 20;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{18, 27};
    EXPECT_EQ(result, expected);
    age = 21;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{18, 28};
    EXPECT_EQ(result, expected);
    age = 22;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{18, 29};
    EXPECT_EQ(result, expected);
    age = 23;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{18, 30};
    EXPECT_EQ(result, expected);
    age = 24;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{18, 31};
    EXPECT_EQ(result, expected);
    age = 25;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{18, 32};
    EXPECT_EQ(result, expected);
    age = 26;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{19, 33};
    EXPECT_EQ(result, expected);

    age = android_specific_values::HIGHEST_DISPLAYED_AGE + general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{android_specific_values::HIGHEST_DISPLAYED_AGE,
                                  server_parameter_restrictions::HIGHEST_ALLOWED_AGE};
    EXPECT_EQ(result, expected);

    age--;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{android_specific_values::HIGHEST_DISPLAYED_AGE - 1,
                                  server_parameter_restrictions::HIGHEST_ALLOWED_AGE};
    EXPECT_EQ(result, expected);

    age--;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{android_specific_values::HIGHEST_DISPLAYED_AGE - 2,
                                  server_parameter_restrictions::HIGHEST_ALLOWED_AGE};
    EXPECT_EQ(result, expected);

    age--;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{android_specific_values::HIGHEST_DISPLAYED_AGE - 3,
                                  server_parameter_restrictions::HIGHEST_ALLOWED_AGE};
    EXPECT_EQ(result, expected);

    age = android_specific_values::HIGHEST_DISPLAYED_AGE - general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{
            android_specific_values::HIGHEST_DISPLAYED_AGE - 2 * general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS,
            server_parameter_restrictions::HIGHEST_ALLOWED_AGE};
    EXPECT_EQ(result, expected);

    age--;
    result = calculateAgeRangeFromUserAge(age);
    expected = AgeRangeDataObject{
            android_specific_values::HIGHEST_DISPLAYED_AGE - 2 * general_values::DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS -
            1, android_specific_values::HIGHEST_DISPLAYED_AGE - 1};
    EXPECT_EQ(result, expected);
}

TEST_F(GeneralUtilityTesting, makePrettyJson) {
    bsoncxx::builder::stream::document document_builder;

    std::string result = makePrettyJson(document_builder.view());
    EXPECT_EQ(result, "{ }");

    document_builder
            << "a" << "\"";

    result = "{\n      \"a\" : \"\"\" \n}";
    EXPECT_EQ(makePrettyJson(document_builder.view()), result);

    document_builder.clear();
    document_builder
            << "a" << "}";

    result = "{\n      \"a\" : \"}\" \n}";
    EXPECT_EQ(makePrettyJson(document_builder.view()), result);

    document_builder.clear();
    document_builder
            << "a" << "123";

    result = "{\n      \"a\" : \"123\" \n}";
    EXPECT_EQ(makePrettyJson(document_builder.view()), result);

    document_builder.clear();
    document_builder
            << "a" << 123;

    result = "{\n      \"a\" : 123 \n}";
    EXPECT_EQ(makePrettyJson(document_builder.view()), result);

    std::ifstream input_stream(
            "/home/jeremiah/CLionProjects/LetsGoServer/server/test/utility/utility_general_functions/picture_thumbnail.txt");

    std::string thumbnail;

    for (char c; input_stream.get(c);) {
        thumbnail += c;
    }

    document_builder
            << "key" << thumbnail;

    result = "{\n     \"Error\": \"Error when converting to json\"\n}";
    EXPECT_EQ(makePrettyJson(document_builder.view()), result);
}

TEST_F(GeneralUtilityTesting, serverInternalDeleteAccount_NoSession) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    //make sure fails if no session passed
    bool returnVal = serverInternalDeleteAccount(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            nullptr,
            bsoncxx::oid{},
            currentTimestamp
    );

    EXPECT_EQ(returnVal, false);
}

TEST_F(GeneralUtilityTesting, serverInternalDeleteAccount) {

    const bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    buildAndCheckDeleteAccount(
            generated_account_oid,
            [&](const std::chrono::milliseconds& current_timestamp){
                bool successful = false;

                mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* session) {
                    successful = serverInternalDeleteAccount(
                            mongo_cpp_client,
                            accounts_db,
                            user_accounts_collection,
                            session,
                            generated_account_oid,
                            current_timestamp
                    );
                };

                mongocxx::client_session session = mongo_cpp_client.start_session();

                //NOTE: this must go before the code below, this will block for the transaction to finish
                try {
                    session.with_transaction(transaction_callback);
                } catch (const mongocxx::logic_error& e) {
                    successful = false;
                    std::cout << e.what() << '\n';
                }

                return successful;
            }
    );
}

TEST_F(GeneralUtilityTesting, deleteAccountPictures) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    UserAccountDoc user_account_object(generated_account_oid);

    bsoncxx::builder::stream::document user_account_doc;
    user_account_object.convertToDocument(user_account_doc);

    std::vector<UserPictureDoc> pictures_before_delete;

    for (const UserAccountDoc::PictureReference& picture_obj : user_account_object.pictures) {
        if (picture_obj.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

            picture_obj.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            pictures_before_delete.emplace_back(UserPictureDoc(picture_reference));
        }
    }

    bool return_val = deleteAccountPictures(
            mongo_cpp_client,
            accounts_db,
            nullptr,
            currentTimestamp,
            user_account_doc.view()
    );

    int dummy_deleted_account_thumbnail_size = 0;

    checkDeleteAccountPicturesResults(
            return_val,
            pictures_before_delete,
            currentTimestamp,
            user_pictures_collection,
            dummy_deleted_account_thumbnail_size,
            "",
            ""
    );

}

/*
TEST_F(GeneralUtilityTesting, deleteAccountPictures_extractThumbnailSize) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    UserAccountDoc user_account_object(generated_account_oid);

    bsoncxx::builder::stream::document user_account_doc;
    user_account_object.convertToDocument(user_account_doc);

    std::vector<UserPictureDoc> pictures_before_delete;
    std::string thumbnail;
    std::string thumbnail_reference_oid;

    for (const UserAccountDoc::PictureReference& picture_obj : user_account_object.pictures) {
        if (picture_obj.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

            picture_obj.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            pictures_before_delete.emplace_back(UserPictureDoc(picture_reference));

            if (thumbnail.empty()) {
                thumbnail = pictures_before_delete.back().thumbnail_in_bytes;
                thumbnail_reference_oid = picture_reference.to_string();
            }
        }
    }

    int deleted_account_thumbnail_size = 0;
    std::string deleted_account_thumbnail_reference_oid;

    bool return_val = deleteAccountPictures(
            mongo_cpp_client,
            accounts_db,
            nullptr,
            currentTimestamp,
            user_account_doc.view(),
            &deleted_account_thumbnail_size,
            deleted_account_thumbnail_reference_oid
    );

    checkDeleteAccountPicturesResults(
            return_val,
            pictures_before_delete,
            currentTimestamp,
            user_pictures_collection,
            deleted_account_thumbnail_size,
            thumbnail_reference_oid,
            deleted_account_thumbnail_reference_oid
    );

}
*/

TEST_F(GeneralUtilityTesting, findAndDeletePictureDocument) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    UserPictureDoc user_picture = generateRandomUserPicture(currentTimestamp);
    user_picture.setIntoCollection();

    //TEST: findAndDeletePictureDocument() works correctly with basic parameters.

    bool return_val = findAndDeletePictureDocument(
            mongo_cpp_client,
            user_pictures_collection,
            user_picture.current_object_oid,
            currentTimestamp,
            nullptr
    );

    checkDeletePictureDocumentResult(
            currentTimestamp,
            user_pictures_collection,
            return_val,
            user_picture,
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr,
            nullptr
    );

}

/*
TEST_F(GeneralUtilityTesting, findAndDeletePictureDocument_ExtractThumbnail) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    UserPictureDoc user_picture = generateRandomUserPicture(currentTimestamp);
    user_picture.setIntoCollection();

    //TEST: findAndDeletePictureDocument() properly returns thumbnail

    user_picture.setIntoCollection();

    int thumbnail_size = 0;

    bool return_val = findAndDeletePictureDocument(
            mongo_cpp_client,
            user_pictures_collection,
            user_picture.current_object_oid,
            currentTimestamp,
            &thumbnail_size,
            nullptr
    );

    checkDeletePictureDocumentResult(
            currentTimestamp,
            user_pictures_collection,
            return_val,
            user_picture,
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr,
            &thumbnail_size
    );
}
*/

TEST_F(GeneralUtilityTesting, deletePictureDocument) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_user_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    UserPictureDoc user_picture = generateRandomUserPicture(currentTimestamp);
    user_picture.setIntoCollection();

    auto picture_doc = user_pictures_collection.find_one(
            document{} << "_id" << user_picture.current_object_oid << finalize);

    ASSERT_TRUE(picture_doc.operator bool());

    //NOTE: 'session' parameter does not change functionality.

    std::unique_ptr<std::string> admin_name = nullptr;

    bool return_val = deletePictureDocument(
            *picture_doc,
            user_pictures_collection,
            deleted_user_pictures_collection,
            user_picture.current_object_oid,
            currentTimestamp,
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            "",
            nullptr
    );

    checkDeletePictureDocumentResult(
            currentTimestamp,
            user_pictures_collection,
            return_val,
            user_picture,
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            admin_name,
            nullptr
    );

}

/*
TEST_F(GeneralUtilityTesting, deletePictureDocument_ExtractThumbnail) {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_user_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    UserPictureDoc user_picture = generateRandomUserPicture(currentTimestamp);
    user_picture.setIntoCollection();

    auto picture_doc = user_pictures_collection.find_one(
            document{} << "_id" << user_picture.current_object_oid << finalize);

    ASSERT_TRUE(picture_doc.operator bool());

    //NOTE: session does not change functionality

    std::unique_ptr<std::string> admin_name = std::make_unique<std::string>("admin_name");

    user_picture.setIntoCollection();

    //TEST: thumbnail properly extracted
    //TEST: ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE

    int thumbnail_size;

    bool return_val = deletePictureDocument(
            *picture_doc,
            user_pictures_collection,
            deleted_user_pictures_collection,
            user_picture.current_object_oid,
            currentTimestamp,
            ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
            *admin_name,
            &thumbnail_size,
            nullptr
    );

    checkDeletePictureDocumentResult(
            currentTimestamp,
            user_pictures_collection,
            return_val,
            user_picture,
            ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
            admin_name,
            &thumbnail_size
    );

}
*/

TEST_F(GeneralUtilityTesting, removePictureArrayElement_OutOfBounds) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc user_account_doc(generated_account_oid);

    bsoncxx::builder::stream::document user_account_builder;
    user_account_doc.convertToDocument(user_account_builder);

    bool return_val = removePictureArrayElement(
            general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT,
            user_accounts_collection,
            user_account_builder,
            generated_account_oid,
            nullptr
    );

    EXPECT_EQ(return_val, false);

    UserAccountDoc after_user_account_doc(generated_account_oid);

    EXPECT_EQ(after_user_account_doc, user_account_doc);
}

TEST_F(GeneralUtilityTesting, removePictureArrayElement) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc user_account_doc(generated_account_oid);

    bsoncxx::builder::stream::document user_account_builder;
    user_account_doc.convertToDocument(user_account_builder);

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

        bool return_val = removePictureArrayElement(
                0,
                user_accounts_collection,
                user_account_builder,
                generated_account_oid,
                session
        );

        EXPECT_EQ(return_val, true);
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //NOTE: this must go before the code below, this will block for the transaction to finish
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
        ASSERT_EQ(std::string(e.what()), "dummy_string");
    }

    user_account_doc.pictures[0].removePictureReference();

    UserAccountDoc after_user_account_doc(generated_account_oid);

    EXPECT_EQ(after_user_account_doc, user_account_doc);
}

TEST_F(GeneralUtilityTesting, extractMinMaxAgeRange) {

    bsoncxx::builder::stream::document builder;

    const int embedded_min_age = 21;
    const int embedded_max_age = 29;

    builder
            << user_account_keys::AGE_RANGE << open_document
            << user_account_keys::age_range::MIN << embedded_min_age
            << user_account_keys::age_range::MAX << embedded_max_age
            << close_document;

    int min_age_range = -2;
    int max_age_range = -2;

    bool return_val = extractMinMaxAgeRange(
            builder,
            min_age_range,
            max_age_range
    );

    EXPECT_EQ(return_val, true);
    EXPECT_EQ(min_age_range, embedded_min_age);
    EXPECT_EQ(max_age_range, embedded_max_age);

}

TEST_F(GeneralUtilityTesting, extractMinMaxAgeRange_invalidDoc) {

    bsoncxx::builder::stream::document builder;

    const int embedded_max_age = 29;

    builder
            << user_account_keys::AGE_RANGE << open_document
            << user_account_keys::age_range::MAX << embedded_max_age
            << close_document;

    int min_age_range = -2;
    int max_age_range = -2;

    bool return_val = extractMinMaxAgeRange(
            builder,
            min_age_range,
            max_age_range
    );

    EXPECT_EQ(return_val, false);
    EXPECT_EQ(min_age_range, -2);
    EXPECT_EQ(max_age_range, -2);

}

TEST_F(GeneralUtilityTesting, extractMinMaxAgeRange_invalidRange) {

    bsoncxx::builder::stream::document builder;

    const int embedded_min_age = 0;
    const int embedded_max_age = 29;

    builder
            << user_account_keys::AGE_RANGE << open_document
            << user_account_keys::age_range::MIN << embedded_min_age
            << user_account_keys::age_range::MAX << embedded_max_age
            << close_document;

    int min_age_range = -2;
    int max_age_range = -2;

    bool return_val = extractMinMaxAgeRange(
            builder,
            min_age_range,
            max_age_range
    );

    EXPECT_EQ(return_val, false);
    EXPECT_EQ(min_age_range, embedded_min_age);
    EXPECT_EQ(max_age_range, embedded_max_age);

}

TEST_F(GeneralUtilityTesting, checkIfUserMatchesWithEveryone_Everyone) {

    bsoncxx::builder::basic::array gendersToMatchMongoDBArray;
    gendersToMatchMongoDBArray.append(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    bsoncxx::builder::stream::document dummyVerifiedAccountDocumentView;
    dummyVerifiedAccountDocumentView
            << "dummy" << "info";

    MatchesWithEveryoneReturnValue return_val = checkIfUserMatchesWithEveryone(
            gendersToMatchMongoDBArray.view(),
            dummyVerifiedAccountDocumentView.view()
    );

    EXPECT_EQ(return_val, MatchesWithEveryoneReturnValue::USER_MATCHES_WITH_EVERYONE);
}

TEST_F(GeneralUtilityTesting, checkIfUserMatchesWithEveryone_NotEveryone) {

    bsoncxx::builder::basic::array gendersToMatchMongoDBArray;
    gendersToMatchMongoDBArray.append(general_values::MALE_GENDER_VALUE);
    gendersToMatchMongoDBArray.append("first");
    bsoncxx::builder::stream::document dummyVerifiedAccountDocumentView;
    dummyVerifiedAccountDocumentView
            << "dummy" << "info";

    MatchesWithEveryoneReturnValue return_val = checkIfUserMatchesWithEveryone(
            gendersToMatchMongoDBArray.view(),
            dummyVerifiedAccountDocumentView.view()
    );

    EXPECT_EQ(return_val, MatchesWithEveryoneReturnValue::USER_DOES_NOT_MATCH_WITH_EVERYONE);
}

TEST_F(GeneralUtilityTesting, checkIfUserMatchesWithEveryone_InvalidDoc) {

    bsoncxx::builder::basic::array gendersToMatchMongoDBArray;
    gendersToMatchMongoDBArray.append(123);
    bsoncxx::builder::stream::document dummyVerifiedAccountDocumentView;
    dummyVerifiedAccountDocumentView
            << "dummy" << "info";

    MatchesWithEveryoneReturnValue return_val = checkIfUserMatchesWithEveryone(
            gendersToMatchMongoDBArray.view(),
            dummyVerifiedAccountDocumentView.view()
    );

    EXPECT_EQ(return_val, MatchesWithEveryoneReturnValue::MATCHES_WITH_EVERYONE_ERROR_OCCURRED);
}

TEST_F(GeneralUtilityTesting, initializeTmByDate) {

    //Daylight saving active
    //Saturday, June 4, 2022 12:00:00 AM
    time_t original_timestamp = 1654300800;
    tm return_val = initializeTmByDate(2022, 6, 4);

    time_t lTimeEpoch = timegm(&return_val);

    EXPECT_EQ(original_timestamp, lTimeEpoch);

    //Daylight saving NOT active
    //Sunday, December 9, 2018 12:00:00 AM
    original_timestamp = 1544313600;
    return_val = initializeTmByDate(2018, 12, 9);

    lTimeEpoch = timegm(&return_val);

    EXPECT_EQ(original_timestamp, lTimeEpoch);

    time_t current_timestamp = getCurrentTimestamp().count() / 1000;

    //Only year,month and month_day are passed into initializeTmByDate(). This means that it will
    // never calculate hours, minutes etc... In order to fix this the 'extra' stuff is trimmed off.
    // 86400 is the number of seconds in a day. This means it will bring the timestamp to 0 for all fields
    // less than month_day.
    current_timestamp -= current_timestamp % 86400;

    tm current_tm = *gmtime(&current_timestamp);

    return_val = initializeTmByDate(current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday);

    EXPECT_EQ(current_tm.tm_sec, return_val.tm_sec);
    EXPECT_EQ(current_tm.tm_min, return_val.tm_min);
    EXPECT_EQ(current_tm.tm_hour, return_val.tm_hour);
    EXPECT_EQ(current_tm.tm_mday, return_val.tm_mday);
    EXPECT_EQ(current_tm.tm_mon, return_val.tm_mon);
    EXPECT_EQ(current_tm.tm_year, return_val.tm_year);
    EXPECT_EQ(current_tm.tm_wday, return_val.tm_wday);
    EXPECT_EQ(current_tm.tm_yday, return_val.tm_yday);
    EXPECT_EQ(current_tm.tm_isdst, return_val.tm_isdst);

    lTimeEpoch = timegm(&return_val);

    EXPECT_EQ(current_timestamp, lTimeEpoch);
}

TEST_F(GeneralUtilityTesting, calculateAge) {

    //Monday, June 6, 2022 12:00:00 AM GMT
    std::chrono::milliseconds timestamp{1654473600000};

    //BirthDay; Thursday, June 04, 2015
    int birth_year = 2015;
    int birth_month = 6;
    int birth_day_of_month = 4;
    int birth_day_of_year = 154;

    int first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    int second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    int expected_age = 7;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);

    //TEST DAY BEFORE AND AFTER AGE CHANGES

    //BirthDay; Thursday, October 23, 1986
    birth_year = 1986;
    birth_month = 10;
    birth_day_of_month = 23;
    birth_day_of_year = 295;

    //Test day before birthday (1 ms before birthday actually)
    //Saturday, October 22, 2022 11:59:59 PM GMT
    timestamp = std::chrono::milliseconds{1666483199999};

    first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    expected_age = 35;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);

    //Test birthday (1 ms before birthday actually)
    //Sunday, October 23, 2022 12:00:00 PM GMT
    timestamp = std::chrono::milliseconds{1666483200000};

    first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    expected_age = 36;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);

    //BirthDay; Friday, February 29, 2008 (Extra day during a leap year)
    birth_year = 2008;
    birth_month = 2;
    birth_day_of_month = 29;
    birth_day_of_year = 59;

    //TEST ON NON LEAP YEAR

    //Test before birthday (1 ms before birthday)
    //Monday, February 28, 2022 11:59:59 PM
    timestamp = std::chrono::milliseconds{1646092799999};

    first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    expected_age = 13;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);

    //Test birthday (no leap year, so March instead of February).
    //Tuesday, March 1, 2022 12:00:00 AM (day after leap year
    timestamp = std::chrono::milliseconds{1646092800000};

    first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    expected_age = 14;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);

    //TEST ON LEAP YEAR

    //Wednesday, February 28, 2024 11:59:59 PM
    timestamp = std::chrono::milliseconds{1709164799999};

    first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    expected_age = 15;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);

    //Thursday, February 29, 2024 12:00:00 AM
    timestamp = std::chrono::milliseconds{1709164800000};

    first_func_age = calculateAge(timestamp, birth_year, birth_month, birth_day_of_month);
    second_func_age = calculateAge(timestamp, birth_year, birth_day_of_year);

    expected_age = 16;

    EXPECT_EQ(first_func_age, expected_age);
    EXPECT_EQ(second_func_age, expected_age);
}

TEST_F(GeneralUtilityTesting, isInvalidLetsGoDesktopInterfaceVersion) {
    EXPECT_EQ(isInvalidLetsGoDesktopInterfaceVersion(0), true);
    EXPECT_EQ(isInvalidLetsGoDesktopInterfaceVersion(std::numeric_limits<unsigned int>::max()), false);
    EXPECT_EQ(isInvalidLetsGoDesktopInterfaceVersion(general_values::MINIMUM_ACCEPTED_DESKTOP_INTERFACE_VERSION - 1),
              true);
    EXPECT_EQ(isInvalidLetsGoDesktopInterfaceVersion(general_values::MINIMUM_ACCEPTED_DESKTOP_INTERFACE_VERSION),
              false);
}

TEST_F(GeneralUtilityTesting, validAdminInfo) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    std::string admin_name = "admin";
    std::string admin_password = "password";

    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = admin_name;
    admin_account_doc.password = admin_password;
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    bool return_val = validAdminInfo(
            admin_name,
            admin_password,
            store_error_message
    );

    EXPECT_EQ(return_val, true);
    EXPECT_EQ(error_message, "");

    bsoncxx::document::value expected_admin_doc = document{}
            << "_id" << admin_account_doc.current_object_oid
            << admin_account_key::NAME << admin_name
            << admin_account_key::PRIVILEGE_LEVEL << AdminLevelEnum::PRIMARY_DEVELOPER

            << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_REPORTS << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_BLOCKS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG << negative_one_date

            << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << finalize;

    std::cout << makePrettyJson(expected_admin_doc) << '\n';
    std::cout << makePrettyJson(*admin_info_doc_value) << '\n';

    EXPECT_EQ(expected_admin_doc, (*admin_info_doc_value));

}

TEST_F(GeneralUtilityTesting, validAdminInfo_invalidLambda) {

    std::string admin_name = "admin";
    std::string admin_password = "password";

    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = admin_name;
    admin_account_doc.password = admin_password;
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    bool return_val = validAdminInfo(
            admin_name,
            admin_password,
            nullptr
    );

    EXPECT_EQ(return_val, false);
}

TEST_F(GeneralUtilityTesting, validAdminInfo_adminAccountDoesNotExist) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    std::string admin_name = "admin";
    std::string admin_password = "password";

    bool return_val = validAdminInfo(
            admin_name,
            admin_password,
            store_error_message
    );

    EXPECT_EQ(return_val, false);
    EXPECT_EQ(error_message, "Admin account does not exist.");
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_User_Success) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::SUCCESS);
    EXPECT_EQ(userAccountOIDStr, generated_account_oid.to_string());
    EXPECT_EQ(loginTokenStr, generated_account.logged_in_token);
    EXPECT_EQ(installationId, generated_account.installation_ids.front());

}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_User_InvalidOid) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    login_info.mutable_current_account_id()->pop_back();

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::INVALID_USER_OID);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_User_AdminInfoUsed) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    login_info.set_admin_info_used(true);

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_User_OutdatedVersion) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    login_info.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION - 1);

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::OUTDATED_VERSION);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_User_InvalidLoginToken) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    login_info.mutable_logged_in_token()->pop_back();

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::INVALID_LOGIN_TOKEN);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_User_InvalidInstallationId) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    login_info.mutable_installation_id()->pop_back();

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::INVALID_INSTALLATION_ID);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_Admin_Success) {

    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = "admin";
    admin_account_doc.password = "password";
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    LoginToServerBasicInfo login_info;

    setupAdminLoginInfo(
            &login_info,
            admin_account_doc.name,
            admin_account_doc.password
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId,
            store_error_message
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::SUCCESS);
    EXPECT_EQ(error_message, "");

    bsoncxx::document::value expected_admin_doc = document{}
            << "_id" << admin_account_doc.current_object_oid
            << admin_account_key::NAME << admin_account_doc.name
            << admin_account_key::PRIVILEGE_LEVEL << AdminLevelEnum::PRIMARY_DEVELOPER

            << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_REPORTS << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_BLOCKS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG << negative_one_date

            << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << finalize;

    EXPECT_EQ(expected_admin_doc, (*admin_info_doc_value));
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_Admin_NoErrorFunc) {

    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = "admin";
    admin_account_doc.password = "password";
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    LoginToServerBasicInfo login_info;

    setupAdminLoginInfo(
            &login_info,
            admin_account_doc.name,
            admin_account_doc.password
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId,
            nullptr
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::LG_ERROR);
    EXPECT_EQ(error_message, "");
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_Admin_NotAdminLogin) {

    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = "admin";
    admin_account_doc.password = "password";
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    LoginToServerBasicInfo login_info;

    setupAdminLoginInfo(
            &login_info,
            admin_account_doc.name,
            admin_account_doc.password
    );

    login_info.set_admin_info_used(false);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId,
            store_error_message
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_Admin_OutdatedVersion) {

    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = "admin";
    admin_account_doc.password = "password";
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    LoginToServerBasicInfo login_info;

    setupAdminLoginInfo(
            &login_info,
            admin_account_doc.name,
            admin_account_doc.password
    );

    login_info.set_lets_go_version(general_values::MINIMUM_ACCEPTED_DESKTOP_INTERFACE_VERSION - 1);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId,
            store_error_message
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::OUTDATED_VERSION);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_Admin_AdminDoesNotExist) {

    LoginToServerBasicInfo login_info;

    setupAdminLoginInfo(
            &login_info,
            "admin",
            "password"
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId,
            store_error_message
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::LG_ERROR);
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_AdminOrClient_Admin_Success) {
    bsoncxx::types::b_date negative_one_date = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    AdminAccountDoc admin_account_doc;

    admin_account_doc.name = "admin";
    admin_account_doc.password = "password";
    admin_account_doc.privilege_level = AdminLevelEnum::PRIMARY_DEVELOPER;

    admin_account_doc.last_time_extracted_age_gender_statistics = negative_one_date;

    admin_account_doc.last_time_extracted_reports = negative_one_date;
    admin_account_doc.last_time_extracted_blocks = negative_one_date;

    admin_account_doc.last_time_extracted_feedback_activity = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_other = negative_one_date;
    admin_account_doc.last_time_extracted_feedback_bug = negative_one_date;

    admin_account_doc.number_feedback_marked_as_spam = 0;
    admin_account_doc.number_reports_marked_as_spam = 0;
    admin_account_doc.setIntoCollection();

    LoginToServerBasicInfo login_info;

    setupAdminLoginInfo(
            &login_info,
            admin_account_doc.name,
            admin_account_doc.password
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ADMIN_AND_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId,
            store_error_message
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::SUCCESS);
    EXPECT_EQ(error_message, "");

    bsoncxx::document::value expected_admin_doc = document{}
            << "_id" << admin_account_doc.current_object_oid
            << admin_account_key::NAME << admin_account_doc.name
            << admin_account_key::PRIVILEGE_LEVEL << AdminLevelEnum::PRIMARY_DEVELOPER

            << admin_account_key::LAST_TIME_EXTRACTED_AGE_GENDER_STATISTICS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_REPORTS << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_BLOCKS << negative_one_date

            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_ACTIVITY << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_OTHER << negative_one_date
            << admin_account_key::LAST_TIME_EXTRACTED_FEEDBACK_BUG << negative_one_date

            << admin_account_key::NUMBER_FEEDBACK_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << admin_account_key::NUMBER_REPORTS_MARKED_AS_SPAM << bsoncxx::types::b_int64{0}
            << finalize;

    EXPECT_EQ(expected_admin_doc, (*admin_info_doc_value));
}

TEST_F(GeneralUtilityTesting, isLoginToServerBasicInfoValid_AdminOrClient_User_Success) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationId;

    LoginToServerBasicInfo login_info;

    setupUserLoginInfo(
            &login_info,
            generated_account_oid,
            generated_account.logged_in_token,
            generated_account.installation_ids.front()
    );

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ADMIN_AND_CLIENT,
            login_info,
            userAccountOIDStr,
            loginTokenStr,
            installationId
    );

    EXPECT_EQ(basicInfoReturnStatus, ReturnStatus::SUCCESS);
    EXPECT_EQ(userAccountOIDStr, generated_account_oid.to_string());
    EXPECT_EQ(loginTokenStr, generated_account.logged_in_token);
    EXPECT_EQ(installationId, generated_account.installation_ids.front());

}

TEST_F(GeneralUtilityTesting, isInvalidOIDString) {

    std::string oid;

    EXPECT_EQ(isInvalidOIDString(oid), true);

    oid = bsoncxx::oid{}.to_string();

    EXPECT_EQ(isInvalidOIDString(oid), false);

    //valid
    oid = "000000000000000000000000";

    EXPECT_EQ(isInvalidOIDString(oid), false);

    oid[4] = '.';

    EXPECT_EQ(isInvalidOIDString(oid), true);

    oid[4] = '0';
    oid.push_back('0');

    EXPECT_EQ(isInvalidOIDString(oid), true);

    oid.pop_back();
    oid.pop_back();

    EXPECT_EQ(isInvalidOIDString(oid), true);

}

TEST_F(GeneralUtilityTesting, isInvalidLetsGoAndroidVersion) {
    EXPECT_EQ(isInvalidLetsGoDesktopInterfaceVersion(std::numeric_limits<unsigned int>::max()), false);
    EXPECT_EQ(isInvalidLetsGoAndroidVersion(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION), false);
    EXPECT_EQ(isInvalidLetsGoAndroidVersion(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION - 1), true);
    EXPECT_EQ(isInvalidLetsGoAndroidVersion(0), true);
}

TEST_F(GeneralUtilityTesting, isInvalidUUID) {
    std::string uuid = generateUUID();

    EXPECT_EQ(isInvalidUUID(uuid), false);

    uuid.pop_back();
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid.push_back('1');
    EXPECT_EQ(isInvalidUUID(uuid), false);

    uuid.push_back('1');
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid.erase(4, 1);
    uuid.push_back('a');
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid[8] = 'a';
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid[10] = '*';
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid.erase(15, 1);
    uuid.push_back('a');
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid.erase(22, 1);
    uuid.insert(0, 1, 'a');
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid[23] = 'a';
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    uuid.erase(25, 1);
    uuid.insert(0, 1, 'a');
    EXPECT_EQ(isInvalidUUID(uuid), true);

    uuid = generateUUID();

    for (char& c : uuid) {
        c = (char) toupper(c);
    }
    EXPECT_EQ(isInvalidUUID(uuid), false);

    uuid = generateUUID();

    for (char& c : uuid) {
        c = (char) tolower(c);
    }
    EXPECT_EQ(isInvalidUUID(uuid), false);
}

TEST_F(GeneralUtilityTesting, isInvalidEmailAddress) {

    std::string email;

    EXPECT_EQ(isInvalidEmailAddress(email), true);

    email = std::string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1, 'a');

    EXPECT_EQ(isInvalidEmailAddress(email), true);

    email = "test@gmail.com";

    EXPECT_EQ(isInvalidEmailAddress(email), false);

    email = "testGmail.com";

    EXPECT_EQ(isInvalidEmailAddress(email), true);

    email = "@gmail.com";

    EXPECT_EQ(isInvalidEmailAddress(email), true);

    email = "test@gmail.";

    EXPECT_EQ(isInvalidEmailAddress(email), true);

    email = "test@gmailCom";

    EXPECT_EQ(isInvalidEmailAddress(email), true);

    email = "a@a.a";

    EXPECT_EQ(isInvalidEmailAddress(email), false);
}

TEST_F(GeneralUtilityTesting, saveActivitiesToMemberSharedInfoMessage_realistic) {

    bsoncxx::oid dummy_account_oid{};
    bsoncxx::builder::stream::document dummy_matching_account_doc{};

    dummy_matching_account_doc
            << "key" << "data";

    bsoncxx::builder::basic::array match_categories_arr;

    //Monday, June 6, 2022 5:00:00 AM
    std::chrono::milliseconds start_timestamp{1654491600000};
    std::chrono::milliseconds stop_timestamp{start_timestamp.count() + (2L * 60L * 60L * 1000L)};

    match_categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 5
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << close_array
                    << finalize
    );

    match_categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 19
                    << user_account_keys::categories::TIMEFRAMES << open_array

                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{
                    start_timestamp.count()}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document

                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{
                    stop_timestamp.count()}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document

                    << close_array
                    << finalize
    );

    match_categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 3
                    << user_account_keys::categories::TIMEFRAMES << open_array

                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{
                    start_timestamp.count()}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                    << close_document

                    << open_document
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{
                    stop_timestamp.count()}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                    << close_document

                    << close_array
                    << finalize
    );

    match_categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 5
                    << user_account_keys::categories::TIMEFRAMES << open_array
                    << close_array
                    << finalize
    );

    MemberSharedInfoMessage returned_response_message;

    bool return_val = saveActivitiesToMemberSharedInfoMessage(
            dummy_account_oid,
            match_categories_arr.view(),
            &returned_response_message,
            dummy_matching_account_doc.view());

    MemberSharedInfoMessage expected_response_message;

    auto* first_expected_activity = expected_response_message.mutable_activities()->Add();

    first_expected_activity->set_activity_index(5);

    auto* second_expected_activity = expected_response_message.mutable_activities()->Add();

    second_expected_activity->set_activity_index(19);

    auto* expected_time_frame = second_expected_activity->mutable_time_frame_array()->Add();

    expected_time_frame->set_start_time_frame(start_timestamp.count());
    expected_time_frame->set_stop_time_frame(stop_timestamp.count());

    EXPECT_EQ(return_val, true);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(returned_response_message,
                                                                   expected_response_message)
                                                                   );

}

TEST_F(GeneralUtilityTesting, saveActivitiesToMemberSharedInfoMessage_noActivities) {

    bsoncxx::oid dummy_account_oid{};
    bsoncxx::builder::stream::document dummy_matching_account_doc{};

    dummy_matching_account_doc
            << "key" << "data";

    bsoncxx::builder::basic::array match_categories_arr;

    MemberSharedInfoMessage returned_response_message;

    bool return_val = saveActivitiesToMemberSharedInfoMessage(
            dummy_account_oid,
            match_categories_arr.view(),
            &returned_response_message,
            dummy_matching_account_doc.view());

    MemberSharedInfoMessage expected_response_message;

    EXPECT_EQ(return_val, false);

    EXPECT_EQ(
            google::protobuf::util::MessageDifferencer::Equivalent(returned_response_message,
                                                                   expected_response_message),
            true);

}

TEST_F(GeneralUtilityTesting, saveActivitiesToMemberSharedInfoMessage_tooManyActivities) {

    bsoncxx::oid dummy_account_oid{};
    bsoncxx::builder::stream::document dummy_matching_account_doc{};

    dummy_matching_account_doc
            << "key" << "data";

    bsoncxx::builder::basic::array match_categories_arr;

    for (int i = 0; i < server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT + 1; i++) {
        match_categories_arr.append(
                document{}
                        << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                        << user_account_keys::categories::INDEX_VALUE << i
                        << user_account_keys::categories::TIMEFRAMES << open_array
                        << close_array
                        << finalize
        );
    }

    MemberSharedInfoMessage returned_response_message;

    bool return_val = saveActivitiesToMemberSharedInfoMessage(
            dummy_account_oid,
            match_categories_arr.view(),
            &returned_response_message,
            dummy_matching_account_doc.view());

    MemberSharedInfoMessage expected_response_message;

    for (int i = 0; i < server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT + 1; i++) {
        auto* expected_activity = expected_response_message.mutable_activities()->Add();
        expected_activity->set_activity_index(i);
    }

    EXPECT_EQ(return_val, true);
    EXPECT_EQ(
            google::protobuf::util::MessageDifferencer::Equivalent(returned_response_message,
                                                                   expected_response_message),
            true);
}

TEST_F(GeneralUtilityTesting, saveActivitiesToMemberSharedInfoMessage_tooManyTimeframes) {

    bsoncxx::oid dummy_account_oid{};
    bsoncxx::builder::stream::document dummy_matching_account_doc{};

    dummy_matching_account_doc
            << "key" << "data";

    bsoncxx::builder::basic::array match_categories_arr;
    bsoncxx::builder::basic::array timeframes_arr;

    //Monday, June 6, 2022 5:00:00 AM
    std::chrono::milliseconds timestamp{1654491600000};

    MemberSharedInfoMessage expected_response_message;
    auto* expected_activity = expected_response_message.mutable_activities()->Add();
    expected_activity->set_activity_index(5);

    auto* message_time_frame_array = expected_activity->mutable_time_frame_array();

    for (int i = 0; i < server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY + 1; i++) {

        auto* message_time_frame = message_time_frame_array->Add();

        timestamp += std::chrono::milliseconds{60L * 60L * 1000L};

        message_time_frame->set_start_time_frame(timestamp.count());
        timeframes_arr.append(
                document{}
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                        << user_account_keys::categories::timeframes::TIME << timestamp.count()
                        << finalize
        );

        timestamp += std::chrono::milliseconds{60L * 60L * 1000L};

        message_time_frame->set_stop_time_frame(timestamp.count());
        timeframes_arr.append(
                document{}
                        << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                        << user_account_keys::categories::timeframes::TIME << timestamp.count()
                        << finalize
        );
    }

    match_categories_arr.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << 5
                    << user_account_keys::categories::TIMEFRAMES << timeframes_arr
                    << finalize
    );

    MemberSharedInfoMessage returned_response_message;

    bool return_val = saveActivitiesToMemberSharedInfoMessage(
            dummy_account_oid,
            match_categories_arr.view(),
            &returned_response_message,
            dummy_matching_account_doc.view());

    EXPECT_EQ(return_val, true);
    EXPECT_EQ(
            google::protobuf::util::MessageDifferencer::Equivalent(returned_response_message,
                                                                   expected_response_message),
            true);
}

TEST_F(GeneralUtilityTesting, removeExcessPicturesIfNecessary_PicturesOk) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc generated_account(generated_account_oid);

    bsoncxx::builder::stream::document user_account_doc;
    generated_account.convertToDocument(user_account_doc);

    std::vector<std::pair<std::string, std::chrono::milliseconds>> member_picture_timestamps;

    for (const UserAccountDoc::PictureReference& pic : generated_account.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
            pic.getPictureReference(picture_reference, timestamp_stored);

            member_picture_timestamps.emplace_back(
                    picture_reference.to_string(),
                    timestamp_stored.value.count()
            );
        } else {
            member_picture_timestamps.emplace_back(
                    "",
                    -1L
            );
        }
    }

    bool return_val = removeExcessPicturesIfNecessary(
            mongo_cpp_client,
            user_accounts_collection,
            current_timestamp,
            generated_account_oid,
            user_account_doc.view(),
            member_picture_timestamps
    );

    EXPECT_EQ(return_val, true);

    UserAccountDoc extracted_account(generated_account_oid);

    EXPECT_EQ(extracted_account, generated_account);
}

TEST_F(GeneralUtilityTesting, removeExcessPicturesIfNecessary_arrayTooLarge_extraNullValue) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc generated_account(generated_account_oid);

    generated_account.pictures.emplace_back();
    generated_account.setIntoCollection();

    bsoncxx::builder::stream::document user_account_doc;
    generated_account.convertToDocument(user_account_doc);

    std::vector<std::pair<std::string, std::chrono::milliseconds>> member_picture_timestamps;

    for (const UserAccountDoc::PictureReference& pic : generated_account.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
            pic.getPictureReference(picture_reference, timestamp_stored);

            member_picture_timestamps.emplace_back(
                    picture_reference.to_string(),
                    timestamp_stored.value.count()
            );
        } else {
            member_picture_timestamps.emplace_back(
                    "",
                    -1L
            );
        }
    }

    bool return_val = removeExcessPicturesIfNecessary(
            mongo_cpp_client,
            user_accounts_collection,
            current_timestamp,
            generated_account_oid,
            user_account_doc.view(),
            member_picture_timestamps
    );

    EXPECT_EQ(return_val, true);

    generated_account.pictures.pop_back();
    UserAccountDoc extracted_account(generated_account_oid);

    EXPECT_EQ(extracted_account, generated_account);
}

TEST_F(GeneralUtilityTesting, removeExcessPicturesIfNecessary_arrayTooLarge_extraPictureValue) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc generated_account(generated_account_oid);
    UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);

    generated_account.pictures.emplace_back(
            UserAccountDoc::PictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            )
    );
    user_picture_doc.picture_index = (int) generated_account.pictures.size() - 1;

    user_picture_doc.setIntoCollection();
    generated_account.setIntoCollection();

    bsoncxx::builder::stream::document user_account_doc;
    generated_account.convertToDocument(user_account_doc);

    std::vector<std::pair<std::string, std::chrono::milliseconds>> member_picture_timestamps;

    for (const UserAccountDoc::PictureReference& pic : generated_account.pictures) {
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
            pic.getPictureReference(picture_reference, timestamp_stored);

            member_picture_timestamps.emplace_back(
                    picture_reference.to_string(),
                    timestamp_stored.value.count()
            );
        } else {
            member_picture_timestamps.emplace_back(
                    "",
                    -1L
            );
        }
    }

    bool return_val = removeExcessPicturesIfNecessary(
            mongo_cpp_client,
            user_accounts_collection,
            current_timestamp,
            generated_account_oid,
            user_account_doc.view(),
            member_picture_timestamps
    );

    EXPECT_EQ(return_val, true);

    generated_account.pictures.pop_back();
    UserAccountDoc extracted_account(generated_account_oid);

    EXPECT_EQ(extracted_account, generated_account);

    UserPictureDoc extracted_user_picture_doc(user_picture_doc.current_object_oid);

    EXPECT_EQ(extracted_user_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedUserPictureDoc generated_deleted_picture_doc(
            user_picture_doc,
            nullptr,
            bsoncxx::types::b_date{current_timestamp},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr
    );

    DeletedUserPictureDoc extracted_deleted_picture_doc(user_picture_doc.current_object_oid);

    EXPECT_EQ(extracted_deleted_picture_doc, generated_deleted_picture_doc);
}

TEST_F(GeneralUtilityTesting, extractPicturesToVectorFromChatRoom) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;

    std::vector<std::pair<std::string, std::chrono::milliseconds>> member_picture_timestamps;
    std::set<int> index_used;

    extractPicturesToVectorFromChatRoom(
            mongo_cpp_client,
            user_accounts_collection,
            picture_timestamps_array,
            user_account_doc.view(),
            generated_account_oid,
            current_timestamp,
            member_picture_timestamps,
            index_used
    );

    std::vector<std::pair<std::string, std::chrono::milliseconds>> generated_member_picture_timestamps;
    std::set<int> generated_index_used;

    for (size_t i = 0; i < user_account.pictures.size(); i++) {
        if (user_account.pictures[i].pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            generated_member_picture_timestamps.emplace_back(
                    picture_reference.to_string(),
                    timestamp_stored
            );

            generated_index_used.insert((int) i);
        } else {
            generated_member_picture_timestamps.emplace_back(
                    "",
                    -1L
            );
        }
    }

    EXPECT_EQ(generated_member_picture_timestamps, member_picture_timestamps);
    EXPECT_EQ(index_used, generated_index_used);

}

TEST_F(GeneralUtilityTesting, extractPicturesToVectorFromChatRoom_allNull) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        pic.removePictureReference();
    }

    user_account.setIntoCollection();

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;

    std::vector<std::pair<std::string, std::chrono::milliseconds>> member_picture_timestamps;
    std::set<int> index_used;

    extractPicturesToVectorFromChatRoom(
            mongo_cpp_client,
            user_accounts_collection,
            picture_timestamps_array,
            user_account_doc.view(),
            generated_account_oid,
            current_timestamp,
            member_picture_timestamps,
            index_used
    );

    std::vector<std::pair<std::string, std::chrono::milliseconds>> generated_member_picture_timestamps;
    std::set<int> generated_index_used;

    for (size_t i = 0; i < user_account.pictures.size(); i++) {
        generated_member_picture_timestamps.emplace_back(
                "",
                -1L
        );
    }

    EXPECT_EQ(generated_member_picture_timestamps, member_picture_timestamps);
    EXPECT_EQ(index_used, generated_index_used);
}

TEST_F(GeneralUtilityTesting, extractPicturesToVectorFromChatRoom_allPictures) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (!pic.pictureStored()) {

            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;
            user_picture_doc.setIntoCollection();

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );
        }
        picture_index++;
    }
    user_account.setIntoCollection();

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;

    std::vector<std::pair<std::string, std::chrono::milliseconds>> member_picture_timestamps;
    std::set<int> index_used;

    extractPicturesToVectorFromChatRoom(
            mongo_cpp_client,
            user_accounts_collection,
            picture_timestamps_array,
            user_account_doc.view(),
            generated_account_oid,
            current_timestamp,
            member_picture_timestamps,
            index_used
    );

    std::vector<std::pair<std::string, std::chrono::milliseconds>> generated_member_picture_timestamps;
    std::set<int> generated_index_used;

    for (size_t i = 0; i < user_account.pictures.size(); i++) {
        bsoncxx::oid picture_reference;
        bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

        user_account.pictures[i].getPictureReference(
                picture_reference,
                timestamp_stored
        );

        generated_member_picture_timestamps.emplace_back(
                picture_reference.to_string(),
                timestamp_stored
        );

        generated_index_used.insert((int) i);
    }

    EXPECT_EQ(generated_member_picture_timestamps, member_picture_timestamps);
    EXPECT_EQ(index_used, generated_index_used);
}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;

    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    bool first_picture = true;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            UserPictureDoc user_picture_doc(picture_reference);

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc.picture_in_bytes);
            picture->set_file_size(user_picture_doc.picture_size_in_bytes);
            picture->set_index_number(user_picture_doc.picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc.timestamp_stored.value.count());

            if (first_picture) {
                //set thumbnail
                generated_message.set_account_thumbnail(user_picture_doc.thumbnail_in_bytes);
                generated_message.set_account_thumbnail_size(user_picture_doc.thumbnail_size_in_bytes);
                generated_message.set_account_thumbnail_index(user_picture_doc.picture_index);
                generated_message.set_account_thumbnail_timestamp(user_picture_doc.timestamp_stored.value.count());

                first_picture = false;
            }
        }
    }

    EXPECT_EQ(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            ),
            true);

}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_allPicsNull) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        pic.removePictureReference();
    }

    user_account.setIntoCollection();

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;

    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    EXPECT_EQ(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            ),
            true);
}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_corruptedThumbnail) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    bool first_picture_found = false;
    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (!pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;

            if (!first_picture_found) {
                //corrupt thumbnail
                user_picture_doc.thumbnail_size_in_bytes -= 1;
                first_picture_found = true;
            }

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        } else if (!first_picture_found) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            UserPictureDoc user_picture_doc(picture_reference);

            //corrupt thumbnail
            user_picture_doc.thumbnail_size_in_bytes -= 1;
            first_picture_found = true;

            user_picture_doc.setIntoCollection();
        }
        picture_index++;
    }

    user_account.setIntoCollection();
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;
    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    user_account.pictures.front().removePictureReference();

    UserPictureDoc corrupt_picture_doc(picture_reference);

    EXPECT_EQ(corrupt_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedUserPictureDoc deleted_picture_doc(picture_reference);

    //the full delete will be check in findAndDeletePictureDocument() tests
    EXPECT_EQ(deleted_picture_doc.current_object_oid.to_string(), picture_reference.to_string());

    EXPECT_EQ(extracted_user_account, user_account);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    picture_index = 0;
    for (const UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference_inner;
            bsoncxx::types::b_date timestamp_stored_inner = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference_inner,
                    timestamp_stored_inner
            );

            UserPictureDoc user_picture_doc(picture_reference_inner);

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc.picture_in_bytes);
            picture->set_file_size(user_picture_doc.picture_size_in_bytes);
            picture->set_index_number(user_picture_doc.picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc.timestamp_stored.value.count());

            //second picture should be used as thumbnail
            if (picture_index == 1) {
                //set thumbnail
                generated_message.set_account_thumbnail(user_picture_doc.thumbnail_in_bytes);
                generated_message.set_account_thumbnail_size(user_picture_doc.thumbnail_size_in_bytes);
                generated_message.set_account_thumbnail_index(user_picture_doc.picture_index);
                generated_message.set_account_thumbnail_timestamp(user_picture_doc.timestamp_stored.value.count());
            }
        }

        picture_index++;
    }

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            )
    );

}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_multipleCorruptedThumbnails) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    //corrupt first 2 pictures
    int pic_number = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (!pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = pic_number;

            if (pic_number < 2) {
                //corrupt thumbnail
                user_picture_doc.thumbnail_size_in_bytes -= 1;
            }

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        } else if (pic_number < 2) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            UserPictureDoc user_picture_doc(picture_reference);

            //corrupt thumbnail
            user_picture_doc.thumbnail_size_in_bytes -= 1;

            user_picture_doc.setIntoCollection();
        }
        pic_number++;
    }

    user_account.setIntoCollection();
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;
    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message
    );

    UserAccountDoc extracted_user_account(generated_account_oid);

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    user_account.pictures.front().removePictureReference();

    UserPictureDoc corrupt_picture_doc(picture_reference);

    EXPECT_EQ(corrupt_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedUserPictureDoc deleted_picture_doc(picture_reference);

    //the full delete will be check in findAndDeletePictureDocument() tests
    EXPECT_EQ(deleted_picture_doc.current_object_oid.to_string(), picture_reference.to_string());

    user_account.pictures[1].getPictureReference(
            picture_reference,
            timestamp_stored
    );

    corrupt_picture_doc.getFromCollection(picture_reference);

    EXPECT_EQ(corrupt_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    deleted_picture_doc.getFromCollection(picture_reference);

    //the full delete will be check in findAndDeletePictureDocument() tests
    EXPECT_EQ(deleted_picture_doc.current_object_oid.to_string(), picture_reference.to_string());

    MemberSharedInfoMessage generated_message;

    int picture_index = 0;
    for (const UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            std::unique_ptr<UserPictureDoc> user_picture_doc = nullptr;

            if (picture_index == 1) {
                //picture will be deleted for index #2
                user_picture_doc = static_cast<std::unique_ptr<UserPictureDoc>>(std::make_unique<DeletedUserPictureDoc>(
                        picture_reference));
            } else {
                user_picture_doc = std::make_unique<UserPictureDoc>(picture_reference);
            }

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc->picture_in_bytes);
            picture->set_file_size(user_picture_doc->picture_size_in_bytes);
            picture->set_index_number(user_picture_doc->picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc->timestamp_stored.value.count());

            //third picture should be used as thumbnail
            if (picture_index == 2) {
                //set thumbnail
                generated_message.set_account_thumbnail(user_picture_doc->thumbnail_in_bytes);
                generated_message.set_account_thumbnail_size(user_picture_doc->thumbnail_size_in_bytes);
                generated_message.set_account_thumbnail_index(user_picture_doc->picture_index);
                generated_message.set_account_thumbnail_timestamp(user_picture_doc->timestamp_stored.value.count());
            }
        }

        picture_index++;
    }

    user_account.pictures[1].removePictureReference();

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            )
    );

}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_corruptedPicture) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (!pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;

            if (picture_index == 0) {
                //corrupt picture
                user_picture_doc.picture_size_in_bytes -= 1;
            }

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        } else if (picture_index == 0) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            UserPictureDoc user_picture_doc(picture_reference);

            //corrupt picture
            user_picture_doc.picture_size_in_bytes -= 1;

            user_picture_doc.setIntoCollection();
        }
        picture_index++;
    }

    user_account.setIntoCollection();
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;
    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    bsoncxx::builder::stream::document doc_doc_doc;
    extracted_user_account.convertToDocument(doc_doc_doc);

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    user_account.pictures.front().removePictureReference();

    UserPictureDoc corrupt_picture_doc(picture_reference);

    EXPECT_EQ(corrupt_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedUserPictureDoc deleted_picture_doc(picture_reference);

    //the full delete will be check in findAndDeletePictureDocument() tests
    EXPECT_EQ(deleted_picture_doc.current_object_oid.to_string(), picture_reference.to_string());

    EXPECT_EQ(extracted_user_account, user_account);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    picture_index = 0;
    for (const UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference_inner;
            bsoncxx::types::b_date timestamp_stored_inner = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference_inner,
                    timestamp_stored_inner
            );

            UserPictureDoc user_picture_doc(picture_reference_inner);

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc.picture_in_bytes);
            picture->set_file_size(user_picture_doc.picture_size_in_bytes);
            picture->set_index_number(user_picture_doc.picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc.timestamp_stored.value.count());

            //second picture should be used as thumbnail
            if (picture_index == 1) {
                //set thumbnail
                generated_message.set_account_thumbnail(user_picture_doc.thumbnail_in_bytes);
                generated_message.set_account_thumbnail_size(user_picture_doc.thumbnail_size_in_bytes);
                generated_message.set_account_thumbnail_index(user_picture_doc.picture_index);
                generated_message.set_account_thumbnail_timestamp(user_picture_doc.timestamp_stored.value.count());
            }
        }

        picture_index++;
    }

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            )
    );

}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_singlePicture) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    //only set picture index 1
    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (picture_index == 1
            && !pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        } else if (picture_index != 1) {
            pic.removePictureReference();
        }
        picture_index++;
    }

    user_account.setIntoCollection();
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;
    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    picture_index = 0;
    for (const UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference_inner;
            bsoncxx::types::b_date timestamp_stored_inner = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference_inner,
                    timestamp_stored_inner
            );

            UserPictureDoc user_picture_doc(picture_reference_inner);

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc.picture_in_bytes);
            picture->set_file_size(user_picture_doc.picture_size_in_bytes);
            picture->set_index_number(user_picture_doc.picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc.timestamp_stored.value.count());

            //only one index to be used as thumbnail
            //set thumbnail
            generated_message.set_account_thumbnail(user_picture_doc.thumbnail_in_bytes);
            generated_message.set_account_thumbnail_size(user_picture_doc.thumbnail_size_in_bytes);
            generated_message.set_account_thumbnail_index(user_picture_doc.picture_index);
            generated_message.set_account_thumbnail_timestamp(user_picture_doc.timestamp_stored.value.count());
        }

        picture_index++;
    }

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            )
    );

}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_allPicture) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (!pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        }
        picture_index++;
    }

    user_account.setIntoCollection();
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;
    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    picture_index = 0;
    for (const UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference_inner;
            bsoncxx::types::b_date timestamp_stored_inner = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference_inner,
                    timestamp_stored_inner
            );

            UserPictureDoc user_picture_doc(picture_reference_inner);

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc.picture_in_bytes);
            picture->set_file_size(user_picture_doc.picture_size_in_bytes);
            picture->set_index_number(user_picture_doc.picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc.timestamp_stored.value.count());

            //first index should be used as thumbnail
            if (picture_index == 0) {
                //set thumbnail
                generated_message.set_account_thumbnail(user_picture_doc.thumbnail_in_bytes);
                generated_message.set_account_thumbnail_size(user_picture_doc.thumbnail_size_in_bytes);
                generated_message.set_account_thumbnail_index(user_picture_doc.picture_index);
                generated_message.set_account_thumbnail_timestamp(user_picture_doc.timestamp_stored.value.count());
            }
        }
        picture_index++;
    }

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            )
    );

}

TEST_F(GeneralUtilityTesting, savePicturesToMemberSharedInfoMessage_multiplePictureWithGapInArray) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {

        if (picture_index == 0) { //picture here
            if (!pic.pictureStored()) {
                UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
                user_picture_doc.picture_index = picture_index;

                pic.setPictureReference(
                        user_picture_doc.current_object_oid,
                        user_picture_doc.timestamp_stored
                );

                user_picture_doc.setIntoCollection();
            }
        } else if (picture_index == 1) { //no picture here
            if (pic.pictureStored()) {
                pic.removePictureReference();
            }
        } else if (picture_index == 2) { //picture here
            if (!pic.pictureStored()) {
                UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
                user_picture_doc.picture_index = picture_index;

                pic.setPictureReference(
                        user_picture_doc.current_object_oid,
                        user_picture_doc.timestamp_stored
                );

                user_picture_doc.setIntoCollection();
            }
        }
        picture_index++;
    }

    user_account.setIntoCollection();
    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    bsoncxx::array::view picture_timestamps_array = user_account_doc.view()[user_account_keys::PICTURES].get_array().value;
    MemberSharedInfoMessage passed_in_message;

    savePicturesToMemberSharedInfoMessage(
            mongo_cpp_client,
            user_pictures_collection,
            user_accounts_collection,
            current_timestamp,
            picture_timestamps_array,
            user_account_doc,
            generated_account_oid,
            &passed_in_message);

    UserAccountDoc extracted_user_account(generated_account_oid);

    //make sure user account did not change
    EXPECT_EQ(extracted_user_account, user_account);

    MemberSharedInfoMessage generated_message;

    picture_index = 0;
    for (const UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference_inner;
            bsoncxx::types::b_date timestamp_stored_inner = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference_inner,
                    timestamp_stored_inner
            );

            UserPictureDoc user_picture_doc(picture_reference_inner);

            //save picture to response
            auto picture = generated_message.add_picture();
            picture->set_file_in_bytes(user_picture_doc.picture_in_bytes);
            picture->set_file_size(user_picture_doc.picture_size_in_bytes);
            picture->set_index_number(user_picture_doc.picture_index);
            picture->set_timestamp_picture_last_updated(user_picture_doc.timestamp_stored.value.count());

            //first index should be used as thumbnail
            if (picture_index == 0) {
                //set thumbnail
                generated_message.set_account_thumbnail(user_picture_doc.thumbnail_in_bytes);
                generated_message.set_account_thumbnail_size(user_picture_doc.thumbnail_size_in_bytes);
                generated_message.set_account_thumbnail_index(user_picture_doc.picture_index);
                generated_message.set_account_thumbnail_timestamp(user_picture_doc.timestamp_stored.value.count());
            }
        }
        picture_index++;
    }

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    generated_message,
                    passed_in_message
            ));

}

TEST_F(GeneralUtilityTesting, extractThumbnailFromUserPictureDocument) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (picture_index == 0 && !pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        } else if (picture_index > 0) { //no picture here
            if (pic.pictureStored()) {
                pic.removePictureReference();
            }
        }

        picture_index++;
    }

    user_account.setIntoCollection();

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    UserPictureDoc user_picture(picture_reference);

    bsoncxx::builder::stream::document builder;
    user_picture.convertToDocument(builder, false);

    std::string thumbnail;

    auto error_function = [](const std::string&) {};

    //extracts thumbnail from a document view from user pictures collection
    bool return_val = extractThumbnailFromUserPictureDocument(
            builder.view(),
            thumbnail,
            error_function
    );

    EXPECT_EQ(return_val, true);
    EXPECT_EQ(thumbnail, builder.view()[user_pictures_keys::THUMBNAIL_IN_BYTES].get_string().value.to_string());

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account, user_account);
}

TEST_F(GeneralUtilityTesting, extractThumbnailFromUserPictureDocument_corruptThumbnail) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    int picture_index = 0;
    for (UserAccountDoc::PictureReference& pic : user_account.pictures) {
        if (picture_index == 0 && !pic.pictureStored()) {
            UserPictureDoc user_picture_doc = generateRandomUserPicture(current_timestamp);
            user_picture_doc.picture_index = picture_index;

            pic.setPictureReference(
                    user_picture_doc.current_object_oid,
                    user_picture_doc.timestamp_stored
            );

            user_picture_doc.setIntoCollection();
        } else if (picture_index > 0) { //no picture here
            if (pic.pictureStored()) {
                pic.removePictureReference();
            }
        }

        picture_index++;
    }

    user_account.setIntoCollection();

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    UserPictureDoc user_picture(picture_reference);
    user_picture.thumbnail_size_in_bytes--;
    user_picture.setIntoCollection();

    bsoncxx::builder::stream::document builder;
    user_picture.convertToDocument(builder, false);

    std::string thumbnail;
    std::string error_string;

    auto error_function = [&](const std::string& errorString) {
        error_string = errorString;
    };

    //extracts thumbnail from a document view from user pictures collection
    bool return_val = extractThumbnailFromUserPictureDocument(
            builder.view(),
            thumbnail,
            error_function
    );

    EXPECT_EQ(return_val, false);
    EXPECT_FALSE(error_string.empty());
    EXPECT_EQ(thumbnail, builder.view()[user_pictures_keys::THUMBNAIL_IN_BYTES].get_string().value.to_string());

    UserAccountDoc extracted_user_account(generated_account_oid);

    EXPECT_EQ(extracted_user_account, user_account);
}

TEST_F(GeneralUtilityTesting, extractAllPicturesToResponse) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    std::vector<request_fields::PictureResponse> generated_picture_response_vector;
    std::set<int> non_empty_indexes;
    int picture_index = 0;
    bsoncxx::builder::basic::array requestedPictureOid;
    for (const auto& pic : user_account.pictures) {
        if (pic.pictureStored()) {
            non_empty_indexes.insert(picture_index);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            requestedPictureOid.append(picture_reference);

            UserPictureDoc picture_doc(picture_reference);

            request_fields::PictureResponse pictureResponse;

            pictureResponse.set_timestamp(picture_doc.timestamp_stored.value.count());
            pictureResponse.mutable_picture_info()->set_file_size(picture_doc.picture_size_in_bytes);
            pictureResponse.mutable_picture_info()->set_file_in_bytes(picture_doc.picture_in_bytes);
            pictureResponse.mutable_picture_info()->set_index_number(picture_doc.picture_index);
            pictureResponse.set_return_status(SUCCESS);

            generated_picture_response_vector.emplace_back(pictureResponse);
        }
        picture_index++;
    }

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    std::vector<request_fields::PictureResponse> picture_response_vector;

    std::function<void(int)> setPictureEmptyResponse =
            [&picture_response_vector, &current_timestamp](int indexNumber) {

                request_fields::PictureResponse pictureResponse;

                //this means there is no picture at this index
                pictureResponse.set_timestamp(current_timestamp.count());
                pictureResponse.mutable_picture_info()->set_file_size(0);
                pictureResponse.mutable_picture_info()->set_file_in_bytes("");
                pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
                pictureResponse.set_return_status(SUCCESS);

                picture_response_vector.emplace_back(pictureResponse);
            };

    //std::function<void(const bsoncxx::oid& , std::string&&, int, int, const std::chrono::milliseconds&)>;
    auto setPictureToResponse =
            [&picture_response_vector](
                    [[maybe_unused]] const bsoncxx::oid& picture_oid,
                    std::string&& pictureByteString,
                    int pictureSize,
                    int indexNumber,
                    const std::chrono::milliseconds& picture_timestamp,
                    [[maybe_unused]] bool extracted_from_deleted_pictures,
                    [[maybe_unused]] bool references_removed_after_delete
            ) {

                //NOTE: no reason to send picture_oid back to the client
                request_fields::PictureResponse pictureResponse;

                pictureResponse.set_timestamp(picture_timestamp.count());
                pictureResponse.mutable_picture_info()->set_file_size(pictureSize);
                pictureResponse.mutable_picture_info()->set_file_in_bytes(std::move(pictureByteString));
                pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
                pictureResponse.set_return_status(SUCCESS);

                picture_response_vector.emplace_back(pictureResponse);
            };

    bool return_val = extractAllPicturesToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_account_doc.view(),
            requestedPictureOid,
            non_empty_indexes,
            generated_account_oid,
            nullptr, //session doesn't do anything here
            setPictureToResponse,
            setPictureEmptyResponse,
            nullptr //If searching deleted pictures is not wanted; set to null.
    );

    EXPECT_TRUE(return_val);

    std::sort(
            generated_picture_response_vector.begin(),
            generated_picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    std::sort(
            picture_response_vector.begin(),
            picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    UserAccountDoc extracted_user_account(generated_account_oid);

    //accounts should not have changed
    EXPECT_EQ(user_account, extracted_user_account);

    ASSERT_EQ(picture_response_vector.size(), generated_picture_response_vector.size());

    for (size_t i = 0; i < picture_response_vector.size(); i++) {
        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        picture_response_vector[i],
                        generated_picture_response_vector[i]
                )
        );
    }

}

TEST_F(GeneralUtilityTesting, extractAllPicturesToResponse_noPicturesFound) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    std::vector<request_fields::PictureResponse> generated_picture_response_vector;
    std::set<int> non_empty_indexes;
    bsoncxx::builder::basic::array requestedPictureOid;


    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    std::vector<request_fields::PictureResponse> picture_response_vector;

    std::function<void(int)> setPictureEmptyResponse =
            [&picture_response_vector, &current_timestamp](int indexNumber) {

                request_fields::PictureResponse pictureResponse;

                //this means there is no picture at this index
                pictureResponse.set_timestamp(current_timestamp.count());
                pictureResponse.mutable_picture_info()->set_file_size(0);
                pictureResponse.mutable_picture_info()->set_file_in_bytes("");
                pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
                pictureResponse.set_return_status(SUCCESS);

                picture_response_vector.emplace_back(pictureResponse);
            };

    //std::function<void(const bsoncxx::oid& , std::string&&, int, int, const std::chrono::milliseconds&)>;
    auto setPictureToResponse =
            [&picture_response_vector](
                    [[maybe_unused]] const bsoncxx::oid& picture_oid,
                    std::string&& pictureByteString,
                    int pictureSize,
                    int indexNumber,
                    const std::chrono::milliseconds& picture_timestamp,
                    [[maybe_unused]] bool extracted_from_deleted_pictures,
                    [[maybe_unused]] bool references_removed_after_delete
            ) {

                //NOTE: no reason to send picture_oid back to the client
                request_fields::PictureResponse pictureResponse;

                pictureResponse.set_timestamp(picture_timestamp.count());
                pictureResponse.mutable_picture_info()->set_file_size(pictureSize);
                pictureResponse.mutable_picture_info()->set_file_in_bytes(std::move(pictureByteString));
                pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
                pictureResponse.set_return_status(SUCCESS);

                picture_response_vector.emplace_back(pictureResponse);
            };

    bool return_val = extractAllPicturesToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_account_doc.view(),
            requestedPictureOid,
            non_empty_indexes,
            generated_account_oid,
            nullptr, //session doesn't do anything here
            setPictureToResponse,
            setPictureEmptyResponse,
            nullptr //If searching deleted pictures is not wanted; set to null.
    );

    EXPECT_TRUE(return_val);

    std::sort(
            generated_picture_response_vector.begin(),
            generated_picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            }
    );

    std::sort(
            picture_response_vector.begin(),
            picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            }
    );

    UserAccountDoc extracted_user_account(generated_account_oid);

    //accounts should not have changed
    EXPECT_EQ(user_account, extracted_user_account);

    ASSERT_EQ(picture_response_vector.size(), generated_picture_response_vector.size());

    for (size_t i = 0; i < picture_response_vector.size(); i++) {
        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        picture_response_vector[i],
                        generated_picture_response_vector[i]
                )
        );
    }

}

TEST_F(GeneralUtilityTesting, extractAllPicturesToResponse_doesNotExistInPicturesCol) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    //update all user pictures to be set
    std::vector<request_fields::PictureResponse> generated_picture_response_vector;
    std::set<int> non_empty_indexes;
    int picture_index = 0;
    bsoncxx::builder::basic::array requestedPictureOid;
    for (auto& pic : user_account.pictures) {
        non_empty_indexes.insert(picture_index);
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            requestedPictureOid.append(picture_reference);

            UserPictureDoc picture_doc(picture_reference);

            request_fields::PictureResponse pictureResponse;

            pictureResponse.set_timestamp(picture_doc.timestamp_stored.value.count());
            pictureResponse.mutable_picture_info()->set_file_size(picture_doc.picture_size_in_bytes);
            pictureResponse.mutable_picture_info()->set_file_in_bytes(picture_doc.picture_in_bytes);
            pictureResponse.mutable_picture_info()->set_index_number(picture_doc.picture_index);
            pictureResponse.set_return_status(SUCCESS);

            generated_picture_response_vector.emplace_back(pictureResponse);
        } else {
            UserPictureDoc picture_doc = generateRandomUserPicture(current_timestamp);
            picture_doc.picture_index = picture_index;

            pic.setPictureReference(
                    picture_doc.current_object_oid,
                    picture_doc.timestamp_stored
            );

            picture_doc.setIntoCollection();

            requestedPictureOid.append(picture_doc.current_object_oid);

            request_fields::PictureResponse pictureResponse;

            pictureResponse.set_timestamp(picture_doc.timestamp_stored.value.count());
            pictureResponse.mutable_picture_info()->set_file_size(picture_doc.picture_size_in_bytes);
            pictureResponse.mutable_picture_info()->set_file_in_bytes(picture_doc.picture_in_bytes);
            pictureResponse.mutable_picture_info()->set_index_number(picture_doc.picture_index);
            pictureResponse.set_return_status(SUCCESS);

            generated_picture_response_vector.emplace_back(pictureResponse);
        }
        picture_index++;
    }

    user_account.setIntoCollection();

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    //All pictures were set above.
    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    user_pictures_collection.delete_one(
            document{}
                    << "_id" << picture_reference
                    << finalize
    );

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    std::vector<request_fields::PictureResponse> picture_response_vector;

    std::function<void(int)> setPictureEmptyResponse =
            [&picture_response_vector, &current_timestamp](int indexNumber) {

                request_fields::PictureResponse pictureResponse;

                //this means there is no picture at this index
                pictureResponse.set_timestamp(current_timestamp.count());
                pictureResponse.mutable_picture_info()->set_file_size(0);
                pictureResponse.mutable_picture_info()->set_file_in_bytes("");
                pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
                pictureResponse.set_return_status(SUCCESS);

                picture_response_vector.emplace_back(pictureResponse);
            };

    //std::function<void(const bsoncxx::oid& , std::string&&, int, int, const std::chrono::milliseconds&)>;
    auto setPictureToResponse =
            [&picture_response_vector](
                    [[maybe_unused]] const bsoncxx::oid& picture_oid,
                    std::string&& pictureByteString,
                    int pictureSize,
                    int indexNumber,
                    const std::chrono::milliseconds& picture_timestamp,
                    [[maybe_unused]] bool extracted_from_deleted_pictures,
                    [[maybe_unused]] bool references_removed_after_delete
            ) {

                //NOTE: no reason to send picture_oid back to the client
                request_fields::PictureResponse pictureResponse;

                pictureResponse.set_timestamp(picture_timestamp.count());
                pictureResponse.mutable_picture_info()->set_file_size(pictureSize);
                pictureResponse.mutable_picture_info()->set_file_in_bytes(std::move(pictureByteString));
                pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
                pictureResponse.set_return_status(SUCCESS);

                picture_response_vector.emplace_back(pictureResponse);
            };

    bool return_val = extractAllPicturesToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_account_doc.view(),
            requestedPictureOid,
            non_empty_indexes,
            generated_account_oid,
            nullptr, //session doesn't do anything here
            setPictureToResponse,
            setPictureEmptyResponse,
            nullptr //If searching deleted pictures is not wanted; set to null.
    );

    EXPECT_TRUE(return_val);

    user_account.pictures.front().removePictureReference();

    UserAccountDoc extracted_user_account(generated_account_oid);

    //accounts should not have changed
    EXPECT_EQ(user_account, extracted_user_account);

    std::sort(
            generated_picture_response_vector.begin(),
            generated_picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    generated_picture_response_vector.front().mutable_picture_info()->set_file_in_bytes("");
    generated_picture_response_vector.front().mutable_picture_info()->set_file_size(0);
    generated_picture_response_vector.front().set_timestamp(current_timestamp.count());

    std::sort(
            picture_response_vector.begin(),
            picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    ASSERT_EQ(picture_response_vector.size(), generated_picture_response_vector.size());

    for (size_t i = 0; i < picture_response_vector.size(); i++) {
        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        picture_response_vector[i],
                        generated_picture_response_vector[i]
                )
        );
    }
}

TEST_F(GeneralUtilityTesting, extractAllPicturesToResponse_fromDeletedCol) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    bsoncxx::builder::basic::array requestedPictureOid;
    std::set<int> non_empty_indexes;
    std::vector<request_fields::PictureResponse> generated_picture_response_vector;

    for(int i = 0; i < 2; i++) {
        UserPictureDoc picture_doc = generateRandomUserPicture(current_timestamp);
        picture_doc.picture_index = i;

        DeletedUserPictureDoc deleted_picture(
                picture_doc,
                nullptr,
                bsoncxx::types::b_date{current_timestamp},
                ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
                nullptr
                );

        deleted_picture.setIntoCollection();

        requestedPictureOid.append(deleted_picture.current_object_oid);
        non_empty_indexes.insert(i);

        request_fields::PictureResponse pictureResponse;

        pictureResponse.set_timestamp(picture_doc.timestamp_stored.value.count());
        pictureResponse.mutable_picture_info()->set_file_size(picture_doc.picture_size_in_bytes);
        pictureResponse.mutable_picture_info()->set_file_in_bytes(picture_doc.picture_in_bytes);
        pictureResponse.mutable_picture_info()->set_index_number(picture_doc.picture_index);
        pictureResponse.set_return_status(SUCCESS);

        generated_picture_response_vector.emplace_back(pictureResponse);
    }

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    std::vector<request_fields::PictureResponse> picture_response_vector;

    std::function<void(int)> setPictureEmptyResponse =
            [&picture_response_vector, &current_timestamp](int indexNumber) {

        request_fields::PictureResponse pictureResponse;

        //this means there is no picture at this index
        pictureResponse.set_timestamp(current_timestamp.count());
        pictureResponse.mutable_picture_info()->set_file_size(0);
        pictureResponse.mutable_picture_info()->set_file_in_bytes("");
        pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
        pictureResponse.set_return_status(SUCCESS);

        picture_response_vector.emplace_back(pictureResponse);
    };

    //std::function<void(const bsoncxx::oid& , std::string&&, int, int, const std::chrono::milliseconds&)>;
    auto setPictureToResponse =
            [&picture_response_vector](
                    [[maybe_unused]] const bsoncxx::oid& picture_oid,
                    std::string&& pictureByteString,
                    int pictureSize,
                    int indexNumber,
                    const std::chrono::milliseconds& picture_timestamp,
                    [[maybe_unused]] bool extracted_from_deleted_pictures,
                    [[maybe_unused]] bool references_removed_after_delete
                    ) {

        //NOTE: no reason to send picture_oid back to the client
        request_fields::PictureResponse pictureResponse;

        pictureResponse.set_timestamp(picture_timestamp.count());
        pictureResponse.mutable_picture_info()->set_file_size(pictureSize);
        pictureResponse.mutable_picture_info()->set_file_in_bytes(std::move(pictureByteString));
        pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
        pictureResponse.set_return_status(SUCCESS);

        picture_response_vector.emplace_back(pictureResponse);
    };

    extractAllPicturesToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_account_doc.view(),
            requestedPictureOid,
            non_empty_indexes,
            generated_account_oid,
            nullptr, //session doesn't do anything here
            setPictureToResponse,
            setPictureEmptyResponse,
            &deleted_db //If searching deleted pictures is not wanted; set to null.
            );

    UserAccountDoc extracted_user_account(generated_account_oid);

    //accounts should not have changed
    EXPECT_EQ(user_account, extracted_user_account);

    std::sort(
            generated_picture_response_vector.begin(),
            generated_picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    std::sort(
            picture_response_vector.begin(),
            picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    ASSERT_EQ(picture_response_vector.size(), generated_picture_response_vector.size());

    for (size_t i = 0; i < picture_response_vector.size(); i++) {
        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        picture_response_vector[i],
                        generated_picture_response_vector[i]
                        )
                        );
    }
}

TEST_F(GeneralUtilityTesting, extractAllPicturesToResponse_corruptPicture) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountDoc user_account(generated_account_oid);

    //update all user pictures to be set
    std::vector<request_fields::PictureResponse> generated_picture_response_vector;
    std::set<int> non_empty_indexes;
    int picture_index = 0;
    bsoncxx::builder::basic::array requestedPictureOid;
    for (auto& pic : user_account.pictures) {
        non_empty_indexes.insert(picture_index);
        if (pic.pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            requestedPictureOid.append(picture_reference);

            UserPictureDoc picture_doc(picture_reference);

            request_fields::PictureResponse pictureResponse;

            pictureResponse.set_timestamp(picture_doc.timestamp_stored.value.count());
            pictureResponse.mutable_picture_info()->set_file_size(picture_doc.picture_size_in_bytes);
            pictureResponse.mutable_picture_info()->set_file_in_bytes(picture_doc.picture_in_bytes);
            pictureResponse.mutable_picture_info()->set_index_number(picture_doc.picture_index);
            pictureResponse.set_return_status(SUCCESS);

            generated_picture_response_vector.emplace_back(pictureResponse);
        } else {
            UserPictureDoc picture_doc = generateRandomUserPicture(current_timestamp);
            picture_doc.picture_index = picture_index;

            pic.setPictureReference(
                    picture_doc.current_object_oid,
                    picture_doc.timestamp_stored
                    );

            picture_doc.setIntoCollection();

            requestedPictureOid.append(picture_doc.current_object_oid);

            request_fields::PictureResponse pictureResponse;

            pictureResponse.set_timestamp(picture_doc.timestamp_stored.value.count());
            pictureResponse.mutable_picture_info()->set_file_size(picture_doc.picture_size_in_bytes);
            pictureResponse.mutable_picture_info()->set_file_in_bytes(picture_doc.picture_in_bytes);
            pictureResponse.mutable_picture_info()->set_index_number(picture_doc.picture_index);
            pictureResponse.set_return_status(SUCCESS);

            generated_picture_response_vector.emplace_back(pictureResponse);
        }
        picture_index++;
    }

    user_account.setIntoCollection();

    bsoncxx::oid picture_reference;
    bsoncxx::types::b_date timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    //All pictures were set above.
    user_account.pictures.front().getPictureReference(
            picture_reference,
            timestamp_stored
    );

    UserPictureDoc corrupt_picture_doc(picture_reference);
    corrupt_picture_doc.picture_size_in_bytes--;
    corrupt_picture_doc.setIntoCollection();

    bsoncxx::builder::stream::document user_account_doc;
    user_account.convertToDocument(user_account_doc);

    std::vector<request_fields::PictureResponse> picture_response_vector;

    std::function<void(int)> setPictureEmptyResponse =
            [&picture_response_vector, &current_timestamp](int indexNumber) {

        request_fields::PictureResponse pictureResponse;

        //this means there is no picture at this index
        pictureResponse.set_timestamp(current_timestamp.count());
        pictureResponse.mutable_picture_info()->set_file_size(0);
        pictureResponse.mutable_picture_info()->set_file_in_bytes("");
        pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
        pictureResponse.set_return_status(SUCCESS);

        picture_response_vector.emplace_back(pictureResponse);
    };

    //std::function<void(const bsoncxx::oid& , std::string&&, int, int, const std::chrono::milliseconds&)>;
    auto setPictureToResponse =
            [&picture_response_vector](
                    [[maybe_unused]] const bsoncxx::oid& picture_oid,
                    std::string&& pictureByteString,
                    int pictureSize,
                    int indexNumber,
                    const std::chrono::milliseconds& picture_timestamp,
                    [[maybe_unused]] bool extracted_from_deleted_pictures,
                    [[maybe_unused]] bool references_removed_after_delete
                    ) {

        //NOTE: no reason to send picture_oid back to the client
        request_fields::PictureResponse pictureResponse;

        pictureResponse.set_timestamp(picture_timestamp.count());
        pictureResponse.mutable_picture_info()->set_file_size(pictureSize);
        pictureResponse.mutable_picture_info()->set_file_in_bytes(std::move(pictureByteString));
        pictureResponse.mutable_picture_info()->set_index_number(indexNumber);
        pictureResponse.set_return_status(SUCCESS);

        picture_response_vector.emplace_back(pictureResponse);
    };

    bool return_val = extractAllPicturesToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_account_doc.view(),
            requestedPictureOid,
            non_empty_indexes,
            generated_account_oid,
            nullptr, //session doesn't do anything here
            setPictureToResponse,
            setPictureEmptyResponse,
            nullptr //If searching deleted pictures is not wanted; set to null.
            );

    EXPECT_TRUE(return_val);

    user_account.pictures.front().removePictureReference();

    UserAccountDoc extracted_user_account(generated_account_oid);

    //accounts should not have changed
    EXPECT_EQ(user_account, extracted_user_account);

    UserPictureDoc after_func_corrupt_picture_doc(picture_reference);
    EXPECT_EQ(after_func_corrupt_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedUserPictureDoc generated_deleted_corrupt_picture_doc(
            corrupt_picture_doc,
            nullptr,
            bsoncxx::types::b_date{current_timestamp},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr
            );
    DeletedUserPictureDoc extracted_deleted_corrupt_picture_doc(picture_reference);

    EXPECT_EQ(generated_deleted_corrupt_picture_doc, extracted_deleted_corrupt_picture_doc);

    std::sort(
            generated_picture_response_vector.begin(),
            generated_picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    generated_picture_response_vector.front().mutable_picture_info()->set_file_size(0);
    generated_picture_response_vector.front().mutable_picture_info()->set_file_in_bytes("");
    generated_picture_response_vector.front().set_timestamp(current_timestamp.count());

    std::sort(
            picture_response_vector.begin(),
            picture_response_vector.end(),
            [&](const request_fields::PictureResponse& lhs, const request_fields::PictureResponse& rhs) {
                return lhs.picture_info().index_number() < rhs.picture_info().index_number();
            });

    ASSERT_EQ(picture_response_vector.size(), generated_picture_response_vector.size());

    for (size_t i = 0; i < picture_response_vector.size(); i++) {
        EXPECT_TRUE(
                google::protobuf::util::MessageDifferencer::Equivalent(
                        picture_response_vector[i],
                        generated_picture_response_vector[i]
                )
        );
    }
}

TEST_F(GeneralUtilityTesting, extractPictureToResponse) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string returned_picture_byte_string;
    int returned_picture_size = 0;
    std::chrono::milliseconds returned_timestamp{-1L};

    std::function<void(std::string&&, int, const std::chrono::milliseconds&)> setPictureToResponse =
            [&](std::string&& pictureByteString, int pictureSize,
                    const std::chrono::milliseconds& timestamp) {

        returned_picture_byte_string = std::move(pictureByteString);
        returned_picture_size = pictureSize;
        returned_timestamp = timestamp;
    };

    auto setThumbnailToResponse = [](std::string&&, int, const std::chrono::milliseconds&) {};

    UserAccountDoc user_account_doc(generated_account_oid);

    //extract first picture found
    bsoncxx::oid first_pic_reference;
    bsoncxx::types::b_date first_pic_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
    int first_pic_index = 0;
    bool found_picture = false;
    for(const auto& pic : user_account_doc.pictures) {
        if(pic.pictureStored()) {
            pic.getPictureReference(
                first_pic_reference,
                first_pic_timestamp
            );

            found_picture = true;
            break;
        }
        first_pic_index++;
    }

    ASSERT_TRUE(found_picture);

    bsoncxx::builder::stream::document user_acct_doc;
    user_account_doc.convertToDocument(user_acct_doc);

    bool return_val = extractPictureToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_acct_doc.view(),
            first_pic_reference,
            generated_account_oid,
            first_pic_index,
            setPictureToResponse,
            false,
            setThumbnailToResponse
            );

    EXPECT_TRUE(return_val);

    //make sure nothing changed in user account
    UserAccountDoc after_function_user_account_doc(generated_account_oid);
    EXPECT_EQ(after_function_user_account_doc, user_account_doc);

    UserPictureDoc user_picture_doc(first_pic_reference);

    EXPECT_EQ(returned_picture_byte_string, user_picture_doc.picture_in_bytes);
    EXPECT_EQ(returned_picture_size, user_picture_doc.picture_size_in_bytes);
    EXPECT_EQ(returned_timestamp.count(), user_picture_doc.timestamp_stored.value.count());
}

TEST_F(GeneralUtilityTesting, extractPictureToResponse_withThumbnail) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string returned_picture_byte_string;
    int returned_picture_size = 0;
    std::chrono::milliseconds returned_picture_timestamp{-1L};

    std::string returned_thumbnail_byte_string{-1L};
    int returned_thumbnail_size = 0;
    std::chrono::milliseconds returned_thumbnail_timestamp{-1L};

    std::function<void(std::string&&, int, const std::chrono::milliseconds&)> setPictureToResponse =
            [&](std::string&& pictureByteString, int pictureSize,
                    const std::chrono::milliseconds& timestamp) {

        returned_picture_byte_string = std::move(pictureByteString);
        returned_picture_size = pictureSize;
        returned_picture_timestamp = timestamp;
    };

    auto setThumbnailToResponse = [&](std::string&& thumbnail_byte_string, int thumbnail_size, const std::chrono::milliseconds& thumbnail_timestamp) {
        returned_thumbnail_byte_string = thumbnail_byte_string;
        returned_thumbnail_size = thumbnail_size;
        returned_thumbnail_timestamp = thumbnail_timestamp;
    };

    UserAccountDoc user_account_doc(generated_account_oid);

    //extract first picture found
    bsoncxx::oid first_pic_reference;
    bsoncxx::types::b_date first_pic_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
    int first_pic_index = 0;
    bool found_picture = false;
    for(const auto& pic : user_account_doc.pictures) {
        if(pic.pictureStored()) {
            pic.getPictureReference(
                    first_pic_reference,
                    first_pic_timestamp
                    );

            found_picture = true;
            break;
        }
        first_pic_index++;
    }

    ASSERT_TRUE(found_picture);

    bsoncxx::builder::stream::document user_acct_doc;
    user_account_doc.convertToDocument(user_acct_doc);

    bool return_val = extractPictureToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_acct_doc.view(),
            first_pic_reference,
            generated_account_oid,
            first_pic_index,
            setPictureToResponse,
            true,
            setThumbnailToResponse
            );

    EXPECT_TRUE(return_val);

    //make sure nothing changed in user account
    UserAccountDoc after_function_user_account_doc(generated_account_oid);
    EXPECT_EQ(after_function_user_account_doc, user_account_doc);

    UserPictureDoc user_picture_doc(first_pic_reference);

    EXPECT_EQ(returned_picture_byte_string, user_picture_doc.picture_in_bytes);
    EXPECT_EQ(returned_picture_size, user_picture_doc.picture_size_in_bytes);
    EXPECT_EQ(returned_picture_timestamp.count(), user_picture_doc.timestamp_stored.value.count());

    EXPECT_EQ(returned_thumbnail_byte_string, user_picture_doc.thumbnail_in_bytes);
    EXPECT_EQ(returned_thumbnail_size, user_picture_doc.thumbnail_size_in_bytes);
    EXPECT_EQ(returned_thumbnail_timestamp.count(), user_picture_doc.timestamp_stored.value.count());

}

TEST_F(GeneralUtilityTesting, extractPictureToResponse_corruptFile) {
    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::string returned_picture_byte_string;
    int returned_picture_size = 0;
    std::chrono::milliseconds returned_timestamp{-1L};

    std::function<void(std::string&&, int, const std::chrono::milliseconds&)> setPictureToResponse =
            [&](std::string&& pictureByteString, int pictureSize,
                    const std::chrono::milliseconds& timestamp) {

        returned_picture_byte_string = std::move(pictureByteString);
        returned_picture_size = pictureSize;
        returned_timestamp = timestamp;
    };

    auto setThumbnailToResponse = [](std::string&&, int, const std::chrono::milliseconds&) {};

    UserAccountDoc user_account_doc(generated_account_oid);

    //corrupt first picture found
    bsoncxx::oid corrupt_pic_reference;
    bsoncxx::types::b_date corrupt_pic_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};
    int corrupt_pic_index = 0;
    UserPictureDoc corrupt_user_picture_doc;
    bool found_picture = false;
    for(const auto& pic : user_account_doc.pictures) {
        if(pic.pictureStored()) {

            pic.getPictureReference(
                    corrupt_pic_reference,
                    corrupt_pic_timestamp
                    );

            corrupt_user_picture_doc.getFromCollection(corrupt_pic_reference);
            ASSERT_NE(corrupt_user_picture_doc.current_object_oid.to_string(), "000000000000000000000000");
            corrupt_user_picture_doc.picture_size_in_bytes--;
            corrupt_user_picture_doc.setIntoCollection();

            found_picture = true;
            break;
        }
        corrupt_pic_index++;
    }

    ASSERT_TRUE(found_picture);

    bsoncxx::builder::stream::document user_acct_doc;
    user_account_doc.convertToDocument(user_acct_doc);

    bool return_val = extractPictureToResponse(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            current_timestamp,
            user_acct_doc.view(),
            corrupt_pic_reference,
            generated_account_oid,
            corrupt_pic_index,
            setPictureToResponse,
            false,
            setThumbnailToResponse
            );

    EXPECT_FALSE(return_val);

    user_account_doc.pictures[corrupt_pic_index].removePictureReference();

    //make sure nothing changed in user account
    UserAccountDoc after_function_user_account_doc(generated_account_oid);
    EXPECT_EQ(after_function_user_account_doc, user_account_doc);

    UserPictureDoc after_func_corrupt_picture_doc(corrupt_pic_reference);
    EXPECT_EQ(after_func_corrupt_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    DeletedUserPictureDoc generated_deleted_corrupt_picture_doc(
            corrupt_user_picture_doc,
            nullptr,
            bsoncxx::types::b_date{current_timestamp},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
            nullptr
            );
    DeletedUserPictureDoc extracted_deleted_corrupt_picture_doc(corrupt_pic_reference);

    EXPECT_EQ(generated_deleted_corrupt_picture_doc, extracted_deleted_corrupt_picture_doc);

    EXPECT_EQ(returned_picture_byte_string, "");
    EXPECT_EQ(returned_picture_size, 0);
    EXPECT_EQ(returned_timestamp.count(), -1L);
}

std::chrono::milliseconds calcLatestTimestamp(UserAccountDoc& user_account_doc) {
    std::chrono::milliseconds actual_other_info_latest_timestamp = user_account_doc.categories_timestamp.value;

    if(user_account_doc.bio_timestamp.value > actual_other_info_latest_timestamp) {
        actual_other_info_latest_timestamp = user_account_doc.bio_timestamp.value;
    }

    if(user_account_doc.city_name_timestamp.value > actual_other_info_latest_timestamp) {
        actual_other_info_latest_timestamp = user_account_doc.city_name_timestamp.value;
    }

    if(user_account_doc.gender_timestamp.value > actual_other_info_latest_timestamp) {
        actual_other_info_latest_timestamp = user_account_doc.gender_timestamp.value;
    }

    return actual_other_info_latest_timestamp;
}

TEST_F(GeneralUtilityTesting, checkForValidLoginToken) {

    bsoncxx::stdx::optional<bsoncxx::document::value> findAndUpdateUserAccount;

    ReturnStatus return_val = checkForValidLoginToken(
            findAndUpdateUserAccount,
            "dummy_userAccountOIDStr"
            );

    EXPECT_EQ(return_val, ReturnStatus::NO_VERIFIED_ACCOUNT);

    findAndUpdateUserAccount = bsoncxx::stdx::optional<bsoncxx::document::value>{
        document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << ReturnStatus::SUCCESS
        << finalize
    };

    return_val = checkForValidLoginToken(
            findAndUpdateUserAccount,
            "dummy_userAccountOIDStr"
            );

    EXPECT_EQ(return_val, ReturnStatus::SUCCESS);
}

TEST_F(GeneralUtilityTesting, extractUserAndRemoveChatRoomIdFromUserPicturesReference_thumbnailNotUpdated) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    UserAccountDoc generated_user_account(generated_account_oid);

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
            );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ChatRoomHeaderDoc original_chat_room_header(create_chat_room_response.chat_room_id());

    bsoncxx::builder::stream::document header_builder_doc;
    original_chat_room_header.convertToDocument(header_builder_doc);

    bsoncxx::oid first_pic_reference;
    bsoncxx::types::b_date first_pic_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    bool found_picture = false;
    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {
            pic.getPictureReference(
                    first_pic_reference,
                    first_pic_timestamp
                    );

            found_picture = true;
            break;
        }
    }

    ASSERT_TRUE(found_picture);

    UserPictureDoc before_first_user_picture(first_pic_reference);

    bool return_val = extractUserAndRemoveChatRoomIdFromUserPicturesReference(
            accounts_db,
            header_builder_doc.view(),
            generated_account_oid,
            first_pic_reference.to_string(),
            create_chat_room_response.chat_room_id(),
            nullptr
            );

    EXPECT_EQ(return_val, true);

    //make sure picture reference has not changed
    UserPictureDoc after_first_user_picture(first_pic_reference);
    EXPECT_EQ(before_first_user_picture, after_first_user_picture);
}

TEST_F(GeneralUtilityTesting, extractUserAndRemoveChatRoomIdFromUserPicturesReference_thumbnailUpdated) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    UserAccountDoc generated_user_account(generated_account_oid);

    setupUserLoginInfo(
            create_chat_room_request.mutable_login_info(),
            generated_account_oid,
            generated_user_account.logged_in_token,
            generated_user_account.installation_ids.front()
            );

    createChatRoom(&create_chat_room_request, &create_chat_room_response);

    ChatRoomHeaderDoc original_chat_room_header(create_chat_room_response.chat_room_id());

    bsoncxx::builder::stream::document header_builder_doc;
    original_chat_room_header.convertToDocument(header_builder_doc);

    bsoncxx::oid first_pic_reference;
    bsoncxx::types::b_date first_pic_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    bool found_picture = false;
    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {
            pic.getPictureReference(
                    first_pic_reference,
                    first_pic_timestamp
                    );

            found_picture = true;
            break;
        }
    }

    ASSERT_TRUE(found_picture);

    UserPictureDoc generated_user_picture = generateRandomUserPicture();
    generated_user_picture.picture_index = 0;
    generated_user_picture.setIntoCollection();

    generated_user_account.pictures.front().setPictureReference(
            generated_user_picture.current_object_oid,
            generated_user_picture.timestamp_stored
            );
    generated_user_account.setIntoCollection();

    UserPictureDoc original_first_user_picture(first_pic_reference);

    bool return_val = extractUserAndRemoveChatRoomIdFromUserPicturesReference(
            accounts_db,
            header_builder_doc.view(),
            generated_account_oid,
            generated_user_account.current_object_oid.to_string(),
            create_chat_room_response.chat_room_id(),
            nullptr
            );

    EXPECT_EQ(return_val, true);

    for(int i = 0; i < (int)original_first_user_picture.thumbnail_references.size(); i++) {
        if(original_first_user_picture.thumbnail_references[i] == create_chat_room_response.chat_room_id()) {
            std::cout << "found\n";
            original_first_user_picture.thumbnail_references.erase(
                    original_first_user_picture.thumbnail_references.begin() + i
            );
            break;
        }
    }

    //make sure picture reference has changed
    UserPictureDoc after_original_first_user_picture(first_pic_reference);
    EXPECT_EQ(original_first_user_picture, after_original_first_user_picture);

}

TEST_F(GeneralUtilityTesting, gen_random_alpha_numeric_string) {
    EXPECT_EQ(21, gen_random_alpha_numeric_string(21).size());
}

TEST_F(GeneralUtilityTesting, trimLeadingWhitespace) {

    std::string passed_string;

    trimLeadingWhitespace(passed_string);

    EXPECT_EQ(passed_string, "");

    passed_string = " 1";

    trimLeadingWhitespace(passed_string);

    EXPECT_EQ(passed_string, "1");

    passed_string = "1\f";

    trimLeadingWhitespace(passed_string);

    EXPECT_EQ(passed_string, "1\f");
}

TEST_F(GeneralUtilityTesting, trimTrailingWhitespace) {
    std::string passed_string;

    trimTrailingWhitespace(passed_string);

    EXPECT_EQ(passed_string, "");

    passed_string = "1 ";

    trimTrailingWhitespace(passed_string);

    EXPECT_EQ(passed_string, "1");

    passed_string = " 1";

    trimTrailingWhitespace(passed_string);

    EXPECT_EQ(passed_string, " 1");
}

TEST_F(GeneralUtilityTesting, trimWhitespace) {
    std::string passed_string;

    trimWhitespace(passed_string);

    EXPECT_EQ(passed_string, "");

    passed_string = "1 ";

    trimWhitespace(passed_string);

    EXPECT_EQ(passed_string, "1");

    passed_string = " 1";

    trimWhitespace(passed_string);

    EXPECT_EQ(passed_string, "1");

    passed_string = "1";

    trimWhitespace(passed_string);

    EXPECT_EQ(passed_string, "1");
}