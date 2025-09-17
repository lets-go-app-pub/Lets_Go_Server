//
// Created by jeremiah on 9/5/22.
//

#include <user_account_keys.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <utility_general_functions.h>
#include <gtest/gtest.h>
#include "account_objects.h"
#include "utility_find_matches_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void MatchingElement::convertToDocument(bsoncxx::builder::stream::document& document_result) const  {

    if(activity_statistics != nullptr) {
        bsoncxx::array::view arr_view = activity_statistics->view();

        document_result
                << bsoncxx::builder::concatenate(
                        buildMatchedAccountDoc<true>(
                                oid,
                                point_value,
                                distance,
                                expiration_time.value,
                                match_timestamp.value,
                                from_match_algorithm_list,
                                saved_statistics_oid,
                                &arr_view
                        )
                );
    } else {
        document_result
                << bsoncxx::builder::concatenate(
                        buildMatchedAccountDoc<false>(
                                oid,
                                point_value,
                                distance,
                                expiration_time.value,
                                match_timestamp.value,
                                from_match_algorithm_list,
                                saved_statistics_oid
                        )
                );
    }
}

void MatchingElement::generateRandomValues() {
        oid = bsoncxx::oid{};
        point_value = rand() % 100 + (double)(rand() % 1000)/(1000.0);
        distance = rand() % 100 + (double)(rand() % 1000)/(1000.0);
        expiration_time = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % getCurrentTimestamp().count()}};
        match_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % getCurrentTimestamp().count()}};
        from_match_algorithm_list = rand() % 2;
        saved_statistics_oid = bsoncxx::oid{};
}

