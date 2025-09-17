//
// Created by jeremiah on 10/6/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "RequestStatistics.pb.h"
#include "compare_equivalent_messages.h"
#include "SetAdminFields.pb.h"
#include "set_admin_fields.h"
#include "generate_multiple_random_accounts.h"
#include "deleted_objects.h"
#include "connection_pool_global_variable.h"
#include "generate_randoms.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RemoveUserPictureTesting : public testing::Test {
protected:

    set_admin_fields::RemoveUserPictureRequest request;
    set_admin_fields::SetAdminUnaryCallResponse response;

    bsoncxx::oid requested_account_oid;
    UserAccountDoc requested_user_account;
    UserPictureDoc requested_picture;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_user_oid(requested_account_oid.to_string());
        request.set_picture_oid(requested_picture.current_object_oid.to_string());
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        requested_account_oid = insertRandomAccounts(1, 0);
        requested_user_account.getFromCollection(requested_account_oid);

        findThumbnail(requested_picture);

        EXPECT_NE(requested_picture.current_object_oid.to_string(), "000000000000000000000000");

        setupValidRequest();
    }

    void findThumbnail(UserPictureDoc& thumbnail_picture) {
        for(const auto& pic : requested_user_account.pictures) {
            if(pic.pictureStored()) {
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
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);
        removeUserPicture(&request, &response);
    }

    void checkFunctionFailed() {
        EXPECT_FALSE(response.error_message().empty());
        EXPECT_FALSE(response.successful());

        //These should not have changed
        compareUserAccountDoc();
        comparePictureDoc();

        DeletedUserPictureDoc deleted_picture;
        deleted_picture.getFromCollection();

        EXPECT_EQ(deleted_picture.current_object_oid.to_string(), "000000000000000000000000");
    }

    void checkFunctionSuccessful() {
        EXPECT_TRUE(response.error_message().empty());
        EXPECT_TRUE(response.successful());
    }

    void removePictureReferenceFromUserAccount() {
        //remove picture reference from user account
        for(auto& pic : requested_user_account.pictures) {
            if(pic.pictureStored()) {
                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

                pic.getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                if(picture_reference.to_string() == request.picture_oid()) {
                    pic.removePictureReference();
                    break;
                }
            }
        }
    }

    void checkBasicSuccessfulFunction() {
        checkFunctionSuccessful();

        removePictureReferenceFromUserAccount();
        compareUserAccountDoc();

        //picture no longer exists inside pictures collection
        UserPictureDoc extracted_picture(requested_picture.current_object_oid);
        EXPECT_EQ(extracted_picture.current_object_oid.to_string(), "000000000000000000000000");

        //picture exists inside deleted collection
        DeletedUserPictureDoc generated_deleted_picture(
                requested_picture,
                std::make_unique<bool>(true),
                bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
                ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
                std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME)
        );
        compareDeletedPictureDoc<true>(generated_deleted_picture);
    }

    void compareUserAccountDoc() {
        UserAccountDoc extracted_user_account(requested_account_oid);
        EXPECT_EQ(requested_user_account, extracted_user_account);
    }

    void comparePictureDoc() {
        UserPictureDoc extracted_picture(requested_picture.current_object_oid);
        EXPECT_EQ(requested_picture, extracted_picture);
    }

    template <bool requires_timestamp_stored>
    void compareDeletedPictureDoc(DeletedUserPictureDoc& generated_deleted_picture) {
        DeletedUserPictureDoc extracted_deleted_picture(generated_deleted_picture.current_object_oid);

        if(requires_timestamp_stored) {
            EXPECT_GT(extracted_deleted_picture.timestamp_removed, 0);
            generated_deleted_picture.timestamp_removed = extracted_deleted_picture.timestamp_removed;
        }

        EXPECT_EQ(generated_deleted_picture, extracted_deleted_picture);
    }

    static DeletedUserPictureDoc generateDeletedPicture() {
        DeletedUserPictureDoc generated_deleted_picture(
                generateRandomUserPicture(),
                nullptr,
                bsoncxx::types::b_date{getCurrentTimestamp() - std::chrono::milliseconds{10000}},
                ReasonPictureDeleted::REASON_PICTURE_DELETED_USER_UPDATED_PICTURE_INDEX,
                nullptr
        );
        generated_deleted_picture.setIntoCollection();
        return generated_deleted_picture;
    }

    grpc_chat_commands::CreateChatRoomResponse createChatRoomWrapper() {
        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                requested_account_oid,
                requested_user_account.logged_in_token,
                requested_user_account.installation_ids.front()
        );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        requested_user_account.getFromCollection(requested_account_oid);
        requested_picture.getFromCollection(requested_picture.current_object_oid);

        return create_chat_room_response;
    }
};

TEST_F(RemoveUserPictureTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_message().empty());
                return response.successful();
            }
    );
}

TEST_F(RemoveUserPictureTesting, noAdminPriveledge) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(RemoveUserPictureTesting, invalidUserOid) {
    request.set_user_oid("invalid_user_oid");

    runFunction();

    checkFunctionFailed();
}

TEST_F(RemoveUserPictureTesting, invalidPictureOid) {
    request.set_picture_oid("invalid_picture_oid");

    runFunction();

    checkFunctionFailed();
}

TEST_F(RemoveUserPictureTesting, success) {
    runFunction();

    checkBasicSuccessfulFunction();
}

