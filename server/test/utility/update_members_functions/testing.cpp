//
// Created by jeremiah on 6/21/22.
//
#include <utility_general_functions.h>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>

#include <user_account_keys.h>
#include <ChatMessageStream.pb.h>
#include <grpc_values.h>
#include <clear_database_for_testing.h>
#include <store_mongoDB_error_and_exception.h>
#include <generate_multiple_random_accounts.h>

#include <update_single_other_user.h>
#include <utility_chat_functions.h>
#include <generate_randoms.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class UpdateMembersFunctions : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    UserAccountDoc generated_user_account;

    std::chrono::milliseconds current_timestamp{-1L};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        //must be generated AFTER the account
        current_timestamp = getCurrentTimestamp();

        generated_user_account.getFromCollection(generated_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_noUpdatesNeeded) {

    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::builder::stream::document update_requested_member_doc;
    generated_user_account.convertToDocument(update_requested_member_doc);

    UserPictureDoc thumbnail_picture;
    for (const auto& pic : generated_user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            thumbnail_picture.getFromCollection(picture_reference);
            break;
        }
    }

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
        picture_index_info->set_index_number(i);

        if (generated_user_account.pictures[i].pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
        } else {
            picture_index_info->set_last_updated_timestamp(-1);
        }
    }

    UpdateOtherUserResponse response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &response,
            [&]() {
                num_times_response_function_called++;
            }
    );

    EXPECT_EQ(num_times_response_function_called, 0);

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_updateAccountState) {

    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::builder::stream::document update_requested_member_doc;
    generated_user_account.convertToDocument(update_requested_member_doc);

    UserPictureDoc thumbnail_picture;
    for (const auto& pic : generated_user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            thumbnail_picture.getFromCollection(picture_reference);
            break;
        }
    }

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
        picture_index_info->set_index_number(i);

        if (generated_user_account.pictures[i].pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
        } else {
            picture_index_info->set_last_updated_timestamp(-1);
        }
    }

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
    );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_updateMemberInfoBesidesPictures) {
    UserAccountDoc generated_user_account(generated_account_oid);

    bsoncxx::builder::stream::document update_requested_member_doc;
    generated_user_account.convertToDocument(update_requested_member_doc);

    UserPictureDoc thumbnail_picture;
    for (const auto& pic : generated_user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            thumbnail_picture.getFromCollection(picture_reference);
            break;
        }
    }

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age - 1);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
        picture_index_info->set_index_number(i);

        if (generated_user_account.pictures[i].pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
        } else {
            picture_index_info->set_last_updated_timestamp(-1);
        }
    }

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
    );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            update_requested_member_doc.view(),
            generated_account_oid,
            generated_response.mutable_user_info(),
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
            )
    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_updateToNoPictures) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    UserPictureDoc thumbnail_picture;
    for (const auto& pic : generated_user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            thumbnail_picture.getFromCollection(picture_reference);
            break;
        }
    }

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }
    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(0);
    other_user_info_for_updates.set_thumbnail_index_number(-1);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
        picture_index_info->set_index_number(i);

        if (generated_user_account.pictures[i].pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        } else {
            picture_index_info->set_last_updated_timestamp(-1);
        }
    }

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
        mongo_cpp_client,
        accounts_db,
        update_requested_member_doc.view(),
        user_accounts_collection,
        "dummy_chat_room_collection_name",
        generated_account_oid.to_string(),
        other_user_info_for_updates,
        current_timestamp,
        true,
        AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
        current_timestamp,
        HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
        &passed_response,
        [&]() {
            num_times_response_function_called++;
        }
    );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
        google::protobuf::util::MessageDifferencer::Equivalent(
            passed_response,
            generated_response
        )
    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestOnlyThumbnail_noUpdatesRequired) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    UserPictureDoc thumbnail_picture;
    for (const auto& pic : generated_user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            thumbnail_picture.getFromCollection(picture_reference);
            break;
        }
    }

    generated_user_account.convertToDocument(update_requested_member_doc);

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
        picture_index_info->set_index_number(i);

        if (generated_user_account.pictures[i].pictureStored()) {
            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        } else {
            picture_index_info->set_last_updated_timestamp(-1);
        }
    }

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 0);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestOnlyThumbnail_deletedOnClientNotServer) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(0);
    other_user_info_for_updates.set_thumbnail_index_number(-1);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored.value.count() - 1);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size((int)thumbnail_picture.thumbnail_in_bytes.size());
    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestOnlyThumbnail_deletedServerNotClient) {
    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(1234);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail("~");
    generated_response.mutable_user_info()->set_account_thumbnail_size(1);
    generated_response.mutable_user_info()->set_account_thumbnail_index(-1);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(-1);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestOnlyThumbnail_deletedOnClientAndServer) {
    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(0);
    other_user_info_for_updates.set_thumbnail_index_number(-1);
    other_user_info_for_updates.set_thumbnail_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 0);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestOnlyThumbnail_differentIndexNumberOnClientAndServer) {
    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(1234);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index+1);
    other_user_info_for_updates.set_thumbnail_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size((int)thumbnail_picture.thumbnail_in_bytes.size());
    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestOnlyThumbnail_sameIndexRequiresUpdate) {
    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes - 5);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(current_timestamp.count() - 1);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size((int)thumbnail_picture.thumbnail_in_bytes.size());
    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
        google::protobuf::util::MessageDifferencer::Equivalent(
            passed_response,
            generated_response
        )
    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_allPicturesDeleted) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    UserPictureDoc second_picture = generateRandomUserPicture();
    second_picture.picture_index = 1;
    second_picture.setIntoCollection();

    generated_user_account.pictures[1].setPictureReference(
            second_picture.current_object_oid,
            second_picture.timestamp_stored
            );

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    auto first_picture_value = other_user_info_for_updates.add_pictures_last_updated_timestamps();

    first_picture_value->set_index_number(0);
    first_picture_value->set_last_updated_timestamp(thumbnail_picture.timestamp_stored);

    auto second_picture_value = other_user_info_for_updates.add_pictures_last_updated_timestamps();

    second_picture_value->set_index_number(1);
    second_picture_value->set_last_updated_timestamp(second_picture.timestamp_stored);

    generated_user_account.pictures[0].removePictureReference();
    generated_user_account.pictures[1].removePictureReference();

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail("~");
    generated_response.mutable_user_info()->set_account_thumbnail_size(1);
    generated_response.mutable_user_info()->set_account_thumbnail_index(-1);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(-1);

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number(0);
    first_picture_gen->set_file_size(1);
    first_picture_gen->set_file_in_bytes("~");
    first_picture_gen->set_timestamp_picture_last_updated(-1L);

    auto second_picture_gen = generated_response.mutable_user_info()->add_picture();
    second_picture_gen->set_index_number(1);
    second_picture_gen->set_file_size(1);
    second_picture_gen->set_file_in_bytes("~");
    second_picture_gen->set_timestamp_picture_last_updated(-1L);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_thumbnailPictureWasDeleted) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 1;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[1].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    auto first_picture_value = other_user_info_for_updates.add_pictures_last_updated_timestamps();

    first_picture_value->set_index_number(0);
    first_picture_value->set_last_updated_timestamp(current_timestamp.count());

    auto second_picture_value = other_user_info_for_updates.add_pictures_last_updated_timestamps();

    second_picture_value->set_index_number(1);
    second_picture_value->set_last_updated_timestamp(current_timestamp.count());

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size((int)thumbnail_picture.thumbnail_in_bytes.size());
    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number(0);
    first_picture_gen->set_file_size(1);
    first_picture_gen->set_file_in_bytes("~");
    first_picture_gen->set_timestamp_picture_last_updated(-1L);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_finalPictureWasDeleted) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //make sure every picture is set
    for (auto& pic : generated_user_account.pictures) {
        if(!pic.pictureStored()) {

            UserPictureDoc picture = generateRandomUserPicture();

            pic.setPictureReference(
                picture.current_object_oid,
                picture.timestamp_stored
            );
        }
    }

    bsoncxx::oid thumbnail_oid;
    bsoncxx::types::b_date thumbnail_timestamp{std::chrono::milliseconds{-1L}};

    generated_user_account.pictures[0].getPictureReference(
            thumbnail_oid,
            thumbnail_timestamp
            );

    UserPictureDoc thumbnail_picture(thumbnail_oid);

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    for(int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        if(generated_user_account.pictures[i].pictureStored()) {
            auto picture_value = other_user_info_for_updates.add_pictures_last_updated_timestamps();

            bsoncxx::oid oid;
            bsoncxx::types::b_date timestamp{std::chrono::milliseconds{-1L}};

            generated_user_account.pictures[i].getPictureReference(
                    oid,
                    timestamp
                    );

            picture_value->set_index_number(i);
            picture_value->set_last_updated_timestamp(timestamp.value.count());
        }
    }

    //remove final picture
    generated_user_account.pictures[generated_user_account.pictures.size()-1].removePictureReference();

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

