//
// Created by jeremiah on 3/9/22.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <user_account_keys.h>
#include "update_single_other_user.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//Projects out values for saveUserInfoToMemberSharedInfoMessage.
bsoncxx::document::value buildUpdateSingleOtherUserProjectionDoc() {
    return document{}
        << user_account_keys::FIRST_NAME << 1
        << user_account_keys::PICTURES << 1
        << user_account_keys::AGE << 1
        << user_account_keys::GENDER << 1
        << user_account_keys::CITY << 1
        << user_account_keys::BIO << 1
        << user_account_keys::CATEGORIES << 1
        << user_account_keys::ACCOUNT_TYPE << 1
        << user_account_keys::EVENT_EXPIRATION_TIME << 1
        << user_account_keys::EVENT_VALUES << 1
        << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << 1
    << finalize;
}