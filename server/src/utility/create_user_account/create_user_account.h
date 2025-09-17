//
// Created by jeremiah on 3/3/23.
//

#pragma once

#include <optional>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/client_session.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include "AccountLoginTypeEnum.grpc.pb.h"
#include "UserAccountType.grpc.pb.h"
#include "RequestUserAccountInfo.grpc.pb.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "matching_algorithm.h"
#include "utility_general_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//These values are set by default to the ACCOUNT_TYPE user values.
struct AccountCreationParameterPack {

    //This will auto generate an oid. The oid will then be used as the user account "_id" value.
    const bsoncxx::oid user_account_oid{};

    const std::string installation_id;
    const std::chrono::milliseconds last_verified_time;
    const std::string account_id;

    const UserAccountType account_type;

    const std::string phone_number;
    const std::string bio;
    const std::string city;
    const std::string gender;
    const int age;

    const bsoncxx::builder::basic::array pictures{};

    const bsoncxx::builder::basic::array categories{};

    const std::optional<EventValues> event_values{};

    const bsoncxx::types::b_date event_expiration_time;

    const std::chrono::milliseconds last_time_find_matches_ran;

    const int min_age_range;
    const int max_age_range;
    const bsoncxx::builder::basic::array genders_range{};
    const int max_distance;

    const std::chrono::milliseconds time_sms_can_be_sent_again;
    const int number_swipes_remaining;
    long long swipes_last_updated_time;

    const std::chrono::milliseconds time_email_can_be_sent_again;

    AccountCreationParameterPack() = delete;

    //Used for creating an event account.
    AccountCreationParameterPack(
            const bsoncxx::oid event_account_oid,
            UserAccountType _account_type,
            std::string _bio,
            std::string _city,
            bsoncxx::builder::basic::array _pictures,
            bsoncxx::builder::basic::array _categories,
            EventValues _event_values,
            bsoncxx::types::b_date _event_expiration_time,
            int _min_age_range,
            int _max_age_range,
            bsoncxx::builder::basic::array _genders_range
    ) :
            user_account_oid(event_account_oid),
            installation_id(generateUUID()),
            last_verified_time(std::chrono::milliseconds{-1}),
            account_type(_account_type),
            phone_number(bsoncxx::oid{}.to_string()),
            bio(std::move(_bio)),
            city(std::move(_city)),
            gender(general_values::EVENT_GENDER_VALUE),
            age(general_values::EVENT_AGE_VALUE),
            pictures(std::move(_pictures)),
            categories(std::move(_categories)),
            event_values(std::move(_event_values)),
            event_expiration_time(_event_expiration_time),
            last_time_find_matches_ran(general_values::EVENT_DEFAULT_LAST_TIME_FIND_MATCHES_RAN),
            min_age_range(_min_age_range),
            max_age_range(_max_age_range),
            genders_range(std::move(_genders_range)),
            max_distance(server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE),
            time_sms_can_be_sent_again(bsoncxx::types::b_date{std::chrono::milliseconds{-1}}),
            number_swipes_remaining(matching_algorithm::MAXIMUM_NUMBER_SWIPES),
            swipes_last_updated_time(bsoncxx::types::b_date{std::chrono::milliseconds{-1}}),
            time_email_can_be_sent_again(bsoncxx::types::b_date{std::chrono::milliseconds{-1}}) {}

    //Used for creating a user account.
    AccountCreationParameterPack(
            const std::chrono::milliseconds& current_timestamp,
            std::string _installation_id,
            std::string _account_id,
            std::string _phone_number,
            const int _number_swipes_remaining,
            const long long _swipes_last_updated_time,
            const std::chrono::milliseconds& _time_sms_can_be_sent_again,
            const std::chrono::milliseconds _time_email_can_be_sent_again
    ) :
            installation_id(std::move(_installation_id)),
            last_verified_time(current_timestamp),
            account_id(std::move(_account_id)),
            account_type(UserAccountType::USER_ACCOUNT_TYPE),
            phone_number(std::move(_phone_number)),
            bio("~"),
            city("~"),
            gender("~"),
            age(-1),
            event_expiration_time(general_values::event_expiration_time_values::USER_ACCOUNT),
            last_time_find_matches_ran(current_timestamp),
            min_age_range(-1),
            max_age_range(-1),
            max_distance(server_parameter_restrictions::DEFAULT_MAX_DISTANCE),
            time_sms_can_be_sent_again(_time_sms_can_be_sent_again),
            number_swipes_remaining(_number_swipes_remaining),
            swipes_last_updated_time(_swipes_last_updated_time),
            time_email_can_be_sent_again(_time_email_can_be_sent_again) {}

    //Used for creating the event admin account.
    AccountCreationParameterPack(
            const bsoncxx::oid event_admin_account_oid,
            const std::chrono::milliseconds& current_timestamp,
            std::string _installation_id,
            std::string _phone_number
    ) :
            user_account_oid(event_admin_account_oid),
            installation_id(std::move(_installation_id)),
            last_verified_time(current_timestamp),
            account_type(UserAccountType::USER_ACCOUNT_TYPE),
            phone_number(std::move(_phone_number)),
            bio("~"),
            city("~"),
            gender("~"),
            age(-1),
            event_expiration_time(general_values::event_expiration_time_values::USER_ACCOUNT),
            last_time_find_matches_ran(current_timestamp),
            min_age_range(-1),
            max_age_range(-1),
            max_distance(server_parameter_restrictions::DEFAULT_MAX_DISTANCE),
            time_sms_can_be_sent_again(bsoncxx::types::b_date{std::chrono::milliseconds{-1}}),
            number_swipes_remaining(matching_algorithm::MAXIMUM_NUMBER_SWIPES),
            swipes_last_updated_time(bsoncxx::types::b_date{std::chrono::milliseconds{-1}}),
            time_email_can_be_sent_again(bsoncxx::types::b_date{std::chrono::milliseconds{-1}}) {}
};

//CREATE USER ACCOUNT
//This is the only place creating the user account happens.
//accountID will only be the accountID when account type is google or facebook
//user_account_oid will be set to the user account oid before it is returned
///This function can throw ErrorExtractingFromBsoncxx.
bool createUserAccount(
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::chrono::milliseconds& current_timestamp,
        const AccountLoginType& account_login_type,
        const AccountCreationParameterPack& parameter_pack
);