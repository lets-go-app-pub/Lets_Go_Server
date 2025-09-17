//
// Created by jeremiah on 10/7/22.
//


#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "generate_multiple_random_accounts.h"
#include "set_fields_functions.h"
#include "grpc_values.h"
#include "user_pictures_keys.h"
#include "deleted_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetPictureTesting : public testing::Test {
protected:

    setfields::SetPictureRequest request;
    setfields::SetFieldResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        std::string picture_file_in_bytes = gen_random_alpha_numeric_string(rand() % (server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES - 100) + 100);
        std::string thumbnail_file_in_bytes = gen_random_alpha_numeric_string(rand() % (server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES - 100) + 100);
        request.set_file_in_bytes(picture_file_in_bytes);
        request.set_file_size((int)picture_file_in_bytes.size());
        request.set_thumbnail_in_bytes(thumbnail_file_in_bytes);
        request.set_thumbnail_size((int)thumbnail_file_in_bytes.size());
        request.set_picture_array_index(0);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        setPicture(&request, &response);
    }

    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);
        EXPECT_EQ(user_account, extracted_user_account);
    }

    UserPictureDoc extractPictureAtIndex(size_t index) {
        UserPictureDoc doc;

        bsoncxx::oid picture_reference;
        bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

        EXPECT_LT(index, user_account.pictures.size());
        if(index < user_account.pictures.size()) {
            EXPECT_TRUE(user_account.pictures[index].pictureStored());
            if(user_account.pictures[index].pictureStored()) {
                user_account.pictures[index].getPictureReference(
                        picture_reference,
                        timestamp_stored
                );
                doc.getFromCollection(picture_reference);
            }
        }
        return doc;
    }

    void extractAndCompareDeletedPictureOnSuccess(const UserPictureDoc& initial_picture) {
        DeletedUserPictureDoc generated_deleted_picture(
                initial_picture,
                nullptr,
                bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}},
                ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
                nullptr
        );

        DeletedUserPictureDoc extracted_deleted_picture(initial_picture.current_object_oid);

        EXPECT_EQ(generated_deleted_picture, extracted_deleted_picture);
    }

    UserPictureDoc extractAndCompareInitialUserPictureOnSuccess(
            const std::string& thumbnail_in_bytes_copy,
            const std::string& file_in_bytes_copy
            ) {
        UserPictureDoc extracted_user_picture_doc;

        {
            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

            mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
            mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

            mongocxx::options::find opts;

            opts.projection(
                    document{}
                            << "_id" << 1
                            << finalize
            );

            opts.sort(
                    document{}
                            << user_pictures_keys::TIMESTAMP_STORED << -1
                    << finalize
            );

            opts.limit(1);

            auto result_cursor = user_pictures_collection.find(
                    document{}
                    << finalize,
                    opts
            );

            int num_docs = 0;
            for(const auto& doc : result_cursor) {
                extracted_user_picture_doc.getFromCollection(doc["_id"].get_oid().value);
                num_docs++;
            }

            EXPECT_EQ(num_docs, 1);
        }

        UserPictureDoc generated_user_picture_doc;

        extracted_user_picture_doc.current_object_oid = generated_user_picture_doc.current_object_oid;
        generated_user_picture_doc.user_account_reference = user_account_oid;
        generated_user_picture_doc.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};
        generated_user_picture_doc.picture_index = request.picture_array_index();
        generated_user_picture_doc.thumbnail_in_bytes = thumbnail_in_bytes_copy;
        generated_user_picture_doc.thumbnail_size_in_bytes = request.thumbnail_size();
        generated_user_picture_doc.picture_in_bytes = file_in_bytes_copy;
        generated_user_picture_doc.picture_size_in_bytes = request.file_size();

        EXPECT_EQ(generated_user_picture_doc, extracted_user_picture_doc);

        return extracted_user_picture_doc;
    }
};

TEST_F(SetPictureTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();

                runFunction();

                return response.return_status();
            }
    );

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, invalidIndex_tooSmall) {
    request.set_picture_array_index(-1);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, invalidIndex_tooLarge) {
    request.set_picture_array_index(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, invalidFileInBytes_empty) {
    request.set_file_in_bytes("");
    request.set_file_size(0);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, invalidFileInBytes_tooLarge) {
    std::string picture_file_in_bytes = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES + 1);
    request.set_file_in_bytes(picture_file_in_bytes);
    request.set_file_size((int)picture_file_in_bytes.size());

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, corruptPicture) {
    request.set_file_size(request.file_size() - 1);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::CORRUPTED_FILE);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, invalidThumbnailInBytes_empty) {
    request.set_thumbnail_in_bytes("");
    request.set_thumbnail_size(0);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, invalidThumbnailInBytes_tooLarge) {
    std::string thumbnail_file_in_bytes = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES + 1);
    request.set_thumbnail_in_bytes(thumbnail_file_in_bytes);
    request.set_thumbnail_size((int)thumbnail_file_in_bytes.size());

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, corruptThumbnail) {
    request.set_thumbnail_size(request.thumbnail_size() + 1);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::CORRUPTED_FILE);

    //should be no changes
    compareUserAccounts();
}

TEST_F(SetPictureTesting, successful_pictureExistsAtIndex) {
    request.set_picture_array_index(0);

    UserPictureDoc initial_picture = extractPictureAtIndex(request.picture_array_index());

    //Function moves out these values, so copies are required.
    std::string file_in_bytes_copy = request.file_in_bytes();
    std::string thumbnail_in_bytes_copy = request.thumbnail_in_bytes();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    UserPictureDoc extracted_user_picture_doc = extractAndCompareInitialUserPictureOnSuccess(
            thumbnail_in_bytes_copy,
            file_in_bytes_copy
    );

    user_account.pictures[0].setPictureReference(
            extracted_user_picture_doc.current_object_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
            );

    compareUserAccounts();

    UserPictureDoc extracted_initial_user_picture_doc(initial_picture.current_object_oid);
    EXPECT_EQ(extracted_initial_user_picture_doc.current_object_oid.to_string(), "000000000000000000000000");

    extractAndCompareDeletedPictureOnSuccess(initial_picture);
}

TEST_F(SetPictureTesting, successful_pictureDoesNotExistAtIndex) {

    //final index
    const int picture_index = general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT - 1;

    //clear final index for pictures
    user_account.pictures[picture_index].removePictureReference();

    request.set_picture_array_index(picture_index);

    //Function moves out these values, so copies are required.
    std::string file_in_bytes_copy = request.file_in_bytes();
    std::string thumbnail_in_bytes_copy = request.thumbnail_in_bytes();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_GT(response.timestamp(), 0);

    UserPictureDoc extracted_user_picture_doc = extractAndCompareInitialUserPictureOnSuccess(
            thumbnail_in_bytes_copy,
            file_in_bytes_copy
    );

    user_account.pictures[picture_index].setPictureReference(
            extracted_user_picture_doc.current_object_oid,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    compareUserAccounts();
}

TEST_F(SetPictureTesting, successful_maximumMessageSizeIsAcceptable) {
    request.set_file_in_bytes(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES));
    request.set_thumbnail_in_bytes(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES));

    EXPECT_LE(request.ByteSizeLong(), grpc_values::MAX_RECEIVE_MESSAGE_LENGTH);
}