//    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
//    generated_response.mutable_user_info()->set_account_thumbnail_size(thumbnail_picture.thumbnail_in_bytes.size());
//    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
//    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number((int)generated_user_account.pictures.size()-1);
    first_picture_gen->set_file_size(1);
    first_picture_gen->set_file_in_bytes("~");
    first_picture_gen->set_timestamp_picture_last_updated(-1L);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_thumbnailPictureWasUpdated) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
        thumbnail_picture.current_object_oid,
        thumbnail_picture.timestamp_stored
    );

    UserPictureDoc second_picture = generateRandomUserPicture();
    second_picture.picture_index = 1;
    second_picture.setIntoCollection();

    generated_user_account.pictures[1].setPictureReference(
        second_picture.current_object_oid,
        second_picture.timestamp_stored
    );

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        if (generated_user_account.pictures[i].pictureStored()) {
            auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
            picture_index_info->set_index_number(i);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        }
    }

    UserPictureDoc updated_thumbnail_picture = generateRandomUserPicture();
    updated_thumbnail_picture.picture_index = 0;
    updated_thumbnail_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{thumbnail_picture.timestamp_stored.value.count() + 1000}};
    updated_thumbnail_picture.setIntoCollection();

    //update thumbnail
    generated_user_account.pictures[0].setPictureReference(
            updated_thumbnail_picture.current_object_oid,
            updated_thumbnail_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    update_requested_member_doc.clear();
    generated_user_account.convertToDocument(update_requested_member_doc);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(updated_thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size((int)updated_thumbnail_picture.thumbnail_in_bytes.size());
    generated_response.mutable_user_info()->set_account_thumbnail_index(updated_thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(updated_thumbnail_picture.timestamp_stored);

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number(updated_thumbnail_picture.picture_index);
    first_picture_gen->set_file_size(updated_thumbnail_picture.picture_size_in_bytes);
    first_picture_gen->set_file_in_bytes(updated_thumbnail_picture.picture_in_bytes);
    first_picture_gen->set_timestamp_picture_last_updated(updated_thumbnail_picture.timestamp_stored.value.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_nonThumbnailPictureWasUpdated) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    UserPictureDoc second_picture = generateRandomUserPicture();
    second_picture.picture_index = 1;
    second_picture.setIntoCollection();

    generated_user_account.pictures[1].setPictureReference(
            second_picture.current_object_oid,
            second_picture.timestamp_stored
            );

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(thumbnail_picture.picture_index);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        if (generated_user_account.pictures[i].pictureStored()) {
            auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
            picture_index_info->set_index_number(i);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        }
    }

    UserPictureDoc updated_second_picture = generateRandomUserPicture();
    updated_second_picture.picture_index = 1;
    updated_second_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{second_picture.timestamp_stored.value.count() + 1000}};
    updated_second_picture.setIntoCollection();

    //update second pictures
    generated_user_account.pictures[1].setPictureReference(
            updated_second_picture.current_object_oid,
            updated_second_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    update_requested_member_doc.clear();
    generated_user_account.convertToDocument(update_requested_member_doc);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number(updated_second_picture.picture_index);
    first_picture_gen->set_file_size(updated_second_picture.picture_size_in_bytes);
    first_picture_gen->set_file_in_bytes(updated_second_picture.picture_in_bytes);
    first_picture_gen->set_timestamp_picture_last_updated(updated_second_picture.timestamp_stored.value.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_differentThumbnailIndexNumberOnClientAndServer) {
    //NOTE: This should never happen, it means that the user has both picture updates however for some reason did NOT get the thumbnail
    // update when the picture was updated.

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    UserPictureDoc second_picture = generateRandomUserPicture();
    second_picture.picture_index = 1;
    second_picture.setIntoCollection();

    generated_user_account.pictures[1].setPictureReference(
            second_picture.current_object_oid,
            second_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(second_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(1);
    other_user_info_for_updates.set_thumbnail_timestamp(second_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {

        if (generated_user_account.pictures[i].pictureStored()) {
            auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
            picture_index_info->set_index_number(i);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        }
    }

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size(thumbnail_picture.thumbnail_size_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored.value.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
        google::protobuf::util::MessageDifferencer::Equivalent(
            passed_response,
            generated_response
        )
    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_separatedIndexValues_noUpdates) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    UserPictureDoc third_picture = generateRandomUserPicture();
    third_picture.picture_index = 2;
    third_picture.setIntoCollection();

    generated_user_account.pictures[2].setPictureReference(
            third_picture.current_object_oid,
            third_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        if (generated_user_account.pictures[i].pictureStored()) {
            auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
            picture_index_info->set_index_number(i);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        }
    }

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 0);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );
}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_separatedIndexValues_updateEmptyPicture) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    UserPictureDoc third_picture = generateRandomUserPicture();
    third_picture.picture_index = 2;
    third_picture.setIntoCollection();

    generated_user_account.pictures[2].setPictureReference(
            third_picture.current_object_oid,
            third_picture.timestamp_stored
            );

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        if (generated_user_account.pictures[i].pictureStored()) {
            auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
            picture_index_info->set_index_number(i);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        }
    }

    UserPictureDoc second_picture = generateRandomUserPicture();
    second_picture.picture_index = 1;
    second_picture.setIntoCollection();

    generated_user_account.pictures[1].setPictureReference(
            second_picture.current_object_oid,
            second_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number(second_picture.picture_index);
    first_picture_gen->set_file_size(second_picture.picture_size_in_bytes);
    first_picture_gen->set_file_in_bytes(second_picture.picture_in_bytes);
    first_picture_gen->set_timestamp_picture_last_updated(second_picture.timestamp_stored.value.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, updateSingleOtherUser_requestAllPictures_separatedIndexValues_updateNonEmptyPicture) {

    bsoncxx::builder::stream::document update_requested_member_doc;

    //remove all pictures for user
    for (auto& pic : generated_user_account.pictures) {
        pic.removePictureReference();
    }

    UserPictureDoc thumbnail_picture = generateRandomUserPicture();
    thumbnail_picture.picture_index = 0;
    thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            thumbnail_picture.current_object_oid,
            thumbnail_picture.timestamp_stored
            );

    UserPictureDoc third_picture = generateRandomUserPicture();
    third_picture.picture_index = 2;
    third_picture.setIntoCollection();

    generated_user_account.pictures[2].setPictureReference(
            third_picture.current_object_oid,
            third_picture.timestamp_stored
            );

    ASSERT_NE(thumbnail_picture.current_object_oid.to_string(), "000000000000000000000000");

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(thumbnail_picture.thumbnail_size_in_bytes);
    other_user_info_for_updates.set_thumbnail_index_number(0);
    other_user_info_for_updates.set_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(current_timestamp.count());

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    int pictures = 0;
    //set up user request
    for (int i = 0; i < (int)generated_user_account.pictures.size(); i++) {
        if (generated_user_account.pictures[i].pictureStored()) {
            auto picture_index_info = other_user_info_for_updates.add_pictures_last_updated_timestamps();
            picture_index_info->set_index_number(i);

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_user_account.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            picture_index_info->set_last_updated_timestamp(timestamp_stored.value.count());
            pictures++;
        }
    }

    UserPictureDoc updated_thumbnail_picture = generateRandomUserPicture();
    updated_thumbnail_picture.picture_index = 0;
    updated_thumbnail_picture.timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{thumbnail_picture.timestamp_stored.value.count() + 1000}};
    updated_thumbnail_picture.setIntoCollection();

    generated_user_account.pictures[0].setPictureReference(
            updated_thumbnail_picture.current_object_oid,
            updated_thumbnail_picture.timestamp_stored
            );

    generated_user_account.setIntoCollection();
    generated_user_account.convertToDocument(update_requested_member_doc);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            update_requested_member_doc.view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(updated_thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size(updated_thumbnail_picture.thumbnail_size_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_index(updated_thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(updated_thumbnail_picture.timestamp_stored);

    auto first_picture_gen = generated_response.mutable_user_info()->add_picture();
    first_picture_gen->set_index_number(updated_thumbnail_picture.picture_index);
    first_picture_gen->set_file_size(updated_thumbnail_picture.picture_size_in_bytes);
    first_picture_gen->set_file_in_bytes(updated_thumbnail_picture.picture_in_bytes);
    first_picture_gen->set_timestamp_picture_last_updated(updated_thumbnail_picture.timestamp_stored.value.count());

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

TEST_F(UpdateMembersFunctions, buildUpdateSingleOtherUserProjectionDoc) {

    //Request all info possible while only sending in the projection. This will make sure that all
    // relevant info is projected.

    UserPictureDoc thumbnail_picture;
    for (const auto& pic : generated_user_account.pictures) {
        if (pic.pictureStored()) {

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            pic.getPictureReference(
                    picture_reference,
                    timestamp_stored
                    );

            thumbnail_picture.getFromCollection(picture_reference);
            break;
        }
    }

    OtherUserInfoForUpdates other_user_info_for_updates;

    other_user_info_for_updates.set_account_oid(generated_account_oid.to_string());

    other_user_info_for_updates.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM);

    other_user_info_for_updates.set_thumbnail_size_in_bytes(0);
    other_user_info_for_updates.set_thumbnail_index_number(-1);
    other_user_info_for_updates.set_thumbnail_timestamp(-1);

    other_user_info_for_updates.set_first_name(generated_user_account.first_name);

    other_user_info_for_updates.set_age(generated_user_account.age);

    other_user_info_for_updates.set_member_info_last_updated_timestamp(-1);

    other_user_info_for_updates.set_event_status(LetsGoEventStatus::NOT_AN_EVENT);

    mongocxx::options::find opts;

    opts.projection(buildUpdateSingleOtherUserProjectionDoc());

    auto user_doc_using_projection = user_accounts_collection.find_one(
        document{}
            << "_id" << generated_user_account.current_object_oid
        << finalize,
        opts
    );

    ASSERT_TRUE(user_doc_using_projection);

    UpdateOtherUserResponse passed_response;

    int num_times_response_function_called = 0;

    updateSingleOtherUser(
            mongo_cpp_client,
            accounts_db,
            user_doc_using_projection->view(),
            user_accounts_collection,
            "dummy_chat_room_collection_name",
            generated_account_oid.to_string(),
            other_user_info_for_updates,
            current_timestamp,
            false,
            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
            current_timestamp,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            &passed_response,
            [&]() {
                num_times_response_function_called++;
            }
            );

    EXPECT_EQ(num_times_response_function_called, 1);

    UpdateOtherUserResponse generated_response;

    generated_response.set_timestamp_returned(current_timestamp.count());
    generated_response.set_account_last_activity_time(current_timestamp.count());

    generated_response.mutable_user_info()->set_account_thumbnail(thumbnail_picture.thumbnail_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_size(thumbnail_picture.thumbnail_size_in_bytes);
    generated_response.mutable_user_info()->set_account_thumbnail_index(thumbnail_picture.picture_index);
    generated_response.mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_picture.timestamp_stored);

    for(const auto& pic : generated_user_account.pictures) {
        if(pic.pictureStored()) {

            bsoncxx::oid oid;
            bsoncxx::types::b_date timestamp{std::chrono::milliseconds{-1L}};

            pic.getPictureReference(
                    oid,
                    timestamp
                    );

            UserPictureDoc picture_doc(oid);

            auto generated_picture = generated_response.mutable_user_info()->add_picture();
            generated_picture->set_index_number(picture_doc.picture_index);
            generated_picture->set_file_size(picture_doc.picture_size_in_bytes);
            generated_picture->set_file_in_bytes(picture_doc.picture_in_bytes);
            generated_picture->set_timestamp_picture_last_updated(picture_doc.timestamp_stored.value.count());
        }
    }

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_return_status(ReturnStatus::SUCCESS);

    generated_response.mutable_user_info()->set_account_oid(generated_account_oid.to_string());
    generated_response.mutable_user_info()->set_pictures_checked_for_updates(true);

    generated_response.mutable_user_info()->set_current_timestamp(current_timestamp.count());
    generated_response.set_account_state(AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN);

    bsoncxx::builder::stream::document update_requested_member_doc;
    generated_user_account.convertToDocument(update_requested_member_doc);

    bool return_val = saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            update_requested_member_doc.view(),
            generated_account_oid,
            generated_response.mutable_user_info(),
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            current_timestamp
    );

    EXPECT_TRUE(return_val);

    EXPECT_TRUE(
            google::protobuf::util::MessageDifferencer::Equivalent(
                    passed_response,
                    generated_response
                    )
                    );

}

