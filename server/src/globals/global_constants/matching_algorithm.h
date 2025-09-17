//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <chrono>
#include "server_parameter_restrictions.h"
#include <cmath>

namespace matching_algorithm {

    //Parameters
    //the total number of matches (the best matches) the algorithm returns
    inline const int TOTAL_NUMBER_DOCS_RETURNED = 20;

    //every time the counter VERIFIED_MATCH_LIST_TO_DRAW_FROM_KEY hits this number, the VERIFIED_OTHER_USERS_MATCHED_ACCOUNTS_LIST_KEY will be drawn from and the number will be reset to 0
    // if VERIFIED_MATCH_LIST_TO_DRAW_FROM_KEY is less than this number than VERIFIED_ALGORITHM_MATCHED_ACCOUNTS_LIST_KEY will be drawn from and VERIFIED_MATCH_LIST_TO_DRAW_FROM_KEY will be incremented
    // ex: if NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER is 2, 0 and 1 will draw from the VERIFIED_ALGORITHM_MATCHED_ACCOUNTS_LIST_KEY and 2 or higher will draw from the
    // VERIFIED_OTHER_USERS_MATCHED_ACCOUNTS_LIST_KEY then it will be set to 0 and start over
    inline const int NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER = 2;

    //the amount of time (in milliseconds) that will be subtracted from expired time before comparison is made
    //ex: currentTime = 400, expiredTime = 430, variable = 45; currentTime+variable > expiredTime; so this match will be deleted and not returned to the user
    //This is also used to calculate event expiration time. If changing it, the currently stored events will be off a little from the algorithm. See create_event.cpp on the event_expiration_time variable for more info.
    inline const std::chrono::milliseconds TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED = std::chrono::milliseconds{5L * 60L * 1000L};

    inline const int ARRAY_ELEMENTS_EXTRACTED_BEFORE_GIVE_UP = TOTAL_NUMBER_DOCS_RETURNED * 3; //the number of times each request will attempt to access an element before it gives up
    inline const std::chrono::milliseconds TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND = std::chrono::milliseconds{10L * 60L * 1000L}; //the time in milliseconds between algorithm runs if no matches were found
    inline const std::chrono::milliseconds TIME_BETWEEN_ALGORITHM_RUNS = std::chrono::milliseconds{1L * 1000L}; //the time in seconds between algorithm runs; this is to prevent spamming
    inline const std::chrono::milliseconds TIME_BETWEEN_ALGORITHM_MATCH_INVALIDATION = std::chrono::milliseconds{7L * 24L * 60L * 60L * 1000L}; //the 'algorithm match' list will be cleared regardless if all elements are used or not when this timer expires
    inline const std::chrono::milliseconds MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE = std::chrono::milliseconds{24L * 60L * 60L * 1000L}; //after a match has been 'extracted' from the device (either from the 'algorithm match' list or the 'other users match' list) this is the amount of time it will stay cached on the android device before it is removed

    inline const std::chrono::milliseconds TIME_BETWEEN_SWIPES_UPDATED = std::chrono::milliseconds{4L * 60L * 60L * 1000L}; //the user will get MAXIMUM_NUMBER_SWIPES swipes during this time (in milliseconds), then swipes will be reset; NOTE: DO NOT SET TO 0
    inline const int MAXIMUM_NUMBER_SWIPES = 1000; //number of swipes 'allowed' during TIME_BETWEEN_SWIPES_UPDATED

    inline const int MAXIMUM_NUMBER_RESPONSE_MESSAGES = 5; //maximum number of responses the findMatches.proto-FindMatchesRequest-number_messages can hold, if the request is above MAXIMUM_NUMBER_RESPONSE_MESSAGES it will be set to MAXIMUM_NUMBER_RESPONSE_MESSAGES

    //see user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK for more details
    inline const std::chrono::milliseconds TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS = std::chrono::milliseconds{60L * 1000L};

    //this is the time between previously matched accounts (in seconds)
    // it works alongside PREVIOUSLY_MATCHED_WEIGHT after this amount of time has passed there will be no
    // points subtracted from the total points of the account, however each match will extend this linearly
    // for example after 2 times the accounts have matched it will be 2*PREVIOUSLY_MATCHED_FALLOFF_TIME after 3 it will be 3*PREVIOUSLY_MATCHED_FALLOFF_TIME etc.
    //works with TIME_BETWEEN_SAME_ACCOUNT_MATCHES below
    inline const std::chrono::milliseconds PREVIOUSLY_MATCHED_FALLOFF_TIME =  std::chrono::milliseconds{6L * 60L * 60L * 1000L};

    //the amount of time in milliseconds before a user can match with a specific account again (works with PREVIOUSLY_MATCHED_FALLOFF_TIME above)
    //This is used as a parameter for checking if a match is valid, PREVIOUSLY_MATCHED_FALLOFF_TIME is used directly by the algorithm to calculate points.
    inline const std::chrono::milliseconds TIME_BETWEEN_SAME_ACCOUNT_MATCHES = std::chrono::milliseconds{6L * 60L * 60L * 1000L};

