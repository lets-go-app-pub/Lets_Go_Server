//
// Created by jeremiah on 9/21/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "generate_multiple_random_accounts.h"
#include "report_values.h"
#include "LoginFunction.pb.h"
#include "login_function.h"
#include "icons_info_keys.h"
#include "activities_info_keys.h"
#include "save_activities_and_categories.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class LoginFunctionTesting : public ::testing::Test {
protected:

    loginfunction::LoginRequest request;
    loginfunction::LoginResponse response;

    bsoncxx::oid user_account_oid;

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const std::chrono::milliseconds five_seconds{5000L}; //used when setting a cooldown in the future

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    //Used with tests requiring the account to not be created yet.
    const std::string new_account_phone_number = "+17021234567";

    InfoStoredAfterDeletionDoc original_info_stored_after_delete;
    UserAccountStatisticsDoc original_user_account_statistics;

    void setupValidRequest() {
        request.set_phone_number(user_account_doc.phone_number);
        request.set_account_id("");
        request.set_installation_id(user_account_doc.installation_ids.front());
        request.set_lets_go_version(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION);
        request.set_account_type(AccountLoginType::PHONE_ACCOUNT);
        request.set_device_name(gen_random_alpha_numeric_string(rand() % 100 + 10));
        request.set_api_number(31);
        request.set_birthday_timestamp(user_account_doc.birthday_timestamp.value.count());
        request.set_email_timestamp(user_account_doc.email_timestamp.value.count());
        request.set_gender_timestamp(user_account_doc.gender_timestamp.value.count());
        request.set_name_timestamp(user_account_doc.first_name_timestamp.value.count());
        request.set_categories_timestamp(user_account_doc.categories_timestamp.value.count());
        request.set_post_login_info_timestamp(user_account_doc.post_login_info_timestamp.value.count());

        mongocxx::collection icons_info = accounts_db[collection_names::ICONS_INFO_COLLECTION_NAME];

        mongocxx::options::find opts;

        opts.projection(
                document{}
                    << icons_info_keys::INDEX << 1
                    << icons_info_keys::TIMESTAMP_LAST_UPDATED << 1
                << finalize
                );

        opts.sort(
                document{}
                        << icons_info_keys::INDEX << 1
                << finalize
        );

        auto icons_cursor = icons_info.find(document{} << finalize, opts);

        //Store all icon timestamps inside the request.
        for(const auto& doc : icons_cursor) {
            request.add_icon_timestamps(doc[icons_info_keys::TIMESTAMP_LAST_UPDATED].get_date().value.count());
        }
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        original_info_stored_after_delete.getFromCollection(user_account_doc.phone_number);
        original_user_account_statistics.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    std::string runFunction() {
        //sleep to guarantee unique timestamp
        std::this_thread::sleep_for(std::chrono::milliseconds{4});

        return loginFunctionMongoDb(&request, &response, "addr:port");
    }

    template <bool set_time_sms_can_be_sent_again = false, bool set_logged_in_token_expiration = false>
    void compareUserAccountDocument() {
        UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);

        if(set_time_sms_can_be_sent_again) {
            user_account_doc.time_sms_can_be_sent_again = extracted_user_account_doc.time_sms_can_be_sent_again;
        }

        if(set_logged_in_token_expiration) {
            user_account_doc.logged_in_token_expiration = extracted_user_account_doc.logged_in_token_expiration;
        }

        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }

    PendingAccountDoc setupPendingAccountForNewPhoneNumber() {
        PendingAccountDoc generated_pending_account;
        generated_pending_account.type = AccountLoginType::FACEBOOK_ACCOUNT;
        generated_pending_account.phone_number = new_account_phone_number;
        generated_pending_account.indexing = gen_random_alpha_numeric_string(rand() % 100 + 10);
        generated_pending_account.id = request.installation_id();
        generated_pending_account.verification_code = "123%56"; //using an impossible code, this way it can never be randomly generated
        generated_pending_account.time_verification_code_was_sent = bsoncxx::types::b_date{current_timestamp};

        generated_pending_account.setIntoCollection();

        return generated_pending_account;
    }

    InfoStoredAfterDeletionDoc setupInfoStoredAfterDeletionAccountForNewPhoneNumber() {
        InfoStoredAfterDeletionDoc generated_info_stored_after_delete;
        generated_info_stored_after_delete.phone_number = new_account_phone_number;
        generated_info_stored_after_delete.time_email_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
        generated_info_stored_after_delete.cool_down_on_sms = -1;
        generated_info_stored_after_delete.number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES/2;
        generated_info_stored_after_delete.time_sms_can_be_sent_again = bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{100}};
        generated_info_stored_after_delete.swipes_last_updated_time = current_timestamp.count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count();

        generated_info_stored_after_delete.setIntoCollection();

        return generated_info_stored_after_delete;
    }

    static std::string generateRandomPhoneNumber() {
        std::string phone_number;

        while(phone_number.size() != 12 || (
                phone_number[2] == '5'
                && phone_number[3] == '5'
                && phone_number[4] == '5'
                )
        ) {
            phone_number = "+1";
            for (int j = 0; j < 10; j++) {
                //do not allow area code to start with 0 or 1
                phone_number += std::to_string(j == 0 ? rand() % 8 + 2 : rand() % 10);
            }
        }

        return phone_number;
    }

    void checkInfoStoredAfterDeleteInserted() {
        InfoStoredAfterDeletionDoc generated_info_stored_after_delete;

        generated_info_stored_after_delete.phone_number = new_account_phone_number;
        generated_info_stored_after_delete.time_email_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
        generated_info_stored_after_delete.cool_down_on_sms = -1;
        generated_info_stored_after_delete.number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES;

        InfoStoredAfterDeletionDoc extracted_info_stored_after_delete(new_account_phone_number);
        generated_info_stored_after_delete.current_object_oid = extracted_info_stored_after_delete.current_object_oid;
        generated_info_stored_after_delete.time_sms_can_be_sent_again = extracted_info_stored_after_delete.time_sms_can_be_sent_again;
        generated_info_stored_after_delete.swipes_last_updated_time = extracted_info_stored_after_delete.swipes_last_updated_time;

        generated_info_stored_after_delete.number_failed_sms_verification_attempts = 0;
        generated_info_stored_after_delete.failed_sms_verification_last_update_time = current_timestamp.count() / general_values::TIME_BETWEEN_VERIFICATION_ATTEMPTS.count();

        EXPECT_EQ(extracted_info_stored_after_delete, generated_info_stored_after_delete);
    }

    void checkInfoStoredAfterDeleteUpdated(InfoStoredAfterDeletionDoc& generated_info_stored_after_delete) {
        InfoStoredAfterDeletionDoc extracted_info_stored_after_delete(new_account_phone_number);
        generated_info_stored_after_delete.time_sms_can_be_sent_again = extracted_info_stored_after_delete.time_sms_can_be_sent_again;
        generated_info_stored_after_delete.cool_down_on_sms = (int)((five_seconds.count() - 1L) / 1000L);
        EXPECT_EQ(extracted_info_stored_after_delete, generated_info_stored_after_delete);
    }

    PendingAccountDoc generateMatchingPhoneNumberPendingAccount() {
        PendingAccountDoc matching_phone_number_pending_account;
        matching_phone_number_pending_account.type = AccountLoginType::GOOGLE_ACCOUNT;
        matching_phone_number_pending_account.phone_number = new_account_phone_number;
        matching_phone_number_pending_account.indexing = gen_random_alpha_numeric_string(rand() % 50 + 10);
        matching_phone_number_pending_account.id = generateUUID();
        matching_phone_number_pending_account.verification_code = "1#2356"; //using an impossible code, this way it can never be randomly generated
        matching_phone_number_pending_account.time_verification_code_was_sent = bsoncxx::types::b_date{current_timestamp};

        matching_phone_number_pending_account.setIntoCollection();

        return matching_phone_number_pending_account;
    }

    PendingAccountDoc generateMatchingIndexingPendingAccount() {
        PendingAccountDoc matching_indexing_pending_account;
        matching_indexing_pending_account.type = AccountLoginType::PHONE_ACCOUNT;
        matching_indexing_pending_account.phone_number = generateRandomPhoneNumber();
        matching_indexing_pending_account.indexing = new_account_phone_number;
        matching_indexing_pending_account.id = generateUUID();
        matching_indexing_pending_account.verification_code = "1#2356"; //using an impossible code, this way it can never be randomly generated
        matching_indexing_pending_account.time_verification_code_was_sent = bsoncxx::types::b_date{current_timestamp};

        matching_indexing_pending_account.setIntoCollection();

        return matching_indexing_pending_account;
    }

    PendingAccountDoc generateMatchingInstallationIdPendingAccount() {
        PendingAccountDoc matching_id_pending_account;
        matching_id_pending_account.type = AccountLoginType::FACEBOOK_ACCOUNT;
        matching_id_pending_account.phone_number = generateRandomPhoneNumber();
        matching_id_pending_account.indexing = gen_random_alpha_numeric_string(rand() % 50 + 10);
        matching_id_pending_account.id = request.installation_id();
        matching_id_pending_account.verification_code = "123%56"; //using an impossible code, this way it can never be randomly generated
        matching_id_pending_account.time_verification_code_was_sent = bsoncxx::types::b_date{current_timestamp};

        matching_id_pending_account.setIntoCollection();

        return matching_id_pending_account;
    }

    template <bool set_install_id, bool set_verification_code>
    void setBasicPendingAccountValues(
            PendingAccountDoc& generated_pending_account,
            const std::string& verification_code = ""
            ) {
        generated_pending_account.type = AccountLoginType::PHONE_ACCOUNT;
        generated_pending_account.phone_number = new_account_phone_number;
        generated_pending_account.indexing = new_account_phone_number;
        if(set_install_id) {
            generated_pending_account.id = request.installation_id();
        }

        if(set_verification_code) {
            generated_pending_account.verification_code = verification_code;
        }
    }

    void compareGeneratedResponse(const loginfunction::LoginResponse& generated_response) {
        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                generated_response,
                response
        );

        if(!equivalent) {
            std::cout << "generated_response\n" << generated_response.DebugString() << '\n';
            std::cout << "response\n" << response.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    template<bool birthday_not_needed = true>
    void authenticationTestSmsOnCooldown() {
        loginfunction::LoginResponse generated_response;
        generated_response.set_return_status(LoginValuesToReturnToClient::SMS_ON_COOL_DOWN);
        generated_response.set_access_status(AccessStatus::ACCESS_GRANTED);

        generated_response.set_login_token("~");
        generated_response.set_sms_cool_down(29); //when account was created it was set
        generated_response.set_birthday_not_needed(birthday_not_needed);
        generated_response.set_server_timestamp(response.server_timestamp());

        compareGeneratedResponse(generated_response);

        //make sure user account did not change
        compareUserAccountDocument();
    }

    template<bool birthday_not_needed = true>
    void authenticationTestSmsNotOnCooldown() {
        loginfunction::LoginResponse generated_response;
        generated_response.set_return_status(LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);
        generated_response.set_access_status(AccessStatus::ACCESS_GRANTED);

        generated_response.set_login_token("~");
        generated_response.set_sms_cool_down(-1);
        generated_response.set_birthday_not_needed(birthday_not_needed);
        generated_response.set_server_timestamp(response.server_timestamp());

        compareGeneratedResponse(generated_response);

        //make sure user account did not change
        compareUserAccountDocument<true>();
    }

    loginfunction::LoginResponse buildBasicSuccessfulLoginResponse() {
        loginfunction::LoginResponse generated_response;
        generated_response.set_sms_cool_down(-1); //when account was created it was set
        generated_response.set_birthday_not_needed(true);

        generated_response.set_phone_number(user_account_doc.phone_number);
        generated_response.set_account_oid(user_account_oid.to_string());
        generated_response.set_algorithm_search_options(user_account_doc.search_by_options);

        //set global variables, copying these here don't see a benefit to explicitly copying them
        generated_response.mutable_login_values_to_return_to_client()->mutable_global_constant_values()->CopyFrom(response.login_values_to_return_to_client().global_constant_values());

        generated_response.set_access_status(AccessStatus::ACCESS_GRANTED);
        generated_response.set_login_token(user_account_doc.logged_in_token);
        generated_response.set_return_status(LoginValuesToReturnToClient::LOGGED_IN);

        saveActivitiesAndCategories(
                accounts_db,
                generated_response.mutable_login_values_to_return_to_client()->mutable_server_categories(),
                generated_response.mutable_login_values_to_return_to_client()->mutable_server_activities()
        );

        generated_response.mutable_pre_login_timestamps()->set_birthday_timestamp(user_account_doc.birthday_timestamp.value.count());
        generated_response.mutable_pre_login_timestamps()->set_email_timestamp(user_account_doc.email_timestamp.value.count());
        generated_response.mutable_pre_login_timestamps()->set_gender_timestamp(user_account_doc.gender_timestamp.value.count());
        generated_response.mutable_pre_login_timestamps()->set_name_timestamp(user_account_doc.first_name_timestamp.value.count());
        generated_response.mutable_pre_login_timestamps()->set_categories_timestamp(user_account_doc.categories_timestamp.value.count());

        generated_response.set_post_login_timestamp(user_account_doc.post_login_info_timestamp.value.count());

        for(const auto& pic : user_account_doc.pictures) {
            if(pic.pictureStored()) {
                bsoncxx::oid pic_reference;
                bsoncxx::types::b_date pic_timestamp{std::chrono::milliseconds{-1}};

                pic.getPictureReference(
                        pic_reference,
                        pic_timestamp
                );

                generated_response.add_pictures_timestamps(pic_timestamp.value.count());
            } else {
                generated_response.add_pictures_timestamps(-1);
            }
        }

        for(const auto& other_user_blocked : user_account_doc.other_users_blocked) {
            generated_response.add_blocked_accounts(other_user_blocked.oid_string);
        }
        generated_response.set_server_timestamp(response.server_timestamp());

        generated_response.set_subscription_status(user_account_doc.subscription_status);
        generated_response.set_subscription_expiration_time(user_account_doc.subscription_expiration_time);

        generated_response.set_opted_in_to_promotional_email(user_account_doc.opted_in_to_promotional_email);

        return generated_response;
    }

    void compareOtherDocsOnSuccess() {
        InfoStoredAfterDeletionDoc extracted_info_stored_after_delete(user_account_doc.phone_number);
        EXPECT_EQ(original_info_stored_after_delete, extracted_info_stored_after_delete);

        PendingAccountDoc pending_account_doc(user_account_doc.phone_number);
        EXPECT_EQ(pending_account_doc.current_object_oid.to_string(), "000000000000000000000000");

        UserAccountStatisticsDoc extracted_user_account_statistics(user_account_oid);

        ASSERT_FALSE(extracted_user_account_statistics.login_times.empty());

        original_user_account_statistics.login_times.emplace_back(
                request.installation_id(),
                request.device_name(),
                request.api_number(),
                request.lets_go_version(),
                extracted_user_account_statistics.login_times.back().timestamp
        );

        EXPECT_EQ(original_user_account_statistics, extracted_user_account_statistics);
    }

    void runSuccessfulFunctionTest(const std::function<void(loginfunction::LoginResponse& generated_response)>& lambda) {
        runFunction();

        //This must be done first to set the extra value inside user_account_doc.
        compareUserAccountDocument<false, true>();

        loginfunction::LoginResponse generated_response = buildBasicSuccessfulLoginResponse();

        lambda(generated_response);

        compareGeneratedResponse(generated_response);

        compareOtherDocsOnSuccess();
    }

    size_t countTotalActivitiesInDatabase() {
        mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

        bsoncxx::stdx::optional<bsoncxx::document::value> find_activities = activities_info_collection.find_one(
                document{}
                        << "_id" << activities_info_keys::ID
                        << finalize
        );

        if(find_activities) {
            const bsoncxx::array::view activities_array = find_activities->view()[activities_info_keys::ACTIVITIES].get_array().value;

            return std::distance(activities_array.begin(), activities_array.end());
        }

        return 0;
    }

    void extractAgeOfActivityOver13(
            int& index,
            int& min_age
            ) {
        mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

        bsoncxx::stdx::optional<bsoncxx::document::value> find_activities = activities_info_collection.find_one(
                document{}
                        << "_id" << activities_info_keys::ID
                << finalize
        );

        if(find_activities) {
            const bsoncxx::array::view activities_array = find_activities->view()[activities_info_keys::ACTIVITIES].get_array().value;

            int current_index = 0;
            for(const auto& ele : activities_array) {
                const bsoncxx::document::view activity_doc = ele.get_document().value;

                const int extracted_min_age = activity_doc[activities_info_keys::activities::MIN_AGE].get_int32().value;

                if(extracted_min_age > 13) {
                    index = current_index;
                    min_age = extracted_min_age;
                    return;
                }
                current_index++;
            }
        }

        index = -1;
        min_age = -1;
    }
};

