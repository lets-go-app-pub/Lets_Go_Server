//
// Created by jeremiah on 3/20/21.
//

#include "algorithm_pipeline_helper_functions.h"

#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void generateSortByTotalPointsPipelineStage(mongocxx::pipeline* stages) {
    stages->sort(document{}
        << algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR << -1
    << finalize);
}
