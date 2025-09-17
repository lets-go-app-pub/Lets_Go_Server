//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"


#include "algorithm_pipeline_field_names.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;


//check if user age is in age range of other
void userAgeInsideMatchAgeRange(bsoncxx::builder::stream::document& query_doc, int user_age) {
    query_doc
        << algorithm_pipeline_field_names::MONGO_PIPELINE_AGE_RANGE_MIN_KEY << open_document
            << "$lte" << bsoncxx::types::b_int32{user_age}
        << close_document
        << algorithm_pipeline_field_names::MONGO_PIPELINE_AGE_RANGE_MAX_KEY << open_document
            << "$gte" << bsoncxx::types::b_int32{user_age}
        << close_document;
}