TEST_F(LoginFunctionTesting, invalidVersion) {
    request.set_lets_go_version(0);

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::OUTDATED_VERSION);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, invalidAccountType) {
    request.set_account_type(AccountLoginType(-1));

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::INVALID_ACCOUNT_TYPE);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, accountType_valueNotSet) {
    request.set_account_type(AccountLoginType::LOGIN_TYPE_VALUE_NOT_SET);

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::INVALID_ACCOUNT_TYPE);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, invalidInstallationId) {
    request.set_installation_id(gen_random_alpha_numeric_string(rand() % 10 + 2));

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::INVALID_INSTALLATION_ID);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, invalidPhoneNumber_PHONE_ACCOUNT) {
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);
    request.set_phone_number("+1234321");

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, invalidAccountId_FACEBOOK_ACCOUNT) {
    request.set_account_type(AccountLoginType::FACEBOOK_ACCOUNT);
    request.set_account_id("1");

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, invalidAccountId_GOOGLE_ACCOUNT) {
    request.set_account_type(AccountLoginType::GOOGLE_ACCOUNT);
    request.set_account_id(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + rand() % 100 + 1));

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::INVALID_PHONE_NUMBER_OR_ACCOUNT_ID);

    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, accountDoesNotExist_googleAccount_invalidPhoneNumber) {
    request.set_phone_number("");
    request.set_account_type(AccountLoginType::GOOGLE_ACCOUNT);
    request.set_account_id(gen_random_alpha_numeric_string(100));

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_PHONE_NUMBER_TO_CREATE_ACCOUNT);
}

