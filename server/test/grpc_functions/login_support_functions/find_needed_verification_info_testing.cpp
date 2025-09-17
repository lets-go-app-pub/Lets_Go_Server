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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class FindNeededVerificationInfoTesting : public ::testing::Test {
protected:

    bsoncxx::oid generated_account_oid;

    UserAccountDoc generated_account_doc;

    loginsupport::NeededVeriInfoRequest request;
    loginsupport::NeededVeriInfoResponse response;

    long expected_birthday_timestamp = 0;
    long expected_email_timestamp = 0;
    long expected_gender_timestamp = 0;
    long expected_name_timestamp = 0;
    long expected_categories_timestamp = 0;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                generated_account_oid,
                generated_account_doc.logged_in_token,
                generated_account_doc.installation_ids.front()
        );

        request.set_client_latitude(79.4562);
        request.set_client_longitude(-134.101);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);

        generated_account_doc.status = UserAccountStatus::STATUS_REQUIRES_MORE_INFO;
        generated_account_doc.setIntoCollection();

        setupValidRequest();

        expected_birthday_timestamp = generated_account_doc.birthday_timestamp;
        expected_email_timestamp = generated_account_doc.email_timestamp;
        expected_gender_timestamp = generated_account_doc.gender_timestamp;
        expected_name_timestamp = generated_account_doc.first_name_timestamp;
        expected_categories_timestamp = generated_account_doc.categories_timestamp;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        findNeededVerificationInfo(&request, &response);
    }

    void compareUserAccountDoc() {
        UserAccountDoc extracted_generated_account_doc(generated_account_oid);
        EXPECT_EQ(generated_account_doc, extracted_generated_account_doc);
    }

    void checkResponseSetToSuccess() {
        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        EXPECT_EQ(response.access_status(), AccessStatus::ACCESS_GRANTED);

        checkAllTimestamps();

        EXPECT_EQ(response.post_login_info().user_bio(), "~");
        EXPECT_EQ(response.post_login_info().user_city(), "~");
        EXPECT_EQ(response.post_login_info().min_age(), generated_account_doc.age_range.min);
        EXPECT_EQ(response.post_login_info().max_age(), generated_account_doc.age_range.max);
        EXPECT_EQ(response.post_login_info().max_distance(), generated_account_doc.max_distance);

        ASSERT_EQ(response.post_login_info().gender_range_size(), generated_account_doc.genders_range.size());

        for(size_t i = 0; i < generated_account_doc.genders_range.size(); ++i) {
            EXPECT_EQ(response.post_login_info().gender_range(i), generated_account_doc.genders_range[i]);
        }

        EXPECT_GT(response.server_timestamp(), 0);
    }

    void checkAllTimestamps() {
        EXPECT_EQ(response.pre_login_timestamps().birthday_timestamp(), expected_birthday_timestamp);
        EXPECT_EQ(response.pre_login_timestamps().email_timestamp(), expected_email_timestamp);
        EXPECT_EQ(response.pre_login_timestamps().gender_timestamp(), expected_gender_timestamp);
        EXPECT_EQ(response.pre_login_timestamps().name_timestamp(), expected_name_timestamp);
        EXPECT_EQ(response.pre_login_timestamps().categories_timestamp(), expected_categories_timestamp);

        checkPicturesTimestamps();
    }

    void checkPicturesTimestamps() {
        ASSERT_EQ(generated_account_doc.pictures.size(), response.picture_timestamps().size());

        for(size_t i = 0; i < generated_account_doc.pictures.size(); ++i) {
            if(generated_account_doc.pictures[i].pictureStored()) {

                bsoncxx::oid picture_reference;
                bsoncxx::types::b_date timestamp_stored{std::chrono::milliseconds{-1}};

                generated_account_doc.pictures[i].getPictureReference(
                        picture_reference,
                        timestamp_stored
                );

                EXPECT_EQ(timestamp_stored, response.picture_timestamps(i));
            } else {
                EXPECT_EQ(-1, response.picture_timestamps(i));
            }
        }
    }
};

TEST_F(FindNeededVerificationInfoTesting, invalidLoginInfo) {
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

TEST_F(FindNeededVerificationInfoTesting, invalidLatitude) {
    //latitude needs to be between -90 and 90
    request.set_client_latitude(-120.123);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, invalidLongitude) {
    //latitude needs to be between -180 and 180
    request.set_client_longitude(200.444);

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, userAccountStatusActive) {
    generated_account_doc.status = UserAccountStatus::STATUS_ACTIVE;
    generated_account_doc.setIntoCollection();

    runFunction();

    checkResponseSetToSuccess();

    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, birthdayNotSet) {
    generated_account_doc.birthday_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.access_status(), AccessStatus::NEEDS_MORE_INFO);

    expected_birthday_timestamp = -1;

    checkAllTimestamps();

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, emailNotSet) {
    generated_account_doc.email_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.access_status(), AccessStatus::NEEDS_MORE_INFO);

    expected_email_timestamp = -1;

    checkAllTimestamps();

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, genderNotSet) {
    generated_account_doc.gender_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.access_status(), AccessStatus::NEEDS_MORE_INFO);

    expected_gender_timestamp = -1;

    checkAllTimestamps();

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, nameNotSet) {
    generated_account_doc.first_name_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.access_status(), AccessStatus::NEEDS_MORE_INFO);

    expected_name_timestamp = -1;

    checkAllTimestamps();

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, activitiesNotSet) {
    generated_account_doc.categories_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    generated_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.access_status(), AccessStatus::NEEDS_MORE_INFO);

    expected_categories_timestamp = -1;

    checkAllTimestamps();

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, picturesNotSet) {

    //remove all picture references from user account
    for(auto& pic: generated_account_doc.pictures) {
        pic.removePictureReference();
    }
    generated_account_doc.setIntoCollection();

    runFunction();

    //The document was updated and so must be extracted again to get actual values.
    generated_account_doc.getFromCollection(generated_account_oid);
    checkResponseSetToSuccess();

    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, userAgeIsInvalid) {

    //Should run the block for when default_age_range is invalid

    generated_account_doc.age = 0;
    generated_account_doc.setIntoCollection();

    runFunction();

    EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.access_status(), AccessStatus::NEEDS_MORE_INFO);

    expected_birthday_timestamp = -1;

    generated_account_doc.birthday_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{expected_birthday_timestamp}};
    generated_account_doc.birth_year = -1;
    generated_account_doc.birth_month = -1;
    generated_account_doc.birth_day_of_month = -1;
    generated_account_doc.birth_day_of_year = -1;
    generated_account_doc.age = -1;

    checkAllTimestamps();

    //Account doc should not have changed.
    compareUserAccountDoc();
}

TEST_F(FindNeededVerificationInfoTesting, successfullySetToActive) {
    UserAccountStatisticsDoc generated_user_account_statistics(generated_account_oid);

    runFunction();

    //The document was updated and so must be extracted again to get actual values.
    generated_account_doc.getFromCollection(generated_account_oid);
    checkResponseSetToSuccess();

    compareUserAccountDoc();

    generated_user_account_statistics.locations.emplace_back(
            request.client_longitude(),
            request.client_latitude(),
            bsoncxx::types::b_date{std::chrono::milliseconds{response.server_timestamp() - 1}}
            );

    UserAccountStatisticsDoc extracted_user_account_statistics(generated_account_oid);

    EXPECT_EQ(generated_user_account_statistics, extracted_user_account_statistics);
}
