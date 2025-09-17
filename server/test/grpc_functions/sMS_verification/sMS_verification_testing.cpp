//
// Created by jeremiah on 10/10/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "connection_pool_global_variable.h"
#include "SMSVerification.pb.h"
#include "sMS_verification.h"
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SmsVerificationTesting : public testing::Test {
protected:

    sms_verification::SMSVerificationRequest request;
    sms_verification::SMSVerificationResponse response;

    PendingAccountDoc pending_account;

    InfoStoredAfterDeletionDoc info_stored_after_deletion;

    //does NOT store the account using setIntoCollection()
    void setupValidPendingAccount() {
        std::stringstream random_veri_code_ss;
        random_veri_code_ss
            << std::setfill('0') << std::setw(6) << (rand() % 1000000);

        pending_account.current_object_oid = bsoncxx::oid{};
        pending_account.type = AccountLoginType::PHONE_ACCOUNT;
        pending_account.phone_number = "+12223334444";
        pending_account.indexing = pending_account.phone_number;
        pending_account.id = generateUUID();
        pending_account.verification_code = random_veri_code_ss.str();
        pending_account.time_verification_code_was_sent = bsoncxx::types::b_date{getCurrentTimestamp()};
    }

    //relies on pending account phone number
    //stores the account using setIntoCollection()
    void setupAndStoreValidInfoStoredAfterDeletionAccount() {
        info_stored_after_deletion.phone_number = pending_account.phone_number;
        info_stored_after_deletion.time_sms_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
        info_stored_after_deletion.time_email_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
        info_stored_after_deletion.cool_down_on_sms = 0;
        info_stored_after_deletion.number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES;
        info_stored_after_deletion.swipes_last_updated_time = getCurrentTimestamp().count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count() + 1;
        info_stored_after_deletion.setIntoCollection();
    }

    void setupValidRequest() {
        //Creates a request to generate a new account when one does not exist.
        request.set_account_type(pending_account.type);
        request.set_phone_number_or_account_id(pending_account.indexing);
        request.set_verification_code(pending_account.verification_code);
        request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
        request.set_installation_id(pending_account.id);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        setupValidPendingAccount();
        setupAndStoreValidInfoStoredAfterDeletionAccount();
        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        sMSVerification(&request, &response);
    }

    template <bool pending_account_exists = true>
    void checkSmsVerificationFailed(
            const sms_verification::SMSVerificationResponse_Status return_status
            ) {
        EXPECT_EQ(response.return_status(), return_status);

        UserAccountDoc extracted_user_account_doc;
        EXPECT_EQ(extracted_user_account_doc.current_object_oid.to_string(), "000000000000000000000000");

        PendingAccountDoc extracted_pending_account_doc(pending_account.current_object_oid);

        if(pending_account_exists) {
            EXPECT_EQ(pending_account, extracted_pending_account_doc);
        } else {
            EXPECT_EQ(extracted_pending_account_doc.current_object_oid.to_string(), "000000000000000000000000");
        }

        UserAccountStatisticsDoc extracted_user_account_statistics;
        EXPECT_EQ(extracted_user_account_statistics.current_object_oid.to_string(), "000000000000000000000000");
    }

    void checkPendingAccountRemoved() {
        PendingAccountDoc extracted_pending_account_doc(pending_account.current_object_oid);
        EXPECT_EQ(extracted_pending_account_doc.current_object_oid.to_string(), "000000000000000000000000");
    }

    template <bool set_account_id>
    void checkUserAccountCreated() {

        EXPECT_EQ(response.return_status(), sms_verification::SMSVerificationResponse_Status_SUCCESS);

        UserAccountDoc extracted_user_account_doc;
        extracted_user_account_doc.getFromCollection(pending_account.phone_number);

        EXPECT_GT(extracted_user_account_doc.time_created, 0);

        UserAccountDoc generated_user_account_doc;
        generated_user_account_doc.generateNewUserAccount(
                extracted_user_account_doc.current_object_oid,
                pending_account.phone_number,
                std::vector<std::string>{pending_account.id},
                set_account_id ? std::vector<std::string>{pending_account.indexing} : std::vector<std::string>{},
                extracted_user_account_doc.time_created.value,
                info_stored_after_deletion.time_sms_can_be_sent_again.value,
                info_stored_after_deletion.number_swipes_remaining,
                info_stored_after_deletion.swipes_last_updated_time,
                info_stored_after_deletion.time_email_can_be_sent_again.value
        );

        EXPECT_EQ(generated_user_account_doc, extracted_user_account_doc);

        checkPendingAccountRemoved();

        UserAccountStatisticsDoc extracted_user_account_statistics;
        extracted_user_account_statistics.getFromCollection(extracted_user_account_doc.current_object_oid);

        EXPECT_EQ(extracted_user_account_statistics.current_object_oid.to_string(), extracted_user_account_doc.current_object_oid.to_string());
    }

    void addNewAccountId(
            UserAccountDoc& user_account_doc,
            const std::string& account_id,
            const AccountLoginType login_type
            ) {

        std::string prefix;

        if(login_type == AccountLoginType::GOOGLE_ACCOUNT) {
            prefix = general_values::GOOGLE_ACCOUNT_ID_PREFIX;
        } else if(login_type == AccountLoginType::FACEBOOK_ACCOUNT) {
            prefix = general_values::FACEBOOK_ACCOUNT_ID_PREFIX;
        }

        pending_account.phone_number = user_account_doc.phone_number;
        pending_account.indexing = prefix + account_id;
        pending_account.id = user_account_doc.installation_ids.front();
        pending_account.type = login_type;
        pending_account.setIntoCollection();

        request.set_account_type(login_type);
        request.set_phone_number_or_account_id(account_id);
        request.set_installation_id(user_account_doc.installation_ids.front());

        UserAccountStatisticsDoc user_account_statistics;
        user_account_statistics.getFromCollection(user_account_doc.current_object_oid);

        runFunction();

        EXPECT_EQ(response.return_status(), sms_verification::SMSVerificationResponse_Status_SUCCESS);

        UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
        EXPECT_GT(extracted_user_account_doc.last_verified_time, user_account_doc.last_verified_time);
        user_account_doc.last_verified_time = extracted_user_account_doc.last_verified_time;
        user_account_doc.account_id_list.emplace_back(prefix + account_id);

        EXPECT_EQ(user_account_doc, extracted_user_account_doc);

        checkPendingAccountRemoved();

        UserAccountStatisticsDoc extracted_user_account_statistics;
        extracted_user_account_statistics.getFromCollection(extracted_user_account_doc.current_object_oid);

        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }

    void checkAddNewInstallationId(
            UserAccountDoc& user_account_doc
            ) {

        const std::string new_installation_id = generateUUID();

        pending_account.phone_number = user_account_doc.phone_number;
        pending_account.indexing = user_account_doc.phone_number;
        pending_account.id = new_installation_id;
        pending_account.setIntoCollection();

        request.set_phone_number_or_account_id(pending_account.indexing);
        request.set_installation_id(new_installation_id);

        request.set_installation_id_added_command(sms_verification::SMSVerificationRequest_InstallationIdAddedCommand_UPDATE_ACCOUNT);

        request.set_installation_id_birth_year(user_account_doc.birth_year);
        request.set_installation_id_birth_month(user_account_doc.birth_month);
        request.set_installation_id_birth_day_of_month(user_account_doc.birth_day_of_month);

        UserAccountStatisticsDoc user_account_statistics;
        user_account_statistics.getFromCollection(user_account_doc.current_object_oid);

        runFunction();

        EXPECT_EQ(response.return_status(), sms_verification::SMSVerificationResponse_Status_SUCCESS);

        UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
        EXPECT_GT(extracted_user_account_doc.last_verified_time, user_account_doc.last_verified_time);
        user_account_doc.last_verified_time = extracted_user_account_doc.last_verified_time;
        user_account_doc.installation_ids.insert(user_account_doc.installation_ids.begin(), new_installation_id);

        while(user_account_doc.installation_ids.size() > general_values::MAX_NUMBER_OF_INSTALLATION_IDS_STORED) {
            user_account_doc.installation_ids.pop_back();
        }

        EXPECT_EQ(user_account_doc, extracted_user_account_doc);

        checkPendingAccountRemoved();

        UserAccountStatisticsDoc extracted_user_account_statistics;
        extracted_user_account_statistics.getFromCollection(extracted_user_account_doc.current_object_oid);

        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }
};