TEST_F(LoginFunctionTesting, accountDoesNotExist_facebookAccount_invalidPhoneNumber) {
    request.set_phone_number("");
    request.set_account_type(AccountLoginType::FACEBOOK_ACCOUNT);
    request.set_account_id(gen_random_alpha_numeric_string(100));

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_PHONE_NUMBER_TO_CREATE_ACCOUNT);
}

TEST_F(LoginFunctionTesting, accountDoesNotExist_infoStoredAfterDeleteDoesNotExist_pendingAccountDoesNotExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc generated_pending_account;
    setBasicPendingAccountValues<true, true>(generated_pending_account, verification_code);

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    generated_pending_account.current_object_oid = extracted_pending_account.current_object_oid;
    generated_pending_account.time_verification_code_was_sent = extracted_pending_account.time_verification_code_was_sent;

    EXPECT_EQ(generated_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteInserted();
}

TEST_F(LoginFunctionTesting, accountDoesNotExist_infoStoredAfterDeleteExists_pendingAccountExists_smsNotOnCooldown) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    PendingAccountDoc generated_pending_account = setupPendingAccountForNewPhoneNumber();

    InfoStoredAfterDeletionDoc generated_info_stored_after_delete = setupInfoStoredAfterDeletionAccountForNewPhoneNumber();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    generated_pending_account.time_verification_code_was_sent = extracted_pending_account.time_verification_code_was_sent;
    setBasicPendingAccountValues<true, true>(generated_pending_account, verification_code);

    EXPECT_EQ(generated_pending_account, extracted_pending_account);

    InfoStoredAfterDeletionDoc extracted_info_stored_after_delete(new_account_phone_number);
    generated_info_stored_after_delete.time_sms_can_be_sent_again = extracted_info_stored_after_delete.time_sms_can_be_sent_again;

    EXPECT_EQ(extracted_info_stored_after_delete, generated_info_stored_after_delete);
}

