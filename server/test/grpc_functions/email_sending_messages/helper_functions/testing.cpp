//
// Created by jeremiah on 8/28/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"

#include "report_values.h"
#include "utility_general_functions.h"
#include "generate_multiple_random_accounts.h"

#include "helper_functions/helper_functions.h"
#include "python_handle_gil_state.h"
#include "connection_pool_global_variable.h"
#include "account_recovery_keys.h"
#include "send_email_testing.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class EmailSendingMessagesHelperFunctionsTesting : public ::testing::Test {
protected:

    SendEmailTesting send_email_object{};

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

class AttemptToGenerateUniqueCodeTwiceTesting : public ::testing::Test {
protected:

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection account_recovery_col = accounts_db[collection_names::ACCOUNT_RECOVERY_COLLECTION_NAME];

    std::string randomly_generated_code;

    const std::string phone_number = "+16012022053";
    const std::string second_phone_number = "+15012022053";
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    bsoncxx::oid user_account_oid{};

    AccountRecoveryDoc generated_account_recovery_doc;

    bsoncxx::document::value find_document = document{}
            << account_recovery_keys::PHONE_NUMBER << phone_number
            << finalize;

    int num_times_lambda_called = 0;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        generated_account_recovery_doc.getFromCollection(phone_number);
        EXPECT_EQ(generated_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    std::string generateAndStoreWithSamePhoneNumber() {
        std::string original_verification_code =  gen_random_alpha_numeric_string(
                general_values::EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH
        );

        AccountRecoveryDoc second_user_account_recovery_doc;

        second_user_account_recovery_doc.verification_code = original_verification_code;
        second_user_account_recovery_doc.phone_number = second_phone_number;
        second_user_account_recovery_doc.time_verification_code_generated = bsoncxx::types::b_date{std::chrono::milliseconds {-1}};
        second_user_account_recovery_doc.number_attempts = 5;
        second_user_account_recovery_doc.user_account_oid = user_account_oid;

        second_user_account_recovery_doc.setIntoCollection();

        //Make sure document was properly stored.
        EXPECT_NE(second_user_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");

        return original_verification_code;
    }

    bsoncxx::document::value buildUpdateDocument(const std::string& verification_code) {
        return document{}
                << "$set" << open_document
                        << account_recovery_keys::PHONE_NUMBER << phone_number
                        << account_recovery_keys::VERIFICATION_CODE << verification_code
                        << account_recovery_keys::TIME_VERIFICATION_CODE_GENERATED << bsoncxx::types::b_date{current_timestamp}
                        << account_recovery_keys::NUMBER_ATTEMPTS << 0
                        << account_recovery_keys::USER_ACCOUNT_OID << user_account_oid
                << close_document
        << finalize;
    }

    void extractAndCompareAccountRecovery() {
        AccountRecoveryDoc extracted_account_recovery_doc(phone_number);

        generated_account_recovery_doc.current_object_oid = extracted_account_recovery_doc.current_object_oid;
        generated_account_recovery_doc.verification_code = randomly_generated_code;
        generated_account_recovery_doc.phone_number = phone_number;
        generated_account_recovery_doc.time_verification_code_generated = bsoncxx::types::b_date{current_timestamp};
        generated_account_recovery_doc.number_attempts = 0;
        generated_account_recovery_doc.user_account_oid = user_account_oid;

        EXPECT_EQ(extracted_account_recovery_doc, generated_account_recovery_doc);
    }
};

TEST_F(EmailSendingMessagesHelperFunctionsTesting, success) {

    bsoncxx::oid generated_account_oid = insertRandomAccounts(1, 0);

    UserAccountStatisticsDoc generated_account_statistics(generated_account_oid);

    const std::string email_prefix = gen_random_alpha_numeric_string(rand() % 100);
    const std::string email_address = gen_random_alpha_numeric_string(rand() % 100);
    const std::string email_verification_subject = gen_random_alpha_numeric_string(rand() % 100);
    const std::string email_verification_content = gen_random_alpha_numeric_string(rand() % 100);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    bool result = send_email_helper(
            email_prefix,
            email_address,
            email_verification_subject,
            email_verification_content,
            generated_account_oid,
            current_timestamp,
            &send_email_object
    );

    EXPECT_TRUE(result);

    //Allow the thread pool to store the extra information.
    std::this_thread::sleep_for(std::chrono::milliseconds{300});

    {
        UserAccountStatisticsDoc extracted_account_statistics(generated_account_oid);

        generated_account_statistics.email_sent_times.emplace_back(
                email_address,
                email_prefix,
                email_verification_subject,
                email_verification_content,
                bsoncxx::types::b_date{current_timestamp}
        );

        EXPECT_EQ(extracted_account_statistics, generated_account_statistics);
    }

    //locks this thread for python instance
    PythonHandleGilState gil_state;

    ASSERT_EQ(send_email_object.sent_emails.size(), 1);

    ASSERT_TRUE(send_email_object.sent_emails[0].pArgs);
    ASSERT_TRUE(send_email_object.sent_emails[0].pFunc);

    EXPECT_TRUE(PyTuple_Check(send_email_object.sent_emails[0].pArgs));
    EXPECT_TRUE(PyCallable_Check(send_email_object.sent_emails[0].pFunc));

    char* extracted_email_prefix = nullptr;
    char* extracted_email_address = nullptr;
    char* extracted_email_verification_subject = nullptr;
    char* extracted_email_verification_content = nullptr;

    bool ok = PyArg_ParseTuple(
            send_email_object.sent_emails[0].pArgs,
            "ssss",
            &extracted_email_prefix,
            &extracted_email_address,
            &extracted_email_verification_subject,
            &extracted_email_verification_content
    );

    EXPECT_TRUE(ok);

    ASSERT_TRUE(extracted_email_prefix);
    ASSERT_TRUE(extracted_email_address);
    ASSERT_TRUE(extracted_email_verification_subject);
    ASSERT_TRUE(extracted_email_verification_content);

    EXPECT_EQ(email_prefix, extracted_email_prefix);
    EXPECT_EQ(email_address, extracted_email_address);
    EXPECT_EQ(email_verification_subject, extracted_email_verification_subject);
    EXPECT_EQ(email_verification_content, extracted_email_verification_content);
}

TEST_F(AttemptToGenerateUniqueCodeTwiceTesting, success) {

    auto generate_update_document = [&](const std::string& passed_code) -> bsoncxx::document::value {
        num_times_lambda_called++;
        return buildUpdateDocument(passed_code);
    };

    bool return_val = attempt_to_generate_unique_code_twice(
            randomly_generated_code,
            account_recovery_col,
            find_document,
            generate_update_document
    );

    EXPECT_TRUE(return_val);
    EXPECT_EQ(num_times_lambda_called, 1);

    extractAndCompareAccountRecovery();
}

TEST_F(AttemptToGenerateUniqueCodeTwiceTesting, failedFirstAttempt) {

    const std::string original_verification_code =  generateAndStoreWithSamePhoneNumber();

    auto generate_update_document = [&](const std::string& passed_code) -> bsoncxx::document::value {
        num_times_lambda_called++;
        std::string code_to_store = passed_code;
        if(num_times_lambda_called == 1) { code_to_store = original_verification_code; }
        return buildUpdateDocument(code_to_store);
    };

    bool return_val = attempt_to_generate_unique_code_twice(
            randomly_generated_code,
            account_recovery_col,
            find_document,
            generate_update_document
    );

    EXPECT_TRUE(return_val);
    EXPECT_EQ(num_times_lambda_called, 2);

    extractAndCompareAccountRecovery();
}

TEST_F(AttemptToGenerateUniqueCodeTwiceTesting, failedBothAttempts) {

    const std::string original_verification_code =  generateAndStoreWithSamePhoneNumber();

    auto generate_update_document = [&](const std::string& /*passed_code*/) -> bsoncxx::document::value {
        num_times_lambda_called++;
        return buildUpdateDocument(original_verification_code);
    };

    bool return_val = attempt_to_generate_unique_code_twice(
            randomly_generated_code,
            account_recovery_col,
            find_document,
            generate_update_document
    );

    EXPECT_FALSE(return_val);
    EXPECT_EQ(num_times_lambda_called, 2);

    AccountRecoveryDoc extracted_account_recovery_doc(phone_number);
    EXPECT_EQ(extracted_account_recovery_doc.current_object_oid.to_string(), "000000000000000000000000");
}