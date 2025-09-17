//
// Created by jeremiah on 8/29/22.
//
#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "report_values.h"
#include "chat_room_shared_keys.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"
#include "EmailSendingMessages.pb.h"
#include "helper_functions/send_email_testing.h"
#include "email_sending_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AccountRecoveryTesting : public ::testing::Test {
protected:

    SendEmailTesting send_email_object{};
    email_sending_messages::AccountRecoveryRequest account_recovery_request;
    email_sending_messages::AccountRecoveryResponse account_recovery_response;

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

TEST_F(AccountRecoveryTesting, invalidPhoneNumber) {
    account_recovery_request.set_phone_number("invalidPhoneNumber");
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::INVALID_PHONE_NUMBER);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(AccountRecoveryTesting, invalidLetsGoVersion) {
    account_recovery_request.set_phone_number(generated_account_doc.phone_number);
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION - 1);

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::OUTDATED_VERSION);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(AccountRecoveryTesting, phoneNumberDoesNotExist) {
    std::string non_existing_phone_number = generated_account_doc.phone_number;

    //add one to the phone number to make sure it is unique
    for (int i = (int) non_existing_phone_number.size() - 1; i >= 0; --i) {
        if (non_existing_phone_number[i] == '9') {
            non_existing_phone_number[i] = '0';
        } else if (i == 0) { //this means the number was 999-999-9999
            non_existing_phone_number = "0000000000";
        } else {
            non_existing_phone_number[i] += 1;
            break;
        }
    }

    account_recovery_request.set_phone_number(non_existing_phone_number);
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::SUCCESS);
    EXPECT_EQ(account_recovery_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    EXPECT_GT(account_recovery_response.timestamp(), -1);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(AccountRecoveryTesting, emailOnCooldown) {
    account_recovery_request.set_phone_number(generated_account_doc.phone_number);
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    generated_account_doc.time_email_can_be_sent_again = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{1000}};
    generated_account_doc.setIntoCollection();

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::SUCCESS);
    EXPECT_EQ(account_recovery_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    EXPECT_GT(account_recovery_response.timestamp(), -1);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(AccountRecoveryTesting, accountSuspended) {
    account_recovery_request.set_phone_number(generated_account_doc.phone_number);
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    generated_account_doc.status = UserAccountStatus::STATUS_SUSPENDED;
    generated_account_doc.setIntoCollection();

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::SUCCESS);
    EXPECT_EQ(account_recovery_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    EXPECT_GT(account_recovery_response.timestamp(), -1);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(AccountRecoveryTesting, accountBanned) {
    account_recovery_request.set_phone_number(generated_account_doc.phone_number);
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    generated_account_doc.status = UserAccountStatus::STATUS_BANNED;
    generated_account_doc.setIntoCollection();

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::SUCCESS);
    EXPECT_EQ(account_recovery_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    EXPECT_GT(account_recovery_response.timestamp(), -1);

    UserAccountDoc extracted_account_doc(generated_account_oid);
    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    EXPECT_TRUE(send_email_object.sent_emails.empty());
}

TEST_F(AccountRecoveryTesting, successfullySent) {
    account_recovery_request.set_phone_number(generated_account_doc.phone_number);
    account_recovery_request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);

    accountRecovery(
            &account_recovery_request,
            &account_recovery_response,
            &send_email_object
    );

    EXPECT_EQ(account_recovery_response.account_recovery_status(), email_sending_messages::AccountRecoveryResponse::SUCCESS);
    EXPECT_EQ(account_recovery_response.email_sent_status(), email_sending_messages::EmailSentStatus::EMAIL_SUCCESS);
    EXPECT_GT(account_recovery_response.timestamp(), -1);

    UserAccountDoc extracted_account_doc(generated_account_oid);

    EXPECT_GT(extracted_account_doc.time_email_can_be_sent_again, generated_account_doc.time_email_can_be_sent_again);
    generated_account_doc.time_email_can_be_sent_again = extracted_account_doc.time_email_can_be_sent_again;

    EXPECT_EQ(extracted_account_doc, generated_account_doc);

    AccountRecoveryDoc generated_account_recovery_doc;
    AccountRecoveryDoc extracted_account_recovery_doc(generated_account_doc.phone_number);
    EXPECT_NE(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");
    EXPECT_GT(extracted_account_recovery_doc.time_verification_code_generated.value.count(), 0);
    EXPECT_EQ(extracted_account_recovery_doc.verification_code.size(), general_values::EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH);

    generated_account_recovery_doc.phone_number = generated_account_doc.phone_number;
    generated_account_recovery_doc.user_account_oid = generated_account_doc.current_object_oid;
    generated_account_recovery_doc.number_attempts = 0;
    generated_account_recovery_doc.verification_code = extracted_account_recovery_doc.verification_code;
    generated_account_recovery_doc.time_verification_code_generated = extracted_account_recovery_doc.time_verification_code_generated;
    generated_account_recovery_doc.current_object_oid = extracted_account_recovery_doc.current_object_oid;

    EXPECT_EQ(generated_account_recovery_doc, extracted_account_recovery_doc);

    //Actual values are checked when send_email_helper is tested.
    EXPECT_EQ(send_email_object.sent_emails.size(), 1);
}