    //The maximum amount of time in milliseconds before the algorithm will time out and cancel itself. Ideally want to keep this
    // less than user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK so that there will never be 2 algorithms running for 1 user at a
    // time.
    inline const std::chrono::milliseconds TIMEOUT_FOR_ALGORITHM_AGGREGATION = std::chrono::milliseconds{45L * 1000L};

    //*********************** WEIGHTS ***********************

    //this point value is assigned for an activity match, each activity a user matches with is worth this amount of points
    inline const int ACTIVITY_MATCH_WEIGHT = 10000;
    //this point value is assigned for a category match, each category a user matches with is worth this amount of points
    inline const int CATEGORIES_MATCH_WEIGHT = ACTIVITY_MATCH_WEIGHT/100;

    //all points are added at the end of calculating them to find the final 'score' of the match
    // this is the weight for the percent of overlapping time between the user account and the match account
    // the algorithm works by finding the greatest of total user time and match time (sum of all time frames)
    // then it will divide the total overlapping time by the time selected and multiply it by the weight for the point score
    // formula: overlapPoints = overlapping/(total time)*(OVERLAPPING_ACTIVITY_TIMES_WEIGHT + SHORT_TIMEFRAME_OVERLAP_WEIGHT(if short timeframe))
    inline const int OVERLAPPING_ACTIVITY_TIMES_WEIGHT = ACTIVITY_MATCH_WEIGHT * server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT;
    inline const int OVERLAPPING_CATEGORY_TIMES_WEIGHT = OVERLAPPING_ACTIVITY_TIMES_WEIGHT/100;

    //this weight measures the time between user and match time frames and calculates a score based on it
    // the algorithm works by looking at each 'between' time (time between user and match time frames) and if it is less than MAX_BETWEEN_TIME_WEIGHT_HELPER
    // then it will divide the between time by MAX_BETWEEN_TIME_WEIGHT_HELPER and subtract that value from 1 for the point score
    // as a note this is calculated individually for EACH between time so if there are 3 close time frames the formula will run 3 times and sum the values'
    // formula: if(betweenTime < MAX_BETWEEN_TIME_WEIGHT_HELPER) betweenPoints = BETWEEN_TIMES_WEIGHT*(1- betweenTime/MAX_BETWEEN_TIME_WEIGHT_HELPER)
    inline const int BETWEEN_ACTIVITY_TIMES_WEIGHT = 1000;
    inline const int BETWEEN_CATEGORY_TIMES_WEIGHT = BETWEEN_ACTIVITY_TIMES_WEIGHT/100;

    //these variables add points for short overlapping timeframes, this is because of a situation
    // say userA has Golf 5:00-6:00 and Soccer for ANYTIME
    // match1 has Golf 5:00-6:00
    // match2 has Soccer for ANYTIME
    // ideally match1 is a better match for the user, however with only calculating overlaps
    // match1 and match2 are equal, the below weight fixes this problem by acting as a tiebreaker of sorts
    // (1- timeStats.totalOverlapTime/TOTAL_TIME_FRAME) * SHORT_TIMEFRAME_OVERLAP_WEIGHT **/
    inline const int SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT = 1000;
    inline const int SHORT_TIMEFRAME_CATEGORY_OVERLAP_WEIGHT = SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT/100;

    //helper for BETWEEN_TIMES_WEIGHT; When calculating final points this value is used as a denominator, do not make it 0.
    inline const std::chrono::milliseconds MAX_BETWEEN_TIME_WEIGHT_HELPER = std::chrono::milliseconds{2L * 60L * 60L * 1000L};

    //this weight is subtracted from the total point value
    // the inactive accounts is higher the longer the match has been inactive
    // the value is calculated by multiplying the weight by the difference between the current time and the last active time of the account (LAST_TIME_FIND_MATCHES_RAN)
    // the times are in milliseconds (or mongoDB Date type)
    // formula: INACTIVE_ACCOUNT_WEIGHT*(realWorldTime-matchLastActiveTime)
    //this will make 1 matching activity of ANYTIME falloff every 11 days
    inline const double INACTIVE_ACCOUNT_WEIGHT = static_cast<double>(ACTIVITY_MATCH_WEIGHT + OVERLAPPING_ACTIVITY_TIMES_WEIGHT)/static_cast<double>(std::chrono::milliseconds{12L * 24L * 60L * 60L * 1000L}.count());

    // this weight will calculate the points subtracted from the total score based on how long since a match and how long between matches
    // the weight itself is the max amount of points that can be subtracted from a previously matched account
    // formula: PREVIOUSLY_MATCHED_WEIGHT * (1 - (timeSinceMatched/PREVIOUSLY_MATCHED_FALLOFF_TIME))
    inline const int PREVIOUSLY_MATCHED_WEIGHT = (SHORT_TIMEFRAME_ACTIVITY_OVERLAP_WEIGHT + OVERLAPPING_ACTIVITY_TIMES_WEIGHT + ACTIVITY_MATCH_WEIGHT) * server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT; //this is the maximum amount of points possible

}