TEST_F(LoginFunctionTesting, accountDoesNotExist_infoStoredAfterDeleteExists_pendingAccountExists_smsOnCooldown) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    PendingAccountDoc generated_pending_account = setupPendingAccountForNewPhoneNumber();
    InfoStoredAfterDeletionDoc generated_info_stored_after_delete = setupInfoStoredAfterDeletionAccountForNewPhoneNumber();

    //set sms on cooldown
    generated_info_stored_after_delete.time_sms_can_be_sent_again = bsoncxx::types::b_date{current_timestamp + five_seconds};
    generated_info_stored_after_delete.setIntoCollection();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::SMS_ON_COOL_DOWN);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    generated_pending_account.type = AccountLoginType::PHONE_ACCOUNT;
    generated_pending_account.indexing = new_account_phone_number;

    EXPECT_EQ(generated_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteUpdated(generated_info_stored_after_delete);
}

TEST_F(LoginFunctionTesting, updatePendingAccount_smsOnCooldown_installationId_and_indexing_alreadyExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    PendingAccountDoc matching_id_pending_account = generateMatchingInstallationIdPendingAccount();

    //phone_number, indexing and id must be unique from matching_id_pending_account
    generateMatchingIndexingPendingAccount();

    InfoStoredAfterDeletionDoc generated_info_stored_after_delete = setupInfoStoredAfterDeletionAccountForNewPhoneNumber();

    //set sms on cooldown
    generated_info_stored_after_delete.time_sms_can_be_sent_again = bsoncxx::types::b_date{current_timestamp + five_seconds};
    generated_info_stored_after_delete.setIntoCollection();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::SMS_ON_COOL_DOWN);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    setBasicPendingAccountValues<false, false>(matching_id_pending_account);

    EXPECT_EQ(matching_id_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteUpdated(generated_info_stored_after_delete);
}

