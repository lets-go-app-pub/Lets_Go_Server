//
// Created by jeremiah on 9/1/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "generate_matching_users.h"
#include "user_account_keys.h"

#include "utility_general_functions.h"
#include "helper_functions/find_matches_helper_functions.h"
#include "generate_multiple_random_accounts.h"
#include "generate_multiple_accounts_multi_thread.h"
#include "extract_data_from_bsoncxx.h"
#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RunMatchingAlgorithmTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    UserAccountValues user_account_values;

    bsoncxx::builder::basic::array user_gender_range_builder;
    bsoncxx::builder::stream::document user_account_builder;

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

        user_account_doc.convertToDocument(user_account_builder);
        user_account_values.user_account_doc_view = user_account_builder.view();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void recursionArrayBuilder(
            const bsoncxx::array::view& arr,
            bsoncxx::builder::basic::array& builder
    );

    void recursionDocumentBuilder(
            const bsoncxx::document::view& doc,
            bsoncxx::builder::stream::document& builder
    );

};

void RunMatchingAlgorithmTesting::recursionArrayBuilder(
        const bsoncxx::array::view& arr,
        bsoncxx::builder::basic::array& builder
) {
    for(const auto& x : arr) {
        if(x.type() == bsoncxx::type::k_document) {
            bsoncxx::builder::stream::document nested_builder;
            recursionDocumentBuilder(x.get_document().view(), nested_builder);
            builder.append(nested_builder);
        } else if(x.type() == bsoncxx::type::k_array) {
            bsoncxx::builder::basic::array nested_builder;
            recursionArrayBuilder(x.get_array(), nested_builder);
            builder.append(nested_builder);
        } else {
            builder.append(x.get_value());
        }
    }
}

void RunMatchingAlgorithmTesting::recursionDocumentBuilder(
        const bsoncxx::document::view& doc,
        bsoncxx::builder::stream::document& builder
) {
    for(const auto& x : doc) {
        if(x.key().to_string() != "filter"
           && x.key().to_string() != "stages"
           && x.key().to_string() != "command"
           && x.key().to_string() != "parsedQuery"
                ) {
            if(x.type() == bsoncxx::type::k_document) {
                bsoncxx::builder::stream::document nested_builder;
                recursionDocumentBuilder(x.get_document().view(), nested_builder);
                builder
                        << x.key() << nested_builder;
            } else if(x.type() == bsoncxx::type::k_array) {
                bsoncxx::builder::basic::array nested_builder;
                recursionArrayBuilder(x.get_array(), nested_builder);
                builder
                        << x.key() << nested_builder;
            } else {
                builder
                        << x.key() << x.get_value();
            }
        }
    }
};

TEST_F(RunMatchingAlgorithmTesting, guaranteeProperIndexUsedForQuery) {

    //Generate enough accounts to 'guarantee' the index is used.
    multiThreadInsertAccounts(
            1000,
            (int) std::thread::hardware_concurrency() - 1,
            false
    );

    for(int i = 0; i < 2; ++i) {

        if(i == 1) {
            //Set the current user to only accept an invalid gender. This will allow it to test USER_MATCHING_BY_CATEGORY_AND_ACTIVITY
            // because no matches will be found for the activities.
            //NOTE: In i == 0 there is at least one match that is guaranteed.

            user_account_doc.genders_range.clear();
            user_account_doc.genders_range.emplace_back("!_invalid_1!^");
            user_account_doc.setIntoCollection();
            user_account_doc.convertToDocument(user_account_builder);

            user_account_values.genders_to_match_with_bson_array = user_account_builder.view()[user_account_keys::GENDERS_RANGE].get_array().value;
            user_account_values.user_matches_with_everyone = false;
            user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;
        }

        bsoncxx::builder::stream::document query_doc;

        bool build_aggregation_successful = buildQueryDocForAggregationPipeline(
                user_account_values,
                user_accounts_collection,
                query_doc,
                false
        );

        mongocxx::pipeline pipe;

        pipe.match(query_doc.view());

        EXPECT_TRUE(build_aggregation_successful);
        if (build_aggregation_successful) {

            //syntax for this command at
            // https://www.mongodb.com/docs/manual/reference/command/explain/
            auto explain_doc = accounts_db.run_command(
                document{}
                    << "explain" << open_document
                        << "aggregate" << collection_names::USER_ACCOUNTS_COLLECTION_NAME
                        << "pipeline" << pipe.view_array()
                        << "cursor" << open_document << close_document
                        << "hint" << user_account_collection_index::ALGORITHM_INDEX_NAME
                    << close_document
                    << "verbosity" << "allPlansExecution"
                << finalize
            );

            try {

                auto query_planner = extractFromBsoncxx_k_document(
                        explain_doc,
                        "queryPlanner"
                );

                auto winning_plan = extractFromBsoncxx_k_document(
                        query_planner,
                        "winningPlan"
                );

                auto input_stage = extractFromBsoncxx_k_document(
                        winning_plan,
                        "inputStage"
                );

                auto stage = extractFromBsoncxx_k_utf8(
                        input_stage,
                        "stage"
                );

                auto index_name = extractFromBsoncxx_k_utf8(
                        input_stage,
                        "indexName"
                );

                //guarantee the correct index was used
                EXPECT_EQ(stage, "IXSCAN"); //index was used, 'COLLSCAN' means entire collection was scanned
                EXPECT_EQ(index_name, user_account_collection_index::ALGORITHM_INDEX_NAME);

            } catch (const ErrorExtractingFromBsoncxx& e) {
                //Should always be false.
                EXPECT_EQ(std::string(e.what()), "%2&");
            }

            bsoncxx::builder::stream::document builder;
            recursionDocumentBuilder(
                    explain_doc.view(),
                    builder
            );

            std::cout << makePrettyJson(builder.view()) << '\n';
        }

    }

}

