//
// Created by jeremiah on 3/29/21.
//
#pragma once

#include <string>
#include <functional>

#include <mongocxx/collection.hpp>
#include <bsoncxx/document/value.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <StatusEnum.grpc.pb.h>

#include "user_account_keys.h"
#include "matching_algorithm.h"

//Will search to see if either OTHER_USERS_MATCHED_ACCOUNTS_LIST or ALGORITHM_MATCHED_ACCOUNTS_LIST
// have any valid matching currently inside them after a matching parameter has changed.
//user_account_view is expected to have OTHER_USERS_MATCHED_ACCOUNTS_LIST.OID and ALGORITHM_MATCHED_ACCOUNTS_LIST.OID projected.
//match_parameters_to_query is expected to be formatted for a filter
//fields_to_update_document is expected to be formatted for an add_fields pipeline
//final_user_account_doc will be set on return == true
//Function returns true if successful and false if an error occurs.
bool removeInvalidElementsForUpdatedMatchingParameters(
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::document::view& user_account_view,
        mongocxx::collection& user_accounts_collection,
        mongocxx::stdx::optional<bsoncxx::document::value>& final_user_account_doc,
        const bsoncxx::document::view& match_parameters_to_query,
        const bsoncxx::document::view& fields_to_update_document,
        const bsoncxx::document::view& fields_to_project = bsoncxx::document::view{}
);

inline bsoncxx::document::value buildFilterToClearMatchingElementInfo(
        const std::string& array_key,
        const bsoncxx::array::view& valid_oid_list
        ) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    return document {}
            << "$filter" << open_document
                << "input" << std::string("$").append(array_key)
                << "as" << "acc"
                << "cond" << open_document
                    << "$in" << open_array
                        << std::string("$$acc.").append(user_account_keys::accounts_list::OID)
                        << valid_oid_list
                    << close_array
                << close_document
            << close_document
        << finalize;
}

inline bsoncxx::document::value buildFilterToClearMatchingElementInfoByDistance(
        const std::string& array_key,
        const int max_distance
        ) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::document;
    using bsoncxx::builder::stream::finalize;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    return document {}
            << "$filter" << open_document
                << "input" << std::string("$").append(array_key)
                << "as" << "acc"
                << "cond" << open_document
                    << "$gte" << open_array
                        << std::string("$$acc.").append(user_account_keys::accounts_list::DISTANCE)
                        << max_distance
                    << close_array
                << close_document
            << close_document
        << finalize;
}

inline void appendDocumentToClearMatchingInfoAggregation(
        bsoncxx::builder::stream::document& builder,
        const bsoncxx::array::view& valid_oid_list
        ) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::open_array;

    builder
        << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_array << close_array
        << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << buildFilterToClearMatchingElementInfo(user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST, valid_oid_list)
        << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << buildFilterToClearMatchingElementInfo(user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST, valid_oid_list)
        << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
}

inline void appendDocumentToClearByDistanceAggregation(
        bsoncxx::builder::stream::document& builder,
        const int max_distance
) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::open_array;

    builder
            << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_array << close_array
            << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << buildFilterToClearMatchingElementInfoByDistance(user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST, max_distance)
            << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << buildFilterToClearMatchingElementInfoByDistance(user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST, max_distance)
            << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
}

//Set remove_all_values to true to clear OTHER_USERS_MATCHED_ACCOUNTS_LIST. Set as false to leave 'decent' matches.
template <bool remove_all_values>
inline void appendDocumentToClearMatchingInfoUpdate(bsoncxx::builder::stream::document& builder) {

    using bsoncxx::builder::stream::close_array;
    using bsoncxx::builder::stream::close_document;
    using bsoncxx::builder::stream::open_array;
    using bsoncxx::builder::stream::open_document;

    bsoncxx::builder::stream::document set_doc_builder;

    if(remove_all_values) {
        set_doc_builder
                << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_array << close_array;
    }

    set_doc_builder
            << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_array << close_array
            << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << open_array << close_array
            << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << bsoncxx::types::b_date{std::chrono::milliseconds{-1}};

    builder
        << "$set" << set_doc_builder;

    //HAS_BEEN_EXTRACTED_ACCOUNTS_LIST is cleared because the extracted list checked when creating the
    // 'restricted oid list' to run the algorithm with. If this is still set then none of the extracted
    // accounts will be able to be found again until expiration time is reached naturally.
    if(!remove_all_values) {
        //Leave other users that are 'decent' matches (about one activity with some error for previous matches or inactive).
        builder
            << "$pull" << open_document
                << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_document
                    << user_account_keys::accounts_list::POINT_VALUE << open_document
                        << "$lt" << (double)matching_algorithm::ACTIVITY_MATCH_WEIGHT * 0.7
                    << close_document
                << close_document
            << close_document;
    }
}