TEST_F(SmsVerificationTesting, invalidAccountType) {
    pending_account.setIntoCollection();

    request.set_account_type(AccountLoginType(-1));

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_LG_ERROR);
}

TEST_F(SmsVerificationTesting, invalidAccountId_withFacebookAccount) {
    pending_account.setIntoCollection();

    request.set_account_type(AccountLoginType::FACEBOOK_ACCOUNT);
    request.set_phone_number_or_account_id("123");

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);
}

TEST_F(SmsVerificationTesting, invalidAccountId_withGoogleAccount) {
    pending_account.setIntoCollection();

    request.set_account_type(AccountLoginType::GOOGLE_ACCOUNT);
    request.set_phone_number_or_account_id("123");

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);
}

TEST_F(SmsVerificationTesting, invalidPhoneNumber_withPhoneAccount) {
    pending_account.setIntoCollection();

    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);
    request.set_phone_number_or_account_id("invalid_phone_num");

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);
}

TEST_F(SmsVerificationTesting, invalidVerificationCode) {
    pending_account.setIntoCollection();

    request.set_verification_code(std::string(general_values::VERIFICATION_CODE_NUMBER_OF_DIGITS + 1, '1'));

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_VERIFICATION_CODE);
}

TEST_F(SmsVerificationTesting, invalidLetsGoVersion) {
    pending_account.setIntoCollection();

    request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION - 1);

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_OUTDATED_VERSION);
}

