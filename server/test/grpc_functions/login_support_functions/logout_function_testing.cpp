//
// Created by jeremiah on 9/22/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "generate_multiple_random_accounts.h"
#include "LoginSupportFunctions.pb.h"
#include "setup_login_info.h"
#include "login_support_functions.h"
#include "connection_pool_global_variable.h"
#include "generate_randoms.h"
#include "build_match_made_chat_room.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class LogoutFunctionTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    UserAccountDoc generated_account_doc;

    loginsupport::LoginSupportRequest request;
    loginsupport::LoginSupportResponse response;

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    UserAccountStatisticsDoc statistics_doc_before;

    //must be initialized to use
    SetupTestingForUnMatch matched_user_values;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
        );
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);

        setupValidRequest();

        statistics_doc_before.getFromCollection(generated_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        logoutFunctionMongoDb(&request, &response);
    }

    void compareUserAccountDoc() {
        UserAccountDoc extracted_generated_account_doc(generated_account_oid);
        EXPECT_EQ(generated_account_doc, extracted_generated_account_doc);
    }

    void calculateDefaultUserAge() {

        generated_account_doc.age = server_parameter_restrictions::DEFAULT_USER_AGE_IF_ERROR;

        bool return_val = generateBirthYearForPassedAge(
                generated_account_doc.age,
                generated_account_doc.birth_year,
                generated_account_doc.birth_month,
                generated_account_doc.birth_day_of_month,
                generated_account_doc.birth_day_of_year
        );

        EXPECT_TRUE(return_val);

        const AgeRangeDataObject default_age_range = calculateAgeRangeFromUserAge(generated_account_doc.age);

        EXPECT_NE(default_age_range.min_age, -1);
        EXPECT_NE(default_age_range.max_age, -1);

        generated_account_doc.age_range.min = default_age_range.min_age;
        generated_account_doc.age_range.max = default_age_range.max_age;
    }

    void checkFunctionSuccessful() {
        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        EXPECT_GT(response.timestamp(), 0);

        compareUserAccountDoc();

        UserAccountStatisticsDoc extracted_statistics_doc(generated_account_oid);
        statistics_doc_before.account_logged_out_times.emplace_back(
                bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
                );

        EXPECT_EQ(statistics_doc_before, extracted_statistics_doc);
    }

    grpc_chat_commands::CreateChatRoomResponse createChatRoomForFunc() {
        grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
        grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

        setupUserLoginInfo(
                create_chat_room_request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
                );

        createChatRoom(&create_chat_room_request, &create_chat_room_response);

        EXPECT_EQ(create_chat_room_response.return_status(), ReturnStatus::SUCCESS);

        statistics_doc_before.getFromCollection(generated_account_oid);

        return create_chat_room_response;
    }

    static void compareChatRoomHeader(const ChatRoomHeaderDoc& original_chat_room_header, const std::string& chat_room_id) {
        ChatRoomHeaderDoc extracted_chat_room_header(chat_room_id);
        EXPECT_EQ(original_chat_room_header, extracted_chat_room_header);
    }

    std::string setupMatchToBeAdded() {
        matched_user_values.initialize();

        generated_account_oid = matched_user_values.user_account.current_object_oid;
        generated_account_doc.getFromCollection(generated_account_oid);

        const std::string match_element_oid_string = generated_account_doc.other_accounts_matched_with.front().oid_string;
        generated_account_doc.other_accounts_matched_with.pop_back();
        generated_account_doc.setIntoCollection();

        setupValidRequest();
        statistics_doc_before.getFromCollection(generated_account_oid);

        return match_element_oid_string;
    }

    grpc_chat_commands::CreateChatRoomResponse setupMatchToBeRemoved() {
        grpc_chat_commands::CreateChatRoomResponse creat_chat_room_response = createChatRoomForFunc();

        generated_account_doc.getFromCollection(generated_account_oid);
        generated_account_doc.other_accounts_matched_with.emplace_back(
                bsoncxx::oid{}.to_string(),
                bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{rand() % 100000}}
        );
        generated_account_doc.setIntoCollection();

        return creat_chat_room_response;
    }

};

