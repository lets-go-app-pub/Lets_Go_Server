//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"


#include "user_account_keys.h"
#include "matching_algorithm.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//check if this user was not 'recently' a match for the other account
void userNotRecentMatchForMatch(bsoncxx::builder::stream::document& matchDoc,
                       const bsoncxx::oid& userAccountOID,
                       const std::chrono::milliseconds& currentTimestamp) {
    matchDoc
            << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << open_document
                << "$not" << open_document
                    << "$elemMatch" << open_document
                        << user_account_keys::previously_matched_accounts::OID << userAccountOID
                        << user_account_keys::previously_matched_accounts::TIMESTAMP << open_document
                            << "$gt" << bsoncxx::types::b_date{currentTimestamp - matching_algorithm::TIME_BETWEEN_SAME_ACCOUNT_MATCHES}
                        << close_document
                    << close_document
                << close_document
            << close_document;
}