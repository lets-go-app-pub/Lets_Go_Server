//
// Created by jeremiah on 8/30/22.
//

#pragma once

#include "account_objects.h"
#include "find_matches_helper_objects.h"

//NOTE: This will create a matching account for the passed account. It will attempt minimal changes to
// first_account_doc. However, it will remove second_account doc from any lists that would prevent a match.
bool buildMatchingUserForPassedAccount(
        UserAccountDoc& first_account_doc,
        UserAccountDoc& second_account_doc,
        const bsoncxx::oid& first_account_oid,
        const bsoncxx::oid& second_account_oid
);

//This will take in two users and make them into a match. It will make changes to both users. It will also
// save required values to run the algorithm to user_account_values and user_gender_range_builder.
bool generateMatchingUsers(
        UserAccountDoc& user_account_doc,
        UserAccountDoc& match_account_doc,
        bsoncxx::oid& user_account_oid,
        bsoncxx::oid& match_account_oid,
        UserAccountValues& user_account_values,
        bsoncxx::builder::basic::array& user_gender_range_builder
);