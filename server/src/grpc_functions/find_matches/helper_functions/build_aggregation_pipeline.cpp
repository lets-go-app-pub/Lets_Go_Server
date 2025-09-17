//
// Created by jeremiah on 9/1/22.
//

#include <bsoncxx/builder/stream/document.hpp>

#include "find_matches_helper_functions.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

bool buildQueryDocForAggregationPipeline(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        bsoncxx::builder::stream::document& query_doc,
        bool only_match_with_events
) {

    generateInitialQueryForPipeline(
            user_account_values,
            query_doc,
            only_match_with_events
    );

    //No need to check categories or activities if only matching with events.
    if (!only_match_with_events
        && !generateCategoriesActivitiesMatching(
                query_doc,
                user_account_values,
                user_accounts_collection)
        ) //if an error occurred
    {
            return false;
    }

    return true;
}