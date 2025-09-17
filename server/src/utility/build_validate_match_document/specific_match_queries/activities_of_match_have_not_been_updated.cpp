//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"


#include "user_account_keys.h"

//make sure activities of other account have not been updated
void activitiesOfMatchHaveNotBeenUpdated(bsoncxx::builder::stream::document& matchDoc,
                                         const std::chrono::milliseconds& timeMatchOccurred) {
    matchDoc
            << user_account_keys::CATEGORIES_TIMESTAMP << bsoncxx::builder::stream::open_document
                << "$lte" << bsoncxx::types::b_date{timeMatchOccurred}
            << bsoncxx::builder::stream::close_document;
}
