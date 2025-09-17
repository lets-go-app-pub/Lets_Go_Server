//
// Created by jeremiah on 8/30/22.
//

#include "generate_matching_users.h"

#include <random>
#include "general_values.h"
#include "generate_multiple_random_accounts.h"
#include "utility_general_functions.h"
#include "generate_random_account_info/generate_random_account_info.h"

void guaranteeGendersMatch(
        UserAccountDoc& one_account_doc,
        UserAccountDoc& two_account_doc
) {

    bool first_matches_second = false;
    for(const auto& x : one_account_doc.genders_range) {
        if(x == general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE
           || x == two_account_doc.gender) {
            first_matches_second = true;
        }
    }

    if(!first_matches_second) {
        if(one_account_doc.genders_range.size() == server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH) {
            one_account_doc.genders_range.pop_back();
        }
        one_account_doc.genders_range.emplace_back(two_account_doc.gender);
    }
}

void saveTestCategoryToUserAccountValuesVector(
        const TestCategory& test_category,
        std::vector<ActivityStruct>& activity_or_category_struct
        ) {
    std::vector<TimeFrameStruct> time_frame_structs;
    long total_time = 0;
    long previous_time = -1;

    for(const auto& time_frame : test_category.time_frames) {
        time_frame_structs.emplace_back(
                std::chrono::milliseconds {time_frame.time},
                time_frame.startStopValue
        );

        if(time_frame.startStopValue == -1 && previous_time > 0) {
            total_time += time_frame.time - previous_time;
        }

        previous_time = time_frame.time;
    }

    activity_or_category_struct.emplace_back(test_category.index_value, time_frame_structs);
    activity_or_category_struct.back().totalTime = std::chrono::milliseconds {total_time};
}

