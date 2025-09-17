//
// Created by jeremiah on 4/10/21.
//

#include "build_validate_match_document.h"
#include "specific_match_queries/specific_match_queries.h"

void generatePipelineMatchDocument(
        bsoncxx::builder::stream::document& query_doc,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        const bool user_matches_with_everyone,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        bool only_match_with_events
) {

    //NOTE: Still need matchingIsActivatedOnMatchAccount(matchDoc) called inside universalMatchConditions().
    // STATUS and MATCHING_ACTIVATED are included inside the algorithm index as a partial index. And so these
    // values will allow the algorithm to use the compound index designed for the algorithm.

    universalMatchConditions(
            query_doc,
            current_timestamp,
            user_age,
            min_age_range,
            max_age_range,
            user_gender,
            user_genders_to_match_with_array,
            user_matches_with_everyone,
            only_match_with_events
    );

    userNotOnMatchBlockedList(
            query_doc,
            user_account_oid.to_string()
    );

    userNotRecentMatchForMatch(
            query_doc,
            user_account_oid,
            current_timestamp
    );
}