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
#include "user_account_keys.h"

#include "helper_functions/find_matches_helper_functions.h"
#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "info_for_statistics_objects.h"
#include "request_statistics_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SaveFindMatchesStatisticsTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    bsoncxx::builder::stream::document user_account_doc_builder;

    UserAccountValues user_account_values;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database stats_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    std::vector<bsoncxx::document::value> statistics_documents_to_insert;
    bsoncxx::array::view update_statistics_documents_query;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);
        user_account_doc.convertToDocument(user_account_doc_builder);

        const_cast<double&>(user_account_values.longitude) = 123.45;
        const_cast<double&>(user_account_values.latitude) = -54.321;

        const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_oid;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    enum StatisticsThatWasChanged {
        RAW_ALGORITHM_RESULTS_STATISTICS_CHANGED,
        INSERT_INDIVIDUAL_MATCHED_STATISTICS_CHANGED,
        UPDATE_INDIVIDUAL_MATCHED_STATISTICS_CHANGED,
        SAVE_USER_INFO_STATISTICS_CHANGED,
    };

    void checkFinalDocument(
            StatisticsThatWasChanged statistic_changed,
            MatchAlgorithmStatisticsDoc* generated_match_statistics_doc = nullptr,
            IndividualMatchStatisticsDoc* generated_individual_match_statistics_doc = nullptr
    ) {

        //Make sure any commands have time to reach the entire replica set.
        std::this_thread::sleep_for(std::chrono::milliseconds{50});

        saveFindMatchesStatistics(
                user_account_values,
                mongo_cpp_client,
                stats_db,
                accounts_db,
                statistics_documents_to_insert,
                update_statistics_documents_query
        );

        if(statistic_changed == RAW_ALGORITHM_RESULTS_STATISTICS_CHANGED) {
            MatchAlgorithmStatisticsDoc extracted_statistics_doc;
            extracted_statistics_doc.getFromCollection();
            generated_match_statistics_doc->current_object_oid = extracted_statistics_doc.current_object_oid;

            EXPECT_EQ(*generated_match_statistics_doc, extracted_statistics_doc);
        } else {
            MatchAlgorithmStatisticsDoc extracted_statistics_doc;
            extracted_statistics_doc.getFromCollection();

            EXPECT_EQ(extracted_statistics_doc.current_object_oid.to_string(), "000000000000000000000000");
        }

        if(statistic_changed == INSERT_INDIVIDUAL_MATCHED_STATISTICS_CHANGED
            || statistic_changed == UPDATE_INDIVIDUAL_MATCHED_STATISTICS_CHANGED) {
            IndividualMatchStatisticsDoc extracted_individual_match_statistics(generated_individual_match_statistics_doc->current_object_oid);

            EXPECT_EQ(extracted_individual_match_statistics, *generated_individual_match_statistics_doc);
        } else {
            IndividualMatchStatisticsDoc individual_match_statistics;
            individual_match_statistics.getFromCollection();

            EXPECT_EQ(individual_match_statistics.current_object_oid.to_string(), "000000000000000000000000");
        }

        if(statistic_changed == SAVE_USER_INFO_STATISTICS_CHANGED) {
            UserAccountStatisticsDoc user_account_statistics_doc(user_account_oid);

            ASSERT_EQ(user_account_statistics_doc.locations.size(), 2);

            EXPECT_EQ(user_account_statistics_doc.locations.back().location[0],  user_account_values.longitude);
            EXPECT_EQ(user_account_statistics_doc.locations.back().location[1],  user_account_values.latitude);
        } else {
            UserAccountStatisticsDoc user_account_statistics_doc(user_account_oid);

            ASSERT_FALSE(user_account_statistics_doc.locations.empty());

            EXPECT_NE(user_account_statistics_doc.locations.back().location[0],  user_account_values.longitude);
            EXPECT_NE(user_account_statistics_doc.locations.back().location[1],  user_account_values.latitude);
        }
    }

};

TEST_F(SaveFindMatchesStatisticsTesting, only_saveRawAlgorithmResults) {

    MatchAlgorithmStatisticsDoc generated_statistics_doc;
    generated_statistics_doc.generateRandomValues(user_account_oid, true);

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = generated_statistics_doc.timestamp_run.value;
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = generated_statistics_doc.end_time.value;
    user_account_values.algorithm_run_time_ns = std::chrono::nanoseconds{generated_statistics_doc.algorithm_run_time};
    user_account_values.check_for_activities_run_time_ns = std::chrono::nanoseconds{generated_statistics_doc.query_activities_run_time};
    user_account_values.user_account_doc_view = user_account_doc_builder;

    //do not let user account statistics be updated
    const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = bsoncxx::oid{};

    for(const auto& doc : generated_statistics_doc.results_docs) {
        bsoncxx::builder::stream::document result_builder;
        doc.convertToDocument(result_builder);
        user_account_values.raw_algorithm_match_results.append(result_builder);
    }

    checkFinalDocument(
            StatisticsThatWasChanged::RAW_ALGORITHM_RESULTS_STATISTICS_CHANGED,
            &generated_statistics_doc
    );
}

TEST_F(SaveFindMatchesStatisticsTesting, only_insertNewMatchStatisticsDocuments) {

    //inserts everything inside statistics_documents_to_insert
    bsoncxx::oid match_account_oid = insertRandomAccounts(1, 0);

    IndividualMatchStatisticsDoc individual_match_statistics_doc;
    individual_match_statistics_doc.generateRandomValues(match_account_oid);

    bsoncxx::builder::stream::document match_stats_doc;
    individual_match_statistics_doc.convertToDocument(match_stats_doc);

    statistics_documents_to_insert.emplace_back(match_stats_doc << finalize);

    checkFinalDocument(
            StatisticsThatWasChanged::INSERT_INDIVIDUAL_MATCHED_STATISTICS_CHANGED,
            nullptr,
            &individual_match_statistics_doc
    );
}

TEST_F(SaveFindMatchesStatisticsTesting, only_updatePreviousMatchStatisticsDocuments) {
    //updates everything inside update_statistics_documents_query
    bsoncxx::oid match_account_oid = insertRandomAccounts(1, 0);

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = getCurrentTimestamp();
    long day_timestamp = user_account_values.current_timestamp.count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY;

    IndividualMatchStatisticsDoc individual_match_statistics_doc;
    individual_match_statistics_doc.generateRandomValues(match_account_oid);
    individual_match_statistics_doc.day_timestamp = day_timestamp - (rand() % 10 + 1);
    individual_match_statistics_doc.setIntoCollection();

    bsoncxx::builder::basic::array update_statistics_documents_builder;

    update_statistics_documents_builder.append(individual_match_statistics_doc.current_object_oid);
    update_statistics_documents_query = update_statistics_documents_builder;

    //set up updated values
    individual_match_statistics_doc.day_timestamp = day_timestamp;
    individual_match_statistics_doc.sent_timestamp.emplace_back(user_account_values.current_timestamp);

    checkFinalDocument(
            StatisticsThatWasChanged::UPDATE_INDIVIDUAL_MATCHED_STATISTICS_CHANGED,
            nullptr,
            &individual_match_statistics_doc
    );
}

TEST_F(SaveFindMatchesStatisticsTesting, only_saveUserInfoStatistics) {
    //updates user info doc statistics

    std::cout << user_account_oid.to_string() << '\n';

    user_account_values.user_account_doc_view = user_account_doc_builder;
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = getCurrentTimestamp();

    checkFinalDocument(
            StatisticsThatWasChanged::SAVE_USER_INFO_STATISTICS_CHANGED
    );
}
