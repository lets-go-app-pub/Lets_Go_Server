//
// Created by jeremiah on 8/31/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include "../generate_matching_users.h"
#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"
#include "utility_general_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class GenerateProjectCalculateFinalPointTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    UserAccountValues user_account_values;

    bsoncxx::builder::basic::array user_gender_range_builder;

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

        const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{matching_algorithm::PREVIOUSLY_MATCHED_FALLOFF_TIME * 2};
        const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = user_account_values.current_timestamp + std::chrono::milliseconds{50};
        const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{15000};
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    [[nodiscard]] double generateTimeFalloff(const bsoncxx::array::view& previously_matched_accounts) const {

        bool doc_found = false;
        bsoncxx::document::view prev_account_doc;

        for(const auto& prev_account : previously_matched_accounts) {

            prev_account_doc = prev_account.get_document().value;

            bsoncxx::oid extracted_oid{prev_account_doc[user_account_keys::previously_matched_accounts::OID].get_oid().value};

            if(extracted_oid == match_account_oid) {
                doc_found = true;
                break;
            }
        }

        double generated_time_falloff = 0;

        if(doc_found) {
            const int num_times_matched = prev_account_doc[user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED].get_int32().value;

            double numerator = (double)user_account_values.current_timestamp.count() - (double)prev_account_doc[user_account_keys::previously_matched_accounts::TIMESTAMP].get_date().value.count();
            double denominator = (double)matching_algorithm::PREVIOUSLY_MATCHED_FALLOFF_TIME.count() * num_times_matched;

            if(denominator > 0 && numerator < denominator) {
                generated_time_falloff = -1 * matching_algorithm::PREVIOUSLY_MATCHED_WEIGHT * (1 - numerator/denominator);
            }
        }

        return generated_time_falloff;
    }

    [[nodiscard]] double generateInactivityPointsToSubtract(
            const long match_last_time_find_matches_ran
            ) const {
        if(match_last_time_find_matches_ran == general_values::EVENT_DEFAULT_LAST_TIME_FIND_MATCHES_RAN) {
            return 0.0;
        } else {
            return -1 * matching_algorithm::INACTIVE_ACCOUNT_WEIGHT *
                   (double) (user_account_values.current_timestamp.count() - match_last_time_find_matches_ran);
        }
    }

    [[nodiscard]] double generateCategoryOrActivityPoints(
            const AccountCategoryType type,
            const long total_overlap_time,
            const long total_user_time,
            const long total_match_time,
            const bsoncxx::array::view& between_times_array
            ) const {

        double overlap_points = 0;
        double between_times_points = 0;
        double activity_category_points;

        {
            double overlap_weight = type == AccountCategoryType::CATEGORY_TYPE ?
                                    matching_algorithm::OVERLAPPING_CATEGORY_TIMES_WEIGHT :
                                    matching_algorithm::OVERLAPPING_ACTIVITY_TIMES_WEIGHT;

            double short_overlap_weight = type == AccountCategoryType::CATEGORY_TYPE ?
                                          matching_algorithm::SHORT_TIMEFRAME_CATEGORY_OVERLAP_WEIGHT :
                                          matching_algorithm::SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT;

            if(total_overlap_time > 0) {
                auto overlap_ratio = (double)total_overlap_time/(double)std::max(total_match_time, total_user_time);
                double short_overlap_ratio = short_overlap_weight * (1.0 - (double)total_overlap_time/(double)(user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count()));

                overlap_points = overlap_ratio * (short_overlap_ratio + overlap_weight);
            }
        }

        {
            double between_time_weight = type == AccountCategoryType::CATEGORY_TYPE ?
                                       matching_algorithm::BETWEEN_CATEGORY_TIMES_WEIGHT :
                                       matching_algorithm::BETWEEN_ACTIVITY_TIMES_WEIGHT;

            for(const auto& x : between_times_array) {
                long between_time = x.get_int64().value;

                between_times_points += between_time_weight * (1.0 - (double)between_time/(double)(matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count()));
            }

        }

        {
            activity_category_points = type == AccountCategoryType::CATEGORY_TYPE ?
                                       matching_algorithm::CATEGORIES_MATCH_WEIGHT :
                                       matching_algorithm::ACTIVITY_MATCH_WEIGHT;
        }

        return overlap_points + between_times_points + activity_category_points;
    }

    struct CategoriesInputDocument {
        const int first_activity_index = 0;
        const AccountCategoryType first_activity_type = AccountCategoryType::CATEGORY_TYPE;
        const long total_time_for_match = 0;
        const long total_time_for_user = 0;
        const long total_overlap_time = 0;
        bsoncxx::builder::basic::array between_times_array{};
        const long match_expiration_time = 0;

        [[nodiscard]] bsoncxx::document::value buildDocument() const {
            return document{}
                    << user_account_keys::categories::INDEX_VALUE << first_activity_index
                    << user_account_keys::categories::TYPE << first_activity_type
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << total_time_for_match
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR << total_time_for_user
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << total_overlap_time
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << between_times_array
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << match_expiration_time
                    << finalize;
        }

        CategoriesInputDocument() = delete;

        CategoriesInputDocument(
                int _first_activity_index,
                AccountCategoryType _first_activity_type,
                long _total_time_for_match,
                long _total_time_for_user,
                long _total_overlap_time,
                const std::vector<long>& _between_times_array,
                long _match_expiration_time
        ) : first_activity_index(_first_activity_index),
            first_activity_type(_first_activity_type),
            total_time_for_match(_total_time_for_match),
            total_time_for_user(_total_time_for_user),
            total_overlap_time(_total_overlap_time),
            match_expiration_time (_match_expiration_time) {
            for(long val : _between_times_array) {
                between_times_array.append(val);
            }
        }
    };

    void setupAndRunFunction(
            const std::vector<CategoriesInputDocument>& category_inputs,
            const bsoncxx::array::view previously_matched_accounts_arr,
            const long match_last_time_find_matches_ran
            ) {
        bsoncxx::builder::basic::array categories_array;

        for(const auto& category : category_inputs) {
            categories_array.append(category.buildDocument());
        }

        mongocxx::pipeline pipeline;

        pipeline.match(
                document{}
                        << "_id" << match_account_oid
                << finalize
        );

        pipeline.project(
                document{}
                        << "_id" << 1
                        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << bsoncxx::types::b_date{std::chrono::milliseconds{match_last_time_find_matches_ran}}
                        << user_account_keys::LOCATION << open_document
                            << "type" << "Point"
                            << "coordinates" << open_array
                                << bsoncxx::types::b_double{-122}
                                << bsoncxx::types::b_double{37}
                            << close_array //some random default coordinates; I think It's somewhere in California?
                        << close_document
                        << user_account_keys::CATEGORIES << categories_array
                << finalize
        );

        bsoncxx::builder::stream::document previously_matched_accounts_doc;

        previously_matched_accounts_doc
                << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << previously_matched_accounts_arr;

        user_account_values.user_account_doc_view = previously_matched_accounts_doc;

        generateProjectCalculateFinalPointIntermediatesPipelineStage(
                &pipeline,
                user_account_values
        );

        auto cursor_result = user_accounts_collection.aggregate(pipeline);

        int num_results = 0;
        for(const auto& result_doc : cursor_result) {

            compareDocResult(
                    result_doc,
                    category_inputs,
                    previously_matched_accounts_arr,
                    match_last_time_find_matches_ran
            );

            num_results++;
        }

        EXPECT_EQ(num_results, 1);
    }

    void compareDocResult(
            const bsoncxx::document::view& result_doc,
            const std::vector<CategoriesInputDocument>& category_inputs,
            const bsoncxx::array::view previously_matched_accounts_arr,
            const long match_last_time_find_matches_ran
    ) {
        std::cout << makePrettyJson(result_doc) << '\n';

        double expected_time_fall_off = result_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR].get_double().value;
        double generated_time_fall_off = generateTimeFalloff(
            previously_matched_accounts_arr
        );
        EXPECT_DOUBLE_EQ(expected_time_fall_off, generated_time_fall_off);

        double expected_inactivity_points_to_subtract = result_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY].get_double().value;
        double generated_inactivity_points_to_subtract = generateInactivityPointsToSubtract(
                match_last_time_find_matches_ran
        );
        EXPECT_DOUBLE_EQ(expected_inactivity_points_to_subtract, generated_inactivity_points_to_subtract);

        double generated_category_or_activity_points = 0;

        for(const auto& category_input : category_inputs) {
            generated_category_or_activity_points += generateCategoryOrActivityPoints(
                    category_input.first_activity_type,
                    category_input.total_overlap_time,
                    category_input.total_time_for_user,
                    category_input.total_time_for_match,
                    category_input.between_times_array
            );
        }

        bsoncxx::document::view time_stats_doc = result_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR].get_document().value;
        auto category_activity_element = time_stats_doc[algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY];

        double expected_category_or_activity_points = 0;
        switch (category_activity_element.type()) {
            case bsoncxx::v_noabi::type::k_double:
                expected_category_or_activity_points = category_activity_element.get_double().value;
                break;
            case bsoncxx::v_noabi::type::k_int32:
                expected_category_or_activity_points = (double) category_activity_element.get_int32().value;
                break;
            case bsoncxx::v_noabi::type::k_int64:
                expected_category_or_activity_points = (double) category_activity_element.get_int64().value;
                break;
            default:
                EXPECT_TRUE(false);
                break;
        }

        EXPECT_NEAR(expected_category_or_activity_points, generated_category_or_activity_points, .5);
    }

};

