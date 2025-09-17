//
// Created by jeremiah on 3/20/21.
//

#include <bsoncxx/builder/basic/array.hpp>

#include "algorithm_pipeline_helper_functions.h"

#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"
#include "matching_algorithm.h"
#include "utility_general_functions.h"
#include "AccountCategoryEnum.grpc.pb.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void generateAddFieldsTimeFrameTimeStatisticsPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {

    auto activities_total_time_cases_array = bsoncxx::builder::basic::array{};

    for (auto& user_activity : userAccountValues.user_activities) {
        activities_total_time_cases_array.append(
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                        << user_activity.activityIndex
                        << std::string("$$").append(algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY).append(".").append(user_account_keys::categories::INDEX_VALUE)
                    << close_array
                << close_document
                << "then" << bsoncxx::types::b_int64{user_activity.totalTime.count()}
            << finalize
        );
    }

    bsoncxx::builder::stream::document activities_and_categories_switches = generateConditionalSwitchesForCategories(
        userAccountValues.algorithm_search_options,
        activities_total_time_cases_array.view(),
        [](const bsoncxx::array::view& activities_categories_doc)->bsoncxx::builder::stream::document {
            bsoncxx::builder::stream::document return_doc;
            return_doc
                << "$switch" << open_document
                    << "branches" << activities_categories_doc
                    << "default" << bsoncxx::types::b_int64{ 1 }
                << close_document;
            return return_doc;
        },
        [&userAccountValues]()->bsoncxx::builder::basic::array {
            auto categories_total_time_cases_array = bsoncxx::builder::basic::array{};

            for (auto& user_category : userAccountValues.user_categories) {
                categories_total_time_cases_array.append(
                    document{}
                        << "case" << open_document
                            << "$eq" << open_array
                                << user_category.activityIndex
                                << std::string("$$").append(algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY).append(".").append(user_account_keys::categories::INDEX_VALUE)
                            << close_array
                        << close_document
                        << "then" << bsoncxx::types::b_int64{user_category.totalTime.count()}
                    << finalize
                );
            }

            return categories_total_time_cases_array;
        }
    );

    //Activity matches must be included regardless. It is true that the query will only search for matching activities
    // if a single matching activity is found. However, it is possible that between NO matching activities found and
    // the aggregation operation itself a matching activity could be inserted.

    stages->add_fields(document{}
        << user_account_keys::CATEGORIES << open_document
            << "$map" << open_document
                << "input" << '$' + user_account_keys::CATEGORIES
                << "as" << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY
                << "in" << open_document
                    << "$let" << open_document
                        << "vars" << open_document
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA << open_document
                                << "$reduce" << open_document
                                    << "input" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TIMEFRAMES
                                    << "initialValue" << open_document
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << open_array

                                        << close_array
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR << 0
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR << false
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR << 0
                                    << close_document
                                    << "in" << open_document
                                        << "$let" << open_document
                                            << "vars" << open_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR << open_document
                                                    << "$add" << open_array
                                                        << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR
                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR << open_document
                                                    << "$and" << open_array
                                                        << open_document
                                                            << "$lt" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR
                                                                << 2
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$eq" << open_array
                                                                << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                << -1
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$eq" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR
                                                                << 2
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR << open_document
                                                    << "$and" << open_array
                                                        << open_document
                                                            << "$ne" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                << 0
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$ne" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR
                                                                << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$lt" << open_array
                                                                << open_document
                                                                    << "$subtract" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                    << close_array
                                                                << close_document

                                                                << bsoncxx::types::b_int64{matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count()}
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR << open_document
                                                    << "$and" << open_array
                                                        << open_document
                                                            << "$ne" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR
                                                                << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$eq" << open_array
                                                                << open_document
                                                                    << "$subtract" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR
                                                                    << close_array
                                                                << close_document

                                                                << 0
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                            << close_document
                                            << "in" << open_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << open_document
                                                    << "$add" << open_array
                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                                        << open_document
                                                            << "$cond" << open_document
                                                                << "if" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                                << -1
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR
                                                                                << 1
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document
                                                                << "then" << open_document
                                                                    << "$subtract" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR
                                                                    << close_array
                                                                << close_document
                                                                << "else" << bsoncxx::types::b_int64{ 0 }
                                                            << close_document
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << open_document
                                                    << "$cond" << open_document
                                                        << "if" << open_document
                                                            << "$gte" << open_array
                                                                << '$' + user_account_keys::ACCOUNT_TYPE
                                                                << 1000
                                                            << close_array
                                                        << close_document
                                                        << "then" << open_document
                                                            << "$cond" << open_document
                                                                << "if" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TYPE
                                                                                << AccountCategoryType::ACTIVITY_TYPE
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                                << -1
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$gt" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                                                            << close_array
                                                                        << close_document


                                                                    << close_array
                                                                << close_document
                                                                << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                << "else" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                                            << close_document
                                                        << close_document
                                                        << "else" << open_document
                                                            << "$switch" << open_document
                                                                << "branches" << open_array
                                                                    << open_document
                                                                        << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR
                                                                        << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                    << close_document

                                                                    << open_document
                                                                        << "case" << open_document
                                                                            << "$and" << open_array
                                                                                << open_document
                                                                                    << "$eq" << open_array
                                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR
                                                                                        << 0
                                                                                    << close_array
                                                                                << close_document

                                                                                << open_document
                                                                                    << "$or" << open_array
                                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR
                                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR
                                                                                    << close_array
                                                                                << close_document

                                                                            << close_array
                                                                        << close_document
                                                                        << "then" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                    << close_document


                                                                << close_array
                                                                << "default" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                                            << close_document
                                                        << close_document
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR << open_document
                                                    << "$switch" << open_document
                                                        << "branches" << open_array
                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR
                                                                << "then" << 2
                                                            << close_document

                                                            << open_document
                                                                << "case" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR
                                                                                << 0
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$or" << open_array
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document
                                                                << "then" << 1
                                                            << close_document

                                                        << close_array
                                                        << "default" << 0
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << open_document
                                                    << "$switch" << open_document
                                                        << "branches" << open_array
                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR
                                                                << "then" << open_document
                                                                    << "$concatArrays" << open_array
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                                                        << open_array
                                                                            << open_document
                                                                                << "$subtract" << open_array
                                                                                    << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                    << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                                << close_array
                                                                            << close_document

                                                                        << close_array

                                                                    << close_array
                                                                << close_document
                                                            << close_document

                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR
                                                                << "then" << open_document
                                                                    << "$concatArrays" << open_array
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                                                        << open_array
                                                                            << bsoncxx::types::b_int64{ 0 }
                                                                        << close_array

                                                                    << close_array
                                                                << close_document
                                                            << close_document


                                                        << close_array
                                                        << "default" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR << open_document
                                                    << "$cond" << open_document
                                                        << "if" << open_document
                                                            << "$and" << open_array
                                                                << open_document
                                                                    << "$eq" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                        << 1
                                                                    << close_array
                                                                << close_document

                                                                << open_document
                                                                    << "$eq" << open_array
                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR
                                                                        << 2
                                                                    << close_array
                                                                << close_document

                                                            << close_array
                                                        << close_document
                                                        << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                        << "else" << bsoncxx::types::b_int64{ 0 }
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR << open_document
                                                    << "$cond" << open_document
                                                        << "if" << open_document
                                                            << "$eq" << open_array
                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR
                                                                << 0
                                                            << close_array
                                                        << close_document
                                                        << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                        << "else" << bsoncxx::types::b_int64{ 0 }
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR

                                            << close_document
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document
                        << close_document
                        << "in" << open_document
                            << user_account_keys::categories::INDEX_VALUE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::INDEX_VALUE
                            << user_account_keys::categories::TYPE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TYPE
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR << activities_and_categories_switches
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << open_document
                                << "$cond" << open_document
                                    << "if" << open_document
                                        << "$eq" << open_array
                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                            << 0
                                        << close_array
                                    << close_document
                                    << "then" << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count()}
                                    << "else" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                << close_document
                            << close_document

                        << close_document
                    << close_document
                << close_document
            << close_document
        << close_document
        /*<< user_account_keys::CATEGORIES << open_document
            << "$map" << open_document
                << "input" << '$' + user_account_keys::CATEGORIES
                << "as" << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY
                << "in" << open_document
                    << "$let" << open_document
                        << "vars" << open_document
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA << open_document
                                << "$reduce" << open_document
                                    << "input" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TIMEFRAMES
                                    << "initialValue" << open_document
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << open_array

                                        << close_array
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR << 0
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR << false
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << bsoncxx::types::b_int64{ 0 }
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR << 0
                                    << close_document
                                    << "in" << open_document
                                        << "$let" << open_document
                                            << "vars" << open_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR << open_document
                                                    << "$add" << open_array
                                                        << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR
                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR << open_document
                                                    << "$and" << open_array
                                                        << open_document
                                                            << "$lt" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR
                                                                << 2
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$eq" << open_array
                                                                << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                << -1
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$eq" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR
                                                                << 2
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR << open_document
                                                    << "$and" << open_array
                                                        << open_document
                                                            << "$ne" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                << 0
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$ne" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR
                                                                << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$lt" << open_array
                                                                << open_document
                                                                    << "$subtract" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                    << close_array
                                                                << close_document

                                                                << bsoncxx::types::b_int64{matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count()}
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR << open_document
                                                    << "$and" << open_array
                                                        << open_document
                                                            << "$ne" << open_array
                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR
                                                                << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER
                                                            << close_array
                                                        << close_document

                                                        << open_document
                                                            << "$eq" << open_array
                                                                << open_document
                                                                    << "$subtract" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR
                                                                    << close_array
                                                                << close_document

                                                                << 0
                                                            << close_array
                                                        << close_document

                                                    << close_array
                                                << close_document
                                            << close_document
                                            << "in" << open_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << open_document
                                                    << "$add" << open_array
                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                                        << open_document
                                                            << "$cond" << open_document
                                                                << "if" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                                << -1
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR
                                                                                << 1
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document
                                                                << "then" << open_document
                                                                    << "$subtract" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR
                                                                    << close_array
                                                                << close_document
                                                                << "else" << bsoncxx::types::b_int64{ 0 }
                                                            << close_document
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << open_document
                                                    << "$switch" << open_document
                                                        << "branches" << open_array
                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR
                                                                << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                            << close_document

                                                            << open_document
                                                                << "case" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR
                                                                                << 0
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$or" << open_array
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document
                                                                << "then" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                            << close_document


                                                        << close_array
                                                        << "default" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR << open_document
                                                    << "$switch" << open_document
                                                        << "branches" << open_array
                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_EXPIRATION_OVERLAP_CHECK_VAR
                                                                << "then" << 2
                                                            << close_document

                                                            << open_document
                                                                << "case" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_SET_TO_VAR
                                                                                << 0
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$or" << open_array
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR
                                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document
                                                                << "then" << 1
                                                            << close_document


                                                        << close_array
                                                        << "default" << 0
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << open_document
                                                    << "$switch" << open_document
                                                        << "branches" << open_array
                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_VAR
                                                                << "then" << open_document
                                                                    << "$concatArrays" << open_array
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                                                        << open_array
                                                                            << open_document
                                                                                << "$subtract" << open_array
                                                                                    << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                    << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR
                                                                                << close_array
                                                                            << close_document

                                                                        << close_array

                                                                    << close_array
                                                                << close_document
                                                            << close_document

                                                            << open_document
                                                                << "case" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_CHECK_SECOND_VAR
                                                                << "then" << open_document
                                                                    << "$concatArrays" << open_array
                                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                                                        << open_array
                                                                            << bsoncxx::types::b_int64{ 0 }
                                                                        << close_array

                                                                    << close_array
                                                                << close_document
                                                            << close_document


                                                        << close_array
                                                        << "default" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_OVERLAP_START_TIME_VAR << open_document
                                                    << "$cond" << open_document
                                                        << "if" << open_document
                                                            << "$and" << open_array
                                                                << open_document
                                                                    << "$eq" << open_array
                                                                        << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                        << 1
                                                                    << close_array
                                                                << close_document

                                                                << open_document
                                                                    << "$eq" << open_array
                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR
                                                                        << 2
                                                                    << close_array
                                                                << close_document

                                                            << close_array
                                                        << close_document
                                                        << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                        << "else" << bsoncxx::types::b_int64{ 0 }
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_STOP_TIME_VAR << open_document
                                                    << "$cond" << open_document
                                                        << "if" << open_document
                                                            << "$eq" << open_array
                                                                << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR
                                                                << 0
                                                            << close_array
                                                        << close_document
                                                        << "then" << "$$this." + user_account_keys::categories::timeframes::TIME
                                                        << "else" << bsoncxx::types::b_int64{ 0 }
                                                    << close_document
                                                << close_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TYPE_USER_VAR << "$$this." + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TIMEFRAMES_USER
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_NESTED_VALUE_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CURRENT_NESTED_VALUE_VAR

                                            << close_document
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document
                        << close_document
                        << "in" << open_document
                            << user_account_keys::categories::INDEX_VALUE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::INDEX_VALUE
                            << user_account_keys::categories::TYPE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TYPE
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR << activities_and_categories_switches
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                            << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << open_document
                                << "$cond" << open_document
                                    << "if" << open_document
                                        << "$eq" << open_array
                                            << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                            << 0
                                        << close_array
                                    << close_document
                                    << "then" << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count()}
                                    << "else" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                                << close_document
                            << close_document

                        << close_document
                    << close_document
                << close_document
            << close_document
        << close_document*/
    << finalize);

}
