//
// Created by jeremiah on 3/20/21.
//

#include <iostream>
#include <cmath>
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

void generateProjectCleanUpPipelineStage(
        mongocxx::pipeline* stages,
        UserAccountValues& userAccountValues
) {

    stages->project(document{}
        << algorithm_pipeline_field_names::MONGO_PIPELINE_DISTANCE_KEY << open_document
            << "$toDouble" << generateDistanceFromLocations(userAccountValues.longitude, userAccountValues.latitude) //NOTE: This stage should only need to run for ~10 documents, so re-calculating distance should be OK.
        << close_document
        << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << open_document
            << "$toLong" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
        << close_document
        << algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR << open_document
            << "$toDouble" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR
        << close_document<< algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_STATISTICS_VAR << open_document
            << "$mergeObjects" << open_array
                << open_document
                    << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << '$' + user_account_keys::LAST_TIME_FIND_MATCHES_RAN
                    << user_account_keys::ACCOUNT_TYPE << '$' + user_account_keys::ACCOUNT_TYPE
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_MATCH_RAN_VAR << bsoncxx::types::b_int64{userAccountValues.current_timestamp.count()}
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_EARLIEST_START_TIME_VAR << bsoncxx::types::b_int64{userAccountValues.earliest_time_frame_start_timestamp.count()}
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_MAX_POSSIBLE_TIME_VAR << bsoncxx::types::b_int64{userAccountValues.end_of_time_frame_timestamp.count()}
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR << open_document
                        << "$toDouble" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR
                    << close_document
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY << open_document
                        << "$toDouble" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY
                    << close_document
                    << algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY << open_document
                        << "$toDouble" << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY
                    << close_document
                    << user_account_keys::accounts_list::ACTIVITY_STATISTICS << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR + '.' + user_account_keys::accounts_list::ACTIVITY_STATISTICS
                << close_document
            << close_array
        << close_document
    << finalize);

}
