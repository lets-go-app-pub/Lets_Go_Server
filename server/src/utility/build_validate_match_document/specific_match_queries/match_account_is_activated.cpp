//
// Created by jeremiah on 4/10/21.
//

#include <UserAccountStatusEnum.grpc.pb.h>
#include "specific_match_queries.h"


#include "user_account_keys.h"

//make sure other account is activated for matching
void matchingIsActivatedOnMatchAccount(bsoncxx::builder::stream::document& query_doc) {

    //STATUS & MATCHING_ACTIVATED are both part of the partial index for the matching
    // algorithm
    query_doc
        << user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
        << user_account_keys::MATCHING_ACTIVATED << bsoncxx::types::b_bool{true};
}
