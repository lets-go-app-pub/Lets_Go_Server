//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <chrono>

#include <bsoncxx/builder/basic/array.hpp>

#include "activity_struct.h"
#include "SetFields.pb.h"

enum class FindMatchesArrayNameEnum {
    other_users_matched_list,
    algorithm_matched_list
};

struct MatchInfoStruct {
    bsoncxx::stdx::optional<bsoncxx::document::value> match_user_account_doc;
    bsoncxx::document::value matching_element_doc = bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize;
    bsoncxx::builder::basic::array activity_statistics{};
    FindMatchesArrayNameEnum array_element_is_from;
    int number_swipes_after_extracted = 0;
};

//Primary struct to carry information through the findMatches().
struct UserAccountValues {

    const bsoncxx::oid user_account_oid;

    const double longitude = 0;
    const double latitude = 0;

    const std::chrono::milliseconds current_timestamp = std::chrono::milliseconds{0};
    //earliest a time frame for an activity can start and still be valid
    const std::chrono::milliseconds earliest_time_frame_start_timestamp = std::chrono::milliseconds{0};
    //end time of timeframe
    const std::chrono::milliseconds end_of_time_frame_timestamp = std::chrono::milliseconds{0};

    //Projected document from user account, used before algorithm (the source of this document must not be
    // deconstructed in order to keep this view alive).
    bsoncxx::document::view user_account_doc_view;

    //These vectors are copied from the user account document reflecting their respective lists.
    //NOTE: BOTH VECTORS ARE COPIED IN BACKWARDS TO MAKE OPERATIONS FASTER.
    //NOTE: other_users_swiped_yes_list is copied from user_account_doc_view, NOT from a cursor. This means it is ok
    // to store the bsoncxx::document::view inside the vector.
    std::vector<bsoncxx::document::view> other_users_swiped_yes_list;
    //NOTE: algorithm_matched_account_list uses a view_or_value so that the value can be properly stored if the
    // algorithm runs and adds to the vector.
    std::vector<bsoncxx::document::view_or_value> algorithm_matched_account_list;

    //The final matches that will be extracted and sent back (will be done outside the database transaction).
    std::vector<MatchInfoStruct> saved_matches;

    //Used for saving algorithm results for statistics.
    bsoncxx::builder::basic::array raw_algorithm_match_results{};

    bsoncxx::array::view genders_to_match_with_bson_array;
    //will be set to true if genders_to_match_with_bson_array is size 1 with only the element MATCH_EVERYONE_GENDER_RANGE_VALUE
    bool user_matches_with_everyone = false;

    int age = 0;
    int min_age_range = 0;
    int max_age_range = 0;

    int number_swipes_remaining = 0;

    std::string gender;

    //integer representing which list to draw from
    int draw_from_list_value = 0;

    int max_distance = 0;

    AlgorithmSearchOptions algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    //None of these are used if a match is just extracted from the user account and the algorithm is not run.
    std::vector<ActivityStruct> user_activities; //used to store the activities to use in matching algorithm
    std::vector<ActivityStruct> user_categories; //used to store the categories to use in matching algorithm
    std::set<bsoncxx::oid> restricted_accounts; //used to store the restricted accounts to use in matching algorithm (std::unordered_set doesn't play nice with bsoncxx::oid, bsoncxx::oid has no default constructor)

    //these values are used for saving statistics when the algorithm runs
    //stored in nanoseconds using CLOCK_REALTIME
    std::chrono::nanoseconds algorithm_run_time_ns{-1L};
    std::chrono::nanoseconds check_for_activities_run_time_ns{-1L};

    UserAccountValues() = default;

    UserAccountValues(
            const bsoncxx::oid& _user_account_oid,
            double _longitude,
            double _latitude,
            const std::chrono::milliseconds& _current_timestamp,
            const std::chrono::milliseconds& _earliest_time_frame_start_timestamp,
            const std::chrono::milliseconds& _end_of_time_frame_timestamp
    ) : user_account_oid(_user_account_oid),
        longitude(_longitude),
        latitude(_latitude),
        current_timestamp(_current_timestamp),
        earliest_time_frame_start_timestamp(_earliest_time_frame_start_timestamp),
        end_of_time_frame_timestamp(_end_of_time_frame_timestamp)
         {}

    void clearNonConstValues() {
        user_account_doc_view = bsoncxx::document::view();

        other_users_swiped_yes_list.clear();
        algorithm_matched_account_list.clear();

        saved_matches.clear();

        raw_algorithm_match_results = bsoncxx::builder::basic::array{};

        genders_to_match_with_bson_array = bsoncxx::array::view();

        user_matches_with_everyone = false;

        age = 0;
        min_age_range = 0;
        max_age_range = 0;

        number_swipes_remaining = 0;

        gender.clear();

        draw_from_list_value = 0;

        max_distance = 0;

        algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

        user_activities.clear();
        user_categories.clear();
        restricted_accounts.clear();

        algorithm_run_time_ns = std::chrono::nanoseconds{-1L};
        check_for_activities_run_time_ns = std::chrono::nanoseconds{-1L};
    }

};

enum class ValidateArrayElementReturnEnum {
    empty_list,
    unhandleable_error,
    extraction_error,
    success,
    no_longer_valid
};

enum AlgorithmReturnMessage {
    error,
    no_matches_found,
    match_algorithm_ran_recently,
    match_algorithm_returned_empty_recently,
    successfully_extracted
};

enum FinalAlgorithmResults {
    algorithm_did_not_run,
    algorithm_successful,
    set_no_matches_found,
    algorithm_cool_down_found
};

//Return value for algorithm functions. Default constructor is error.
struct AlgorithmReturnValues {

    explicit AlgorithmReturnValues(AlgorithmReturnMessage _returnMessage)
            : returnMessage(_returnMessage) {}

    AlgorithmReturnValues() = default;

    AlgorithmReturnValues(AlgorithmReturnMessage _returnMessage,
                          std::chrono::milliseconds _coolDownOnMatchAlgorithm)
            : returnMessage(_returnMessage),
              coolDownOnMatchAlgorithm(_coolDownOnMatchAlgorithm) {}

    const AlgorithmReturnMessage returnMessage = error;
    const std::chrono::milliseconds coolDownOnMatchAlgorithm = std::chrono::milliseconds{0};
};
