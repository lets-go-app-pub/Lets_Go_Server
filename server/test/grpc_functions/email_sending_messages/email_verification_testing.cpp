//
// Created by jeremiah on 8/29/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"
#include "helper_functions/send_email_testing.h"
#include "EmailSendingMessages.pb.h"
#include "email_sending_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class EmailVerificationTesting : public ::testing::Test {
protected:

    SendEmailTesting send_email_object{};
    email_sending_messages::EmailVerificationRequest email_verification_request;
    email_sending_messages::EmailVerificationResponse email_verification_response;

    bsoncxx::oid generated_account_oid;

    UserAccountDoc generated_account_doc;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_oid = insertRandomAccounts(1, 0);

        generated_account_doc.getFromCollection(generated_account_oid);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

TEST_F(EmailVerificationTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {
                email_verification_request.mutable_login_info()->CopyFrom(login_info);

                emailVerification(&email_verification_request, &email_verification_response, &send_email_object);

                return email_verification_response.return_status();
            }
    );

    UserAccountDoc extracted_account_doc(generated_account_oid);

    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    EmailVerificationDoc extracted_email_verification_doc(generated_account_doc.current_object_oid);
    EXPECT_EQ(extracted_email_verification_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(EmailVerificationTesting, sendEmailOnCooldown) {
    setupUserLoginInfo(
            email_verification_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    generated_account_doc.time_email_can_be_sent_again = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{1000}};
    generated_account_doc.setIntoCollection();

    emailVerification(&email_verification_request, &email_verification_response, &send_email_object);

    EXPECT_EQ(email_verification_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(email_verification_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_ON_COOL_DOWN);
    EXPECT_EQ(email_verification_response.email_address_is_already_verified(), false);
    EXPECT_GT(email_verification_response.timestamp(), 0);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    EmailVerificationDoc extracted_email_verification_doc(generated_account_doc.current_object_oid);
    EXPECT_EQ(extracted_email_verification_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(EmailVerificationTesting, emailAddressAlreadyVerified) {
    setupUserLoginInfo(
            email_verification_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    generated_account_doc.email_address_requires_verification = false;
    generated_account_doc.setIntoCollection();

    emailVerification(&email_verification_request, &email_verification_response, &send_email_object);

    EXPECT_EQ(email_verification_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(email_verification_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_VALUE_NOT_SET);
    EXPECT_EQ(email_verification_response.email_address_is_already_verified(), true);
    EXPECT_GT(email_verification_response.timestamp(), 0);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    generated_account_doc.time_email_can_be_sent_again = extracted_account_doc.time_email_can_be_sent_again;
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    EmailVerificationDoc extracted_email_verification_doc(generated_account_doc.current_object_oid);
    EXPECT_EQ(extracted_email_verification_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(EmailVerificationTesting, successfullyRan) {
    setupUserLoginInfo(
            email_verification_request.mutable_login_info(),
            generated_account_oid,
            generated_account_doc.logged_in_token,
            generated_account_doc.installation_ids.front()
    );

    emailVerification(&email_verification_request, &email_verification_response, &send_email_object);

    EXPECT_EQ(email_verification_response.return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(email_verification_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    EXPECT_EQ(email_verification_response.email_address_is_already_verified(), false);
    EXPECT_GT(email_verification_response.timestamp(), 0);

    UserAccountDoc extracted_account_doc(generated_account_oid);

    EXPECT_GT(extracted_account_doc.time_email_can_be_sent_again, generated_account_doc.time_email_can_be_sent_again);
    generated_account_doc.time_email_can_be_sent_again = extracted_account_doc.time_email_can_be_sent_again;
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    EmailVerificationDoc generated_email_verification_doc;
    EmailVerificationDoc extracted_email_verification_doc(generated_account_doc.current_object_oid);
    EXPECT_NE(extracted_email_verification_doc.current_object_oid.to_string(), "000000000000000000000000");
    EXPECT_GT(extracted_email_verification_doc.time_verification_code_generated.value.count(), 0);
    EXPECT_EQ(extracted_email_verification_doc.verification_code.size(), general_values::EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH);

    generated_email_verification_doc.user_account_reference = generated_account_oid;
    generated_email_verification_doc.verification_code = extracted_email_verification_doc.verification_code;
    generated_email_verification_doc.time_verification_code_generated = extracted_email_verification_doc.time_verification_code_generated;
    generated_email_verification_doc.address_being_verified = generated_account_doc.email_address;
    generated_email_verification_doc.current_object_oid = extracted_email_verification_doc.current_object_oid;

    EXPECT_EQ(generated_email_verification_doc, extracted_email_verification_doc);

    //Actual values are checked when send_email_helper is tested.
    EXPECT_EQ(send_email_object.sent_emails.size(), 1);
}
