//
// Created by jeremiah on 3/20/21.
//

#include "algorithm_pipeline_helper_functions.h"

#include "matching_algorithm.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//NOTE: when limit is after sort in an aggregation pipeline mongodb will combine them for efficiency
void generateLimitPipelineStage(mongocxx::pipeline* stages) {
    stages->limit(matching_algorithm::TOTAL_NUMBER_DOCS_RETURNED);
}