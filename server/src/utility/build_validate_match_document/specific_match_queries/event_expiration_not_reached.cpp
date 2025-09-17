//
// Created by jeremiah on 3/3/23.
//

#include "specific_match_queries.h"
#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//check if it is a user account OR the event is not expired
void eventExpirationNotReached(
        bsoncxx::builder::stream::document& query_doc,
        const std::chrono::milliseconds& current_timestamp
) {
    // This part of the query works with values inside general_values::USER_ACCOUNT.
    query_doc
        << user_account_keys::EVENT_EXPIRATION_TIME << open_document
            << "$gt" << bsoncxx::types::b_date{current_timestamp}
        << close_document;
}