TEST_F(RunMatchingAlgorithmTesting, checkFinalOutputForProperTypes) {

    std::optional<mongocxx::cursor> algorithm_results;

    bool return_val = runMatchingAlgorithm(
            user_account_values,
            user_accounts_collection,
            algorithm_results,
            false
    );

    EXPECT_TRUE(return_val);
    ASSERT_TRUE(algorithm_results);

    int num_docs = 0;
    for(const auto& doc : *algorithm_results) {
        std::cout << makePrettyJson(doc) << '\n';

        EXPECT_EQ(doc[algorithm_pipeline_field_names::MONGO_PIPELINE_DISTANCE_KEY].type(), bsoncxx::type::k_double);
        EXPECT_EQ(doc[algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR].type(), bsoncxx::type::k_int64);
        EXPECT_EQ(doc[algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR].type(), bsoncxx::type::k_double);
        EXPECT_EQ(doc[algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR].type(), bsoncxx::type::k_double);

        auto match_statistics_ele = doc[algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_STATISTICS_VAR];
        EXPECT_EQ(match_statistics_ele.type(), bsoncxx::type::k_document);
        if(match_statistics_ele.type() == bsoncxx::type::k_document) {
            bsoncxx::document::view match_statistics_doc = match_statistics_ele.get_document();

            EXPECT_EQ(match_statistics_doc[user_account_keys::ACCOUNT_TYPE].type(), bsoncxx::type::k_int32);
            EXPECT_EQ(match_statistics_doc[user_account_keys::LAST_TIME_FIND_MATCHES_RAN].type(), bsoncxx::type::k_date);
            EXPECT_EQ(match_statistics_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_MATCH_RAN_VAR].type(), bsoncxx::type::k_int64);
            EXPECT_EQ(match_statistics_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_EARLIEST_START_TIME_VAR].type(), bsoncxx::type::k_int64);
            EXPECT_EQ(match_statistics_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_MAX_POSSIBLE_TIME_VAR].type(), bsoncxx::type::k_int64);
            EXPECT_EQ(match_statistics_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR].type(), bsoncxx::type::k_double);
            EXPECT_EQ(match_statistics_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY].type(), bsoncxx::type::k_double);
            EXPECT_EQ(match_statistics_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY].type(), bsoncxx::type::k_double);

            auto activity_statistics_ele = match_statistics_doc[user_account_keys::accounts_list::ACTIVITY_STATISTICS];
            EXPECT_EQ(activity_statistics_ele.type(), bsoncxx::type::k_array);
            if(activity_statistics_ele.type() == bsoncxx::type::k_array) {

                int num_matching_activities = 0;
                bsoncxx::array::view activity_statistics_arr = activity_statistics_ele.get_array().value;
                for(const auto& stat_ele : activity_statistics_arr) {
                    EXPECT_EQ(stat_ele.type(), bsoncxx::type::k_document);
                    if(stat_ele.type() == bsoncxx::type::k_document) {
                        bsoncxx::document::view stat_doc = stat_ele.get_document().value;

                        EXPECT_EQ(stat_doc[user_account_keys::categories::INDEX_VALUE].type(), bsoncxx::type::k_int32);
                        EXPECT_EQ(stat_doc[user_account_keys::categories::TYPE].type(), bsoncxx::type::k_int32);
                        EXPECT_EQ(stat_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR].type(), bsoncxx::type::k_int64);
                        EXPECT_EQ(stat_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH].type(), bsoncxx::type::k_int64);
                        EXPECT_EQ(stat_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR].type(), bsoncxx::type::k_int64);
                        EXPECT_EQ(stat_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR].type(), bsoncxx::type::k_int64);

                        auto between_times_array_ele = stat_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR];
                        EXPECT_EQ(between_times_array_ele.type(), bsoncxx::type::k_array);
                        if(between_times_array_ele.type() == bsoncxx::type::k_array) {
                            bsoncxx::array::view between_times_array = between_times_array_ele.get_array().value;
                            for(const auto& between_time : between_times_array) {
                                EXPECT_EQ(between_time.type(), bsoncxx::type::k_int64);
                            }
                        }
                        num_matching_activities++;
                    }
                }
                EXPECT_EQ(num_matching_activities, 1);
            }

        }

        num_docs++;
    }
    EXPECT_EQ(num_docs, 1);
}

TEST_F(RunMatchingAlgorithmTesting, noMatchesFound) {

    user_accounts_collection.delete_one(
            document{}
                << "_id" << match_account_oid
            << finalize
    );

    std::optional<mongocxx::cursor> algorithm_results;

    bool return_val = runMatchingAlgorithm(
            user_account_values,
            user_accounts_collection,
            algorithm_results,
            false
    );

    EXPECT_TRUE(return_val);
    ASSERT_TRUE(algorithm_results);

    EXPECT_EQ(std::distance(algorithm_results->begin(), algorithm_results->end()), 0);
}
