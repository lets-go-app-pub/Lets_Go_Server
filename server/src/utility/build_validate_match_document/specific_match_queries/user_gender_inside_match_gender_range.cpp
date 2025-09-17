//
// Created by jeremiah on 4/10/21.
//

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

//make sure the user gender is inside the matching gender range
void userGenderInsideMatchGenderRange(
        bsoncxx::builder::stream::document& query_doc,
        const std::string& user_gender
) {
    query_doc
            << user_account_keys::GENDERS_RANGE << open_document
                << "$in" << open_array
                    << bsoncxx::types::b_string{user_gender} //check if in gender range of other
                    //NOTE: This used to make sure that the other user's GENDER_RANGE was only 1 element in size AND Everyone,
                    // however indexing does not support that unless the size is a separate field and has an index placed on it.
                    << general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE
                << close_array
            << close_document;
}