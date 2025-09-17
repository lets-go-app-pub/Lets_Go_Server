//
// Created by jeremiah on 4/10/21.
//

#include "specific_match_queries.h"

void universalMatchConditions(
        bsoncxx::builder::stream::document& query_doc,
        const std::chrono::milliseconds& current_timestamp,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        const bool user_matches_with_everyone,
        bool only_match_with_events
) {
    matchingIsActivatedOnMatchAccount(query_doc);
    userAgeInsideMatchAgeRange(
            query_doc,
            user_age
    );
    matchAgeInsideUserAgeRangeOrEvent(
            query_doc,
            min_age_range,
            max_age_range,
            only_match_with_events
    );
    userGenderInsideMatchGenderRange(
            query_doc,
            user_gender
    );
    matchGenderInsideUserGenderRangeOrEvent(
            query_doc,
            user_genders_to_match_with_array,
            user_matches_with_everyone,
            only_match_with_events
    );
    eventExpirationNotReached(
            query_doc,
            current_timestamp
    );
}