TEST_F(LoginFunctionTesting, updatePendingAccount_smsOnCooldown_installationId_and_phoneNumber_alreadyExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    PendingAccountDoc matching_id_pending_account = generateMatchingInstallationIdPendingAccount();

    //phone_number, indexing and id must be unique from matching_id_pending_account
    generateMatchingPhoneNumberPendingAccount();

    InfoStoredAfterDeletionDoc generated_info_stored_after_delete = setupInfoStoredAfterDeletionAccountForNewPhoneNumber();

    //set sms on cooldown
    generated_info_stored_after_delete.time_sms_can_be_sent_again = bsoncxx::types::b_date{current_timestamp + five_seconds};
    generated_info_stored_after_delete.setIntoCollection();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::SMS_ON_COOL_DOWN);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    setBasicPendingAccountValues<false, false>(matching_id_pending_account);

    EXPECT_EQ(matching_id_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteUpdated(generated_info_stored_after_delete);
}

TEST_F(LoginFunctionTesting, updatePendingAccount_smsOnCooldown_phoneNumber_and_indexing_alreadyExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    generateMatchingPhoneNumberPendingAccount();

    //phone_number, indexing and id must be unique from matching_id_pending_account
    generateMatchingIndexingPendingAccount();

    InfoStoredAfterDeletionDoc generated_info_stored_after_delete = setupInfoStoredAfterDeletionAccountForNewPhoneNumber();

    //set sms on cooldown
    generated_info_stored_after_delete.time_sms_can_be_sent_again = bsoncxx::types::b_date{current_timestamp + five_seconds};
    generated_info_stored_after_delete.setIntoCollection();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::SMS_ON_COOL_DOWN);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    //account is not upserted in this case, old accounts are removed
    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    EXPECT_EQ(extracted_pending_account.current_object_oid.to_string(), "000000000000000000000000");

    checkInfoStoredAfterDeleteUpdated(generated_info_stored_after_delete);
}

TEST_F(LoginFunctionTesting, updatePendingAccount_smsNotOnCooldown_installationId_and_indexing_alreadyExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    PendingAccountDoc matching_id_pending_account = generateMatchingInstallationIdPendingAccount();

    //phone_number, indexing and id must be unique from matching_id_pending_account
    generateMatchingIndexingPendingAccount();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    matching_id_pending_account.time_verification_code_was_sent = extracted_pending_account.time_verification_code_was_sent;
    setBasicPendingAccountValues<false, true>(
            matching_id_pending_account,
            verification_code
    );

    EXPECT_EQ(matching_id_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteInserted();
}

TEST_F(LoginFunctionTesting, updatePendingAccount_smsNotOnCooldown_installationId_and_phoneNumber_alreadyExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    PendingAccountDoc matching_id_pending_account = generateMatchingInstallationIdPendingAccount();

    //phone_number, indexing and id must be unique from matching_id_pending_account
    generateMatchingPhoneNumberPendingAccount();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);
    matching_id_pending_account.time_verification_code_was_sent = extracted_pending_account.time_verification_code_was_sent;
    setBasicPendingAccountValues<false, true>(
            matching_id_pending_account,
            verification_code
    );
    EXPECT_EQ(matching_id_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteInserted();
}

TEST_F(LoginFunctionTesting, updatePendingAccount_smsNotOnCooldown_phoneNumber_and_indexing_alreadyExist) {
    request.set_phone_number(new_account_phone_number);
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);

    generateMatchingPhoneNumberPendingAccount();

    //phone_number, indexing and id must be unique from matching_id_pending_account
    generateMatchingIndexingPendingAccount();

    const std::string verification_code = runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

    //original user_account_doc should not have changed
    compareUserAccountDocument();

    PendingAccountDoc extracted_pending_account(new_account_phone_number);

    PendingAccountDoc generated_pending_account;
    generated_pending_account.current_object_oid = extracted_pending_account.current_object_oid;
    generated_pending_account.time_verification_code_was_sent = extracted_pending_account.time_verification_code_was_sent;
    setBasicPendingAccountValues<true, true>(
            generated_pending_account,
            verification_code
    );

    EXPECT_EQ(generated_pending_account, extracted_pending_account);

    checkInfoStoredAfterDeleteInserted();
}

TEST_F(LoginFunctionTesting, accountSuspended) {
    user_account_doc.status = UserAccountStatus::STATUS_SUSPENDED;
    user_account_doc.inactive_end_time = bsoncxx::types::b_date{current_timestamp + five_seconds};
    user_account_doc.inactive_message = gen_random_alpha_numeric_string(rand() % 50 + 10);
    user_account_doc.setIntoCollection();

    runFunction();

    loginfunction::LoginResponse generated_response;
    generated_response.set_return_status(LoginValuesToReturnToClient::ACCOUNT_CLOSED);
    generated_response.set_time_out_message(user_account_doc.inactive_message);
    generated_response.set_access_status(AccessStatus::SUSPENDED);

    generated_response.set_login_token("~");
    generated_response.set_sms_cool_down(-1);
    generated_response.set_birthday_not_needed(true);
    generated_response.set_server_timestamp(response.server_timestamp());

    generated_response.set_time_out_duration_remaining(response.time_out_duration_remaining());
    EXPECT_LT(response.time_out_duration_remaining(), five_seconds.count());
    EXPECT_GT(response.time_out_duration_remaining(), 0);

    compareGeneratedResponse(generated_response);

    //make sure user account did not change
    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, accountBanned) {
    user_account_doc.status = UserAccountStatus::STATUS_BANNED;
    user_account_doc.inactive_end_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    user_account_doc.inactive_message = gen_random_alpha_numeric_string(rand() % 50 + 10);
    user_account_doc.setIntoCollection();

    runFunction();

    loginfunction::LoginResponse generated_response;
    generated_response.set_return_status(LoginValuesToReturnToClient::ACCOUNT_CLOSED);
    generated_response.set_time_out_message(user_account_doc.inactive_message);
    generated_response.set_access_status(AccessStatus::BANNED);

    generated_response.set_login_token("~");
    generated_response.set_sms_cool_down(-1);
    generated_response.set_birthday_not_needed(true);
    generated_response.set_server_timestamp(response.server_timestamp());

    compareGeneratedResponse(generated_response);

    //make sure user account did not change
    compareUserAccountDocument();
}

