//
// Created by jeremiah on 11/8/21.
//

#include "build_user_statistics_document.h"
#include "user_account_statistics_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bsoncxx::document::value buildUserStatisticsDocument(
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::array::view& phone_number_array
) {
    return document{}
        << "_id" << user_account_oid
        << user_account_statistics_keys::LOGIN_TIMES << open_array << close_array
        << user_account_statistics_keys::TIMES_MATCH_OCCURRED << open_array << close_array
        << user_account_statistics_keys::LOCATIONS << open_array << close_array
        << user_account_statistics_keys::PHONE_NUMBERS << phone_number_array
        << user_account_statistics_keys::NAMES << open_array << close_array
        << user_account_statistics_keys::BIOS << open_array << close_array
        << user_account_statistics_keys::CITIES << open_array << close_array
        << user_account_statistics_keys::GENDERS << open_array << close_array
        << user_account_statistics_keys::BIRTH_INFO << open_array << close_array
        << user_account_statistics_keys::EMAIL_ADDRESSES << open_array << close_array
        << user_account_statistics_keys::CATEGORIES << open_array << close_array
        << user_account_statistics_keys::AGE_RANGES << open_array << close_array
        << user_account_statistics_keys::GENDER_RANGES << open_array << close_array
        << user_account_statistics_keys::MAX_DISTANCES << open_array << close_array
        << user_account_statistics_keys::SMS_SENT_TIMES << open_array << close_array
        << user_account_statistics_keys::EMAIL_SENT_TIMES << open_array << close_array
        << user_account_statistics_keys::EMAILS_VERIFIED << open_array << close_array
        << user_account_statistics_keys::ACCOUNT_RECOVERY_TIMES << open_array << close_array
        << user_account_statistics_keys::ACCOUNT_LOGGED_OUT_TIMES << open_array << close_array
        << user_account_statistics_keys::ACCOUNT_SEARCH_BY_OPTIONS << open_array << close_array
        << user_account_statistics_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL << open_array << close_array
    << finalize;
}