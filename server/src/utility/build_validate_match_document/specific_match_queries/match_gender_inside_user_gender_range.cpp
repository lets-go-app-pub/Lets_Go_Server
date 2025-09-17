//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"

#include <bsoncxx/builder/basic/array.hpp>

#include "user_account_keys.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//check if match gender is inside user gender range
void matchGenderInsideUserGenderRangeOrEvent(
        bsoncxx::builder::stream::document& query_doc,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        const bool user_matches_with_everyone,
        const bool only_match_with_events
) {

    if(only_match_with_events) {
        query_doc
                << user_account_keys::GENDER << general_values::EVENT_GENDER_VALUE;
    }
    else if (!user_matches_with_everyone) { //if this user matches with specific gender(s)

        //This creates a copy, but it should be short.
        bsoncxx::builder::basic::array all_genders;
        for(auto& ele : user_genders_to_match_with_array) {
            all_genders.append(ele.get_string().value.to_string());
        }
        all_genders.append(general_values::EVENT_GENDER_VALUE);

        //check gender of other account is an event OR matches this accounts' gender range
        query_doc
                << user_account_keys::GENDER << open_document
                    << "$in" << all_genders.view()
                << close_document;
    }
}