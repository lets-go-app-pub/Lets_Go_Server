//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/database.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "find_matches_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "matching_algorithm.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

AlgorithmReturnValues beginAlgorithmCheckCoolDown(
        UserAccountValues& user_account_values,
        mongocxx::client_session* session,
        mongocxx::collection& user_accounts_collection
) {

    std::chrono::milliseconds last_time_match_algorithm_ran;
    std::chrono::milliseconds last_time_empty_match_returned;

    try {
        last_time_match_algorithm_ran = extractFromBsoncxx_k_date(
                user_account_values.user_account_doc_view,
                user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN
        ).value;

        last_time_empty_match_returned = extractFromBsoncxx_k_date(
                user_account_values.user_account_doc_view,
                user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED
        ).value;
    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return {};
    }

    std::chrono::milliseconds time_algorithm_comes_off_short_cool_down =
            user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS;
    std::chrono::milliseconds time_algorithm_comes_off_no_matches_found_cool_down =
            user_account_values.current_timestamp -
            matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND;

    //sets the cool down remaining in seconds to the response
    if (last_time_empty_match_returned >
        time_algorithm_comes_off_no_matches_found_cool_down) { //if empty match puts algorithm on cool down

        return {
                match_algorithm_returned_empty_recently,
                last_time_empty_match_returned - time_algorithm_comes_off_no_matches_found_cool_down
        };
    } else if (last_time_match_algorithm_ran >
               time_algorithm_comes_off_short_cool_down) { //if algorithm is on cool down from running recently

        return {
                match_algorithm_ran_recently,
                last_time_match_algorithm_ran - time_algorithm_comes_off_short_cool_down
        };
    }

    //find matches for passed account
    return organizeUserInfoForAlgorithm(
            user_account_values,
            session,
            user_accounts_collection
            );
}