TEST_F(SmsVerificationTesting, invalidInstallationId) {
    pending_account.setIntoCollection();

    request.set_installation_id("invalid_uuid");

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_INSTALLATION_ID);
}

TEST_F(SmsVerificationTesting, verificationCodeDoesNotMatch) {
    pending_account.setIntoCollection();

    std::cout << request.verification_code() << '\n';

    bool has_run_once = false;

    //make sure it is not the same code
    while (!has_run_once
            || request.verification_code() == pending_account.verification_code) {
        std::stringstream random_veri_code_ss;
        random_veri_code_ss
                << std::setfill('0') << std::setw(6) << (rand() % 1000000);
        request.set_verification_code(random_veri_code_ss.str());
        has_run_once = true;
    }

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_VERIFICATION_CODE);
}

TEST_F(SmsVerificationTesting, verificationCodeExpired) {
    pending_account.time_verification_code_was_sent = bsoncxx::types::b_date{getCurrentTimestamp() - general_values::TIME_UNTIL_VERIFICATION_CODE_EXPIRES};
    pending_account.setIntoCollection();

    runFunction();

    checkSmsVerificationFailed<false>(sms_verification::SMSVerificationResponse_Status_VERIFICATION_CODE_EXPIRED);
}

TEST_F(SmsVerificationTesting, createNewAccount_phoneNumber) {
    pending_account.type = AccountLoginType::PHONE_ACCOUNT;
    pending_account.indexing = pending_account.phone_number;
    pending_account.setIntoCollection();

    runFunction();

    checkUserAccountCreated<false>();
}

TEST_F(SmsVerificationTesting, createNewAccount_facebookAccount) {
    const std::string facebook_id = "facebook_id";

    pending_account.type = AccountLoginType::FACEBOOK_ACCOUNT;
    pending_account.indexing = general_values::FACEBOOK_ACCOUNT_ID_PREFIX + facebook_id;
    pending_account.setIntoCollection();

    request.set_account_type(pending_account.type);
    request.set_phone_number_or_account_id(facebook_id);

    runFunction();

    checkUserAccountCreated<true>();
}

TEST_F(SmsVerificationTesting, createNewAccount_googleAccount) {
    const std::string google_id = "google_id";

    pending_account.type = AccountLoginType::GOOGLE_ACCOUNT;
    pending_account.indexing = general_values::GOOGLE_ACCOUNT_ID_PREFIX + google_id;
    pending_account.setIntoCollection();

    request.set_account_type(pending_account.type);
    request.set_phone_number_or_account_id(google_id);

    runFunction();

    checkUserAccountCreated<true>();
}

TEST_F(SmsVerificationTesting, updateAccount_standardVerification) {
    const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);
    pending_account.phone_number = user_account_doc.phone_number;
    pending_account.indexing = user_account_doc.phone_number;
    pending_account.id = user_account_doc.installation_ids.front();
    pending_account.setIntoCollection();

    request.set_phone_number_or_account_id(pending_account.indexing);
    request.set_installation_id(user_account_doc.installation_ids.front());

    UserAccountStatisticsDoc user_account_statistics;
    user_account_statistics.getFromCollection(user_account_oid);

    runFunction();

    EXPECT_EQ(response.return_status(), sms_verification::SMSVerificationResponse_Status_SUCCESS);

    UserAccountDoc extracted_user_account_doc(user_account_oid);
    EXPECT_GT(extracted_user_account_doc.last_verified_time, user_account_doc.last_verified_time);
    user_account_doc.last_verified_time = extracted_user_account_doc.last_verified_time;

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    checkPendingAccountRemoved();

    UserAccountStatisticsDoc extracted_user_account_statistics;
    extracted_user_account_statistics.getFromCollection(extracted_user_account_doc.current_object_oid);

    EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
}

