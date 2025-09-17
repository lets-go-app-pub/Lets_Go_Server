//
// Created by jeremiah on 11/15/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include "find_matches_helper_functions.h"
#include "user_account_keys.h"

bsoncxx::document::value projectUserAccountFieldsOutForStatistics() {

    //NOTE: projecting most of the fields so that statistics can be stored
    return bsoncxx::builder::stream::document{}
        << user_account_keys::DISCIPLINARY_RECORD << 0
        << user_account_keys::ACCOUNT_ID_LIST << 0
        << user_account_keys::NUMBER_TIMES_SWIPED_YES << 0
        << user_account_keys::NUMBER_TIMES_SWIPED_NO << 0
        << user_account_keys::NUMBER_TIMES_SWIPED_BLOCK << 0
        << user_account_keys::NUMBER_TIMES_SWIPED_REPORT << 0
        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES << 0
        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO << 0
        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK << 0
        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT << 0
        << user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION << 0
        << user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT << 0
        << user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION << 0
        << user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT << 0
        << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << 0
    << bsoncxx::builder::stream::finalize;
}