TEST_F(LoginFunctionTesting, new_installationId_smsNotOnCooldown) {
    const std::string new_installation_id = generateUUID();
    request.set_installation_id(new_installation_id);

    user_account_doc.time_sms_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    user_account_doc.setIntoCollection();

    runFunction();

    authenticationTestSmsNotOnCooldown<false>();
}

TEST_F(LoginFunctionTesting, new_installationId_smsOnCooldown) {
    const std::string new_installation_id = generateUUID();
    request.set_installation_id(new_installation_id);

    runFunction();

    authenticationTestSmsOnCooldown<false>();
}

TEST_F(LoginFunctionTesting, new_accountId_smsNotOnCooldown) {
    const std::string new_account_id = gen_random_alpha_numeric_string(rand() % 20 + 10);

    request.set_account_type(AccountLoginType::GOOGLE_ACCOUNT);
    request.set_account_id(new_account_id);

    user_account_doc.time_sms_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    user_account_doc.setIntoCollection();

    runFunction();

    authenticationTestSmsNotOnCooldown();
}

TEST_F(LoginFunctionTesting, new_accountId_smsOnCooldown) {
    const std::string new_account_id = gen_random_alpha_numeric_string(rand() % 20 + 10);

    request.set_account_type(AccountLoginType::GOOGLE_ACCOUNT);
    request.set_account_id(new_account_id);

    runFunction();

    authenticationTestSmsOnCooldown();
}

TEST_F(LoginFunctionTesting, lastVerifiedTime_expired_smsNotOnCooldown) {
    user_account_doc.last_verified_time = bsoncxx::types::b_date{current_timestamp - general_values::TIME_BETWEEN_ACCOUNT_VERIFICATION - std::chrono::milliseconds{1}};
    user_account_doc.time_sms_can_be_sent_again = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    user_account_doc.setIntoCollection();

    runFunction();

    authenticationTestSmsNotOnCooldown();
}

TEST_F(LoginFunctionTesting, lastVerifiedTime_expired_smsOnCooldown) {
    user_account_doc.last_verified_time = bsoncxx::types::b_date{current_timestamp - general_values::TIME_BETWEEN_ACCOUNT_VERIFICATION - std::chrono::milliseconds{1}};
    user_account_doc.setIntoCollection();

    runFunction();

    authenticationTestSmsOnCooldown();
}

TEST_F(LoginFunctionTesting, successful_noUpdatesRequired) {
    runSuccessfulFunctionTest(
            [](loginfunction::LoginResponse& /*generated_response*/){}
    );
}

TEST_F(LoginFunctionTesting, successful_iconRequiresUpdate_passedArrayTooShort) {
    const int number_to_trim = 4;

    int index_missing[number_to_trim];
    //erase the final two icon timestamp elements
    for(int i = 0; i < number_to_trim; ++i) {
        index_missing[number_to_trim-1-i] = request.icon_timestamps_size() - 1;
        request.mutable_icon_timestamps()->erase(
                request.mutable_icon_timestamps()->begin() + request.icon_timestamps_size() - 1
        );
    }

    runSuccessfulFunctionTest(
            [&index_missing](loginfunction::LoginResponse& generated_response){
                for(int i : index_missing) {
                    generated_response.mutable_login_values_to_return_to_client()->add_icons_index(i);
                }
            }
    );
}

TEST_F(LoginFunctionTesting, successful_iconRequiresUpdate_existingElementRequiresUpdate) {
    const int random_index = rand() % request.icon_timestamps_size();

    request.mutable_icon_timestamps()->Set(random_index, -1L);

    runSuccessfulFunctionTest(
            [random_index](loginfunction::LoginResponse& generated_response){
                generated_response.mutable_login_values_to_return_to_client()->add_icons_index(random_index);
            }
    );
}

TEST_F(LoginFunctionTesting, successful_birthdayRequiresUpdate) {
    request.set_birthday_timestamp(user_account_doc.birthday_timestamp.value.count() - 1);

    runSuccessfulFunctionTest(
            [this](loginfunction::LoginResponse& generated_response){
                generated_response.mutable_birthday_info()->set_age(user_account_doc.age);
                generated_response.mutable_birthday_info()->set_birth_day_of_month(user_account_doc.birth_day_of_month);
                generated_response.mutable_birthday_info()->set_birth_month(user_account_doc.birth_month);
                generated_response.mutable_birthday_info()->set_birth_year(user_account_doc.birth_year);
            }
    );
}

