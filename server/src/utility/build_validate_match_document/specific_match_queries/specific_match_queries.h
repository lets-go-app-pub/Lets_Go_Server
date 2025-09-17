//
// Created by jeremiah on 4/10/21.
//
#pragma once

#include <bsoncxx/builder/stream/document.hpp>
#include <AccountCategoryEnum.grpc.pb.h>

//generate the match conditions that all match filters use
void universalMatchConditions(
        bsoncxx::builder::stream::document& query_doc,
        const std::chrono::milliseconds& current_timestamp,
        int user_age,
        int min_age_range,
        int max_age_range,
        const std::string& user_gender,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        bool user_matches_with_everyone,
        bool only_match_with_events
);

//make sure other account activated
void matchingIsActivatedOnMatchAccount(
        bsoncxx::builder::stream::document& query_doc
);

//check if user age is in age range of other
void userAgeInsideMatchAgeRange(
        bsoncxx::builder::stream::document& query_doc,
        int user_age
);

//check age of other account matches this account (this shouldn't technically be needed
// because if the user age range changes the vector will be wiped and they can't change age manually, but it is indexed anyway)
void matchAgeInsideUserAgeRangeOrEvent(
        bsoncxx::builder::stream::document& query_doc,
        int min_age_range,
        int max_age_range,
        bool only_match_with_events
);

//make sure the user gender is inside the matching gender range
void userGenderInsideMatchGenderRange(
        bsoncxx::builder::stream::document& query_doc,
        const std::string& user_gender
);

//check if match gender is inside user gender range
void matchGenderInsideUserGenderRangeOrEvent(
        bsoncxx::builder::stream::document& query_doc,
        const bsoncxx::array::view& user_genders_to_match_with_array,
        bool user_matches_with_everyone,
        bool only_match_with_events
);

//check if it is a user account OR the event is not expired
void eventExpirationNotReached(
        bsoncxx::builder::stream::document& query_doc,
        const std::chrono::milliseconds& current_timestamp
);

//make sure activities of other account have not been updated
void activitiesOfMatchHaveNotBeenUpdated(
        bsoncxx::builder::stream::document& matchDoc,
        const std::chrono::milliseconds& timeMatchOccurred
);

//at least one activity or category of other account needs to match at least one from this account
void atLeastOneCategoryMatches(
        bsoncxx::builder::stream::document& matchDoc,
        const bsoncxx::array::view& userActivitiesMongoDBArray,
        AccountCategoryType categoryTypeToMatch
);

//make sure this account is not on other accounts blocked list
void userNotOnMatchBlockedList(
        bsoncxx::builder::stream::document& matchDoc,
        const std::string& userAccountOIDStr
);

//check if this user was not 'recently' a match for the other account
void userNotRecentMatchForMatch(
        bsoncxx::builder::stream::document& matchDoc,
        const bsoncxx::oid& userAccountOID,
        const std::chrono::milliseconds& currentTimestamp
);

//check if distance is still in distance range of other
void distanceStillInMatchMaxDistance(
        bsoncxx::builder::stream::document& matchDoc,
        const double& matchDistance
);