TEST_F(LogoutFunctionTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info)->ReturnStatus {
                request.Clear();
                response.Clear();

                request.mutable_login_info()->CopyFrom(login_info);

                runFunction();

                return response.return_status();
            }
    );

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(LogoutFunctionTesting, nothingRequiresUpdated) {
    runFunction();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, invalidUserAge) {
    generated_account_doc.birth_year -= server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 2;

    generated_account_doc.age = calculateAge(
            current_timestamp,
            generated_account_doc.birth_year,
            generated_account_doc.birth_month,
            generated_account_doc.birth_day_of_month
    );

    generated_account_doc.setIntoCollection();

    runFunction();

    calculateDefaultUserAge();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, notEnoughPicturesStored) {
    generated_account_doc.pictures.pop_back();
    generated_account_doc.setIntoCollection();

    runFunction();

    generated_account_doc.pictures.emplace_back();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, tooManyPicturesStored) {
    UserPictureDoc generated_picture = generateRandomUserPicture();

    generated_account_doc.pictures.emplace_back(
            generated_picture.current_object_oid,
            generated_picture.timestamp_stored
    );

    generated_account_doc.setIntoCollection();

    runFunction();

    generated_account_doc.pictures.pop_back();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, pictureIsNotFoundInsideUserPictureCollection) {

    generated_account_doc.pictures[0].setPictureReference(
            bsoncxx::oid{},
            bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{rand() % 100000}}
    );
    generated_account_doc.setIntoCollection();

    runFunction();

    generated_account_doc.pictures[0].removePictureReference();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, thumbnailIsCorrupt) {
    int index_modified = -1;

    //corrupt thumbnail
    for(size_t i = 0; i < generated_account_doc.pictures.size(); ++i) {
        if(generated_account_doc.pictures[i].pictureStored()) {
            index_modified = (int)i;

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_account_doc.pictures[i].getPictureReference(
                picture_reference,
                timestamp_stored
            );

            UserPictureDoc picture_doc(picture_reference);

            picture_doc.thumbnail_size_in_bytes--;
            picture_doc.setIntoCollection();

            break;
        }
    }

    ASSERT_GT(index_modified, -1);

    runFunction();

    generated_account_doc.pictures[index_modified].removePictureReference();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, pictureIsCorrupt) {

    int index_modified = -1;

    //corrupt picture
    for(size_t i = 0; i < generated_account_doc.pictures.size(); ++i) {
        if(generated_account_doc.pictures[i].pictureStored()) {
            index_modified = (int)i;

            bsoncxx::oid picture_reference;
            bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

            generated_account_doc.pictures[i].getPictureReference(
                    picture_reference,
                    timestamp_stored
            );

            UserPictureDoc picture_doc(picture_reference);

            picture_doc.picture_size_in_bytes--;
            picture_doc.setIntoCollection();

            break;
        }
    }

    ASSERT_GT(index_modified, -1);

    runFunction();

    generated_account_doc.pictures[index_modified].removePictureReference();

    checkFunctionSuccessful();
}

TEST_F(LogoutFunctionTesting, existsInsideUserAccountChatRoom_existsInsideChatRoomHeader) {
    grpc_chat_commands::CreateChatRoomResponse creat_chat_room_response = createChatRoomForFunc();

    ChatRoomHeaderDoc original_chat_room_header(creat_chat_room_response.chat_room_id());

    generated_account_doc.getFromCollection(generated_account_oid);

    runFunction();

    checkFunctionSuccessful();

    compareChatRoomHeader(original_chat_room_header, creat_chat_room_response.chat_room_id());
}

TEST_F(LogoutFunctionTesting, existsInsideUserAccountChatRoom_doesNotExistInsideChatRoomHeader) {
    grpc_chat_commands::CreateChatRoomResponse creat_chat_room_response = createChatRoomForFunc();

    generated_account_doc.getFromCollection(generated_account_oid);

    ChatRoomHeaderDoc original_chat_room_header(creat_chat_room_response.chat_room_id());
    original_chat_room_header.accounts_in_chat_room[0].state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
    original_chat_room_header.setIntoCollection();

    runFunction();

    generated_account_doc.chat_rooms.pop_back();

    checkFunctionSuccessful();

    compareChatRoomHeader(original_chat_room_header, creat_chat_room_response.chat_room_id());
}

TEST_F(LogoutFunctionTesting, addMatchingAccountToArray) {
    const std::string match_element_oid_string = setupMatchToBeAdded();

    ChatRoomHeaderDoc original_chat_room_header(matched_user_values.matching_chat_room_id);

    runFunction();

    generated_account_doc.other_accounts_matched_with.emplace_back(
            match_element_oid_string,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    checkFunctionSuccessful();

    compareChatRoomHeader(original_chat_room_header, matched_user_values.matching_chat_room_id);
}

TEST_F(LogoutFunctionTesting, removeMatchingAccountFromArray) {
    grpc_chat_commands::CreateChatRoomResponse creat_chat_room_response = setupMatchToBeRemoved();

    ChatRoomHeaderDoc original_chat_room_header(creat_chat_room_response.chat_room_id());

    runFunction();

    generated_account_doc.other_accounts_matched_with.pop_back();

    checkFunctionSuccessful();

    compareChatRoomHeader(original_chat_room_header, creat_chat_room_response.chat_room_id());
}

TEST_F(LogoutFunctionTesting, addAndRemoveMatchingAccountsToArray) {
    const std::string match_element_oid_string = setupMatchToBeAdded();

    grpc_chat_commands::CreateChatRoomResponse creat_chat_room_response = setupMatchToBeRemoved();

    ChatRoomHeaderDoc match_chat_room_header(matched_user_values.matching_chat_room_id);
    ChatRoomHeaderDoc standard_chat_room_header(creat_chat_room_response.chat_room_id());

    runFunction();

    generated_account_doc.other_accounts_matched_with.pop_back();

    generated_account_doc.other_accounts_matched_with.emplace_back(
            match_element_oid_string,
            bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}}
    );

    checkFunctionSuccessful();

    compareChatRoomHeader(match_chat_room_header, matched_user_values.matching_chat_room_id);
    compareChatRoomHeader(standard_chat_room_header, creat_chat_room_response.chat_room_id());
}