TEST_F(LoginFunctionTesting, successful_emailRequiresUpdate) {
    request.set_email_timestamp(user_account_doc.email_timestamp.value.count() - 1);

    runSuccessfulFunctionTest(
            [this](loginfunction::LoginResponse& generated_response){
                generated_response.mutable_email_info()->set_email(user_account_doc.email_address);
                generated_response.mutable_email_info()->set_requires_email_verification(user_account_doc.email_address_requires_verification);
            }
    );
}

TEST_F(LoginFunctionTesting, successful_genderRequiresUpdate) {
    request.set_gender_timestamp(user_account_doc.gender_timestamp.value.count() - 1);

    runSuccessfulFunctionTest(
            [this](loginfunction::LoginResponse& generated_response){
                generated_response.set_gender(user_account_doc.gender);
            }
    );
}

TEST_F(LoginFunctionTesting, successful_nameRequiresUpdate) {
    request.set_name_timestamp(user_account_doc.first_name_timestamp.value.count() - 1);

    runSuccessfulFunctionTest(
            [this](loginfunction::LoginResponse& generated_response){
                generated_response.set_name(user_account_doc.first_name);
            }
    );
}

TEST_F(LoginFunctionTesting, successful_activitiesRequireUpdate) {
    request.set_categories_timestamp(user_account_doc.categories_timestamp.value.count() - 1);

    runSuccessfulFunctionTest(
            [this](loginfunction::LoginResponse& generated_response) {
                for(const auto& category : user_account_doc.categories) {
                    if(category.type == AccountCategoryType::CATEGORY_TYPE) {
                        continue;
                    }
                    auto* category_ele = generated_response.add_categories_array();
                    category.convertToCategoryActivityMessage(category_ele);
                }
            }
    );
}

TEST_F(LoginFunctionTesting, successful_postLoginInfoRequiresUpdate) {
    request.set_post_login_info_timestamp(user_account_doc.post_login_info_timestamp.value.count() - 1);

    runSuccessfulFunctionTest(
            [this](loginfunction::LoginResponse& generated_response){
                generated_response.mutable_post_login_info()->set_user_bio(user_account_doc.bio);
                generated_response.mutable_post_login_info()->set_user_city(user_account_doc.city);
                for(const auto& gender : user_account_doc.genders_range) {
                    generated_response.mutable_post_login_info()->add_gender_range(gender);
                }
                generated_response.mutable_post_login_info()->set_min_age(user_account_doc.age_range.min);
                generated_response.mutable_post_login_info()->set_max_age(user_account_doc.age_range.max);
                generated_response.mutable_post_login_info()->set_max_distance(user_account_doc.max_distance);
            }
    );
}

TEST_F(LoginFunctionTesting, successful_accountStatus_requiresMoreInfo) {
    user_account_doc.status = UserAccountStatus::STATUS_REQUIRES_MORE_INFO;
    user_account_doc.setIntoCollection();

    runSuccessfulFunctionTest(
            [&](loginfunction::LoginResponse& generated_response){
                generated_response.set_access_status(AccessStatus::NEEDS_MORE_INFO);
                generated_response.clear_post_login_timestamp();


            }
    );
}

TEST_F(LoginFunctionTesting, successful_invalidActivitiesStored_activityIndexTooLarge) {
    const size_t number_activities = countTotalActivitiesInDatabase();

    //Set activity index to too large.
    {
        user_account_doc.categories.clear();
        user_account_doc.categories.emplace_back(
                TestCategory(AccountCategoryType::ACTIVITY_TYPE, (int) number_activities)
        );
        user_account_doc.categories.emplace_back(
                TestCategory(AccountCategoryType::CATEGORY_TYPE, 1)
        );
        user_account_doc.setIntoCollection();
    }

    runFunction();

    user_account_doc.categories.clear();

    user_account_doc.categories.emplace_back(
            TestCategory(AccountCategoryType::ACTIVITY_TYPE, 0)
    );
    user_account_doc.categories.emplace_back(
            TestCategory(AccountCategoryType::CATEGORY_TYPE, 0)
    );

    UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);

    user_account_doc.logged_in_token_expiration = extracted_user_account_doc.logged_in_token_expiration;
    user_account_doc.categories_timestamp = extracted_user_account_doc.categories_timestamp;

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    loginfunction::LoginResponse generated_response = buildBasicSuccessfulLoginResponse();

    auto* category = generated_response.add_categories_array();
    category->set_activity_index(0);

    compareGeneratedResponse(generated_response);

    compareOtherDocsOnSuccess();
}

TEST_F(LoginFunctionTesting, successful_invalidActivitiesStored_minAgeTooLarge) {

    int activity_index = -1;
    int min_age = -1;

    extractAgeOfActivityOver13(
            activity_index,
            min_age
    );

    ASSERT_GT(activity_index, -1);
    ASSERT_GT(min_age, -1);

    //Set user age to lower than min age of activity.
    {
        user_account_doc.categories.clear();
        user_account_doc.categories.emplace_back(
                TestCategory(AccountCategoryType::ACTIVITY_TYPE, (int)activity_index)
        );
        user_account_doc.categories.emplace_back(
                TestCategory(AccountCategoryType::CATEGORY_TYPE, 1)
        );

        user_account_doc.age = min_age - 1;

        bool return_val = generateBirthYearForPassedAge(
                user_account_doc.age,
                user_account_doc.birth_year,
                user_account_doc.birth_month,
                user_account_doc.birth_day_of_month,
                user_account_doc.birth_day_of_year
        );

        EXPECT_TRUE(return_val);

        user_account_doc.setIntoCollection();
    }

    runFunction();

    user_account_doc.categories.clear();

    user_account_doc.categories.emplace_back(
            TestCategory(AccountCategoryType::ACTIVITY_TYPE, 0)
    );
    user_account_doc.categories.emplace_back(
            TestCategory(AccountCategoryType::CATEGORY_TYPE, 0)
    );

    UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);

    user_account_doc.logged_in_token_expiration = extracted_user_account_doc.logged_in_token_expiration;
    user_account_doc.categories_timestamp = extracted_user_account_doc.categories_timestamp;

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    loginfunction::LoginResponse generated_response = buildBasicSuccessfulLoginResponse();

    auto* category = generated_response.add_categories_array();
    category->set_activity_index(0);

    compareGeneratedResponse(generated_response);

    compareOtherDocsOnSuccess();

}

