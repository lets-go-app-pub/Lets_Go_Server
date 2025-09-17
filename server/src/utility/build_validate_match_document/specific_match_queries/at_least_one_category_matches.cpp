//
// Created by jeremiah on 8/8/21.
//

#include "specific_match_queries.h"


#include "user_account_keys.h"

//at least one activity or category of other account needs to match at least one from this account
void atLeastOneCategoryMatches(bsoncxx::builder::stream::document& matchDoc,
                               const bsoncxx::array::view& userCategoriesMongoDBArray,
                               AccountCategoryType categoryTypeToMatch
                       ) {

    matchDoc
        << user_account_keys::CATEGORIES << bsoncxx::builder::stream::open_document
            << "$elemMatch" << bsoncxx::builder::stream::open_document
                << user_account_keys::categories::TYPE << categoryTypeToMatch
                << user_account_keys::categories::INDEX_VALUE << bsoncxx::builder::stream::open_document
                    << "$in" << userCategoriesMongoDBArray
                << bsoncxx::builder::stream::close_document
            << bsoncxx::builder::stream::close_document
        << bsoncxx::builder::stream::close_document;
}