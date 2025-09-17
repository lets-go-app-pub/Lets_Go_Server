//
// Created by jeremiah on 9/3/22.
//

#pragma once

#include <bsoncxx/builder/stream/document.hpp>
#include "user_account_keys.h"
#include "find_matches_helper_objects.h"
#include "FindMatches.grpc.pb.h"
#include <utility_chat_functions.h>

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

template <bool save_activity_statistics_array>
inline bsoncxx::document::value buildMatchedAccountDoc(
        const bsoncxx::oid& account_oid,
        const double point_value,
        const double match_distance,
        const std::chrono::milliseconds& expiration_time,
        const std::chrono::milliseconds& match_timestamp,
        const bool from_algorithm_match_list,
        const bsoncxx::oid saved_statistics_oid,
        const bsoncxx::array::view* const activity_statistics_array = nullptr
) {
    document builder{};

    builder
            << user_account_keys::accounts_list::OID << bsoncxx::types::b_oid{account_oid}
            << user_account_keys::accounts_list::POINT_VALUE << bsoncxx::types::b_double{point_value}
            << user_account_keys::accounts_list::DISTANCE << bsoncxx::types::b_double{match_distance}
            << user_account_keys::accounts_list::EXPIRATION_TIME << bsoncxx::types::b_date{expiration_time}
            << user_account_keys::accounts_list::MATCH_TIMESTAMP << bsoncxx::types::b_date{match_timestamp}
            << user_account_keys::accounts_list::FROM_MATCH_ALGORITHM_LIST << bsoncxx::types::b_bool{from_algorithm_match_list}
            << user_account_keys::accounts_list::SAVED_STATISTICS_OID << saved_statistics_oid;

    if(save_activity_statistics_array && activity_statistics_array != nullptr) {
        builder
            << user_account_keys::accounts_list::ACTIVITY_STATISTICS << *activity_statistics_array;
    }

    return builder
            << finalize;
}

inline bool setupSuccessfulSingleMatchMessage(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        const bsoncxx::document::view& match_account_document,
        const bsoncxx::oid match_account_oid,
        const double distance_at_match_time,
        const double match_point_value,
        const std::chrono::milliseconds& expiration_time,
        const std::chrono::milliseconds& swipes_time_before_reset,
        const int number_swipes_after_extracted,
        const std::chrono::milliseconds& current_timestamp,
        const FindMatchesArrayNameEnum array_element_from,
        findmatches::SingleMatchMessage* match_response_message
        ) {

    auto member_info_ptr = match_response_message->mutable_member_info();
    member_info_ptr->set_account_oid(match_account_oid.to_string());
    member_info_ptr->set_distance(distance_at_match_time);

    if (!saveUserInfoToMemberSharedInfoMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            match_account_document,
            match_account_oid,
            member_info_ptr,
            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
            current_timestamp)
            ) {
        return false;
    }

    match_response_message->set_point_value(match_point_value);
    match_response_message->set_expiration_time(expiration_time.count());

    match_response_message->set_swipes_time_before_reset(swipes_time_before_reset.count());
    match_response_message->set_swipes_remaining(number_swipes_after_extracted);
    match_response_message->set_timestamp(current_timestamp.count());
    match_response_message->set_other_user_match(array_element_from == FindMatchesArrayNameEnum::other_users_matched_list);

    return true;
}