//NOTE: This will attempt minimal changes to first_account_doc. However, it will remove second_account
// doc from any lists that will prevent a match.
bool buildMatchingUserForPassedAccount(
        UserAccountDoc& first_account_doc,
        UserAccountDoc& second_account_doc,
        const bsoncxx::oid& first_account_oid,
        const bsoncxx::oid& second_account_oid
) {
    //make the second user a match for the first user

    //user age inside match age range
    second_account_doc.age_range.min = first_account_doc.age - 1;
    second_account_doc.age_range.max = first_account_doc.age + 1;

    const int age_range_difference = first_account_doc.age_range.max - first_account_doc.age_range.min;
    //match age inside user age range
    if(age_range_difference > 0) {
        second_account_doc.age = first_account_doc.age_range.min + (rand() % age_range_difference);
    } else {
        second_account_doc.age = first_account_doc.age_range.max;
    }

    bool return_val = generateBirthYearForPassedAge(
            second_account_doc.age,
            second_account_doc.birth_year,
            second_account_doc.birth_month,
            second_account_doc.birth_day_of_month,
            second_account_doc.birth_day_of_year
    );

    if(!return_val) {
        return false;
    }

    if(first_account_doc.genders_range.empty() || first_account_doc.gender.empty()) {
        return false;
    }

    if(first_account_doc.genders_range.size() == 1
        && first_account_doc.genders_range[0] == general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE) {
        if(second_account_doc.gender.empty()) {
            return false;
        }
    } else { //user not set to everyone
        second_account_doc.gender = first_account_doc.genders_range[0];
    }

    second_account_doc.genders_range.clear();

    if(rand() % 3 < 2) {
        second_account_doc.genders_range.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    } else {

        //must always have 1 gender (the matching gender)
        const size_t num_genders = (rand() % (server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH - 1)) + 1;

        second_account_doc.genders_range.emplace_back(first_account_doc.gender);

        //iterate until enough genders have been added
        while(second_account_doc.genders_range.size() < num_genders) {
            std::string gender;
            int random_gender = rand() % 3;
            switch (random_gender)
            {
                case 0:
                    gender = general_values::MALE_GENDER_VALUE;
                    break;
                case 1:
                    gender = general_values::FEMALE_GENDER_VALUE;
                    break;
                case 2:
                    gender = pickRandomGenderOther();
                    break;
                default:
                    gender = "";
                    break;
            }

            //if gender does not already exist inside vector, add it
            if(std::find(second_account_doc.genders_range.begin(), second_account_doc.genders_range.end(), gender) == second_account_doc.genders_range.end()) {
                second_account_doc.genders_range.emplace_back(gender);
            }
        }

        std::shuffle(second_account_doc.genders_range.begin(), second_account_doc.genders_range.end(), std::mt19937(std::random_device()()));
    }

    //user not inside match PREVIOUSLY_MATCHED_ACCOUNTS
    for(auto it = second_account_doc.previously_matched_accounts.begin(); it != second_account_doc.previously_matched_accounts.end(); ++it) {
        if(it->oid.to_string() == first_account_oid.to_string()) {
            second_account_doc.previously_matched_accounts.erase(it);
            break;
        }
    }

    //user not inside match OTHER_USERS_BLOCKED
    for(auto it = second_account_doc.other_users_blocked.begin(); it != second_account_doc.other_users_blocked.end(); ++it) {
        if(it->oid_string == first_account_oid.to_string()) {
            second_account_doc.other_users_blocked.erase(it);
            break;
        }
    }

    bool first_account_doc_changed = false;

    //user not inside match PREVIOUSLY_MATCHED_ACCOUNTS
    for(auto it = first_account_doc.previously_matched_accounts.begin(); it != first_account_doc.previously_matched_accounts.end(); ++it) {
        if(it->oid.to_string() == second_account_oid.to_string()) {
            first_account_doc_changed = true;
            first_account_doc.previously_matched_accounts.erase(it);
            break;
        }
    }

    //match not inside user OTHER_USERS_BLOCKED
    for(auto it = first_account_doc.other_users_blocked.begin(); it != first_account_doc.other_users_blocked.end(); ++it) {
        if(it->oid_string == second_account_oid.to_string()) {
            first_account_doc_changed = true;
            first_account_doc.other_users_blocked.erase(it);
            break;
        }
    }

    //match not inside user HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
    for(auto it = first_account_doc.has_been_extracted_accounts_list.begin(); it != first_account_doc.has_been_extracted_accounts_list.end(); ++it) {
        if(it->oid.to_string() == second_account_oid.to_string()) {
            first_account_doc_changed = true;
            first_account_doc.has_been_extracted_accounts_list.erase(it);
            break;
        }
    }

    //match not inside user ALGORITHM_MATCHED_ACCOUNTS_LIST
    for(auto it = first_account_doc.algorithm_matched_accounts_list.begin(); it != first_account_doc.algorithm_matched_accounts_list.end(); ++it) {
        if(it->oid.to_string() == second_account_oid.to_string()) {
            first_account_doc_changed = true;
            first_account_doc.algorithm_matched_accounts_list.erase(it);
            break;
        }
    }

    //match not inside user OTHER_USERS_MATCHED_ACCOUNTS_LIST
    for(auto it = first_account_doc.other_users_matched_accounts_list.begin(); it != first_account_doc.other_users_matched_accounts_list.end(); ++it) {
        if(it->oid.to_string() == second_account_oid.to_string()) {
            first_account_doc_changed = true;
            first_account_doc.other_users_matched_accounts_list.erase(it);
            break;
        }
    }

    //match not inside user OTHER_ACCOUNTS_MATCHED_WITH
    for(auto it = first_account_doc.other_accounts_matched_with.begin(); it != first_account_doc.other_accounts_matched_with.end(); ++it) {
        if(it->oid_string == second_account_oid.to_string()) {
            first_account_doc_changed = true;
            first_account_doc.other_accounts_matched_with.erase(it);
            break;
        }
    }

    //user inside match MAX_DISTANCE
    //match inside user MAX_DISTANCE
    if(first_account_doc.max_distance <= 0 || second_account_doc.max_distance <= 0) {
        return false;
    }

    second_account_doc.location = first_account_doc.location;

    //matching activities
    second_account_doc.categories.clear();
    std::copy(first_account_doc.categories.begin(), first_account_doc.categories.end(), std::back_inserter(second_account_doc.categories));

    //save to database
    if(first_account_doc_changed) {
        first_account_doc.setIntoCollection();
    }

    second_account_doc.setIntoCollection();

    return true;
}