TEST_F(LoginFunctionTesting, successful_accountWasSuspendedButSuspensionExpired) {
    //Account was suspended, however it has expired.
    user_account_doc.status = UserAccountStatus::STATUS_SUSPENDED;
    user_account_doc.inactive_end_time = bsoncxx::types::b_date{current_timestamp - std::chrono::milliseconds{100}};
    user_account_doc.inactive_message = gen_random_alpha_numeric_string(rand() % 50 + 10);
    user_account_doc.setIntoCollection();

    //Account status set back to active.
    user_account_doc.status = UserAccountStatus::STATUS_ACTIVE;
    user_account_doc.inactive_end_time = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
    user_account_doc.inactive_message = "";

    runSuccessfulFunctionTest(
            [](loginfunction::LoginResponse& /*generated_response*/) {}
    );
}

TEST_F(LoginFunctionTesting, successful_ageRequiresUpdated) {
    //Age is incorrect.
    user_account_doc.age--;
    user_account_doc.setIntoCollection();

    //Age is set to proper value.
    user_account_doc.age++;

    runSuccessfulFunctionTest(
            [](loginfunction::LoginResponse& /*generated_response*/) {}
    );
}

TEST_F(LoginFunctionTesting, successful_installation_id_isNotLoggedIn) {
    //Logged in installation id is not the installation id the request is passed with.
    user_account_doc.installation_ids.emplace_back(generateUUID());
    user_account_doc.logged_in_installation_id = user_account_doc.installation_ids.back();
    user_account_doc.setIntoCollection();

    runFunction();

    //Logged in installation id is set to the one the request is passed with.
    user_account_doc.logged_in_installation_id = request.installation_id();

    UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
    user_account_doc.logged_in_token_expiration = extracted_user_account_doc.logged_in_token_expiration;
    user_account_doc.logged_in_token = extracted_user_account_doc.logged_in_token;

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    loginfunction::LoginResponse generated_response = buildBasicSuccessfulLoginResponse();

    compareGeneratedResponse(generated_response);

    compareOtherDocsOnSuccess();
}

TEST_F(LoginFunctionTesting, successful_loggedInTokenExpired) {
    //Logged in token is expired.
    user_account_doc.logged_in_token_expiration = bsoncxx::types::b_date{std::chrono::milliseconds{current_timestamp - std::chrono::milliseconds{100}}};
    user_account_doc.setIntoCollection();

    runFunction();

    UserAccountDoc extracted_user_account_doc(user_account_doc.current_object_oid);
    user_account_doc.logged_in_token_expiration = extracted_user_account_doc.logged_in_token_expiration;
    user_account_doc.logged_in_token = extracted_user_account_doc.logged_in_token;

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    loginfunction::LoginResponse generated_response = buildBasicSuccessfulLoginResponse();

    compareGeneratedResponse(generated_response);

    compareOtherDocsOnSuccess();
}

TEST_F(LoginFunctionTesting, installationIdSendsTooManyMessages) {

    request.set_account_id("");

    //The -1 is used because the user does a login during SetUp() when creating their account.
    for(int i = 0; i < general_values::MAXIMUM_NUMBER_SMS_MESSAGES_SENT_BY_INSTALLATION_ID-1; ++i) {
        request.set_account_type(AccountLoginType::PHONE_ACCOUNT);
        request.set_phone_number(generateRandomPhoneNumber());

        runFunction();

        EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

        compareUserAccountDocument();
    }

    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);
    request.set_phone_number(generateRandomPhoneNumber());

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::VERIFICATION_ON_COOL_DOWN);

    compareUserAccountDocument();

    PreLoginCheckersDoc extracted_pre_login_checkers_doc(request.installation_id());
    PreLoginCheckersDoc generated_pre_login_checkers_doc;

    generated_pre_login_checkers_doc.installation_id = request.installation_id();
    generated_pre_login_checkers_doc.number_sms_verification_messages_sent = general_values::MAXIMUM_NUMBER_SMS_MESSAGES_SENT_BY_INSTALLATION_ID + 1;
    generated_pre_login_checkers_doc.sms_verification_messages_last_update_time = extracted_pre_login_checkers_doc.sms_verification_messages_last_update_time;

    EXPECT_EQ(generated_pre_login_checkers_doc, extracted_pre_login_checkers_doc);

    //Allow the account to log in again.
    extracted_pre_login_checkers_doc.sms_verification_messages_last_update_time--;
    extracted_pre_login_checkers_doc.setIntoCollection();

    //Make sure the account can log in again after the time 'expires'.
    request.set_account_type(AccountLoginType::PHONE_ACCOUNT);
    request.set_phone_number(generateRandomPhoneNumber());

    runFunction();

    EXPECT_EQ(response.return_status(), LoginValuesToReturnToClient::REQUIRES_AUTHENTICATION);

    compareUserAccountDocument();
}
