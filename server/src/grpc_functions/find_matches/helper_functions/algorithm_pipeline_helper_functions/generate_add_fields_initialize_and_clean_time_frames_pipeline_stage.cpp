//
// Created by jeremiah on 3/20/21.
//

#include "algorithm_pipeline_helper_functions.h"

#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void generateAddFieldsInitializeAndCleanTimeFramesPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {

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
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << 0
                                        << user_account_keys::categories::TIMEFRAMES << open_array

                                        << close_array
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_VAR << 0
                                        << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_IS_ABOVE_NOW_VAR << false
                                    << close_document
                                    << "in" << open_document
                                        << "$let" << open_document
                                            << "vars" << open_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_IS_ABOVE_NOW_VAR << open_document
                                                    << "$gt" << open_array
                                                        << "$$this." + user_account_keys::categories::timeframes::TIME
                                                        << bsoncxx::types::b_int64{userAccountValues.earliest_time_frame_start_timestamp.count()}
                                                    << close_array
                                                << close_document
                                            << close_document
                                            << "in" << open_document
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_IS_ABOVE_NOW_VAR << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_IS_ABOVE_NOW_VAR
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_VAR << "$$this." + user_account_keys::categories::timeframes::TIME
                                                << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << open_document
                                                    << "$add" << open_array
                                                        << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
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

                                                                        << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_IS_ABOVE_NOW_VAR
                                                                    << close_array
                                                                << close_document
                                                                << "then" << open_document
                                                                    << "$cond" << open_document
                                                                        << "if" << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_IS_ABOVE_NOW_VAR
                                                                        << "then" << open_document
                                                                            << "$subtract" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_VAR
                                                                            << close_array
                                                                        << close_document
                                                                        << "else" << open_document
                                                                            << "$subtract" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                << bsoncxx::types::b_int64{userAccountValues.earliest_time_frame_start_timestamp.count()}
                                                                            << close_array
                                                                        << close_document
                                                                    << close_document
                                                                << close_document
                                                                << "else" << bsoncxx::types::b_int64{ 0 }
                                                            << close_document
                                                        << close_document

                                                    << close_array
                                                << close_document
                                                << user_account_keys::categories::TIMEFRAMES << open_document
                                                    << "$cond" << open_document
                                                        << "if" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_IS_ABOVE_NOW_VAR
                                                        << "then" << open_document
                                                            << "$cond" << open_document
                                                                << "if" << open_document
                                                                    << "$and" << open_array
                                                                        << open_document
                                                                            << "$not" << open_array
                                                                                << "$$value." + algorithm_pipeline_field_names::MONGO_PIPELINE_PREVIOUS_TIME_IS_ABOVE_NOW_VAR
                                                                            << close_array
                                                                        << close_document

                                                                        << open_document
                                                                            << "$eq" << open_array
                                                                                << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                                << -1
                                                                            << close_array
                                                                        << close_document

                                                                    << close_array
                                                                << close_document
                                                                << "then" << open_document
                                                                    << "$concatArrays" << open_array
                                                                        << "$$value." + user_account_keys::categories::TIMEFRAMES
                                                                        << open_array
                                                                            << open_document
                                                                                << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{userAccountValues.earliest_time_frame_start_timestamp.count() + 1}
                                                                                << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                                                                            << close_document

                                                                            << open_document
                                                                                << user_account_keys::categories::timeframes::TIME << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                << user_account_keys::categories::timeframes::START_STOP_VALUE << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                            << close_document

                                                                        << close_array

                                                                    << close_array
                                                                << close_document
                                                                << "else" << open_document
                                                                    << "$concatArrays" << open_array
                                                                        << "$$value." + user_account_keys::categories::TIMEFRAMES
                                                                        << open_array
                                                                            << open_document
                                                                                << user_account_keys::categories::timeframes::TIME << "$$this." + user_account_keys::categories::timeframes::TIME
                                                                                << user_account_keys::categories::timeframes::START_STOP_VALUE << "$$this." + user_account_keys::categories::timeframes::START_STOP_VALUE
                                                                            << close_document

                                                                        << close_array

                                                                    << close_array
                                                                << close_document
                                                            << close_document
                                                        << close_document
                                                        << "else" << "$$value." + user_account_keys::categories::TIMEFRAMES
                                                    << close_document
                                                << close_document
                                            << close_document
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document
                        << close_document
                        << "in" << open_document
                            << "$cond" << open_document
                                << "if" << open_document
                                    << "$eq" << open_array
                                        << open_document
                                            << "$size" << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + user_account_keys::categories::TIMEFRAMES
                                        << close_document

                                        << 0
                                    << close_array
                                << close_document
                                << "then" << open_document
                                    << user_account_keys::categories::TYPE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TYPE
                                    << user_account_keys::categories::INDEX_VALUE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::INDEX_VALUE
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count() - userAccountValues.earliest_time_frame_start_timestamp.count()}
                                    << user_account_keys::categories::TIMEFRAMES << open_array
                                        << open_document
                                            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{userAccountValues.earliest_time_frame_start_timestamp.count() + 1}
                                            << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
                                        << close_document

                                        << open_document
                                            << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count()}
                                            << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
                                        << close_document

                                    << close_array
                                << close_document
                                << "else" << open_document
                                    << user_account_keys::categories::TYPE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::TYPE
                                    << user_account_keys::categories::INDEX_VALUE << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORIES_TEMP_NAME_KEY + '.' + user_account_keys::categories::INDEX_VALUE
                                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                                    << user_account_keys::categories::TIMEFRAMES << "$$" + algorithm_pipeline_field_names::MONGO_PIPELINE_TIMEFRAME_DATA + '.' + user_account_keys::categories::TIMEFRAMES
                                << close_document
                            << close_document
                        << close_document
                    << close_document
                << close_document
            << close_document
        << close_document
    << finalize);

}