//
// Created by jeremiah on 3/20/21.
//

#include <AccountCategoryEnum.grpc.pb.h>
#include "algorithm_pipeline_helper_functions.h"

#include "utility_general_functions.h"
#include "database_names.h"
#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void generateAddFieldsFinalPoints(mongocxx::pipeline* stages) {
    stages->add_fields(document{}
        << algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR << open_document
            << "$add" << open_array
                << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR
                << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_STATS_VAR + '.' + algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY
                << '$' + algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY
            << close_array
        << close_document
    << finalize);
}

