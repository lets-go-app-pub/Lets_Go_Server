//
// Created by jeremiah on 10/5/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "RequestStatistics.pb.h"
#include "request_statistics.h"
#include "generate_multiple_accounts_multi_thread.h"
#include "compare_equivalent_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class AgeGenderStatisticsTesting : public ::testing::Test {
protected:

    request_statistics::AgeGenderStatisticsRequest request;
    request_statistics::AgeGenderStatisticsResponse response;

    AdminAccountDoc admin_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        if(admin_account_doc.current_object_oid.to_string() == "000000000000000000000000") {
            createTempAdminAccount(admin_level);
            admin_account_doc.getFromCollection(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);
        }

        runMatchingAgeGenderStatistics(
                &request, &response
        );
    }

    template <bool update_last_time_extracted = false>
    void compareAdminAccounts() {

        AdminAccountDoc extracted_admin_account_doc(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);

        if(update_last_time_extracted) {
            EXPECT_GT(extracted_admin_account_doc.last_time_extracted_age_gender_statistics, 0);
            admin_account_doc.last_time_extracted_age_gender_statistics = extracted_admin_account_doc.last_time_extracted_age_gender_statistics;
        }

        EXPECT_EQ(admin_account_doc, extracted_admin_account_doc);
    }

    request_statistics::AgeGenderStatisticsResponse generateResponseFromUserAccountsCollection() {
        request_statistics::AgeGenderStatisticsResponse generated_response;

        generated_response.set_success(true);

        mongocxx::options::find opts;

        opts.projection(
                document{}
                        << "_id" << 1
                        << finalize
        );

        auto accounts_list_cursor = user_accounts_collection.find(
                document{}
                        << finalize,
                opts
        );

        std::unordered_map<int, request_statistics::NumberOfTimesGenderSelectedAtAge> age_map;

        for(const auto& doc : accounts_list_cursor) {
            const bsoncxx::oid user_account_oid = doc["_id"].get_oid().value;

            UserAccountDoc user_account(user_account_oid);

            if(user_account.gender == general_values::MALE_GENDER_VALUE) {
                age_map[user_account.age].set_age(user_account.age);
                age_map[user_account.age].set_gender_male(age_map[user_account.age].gender_male() + 1);
            } else if(user_account.gender == general_values::FEMALE_GENDER_VALUE) {
                age_map[user_account.age].set_age(user_account.age);
                age_map[user_account.age].set_gender_female(age_map[user_account.age].gender_female() + 1);
            } else { //other gender
                age_map[user_account.age].set_age(user_account.age);
                age_map[user_account.age].set_gender_other(age_map[user_account.age].gender_other() + 1);
            }
        }

        for(const auto& element : age_map) {
            generated_response.add_gender_selected_at_age_list()->CopyFrom(element.second);
        }

        return generated_response;
    }

    static void sortResponse(request_statistics::AgeGenderStatisticsResponse& passed_response) {
        std::sort(passed_response.mutable_gender_selected_at_age_list()->begin(), passed_response.mutable_gender_selected_at_age_list()->end(),
                  [](
                          const auto& l, const auto& r
                  ){
                      return l.age() < r.age();
                  });
    }


};

TEST_F(AgeGenderStatisticsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );

    //should not have any changes
    compareAdminAccounts();
}

TEST_F(AgeGenderStatisticsTesting, noAdminPriveledge) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());

    //should not have any changes
    compareAdminAccounts();
}

TEST_F(AgeGenderStatisticsTesting, stillOnCoolDown) {

    createTempAdminAccount(AdminLevelEnum::PRIMARY_DEVELOPER);
    admin_account_doc.getFromCollection(TEMP_ADMIN_ACCOUNT_NAME, TEMP_ADMIN_ACCOUNT_PASSWORD);
    admin_account_doc.last_time_extracted_age_gender_statistics = bsoncxx::types::b_date{getCurrentTimestamp()};
    admin_account_doc.setIntoCollection();

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());

    //should not have any changes
    compareAdminAccounts();
}

TEST_F(AgeGenderStatisticsTesting, noAccountsFound) {
    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());
    EXPECT_EQ(response.gender_selected_at_age_list().size(), 0);

    //should not have any changes
    compareAdminAccounts<true>();
}

TEST_F(AgeGenderStatisticsTesting, randomAccountsSetUp) {

    multiThreadInsertAccounts(
            1000,
            (int) std::thread::hardware_concurrency() - 1,
            false
    );

    runFunction();

    request_statistics::AgeGenderStatisticsResponse generated_response = generateResponseFromUserAccountsCollection();

    sortResponse(response);
    sortResponse(generated_response);

    ASSERT_EQ(generated_response.gender_selected_at_age_list_size(), response.gender_selected_at_age_list_size());

    std::cout << "response.gender_selected_at_age_list_size(): " << response.gender_selected_at_age_list_size() << '\n';

    compareEquivalentMessages<request_statistics::AgeGenderStatisticsResponse>(
            generated_response,
            response
    );

    //should not have any changes
    compareAdminAccounts<true>();
}