bool generateMatchingUsers(
        UserAccountDoc& user_account_doc,
        UserAccountDoc& match_account_doc,
        bsoncxx::oid& user_account_oid,
        bsoncxx::oid& match_account_oid,
        UserAccountValues& user_account_values,
        bsoncxx::builder::basic::array& user_gender_range_builder
) {
    user_account_oid = insertRandomAccounts(1, 0);
    match_account_oid = insertRandomAccounts(1, 0);

    //values can be used, so must be set early
    const_cast<std::chrono::milliseconds&>(user_account_values.current_timestamp) = getCurrentTimestamp();
    const_cast<std::chrono::milliseconds&>(user_account_values.end_of_time_frame_timestamp) = user_account_values.current_timestamp + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES;
    const_cast<std::chrono::milliseconds&>(user_account_values.earliest_time_frame_start_timestamp) = user_account_values.current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED;

    user_account_doc.getFromCollection(user_account_oid);
    match_account_doc.getFromCollection(match_account_oid);

    //make the users a match

    //user age inside match age range
    match_account_doc.age_range.min = user_account_doc.age - 1;
    match_account_doc.age_range.max = user_account_doc.age + 1;

    //match age inside user age range
    user_account_doc.age_range.min = match_account_doc.age - 1;
    user_account_doc.age_range.max = match_account_doc.age + 1;

    if(user_account_doc.genders_range.empty() || match_account_doc.genders_range.empty()) {
        return false;
    }

    //match gender inside user gender range
    guaranteeGendersMatch(
            user_account_doc,
            match_account_doc
    );

    //user gender inside match gender range
    guaranteeGendersMatch(
            match_account_doc,
            user_account_doc
    );

    //user not inside match PREVIOUSLY_MATCHED_ACCOUNTS
    for(auto it = match_account_doc.previously_matched_accounts.begin(); it != match_account_doc.previously_matched_accounts.end(); ++it) {
        if(it->oid.to_string() == user_account_oid.to_string()) {
            match_account_doc.previously_matched_accounts.erase(it);
            break;
        }
    }

    //user not inside match OTHER_USERS_BLOCKED
    for(auto it = match_account_doc.other_users_blocked.begin(); it != match_account_doc.other_users_blocked.end(); ++it) {
        if(it->oid_string == user_account_oid.to_string()) {
            match_account_doc.other_users_blocked.erase(it);
            break;
        }
    }

    //user inside match MAX_DISTANCE
    //match inside user MAX_DISTANCE
    if(user_account_doc.max_distance <= 0 || match_account_doc.max_distance <= 0) {
        return false;
    }

    match_account_doc.location = user_account_doc.location;

    //matching activity
    user_account_doc.categories.clear();
    match_account_doc.categories.clear();

    std::vector<TestTimeframe> anytime_timeframe{
            TestTimeframe{
                    user_account_values.earliest_time_frame_start_timestamp.count() + 1,
                    1
            },
            TestTimeframe{
                    user_account_values.end_of_time_frame_timestamp.count(),
                    -1
            }
    };

    TestCategory activity_match{
        AccountCategoryType::ACTIVITY_TYPE,
        0,
        anytime_timeframe
    };
    TestCategory category_match{
        AccountCategoryType::CATEGORY_TYPE,
        0,
        anytime_timeframe
    };

    user_account_doc.categories.emplace_back(activity_match);
    user_account_doc.categories.emplace_back(category_match);

    match_account_doc.categories.emplace_back(activity_match);
    match_account_doc.categories.emplace_back(category_match);

    //save to database
    user_account_doc.setIntoCollection();
    match_account_doc.setIntoCollection();

    //set up UserAccountValues
    user_account_values.age = user_account_doc.age;
    user_account_values.min_age_range = user_account_doc.age_range.min;
    user_account_values.max_age_range = user_account_doc.age_range.max;

    user_account_values.number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES;

    user_account_values.gender = user_account_doc.gender;

    for(const auto& gender : user_account_doc.genders_range) {
        user_gender_range_builder.append(gender);
    }

    user_account_values.genders_to_match_with_bson_array = user_gender_range_builder.view();

    if(user_account_doc.genders_range.size()==1 && user_account_doc.genders_range[0] == general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE) {
        user_account_values.user_matches_with_everyone = true;
    } else {
        user_account_values.user_matches_with_everyone = false;
    }

    const_cast<bsoncxx::oid&>(user_account_values.user_account_oid) = user_account_doc.current_object_oid;

    const_cast<double&>(user_account_values.longitude) = user_account_doc.location.longitude;
    const_cast<double&>(user_account_values.latitude) = user_account_doc.location.latitude;
    const_cast<int&>(user_account_values.max_distance) = user_account_doc.max_distance;

    saveTestCategoryToUserAccountValuesVector(
            category_match,
            user_account_values.user_categories
    );

    saveTestCategoryToUserAccountValuesVector(
            activity_match,
            user_account_values.user_activities
    );

    user_account_values.algorithm_search_options = AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY;

    //make sure user account cannot match
    user_account_values.restricted_accounts.insert(user_account_oid);

    return true;
}