TEST_F(GenerateProjectCalculateFinalPointTesting, noPreviouslyMatchedAccount) {

    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            500,
            500,
            0,
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, accountPreviouslyMatchedAccount) {
    const long match_last_time_find_matches_ran = 500;

    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = std::chrono::milliseconds{matching_algorithm::PREVIOUSLY_MATCHED_FALLOFF_TIME * 2};
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = user_account_values.current_timestamp + std::chrono::milliseconds{50};
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{600};

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            500,
            500,
            0,
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    previously_matched_accounts_arr.append(
            document{}
                << user_account_keys::previously_matched_accounts::OID << match_account_oid
                << user_account_keys::previously_matched_accounts::TIMESTAMP << bsoncxx::types::b_date{user_account_values.current_timestamp - matching_algorithm::PREVIOUSLY_MATCHED_FALLOFF_TIME}
                << user_account_keys::previously_matched_accounts::NUMBER_TIMES_MATCHED << 2
            << finalize
    );

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, overlappingTime_totalUserTime_greaterThan_totalMatchTime) {
    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            500,
            1500,
            250,
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, overlappingTime_totalUserTime_lessThanOrEqualTo_totalMatchTime) {
    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            1111,
            500,
            325,
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, singleBetweenTimesArray) {
    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            500,
            500,
            0,
            std::vector<long>{
                    200
            },
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, multipleBetweenTimesArrays) {
    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            500,
            500,
            0,
            std::vector<long>{
                matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count() - 1,
                300,
                400
                },
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, singleCategory) {
    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::CATEGORY_TYPE,
            500,
            500,
            100,
            std::vector<long>{
                100
                },
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, multipleCategoriesAndActivities) {
    const long match_last_time_find_matches_ran = 500;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::CATEGORY_TYPE,
            500,
            500,
            277,
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            1500,
            200,
            0,
            std::vector<long>{
                400,
                600
                },
            user_account_values.end_of_time_frame_timestamp.count()
    );

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count(),
            user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count(),
            user_account_values.end_of_time_frame_timestamp.count() - user_account_values.earliest_time_frame_start_timestamp.count(),
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

TEST_F(GenerateProjectCalculateFinalPointTesting, lastTimeFindMatchesRan_eventDefault) {

    const long match_last_time_find_matches_ran = general_values::EVENT_DEFAULT_LAST_TIME_FIND_MATCHES_RAN;

    std::vector<CategoriesInputDocument> category_inputs;

    category_inputs.emplace_back(
            0,
            AccountCategoryType::ACTIVITY_TYPE,
            500,
            500,
            0,
            std::vector<long>{},
            user_account_values.end_of_time_frame_timestamp.count()
    );

    bsoncxx::builder::basic::array previously_matched_accounts_arr;

    setupAndRunFunction(
            category_inputs,
            previously_matched_accounts_arr,
            match_last_time_find_matches_ran
    );
}

