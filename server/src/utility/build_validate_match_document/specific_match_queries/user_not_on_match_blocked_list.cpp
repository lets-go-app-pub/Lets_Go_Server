//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"


#include "user_account_keys.h"

//make sure this account is not on other accounts blocked list
void userNotOnMatchBlockedList(bsoncxx::builder::stream::document& matchDoc,
                       const std::string& userAccountOIDStr) {
    matchDoc
        << user_account_keys::OTHER_USERS_BLOCKED + '.' + user_account_keys::other_users_blocked::OID_STRING << bsoncxx::builder::stream::open_document
            << "$ne" << userAccountOIDStr
        << bsoncxx::builder::stream::close_document;
}