TEST_F(SmsVerificationTesting, addNewAccountId_facebookAccount) {
    const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);
    user_account_doc.account_id_list.clear();
    user_account_doc.setIntoCollection();

    addNewAccountId(
            user_account_doc,
            gen_random_alpha_numeric_string(rand() % 100 + 10),
            AccountLoginType::FACEBOOK_ACCOUNT
    );
}

TEST_F(SmsVerificationTesting, addNewAccountId_googleAccount) {
    const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);
    user_account_doc.account_id_list.clear();
    user_account_doc.setIntoCollection();

    addNewAccountId(
            user_account_doc,
            gen_random_alpha_numeric_string(rand() % 100 + 10),
            AccountLoginType::GOOGLE_ACCOUNT
    );
}

TEST_F(SmsVerificationTesting, addInstallationId_updateCurrentAccount_birthdayDoesNotMatch) {
    const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);

    const std::string new_installation_id = generateUUID();

    pending_account.phone_number = user_account_doc.phone_number;
    pending_account.indexing = user_account_doc.phone_number;
    pending_account.id = new_installation_id;
    pending_account.setIntoCollection();

    request.set_phone_number_or_account_id(pending_account.indexing);
    request.set_installation_id(new_installation_id);

    request.set_installation_id_added_command(sms_verification::SMSVerificationRequest_InstallationIdAddedCommand_UPDATE_ACCOUNT);

    request.set_installation_id_birth_year(user_account_doc.birth_year + 1);
    request.set_installation_id_birth_month(user_account_doc.birth_month);
    request.set_installation_id_birth_day_of_month(user_account_doc.birth_day_of_month);

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INCORRECT_BIRTHDAY);
}

TEST_F(SmsVerificationTesting, addInstallationId_updateCurrentAccount_birthdayMatches) {
    const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);

    const std::string new_installation_id = generateUUID();

    checkAddNewInstallationId(user_account_doc);
}

TEST_F(SmsVerificationTesting, addInstallationId_updateCurrentAccount_installationIdListTooLong) {
    const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc user_account_doc(user_account_oid);

    user_account_doc.installation_ids.clear();
    for(int i = 0; i < general_values::MAX_NUMBER_OF_INSTALLATION_IDS_STORED; ++i) {
        user_account_doc.installation_ids.emplace_back(generateUUID());
    }
    user_account_doc.setIntoCollection();

    checkAddNewInstallationId(user_account_doc);
}

TEST_F(SmsVerificationTesting, limitedAttemptsAtGuessing) {
    pending_account.setIntoCollection();

    std::set<std::string> codes{request.verification_code()};

    for(int i = 0; i < general_values::MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS-1; ++i) {

        //Make sure the same code is not used multiple times.
        while (codes.contains(request.verification_code())) {
            std::stringstream random_veri_code_ss;
            random_veri_code_ss
                    << std::setfill('0') << std::setw(6) << (rand() % 1000000);
            request.set_verification_code(random_veri_code_ss.str());
        }
        codes.insert(request.verification_code());

        std::cout << request.verification_code() << '\n';

        runFunction();

        checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_INVALID_VERIFICATION_CODE);
    }

    //Make sure the same code is not used again.
    while (codes.contains(request.verification_code())) {
        std::stringstream random_veri_code_ss;
        random_veri_code_ss
                << std::setfill('0') << std::setw(6) << (rand() % 1000000);
        request.set_verification_code(random_veri_code_ss.str());
    }
    codes.insert(request.verification_code());

    runFunction();

    checkSmsVerificationFailed(sms_verification::SMSVerificationResponse_Status_VERIFICATION_ON_COOLDOWN);

    InfoStoredAfterDeletionDoc info_stored_after_deletion_doc(request.phone_number_or_account_id());

    //Set the time to no longer locked out of verification attempts.
    info_stored_after_deletion_doc.failed_sms_verification_last_update_time--;
    info_stored_after_deletion_doc.setIntoCollection();

    request.set_verification_code(pending_account.verification_code);

    //Make sure that after the time has expired, it works again.
    runFunction();

    checkUserAccountCreated<false>();
}