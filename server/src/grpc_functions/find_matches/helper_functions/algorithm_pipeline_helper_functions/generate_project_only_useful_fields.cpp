//
// Created by jeremiah on 8/8/21.
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

void generateProjectOnlyUsefulFieldsPipelineStage(mongocxx::pipeline* stages) {
    stages->project(document{}
        << user_account_keys::MAX_DISTANCE << 1
        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << 1
        << user_account_keys::ACCOUNT_TYPE << 1
        << user_account_keys::LOCATION << 1//"$" + algorithm_pipeline_field_names::MONGO_PIPELINE_DISTANCE_KEY
        << user_account_keys::CATEGORIES << 1
    << finalize);
}