TEST_F(RemoveUserPictureTesting, pictureExistsInsideDeletedCollection) {

    DeletedUserPictureDoc generated_deleted_picture = generateDeletedPicture();

    request.set_picture_oid(generated_deleted_picture.current_object_oid.to_string());

    runFunction();

    checkFunctionSuccessful();

    compareUserAccountDoc();

    //copy references to user thumbnail
    std::copy(
            generated_deleted_picture.thumbnail_references.begin(),
            generated_deleted_picture.thumbnail_references.end(),
            std::back_inserter(requested_picture.thumbnail_references)
    );
    comparePictureDoc();

    generated_deleted_picture.references_removed_after_delete = std::make_unique<bool>(true);
    compareDeletedPictureDoc<false>(generated_deleted_picture);
}

TEST_F(RemoveUserPictureTesting, pictureHasAlreadyHadReferencesRemoved) {

    DeletedUserPictureDoc generated_deleted_picture = generateDeletedPicture();
    generated_deleted_picture.references_removed_after_delete = std::make_unique<bool>(true);
    generated_deleted_picture.setIntoCollection();

    request.set_picture_oid(generated_deleted_picture.current_object_oid.to_string());

    runFunction();

    checkFunctionSuccessful();

    //Nothing should change
    compareUserAccountDoc();
    comparePictureDoc();
    compareDeletedPictureDoc<false>(generated_deleted_picture);
}

TEST_F(RemoveUserPictureTesting, pictureDoesNotExistAnywhere) {
    request.set_picture_oid(bsoncxx::oid{}.to_string());

    runFunction();

    checkFunctionFailed();
}

TEST_F(RemoveUserPictureTesting, userAccountDoesNotExist) {
    request.set_user_oid(bsoncxx::oid{}.to_string());

    runFunction();

    checkFunctionSuccessful();

    //user account should not have changed
    compareUserAccountDoc();

    //picture no longer exists inside pictures collection
    UserPictureDoc extracted_picture(requested_picture.current_object_oid);
    EXPECT_EQ(extracted_picture.current_object_oid.to_string(), "000000000000000000000000");

    //picture exists inside deleted collection
    DeletedUserPictureDoc generated_deleted_picture(
            requested_picture,
            std::make_unique<bool>(true),
            bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
            std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME)
    );
    compareDeletedPictureDoc<true>(generated_deleted_picture);
}

TEST_F(RemoveUserPictureTesting, pictureElementDoesNotExistInUserAccount) {
    removePictureReferenceFromUserAccount();
    requested_user_account.setIntoCollection();

    runFunction();

    checkFunctionSuccessful();

    //user account should not have changed
    compareUserAccountDoc();

    //picture no longer exists inside pictures collection
    UserPictureDoc extracted_picture(requested_picture.current_object_oid);
    EXPECT_EQ(extracted_picture.current_object_oid.to_string(), "000000000000000000000000");

    //picture exists inside deleted collection
    DeletedUserPictureDoc generated_deleted_picture(
            requested_picture,
            std::make_unique<bool>(true),
            bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
            std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME)
    );
    compareDeletedPictureDoc<true>(generated_deleted_picture);
}

TEST_F(RemoveUserPictureTesting, pictureIsThumbnailInChatRoom) {

    //If there is only one user picture, add another one.
    if(!requested_user_account.pictures[1].pictureStored()) {
        UserPictureDoc new_picture = generateRandomUserPicture();
        new_picture.picture_index = 1;
        new_picture.thumbnail_references.clear();
        new_picture.user_account_reference = requested_account_oid;
        new_picture.setIntoCollection();

        requested_user_account.pictures.emplace_back(
                new_picture.current_object_oid,
                new_picture.timestamp_stored
        );
        requested_user_account.setIntoCollection();
    }

    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response = createChatRoomWrapper();

    ASSERT_FALSE(requested_picture.thumbnail_references.empty());
    EXPECT_EQ(requested_picture.thumbnail_references[0], create_chat_room_response.chat_room_id());

    ChatRoomHeaderDoc chat_room_header(create_chat_room_response.chat_room_id());

    ASSERT_EQ(chat_room_header.accounts_in_chat_room.size(), 1);
    EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].thumbnail_reference, requested_picture.current_object_oid.to_string());

    runFunction();

    checkFunctionSuccessful();

    removePictureReferenceFromUserAccount();

    compareUserAccountDoc();

    //chat room added to new thumbnail
    UserPictureDoc new_thumbnail;
    findThumbnail(new_thumbnail);
    EXPECT_NE(new_thumbnail.current_object_oid.to_string(), "000000000000000000000000");
    ASSERT_FALSE(new_thumbnail.thumbnail_references.empty());
    EXPECT_EQ(new_thumbnail.thumbnail_references[0], create_chat_room_response.chat_room_id());

    // new thumbnail reference set inside chat room
    chat_room_header.getFromCollection(create_chat_room_response.chat_room_id());
    ASSERT_EQ(chat_room_header.accounts_in_chat_room.size(), 1);
    EXPECT_EQ(chat_room_header.accounts_in_chat_room[0].thumbnail_reference, new_thumbnail.current_object_oid.to_string());

    //picture no longer exists inside pictures collection
    UserPictureDoc extracted_picture(requested_picture.current_object_oid);
    EXPECT_EQ(extracted_picture.current_object_oid.to_string(), "000000000000000000000000");

    //picture exists inside deleted collection
    DeletedUserPictureDoc generated_deleted_picture(
            requested_picture,
            std::make_unique<bool>(true),
            bsoncxx::types::b_date{std::chrono::milliseconds{-1}},
            ReasonPictureDeleted::REASON_PICTURE_DELETED_ADMIN_DELETED_PICTURE,
            std::make_unique<std::string>(TEMP_ADMIN_ACCOUNT_NAME)
    );
    compareDeletedPictureDoc<true>(generated_deleted_picture);
}
