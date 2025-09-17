//
// Created by jeremiah on 9/3/22.
//

#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "generate_matching_users.h"
#include "user_account_keys.h"

#include "helper_functions/find_matches_helper_functions.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class FunctionsLeadingToAlgorithmTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid{};
    bsoncxx::oid match_account_oid{};

    UserAccountDoc user_account_doc{};
    UserAccountDoc match_account_doc{};

    UserAccountValues user_account_values{};

    bsoncxx::builder::stream::document user_account_doc_builder{};
    bsoncxx::builder::basic::array user_gender_range_builder{};

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        ASSERT_TRUE(
                generateMatchingUsers(
                        user_account_doc,
                        match_account_doc,
                        user_account_oid,
                        match_account_oid,
                        user_account_values,
                        user_gender_range_builder
                )
        );

        user_account_doc.search_by_options = AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY;

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_oid;

        setUserAccountDocView();

        const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{250};
        const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = std::chrono::milliseconds{
                300};;
        const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = std::chrono::milliseconds{
                500};
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void setUserAccountDocView() {
        user_account_doc_builder.clear();
        user_account_doc.convertToDocument(user_account_doc_builder);

        user_account_values.user_account_doc_view = user_account_doc_builder;
    }

    void setMatchingActivities(
            const std::vector<TestCategory>& activities_categories
    ) {
        user_account_doc.categories.clear();
        match_account_doc.categories.clear();

        std::copy(activities_categories.begin(), activities_categories.end(),
                  std::back_inserter(user_account_doc.categories));
        std::copy(activities_categories.begin(), activities_categories.end(),
                  std::back_inserter(match_account_doc.categories));

        user_account_doc.setIntoCollection();
        match_account_doc.setIntoCollection();

        setUserAccountDocView();
    }

    void successfullyFindMatch(
            const std::function<AlgorithmReturnValues()>& run_func
    ) {
        setMatchingActivities(
                std::vector<TestCategory>{
                        TestCategory{
                                AccountCategoryType::ACTIVITY_TYPE,
                                1
                        },
                        TestCategory{
                                AccountCategoryType::CATEGORY_TYPE,
                                1
                        }
                }
        );

        EXPECT_EQ(user_account_values.algorithm_matched_account_list.size(), 0);

        AlgorithmReturnValues return_values = run_func();

        EXPECT_EQ(return_values.returnMessage, AlgorithmReturnMessage::successfully_extracted);
        EXPECT_EQ(return_values.coolDownOnMatchAlgorithm, std::chrono::milliseconds{0});

        EXPECT_EQ(user_account_values.algorithm_matched_account_list.size(), 1);
        if (!user_account_values.algorithm_matched_account_list.empty()) {
            EXPECT_EQ(
                    user_account_values.algorithm_matched_account_list[0].view()[user_account_keys::accounts_list::OID].get_oid().value.to_string(),
                    match_account_oid.to_string()
            );
        }
    }
};

TEST_F(FunctionsLeadingToAlgorithmTesting, onlyDefaultActivities) {
    AlgorithmReturnValues return_values = organizeUserInfoForAlgorithm(
            user_account_values,
            nullptr,
            user_accounts_collection
    );

    EXPECT_EQ(return_values.returnMessage, AlgorithmReturnMessage::match_algorithm_ran_recently);
    EXPECT_EQ(return_values.coolDownOnMatchAlgorithm,
              matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND);

    EXPECT_EQ(user_account_values.algorithm_matched_account_list.size(), 0);
}

TEST_F(FunctionsLeadingToAlgorithmTesting, noAlgorithmResult) {

    setMatchingActivities(
            std::vector<TestCategory>{
                    TestCategory{
                            AccountCategoryType::ACTIVITY_TYPE,
                            1
                    },
                    TestCategory{
                            AccountCategoryType::CATEGORY_TYPE,
                            1
                    }
            }
    );

    match_account_doc.matching_activated = false;
    match_account_doc.setIntoCollection();

    AlgorithmReturnValues return_values = organizeUserInfoForAlgorithm(
            user_account_values,
            nullptr,
            user_accounts_collection
    );

    EXPECT_EQ(return_values.returnMessage, AlgorithmReturnMessage::no_matches_found);
    EXPECT_EQ(return_values.coolDownOnMatchAlgorithm,
              matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND);

    EXPECT_EQ(user_account_values.algorithm_matched_account_list.size(), 0);
}

TEST_F(FunctionsLeadingToAlgorithmTesting, successfulMatchFound) {
    successfullyFindMatch(
            [&] {
                return organizeUserInfoForAlgorithm(
                        user_account_values,
                        nullptr,
                        user_accounts_collection
                );
            }
    );
}

TEST_F(FunctionsLeadingToAlgorithmTesting, algorithmOnCooldownFrom_emptyMatch) {
    setMatchingActivities(
            std::vector<TestCategory>{
                    TestCategory{
                            AccountCategoryType::ACTIVITY_TYPE,
                            1
                    },
                    TestCategory{
                            AccountCategoryType::CATEGORY_TYPE,
                            1
                    }
            }
    );

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{
            matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND * 2};

    user_account_doc.last_time_empty_match_returned = bsoncxx::types::b_date{
            user_account_values.current_timestamp -
            matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND + std::chrono::milliseconds{1}
    };

    setUserAccountDocView();

    AlgorithmReturnValues return_values = beginAlgorithmCheckCoolDown(
            user_account_values,
            nullptr,
            user_accounts_collection
    );

    EXPECT_EQ(return_values.returnMessage, AlgorithmReturnMessage::match_algorithm_returned_empty_recently);
    EXPECT_EQ(return_values.coolDownOnMatchAlgorithm.count(), 1);

    EXPECT_EQ(user_account_values.algorithm_matched_account_list.size(), 0);
}

TEST_F(FunctionsLeadingToAlgorithmTesting, algorithmOnCooldownFrom_previousRun) {

    const long remaining_cooldown = 1;

    setMatchingActivities(
            std::vector<TestCategory>{
                    TestCategory{
                            AccountCategoryType::ACTIVITY_TYPE,
                            1
                    },
                    TestCategory{
                            AccountCategoryType::CATEGORY_TYPE,
                            1
                    }
            }
    );

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{
            matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND * 2};

    user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{
            user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS +
            std::chrono::milliseconds{remaining_cooldown}
    };

    setUserAccountDocView();

    AlgorithmReturnValues return_values = beginAlgorithmCheckCoolDown(
            user_account_values,
            nullptr,
            user_accounts_collection
    );

    EXPECT_EQ(return_values.returnMessage, AlgorithmReturnMessage::match_algorithm_ran_recently);
    EXPECT_EQ(return_values.coolDownOnMatchAlgorithm.count(), remaining_cooldown);

    EXPECT_EQ(user_account_values.algorithm_matched_account_list.size(), 0);
}

TEST_F(FunctionsLeadingToAlgorithmTesting, algorithmOnCooldownFrom_successfullyRan) {

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = getCurrentTimestamp();

    successfullyFindMatch(
            [&] {
                return beginAlgorithmCheckCoolDown(
                        user_account_values,
                        nullptr,
                        user_accounts_collection
                );
            }
    );

}
