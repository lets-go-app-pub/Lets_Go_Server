//
// Created by jeremiah on 4/10/21.
//

#include <utility_general_functions.h>
#include "specific_match_queries.h"

#include "user_account_keys.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//check age of other account matches this account (this shouldn't technically be needed
// because if the user age range changes the vector will be wiped, and they can't change age manually, but it is indexed anyway)
void matchAgeInsideUserAgeRangeOrEvent(
        bsoncxx::builder::stream::document& query_doc,
        int min_age_range,
        int max_age_range,
        bool only_match_with_events
) {
    if(only_match_with_events) {
        query_doc
                << user_account_keys::AGE << general_values::EVENT_AGE_VALUE;
    } else {
        query_doc
            << "$or" << open_array
                << open_document
                    << user_account_keys::AGE << general_values::EVENT_AGE_VALUE
                << close_document
                << open_document
                    << user_account_keys::AGE << open_document
                        << "$gte" << min_age_range
                        << "$lte" << max_age_range
                    << close_document
                << close_document
            << close_array;
    }
}
