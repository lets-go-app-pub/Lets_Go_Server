//
// Created by jeremiah on 4/10/21.
//
#pragma once

#include <bsoncxx/builder/stream/document.hpp>

//generate match query for algorithm aggregation pipeline
void generatePipelineMatchDocument(
        bsoncxx::builder::stream::document& query_doc,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        bool user_matches_with_everyone,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        bool only_match_with_events
);

//generate match query for validating an array element is still true
void generateValidateArrayElementMatchDocument(
        bsoncxx::builder::stream::document& query_doc,
        const bsoncxx::oid& match_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        bool user_matches_with_everyone
);

//generate match query for when user swipes yes on another user
void generateSwipedYesOnUserMatchDocument(
        bsoncxx::builder::stream::document& query_doc,
        const bsoncxx::oid& match_account_oid,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        bool user_matches_with_everyone,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp,
        bool element_from_match_algorithm_list_element
);

//generate match query for when user swipes yes on an event
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
);