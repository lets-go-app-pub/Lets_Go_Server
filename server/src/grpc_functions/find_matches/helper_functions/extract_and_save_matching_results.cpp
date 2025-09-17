//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/database.hpp>

#include <bsoncxx/builder/stream/document.hpp>

#include "find_matches_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "user_account_keys.h"
#include "algorithm_pipeline_field_names.h"
#include "extract_data_from_bsoncxx.h"
#include "utility_find_matches_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool extractAndSaveMatchingResults(
        UserAccountValues& user_account_values,
        std::optional<mongocxx::cursor>& matching_accounts_cursor
) {

    user_account_values.algorithm_matched_account_list.clear();
    user_account_values.raw_algorithm_match_results.clear();
    bsoncxx::builder::basic::array formatted_match_documents{};

    const int algorithm_vector_size = user_account_values.algorithm_matched_account_list.size();

    try {

        //matchingAccounts was already checked to exist
        for (const auto& match_result_doc : *matching_accounts_cursor) {

            //I think the builder will automatically copy the document view here, even though the cursor is invalidating it
            user_account_values.raw_algorithm_match_results.append(match_result_doc);

            //both matching and verified accounts have to be open
            //if I store it in matching, will it make the algorithm take longer? I don't think so because the fields are unique they are probably indexed
            //if I store it in verified it will clog up the verified account and make it significantly less human-readable

            const bsoncxx::oid account_oid = extractFromBsoncxx_k_oid(
                    match_result_doc,
                    "_id"
            );

            const double point_value = extractFromBsoncxx_k_double(
                    match_result_doc,
                    algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR
            );

            const double match_distance = extractFromBsoncxx_k_double(
                    match_result_doc,
                    algorithm_pipeline_field_names::MONGO_PIPELINE_DISTANCE_KEY
            );

            const std::chrono::milliseconds expiration_time =
                    std::chrono::milliseconds{
                            extractFromBsoncxx_k_int64(
                                    match_result_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                            )
                    };

            const bsoncxx::document::view match_statistics_doc = extractFromBsoncxx_k_document(
                    match_result_doc,
                    algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_STATISTICS_VAR
            );

            const bsoncxx::array::view activity_statistics_array = extractFromBsoncxx_k_array(
                    match_statistics_doc,
                    user_account_keys::accounts_list::ACTIVITY_STATISTICS
            );

            bsoncxx::oid saved_statistics_oid{};

            user_account_values.algorithm_matched_account_list.emplace_back(
                    buildMatchedAccountDoc<true>(
                            account_oid,
                            point_value,
                            match_distance,
                            expiration_time,
                            user_account_values.current_timestamp,
                            true,
                            saved_statistics_oid,
                            &activity_statistics_array
                    )
            );
        }
    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return false;
    }

    if(!user_account_values.algorithm_matched_account_list.empty()) {
        //algorithm_matched_account_list is meant to be stored in reverse, must reverse this in order to
        // keep consistent with this requirement.
        std::reverse(
                user_account_values.algorithm_matched_account_list.begin() + algorithm_vector_size,
                user_account_values.algorithm_matched_account_list.end()
        );
    }

    return true;
}