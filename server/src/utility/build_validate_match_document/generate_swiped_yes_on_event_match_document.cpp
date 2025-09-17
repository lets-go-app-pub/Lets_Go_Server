//
// Created by jeremiah on 4/10/21.
//

#include "build_validate_match_document.h"
#include "specific_match_queries/specific_match_queries.h"

void generateSwipedYesOnEventMatchDocument(
        bsoncxx::builder::stream::document& query_doc,
        const bsoncxx::oid& match_account_oid,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        bool user_matches_with_everyone,
        const std::chrono::milliseconds& current_timestamp
) {

    query_doc
            << "_id" << match_account_oid;

    universalMatchConditions(
            query_doc,
            current_timestamp,
            user_age,
            min_age_range,
            max_age_range,
            user_gender,
            user_genders_to_match_with_array,
            user_matches_with_everyone,
            true
    );
}