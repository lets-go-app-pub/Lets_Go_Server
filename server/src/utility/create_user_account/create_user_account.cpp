//
// Created by jeremiah on 3/3/23.
//

#include "create_user_account.h"

#include "UserAccountStatusEnum.grpc.pb.h"
#include "store_mongoDB_error_and_exception.h"
#include "matching_algorithm.h"
#include "global_bsoncxx_docs.h"
#include "UserSubscriptionStatus.grpc.pb.h"
#include "user_account_statistics_keys.h"
#include "build_user_statistics_document.h"
#include "session_to_run_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool createUserAccount(
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::chrono::milliseconds& current_timestamp,
        const AccountLoginType& account_login_type,
        const AccountCreationParameterPack& parameter_pack
) {

    const long long next_time_to_be_updated = current_timestamp.count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count();
    int number_swipes_remaining = parameter_pack.number_swipes_remaining;
    long long swipes_last_updated_time = parameter_pack.swipes_last_updated_time;

    //if time for updating swipes has elapsed, set them to max
    if(next_time_to_be_updated > parameter_pack.swipes_last_updated_time) {
        number_swipes_remaining = matching_algorithm::MAXIMUM_NUMBER_SWIPES;
        swipes_last_updated_time = next_time_to_be_updated;
    }

    static const bsoncxx::types::b_date DEFAULT_DATE{std::chrono::milliseconds{-1}};
    static const bsoncxx::types::b_string DEFAULT_STRING{"~"};
    static const bsoncxx::types::b_int32 DEFAULT_INT{-1};
    static const bsoncxx::types::b_int32 INT_ZERO{0};

    //NOTE: The facebook and google ids are added to the user account below.
    bsoncxx::builder::stream::document user_account_doc_builder;

    bsoncxx::builder::basic::array account_id_list_array;

    if (account_login_type == AccountLoginType::GOOGLE_ACCOUNT
        || account_login_type == AccountLoginType::FACEBOOK_ACCOUNT
    ) {
        account_id_list_array.append(parameter_pack.account_id);
    }

    bsoncxx::builder::basic::array categories_array;
    if(parameter_pack.categories.view().empty()) {
        bsoncxx::array::value default_categories = buildDefaultCategoriesArray();
        for(auto& ele : default_categories.view()) {
            categories_array.append(ele.get_value());
        }
    } else {
        for(auto& ele : parameter_pack.categories.view()) {
            categories_array.append(ele.get_value());
        }
    }

    bsoncxx::builder::basic::array pictures_array;
    const bsoncxx::array::view pictures_array_view = parameter_pack.pictures.view();
    if(pictures_array_view.empty()) {
        for(int i = 0; i < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT; i++) {
            pictures_array.append(bsoncxx::types::b_null{});
        }
    } else {
        const int null_elements_required =
                general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT -
                (int)std::distance(pictures_array_view.begin(), pictures_array_view.end());

        for(auto& ele : pictures_array_view) {
            pictures_array.append(ele.get_value());
        }

        for(int i = 0; i < null_elements_required; i++) {
            pictures_array.append(bsoncxx::types::b_null{});
        }
    }

    user_account_doc_builder
            << "_id" << parameter_pack.user_account_oid
            << user_account_keys::STATUS << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
            << user_account_keys::INACTIVE_MESSAGE << DEFAULT_STRING
            << user_account_keys::NUMBER_OF_TIMES_TIMED_OUT << INT_ZERO
            << user_account_keys::INACTIVE_END_TIME << bsoncxx::types::b_null{}
            << user_account_keys::INSTALLATION_IDS << open_array
                << bsoncxx::types::b_string{parameter_pack.installation_id}
            << close_array
            << user_account_keys::LAST_VERIFIED_TIME << bsoncxx::types::b_date{parameter_pack.last_verified_time}

            << user_account_keys::TIME_CREATED << bsoncxx::types::b_date{current_timestamp}

            << user_account_keys::ACCOUNT_ID_LIST << account_id_list_array

            << user_account_keys::SUBSCRIPTION_STATUS << UserSubscriptionStatus::NO_SUBSCRIPTION
            << user_account_keys::SUBSCRIPTION_EXPIRATION_TIME << DEFAULT_DATE
            << user_account_keys::ACCOUNT_TYPE << parameter_pack.account_type

            << user_account_keys::PHONE_NUMBER << bsoncxx::types::b_string{parameter_pack.phone_number}
            << user_account_keys::FIRST_NAME << (parameter_pack.event_values ? bsoncxx::types::b_string{parameter_pack.event_values->event_title()} : DEFAULT_STRING)
            << user_account_keys::BIO << parameter_pack.bio
            << user_account_keys::CITY << parameter_pack.city
            << user_account_keys::GENDER << parameter_pack.gender
            << user_account_keys::BIRTH_YEAR << DEFAULT_INT
            << user_account_keys::BIRTH_MONTH << DEFAULT_INT
            << user_account_keys::BIRTH_DAY_OF_MONTH << DEFAULT_INT
            << user_account_keys::BIRTH_DAY_OF_YEAR << DEFAULT_INT
            << user_account_keys::AGE << parameter_pack.age
            << user_account_keys::EMAIL_ADDRESS << DEFAULT_STRING

            << user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION << bsoncxx::types::b_bool{true}

            << user_account_keys::OPTED_IN_TO_PROMOTIONAL_EMAIL << bsoncxx::types::b_bool{!parameter_pack.event_values.operator bool()}

            << user_account_keys::PICTURES << pictures_array

            << user_account_keys::SEARCH_BY_OPTIONS << AlgorithmSearchOptions::USER_MATCHING_BY_CATEGORY_AND_ACTIVITY

            << user_account_keys::CATEGORIES << categories_array
            << user_account_keys::CATEGORIES_TIMESTAMP << DEFAULT_DATE;

    if(parameter_pack.event_values) {
        user_account_doc_builder
                << user_account_keys::EVENT_VALUES << open_document
                        << user_account_keys::event_values::CREATED_BY << parameter_pack.event_values->created_by()
                        << user_account_keys::event_values::CHAT_ROOM_ID << parameter_pack.event_values->chat_room_id()
                        << user_account_keys::event_values::EVENT_TITLE << parameter_pack.event_values->event_title()
                << close_document;
    }

    user_account_doc_builder
            << user_account_keys::EVENT_EXPIRATION_TIME << parameter_pack.event_expiration_time
            << user_account_keys::USER_CREATED_EVENTS << open_array << close_array

            << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << open_array << close_array

            << user_account_keys::MATCHING_ACTIVATED << bsoncxx::types::b_bool{false}
            << user_account_keys::LOCATION << open_document
                << "type" << "Point"
                << "coordinates" << open_array
                    << bsoncxx::types::b_double{-122}
                    << bsoncxx::types::b_double{37}
                << close_array //some random default coordinates; I think It's somewhere in California?
            << close_document

            << user_account_keys::FIND_MATCHES_TIMESTAMP_LOCK << DEFAULT_DATE
            << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << bsoncxx::types::b_date{parameter_pack.last_time_find_matches_ran}

            << user_account_keys::AGE_RANGE << open_document
                << user_account_keys::age_range::MIN << parameter_pack.min_age_range
                << user_account_keys::age_range::MAX << parameter_pack.max_age_range
            << close_document
            << user_account_keys::GENDERS_RANGE << parameter_pack.genders_range
            << user_account_keys::MAX_DISTANCE << bsoncxx::types::b_int32{parameter_pack.max_distance}

            << user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS << open_array << close_array

            << user_account_keys::LAST_TIME_EMPTY_MATCH_RETURNED << DEFAULT_DATE
            << user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN << DEFAULT_DATE

            << user_account_keys::INT_FOR_MATCH_LIST_TO_DRAW_FROM << INT_ZERO
            << user_account_keys::TOTAL_NUMBER_MATCHES_DRAWN << INT_ZERO

            << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_array << close_array
            << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << open_array << close_array
            << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_array << close_array

            << user_account_keys::TIME_SMS_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{parameter_pack.time_sms_can_be_sent_again}
            << user_account_keys::NUMBER_SWIPES_REMAINING << number_swipes_remaining
            << user_account_keys::SWIPES_LAST_UPDATED_TIME << bsoncxx::types::b_int64{swipes_last_updated_time}

            << user_account_keys::LOGGED_IN_TOKEN << DEFAULT_STRING
            << user_account_keys::LOGGED_IN_TOKEN_EXPIRATION << DEFAULT_DATE
            << user_account_keys::LOGGED_IN_INSTALLATION_ID << DEFAULT_STRING

            << user_account_keys::COOL_DOWN_ON_SMS_RETURN_MESSAGE << DEFAULT_INT
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << DEFAULT_INT

            << user_account_keys::TIME_EMAIL_CAN_BE_SENT_AGAIN << bsoncxx::types::b_date{parameter_pack.time_email_can_be_sent_again}

            //NOTE: This value cannot be initially set to 0. This is because when updateSingleOtherUser
            // runs, it will return this. And if the value is set to 0, the client will assume the info does
            // not need updated.
            << user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED << DEFAULT_DATE

            << user_account_keys::BIRTHDAY_TIMESTAMP << DEFAULT_DATE
            << user_account_keys::GENDER_TIMESTAMP << DEFAULT_DATE
            << user_account_keys::FIRST_NAME_TIMESTAMP << DEFAULT_DATE
            << user_account_keys::BIO_TIMESTAMP << DEFAULT_DATE
            << user_account_keys::CITY_NAME_TIMESTAMP << DEFAULT_DATE
            << user_account_keys::POST_LOGIN_INFO_TIMESTAMP << DEFAULT_DATE
            << user_account_keys::EMAIL_TIMESTAMP << DEFAULT_DATE

            << user_account_keys::CHAT_ROOMS << open_array << close_array

            << user_account_keys::OTHER_USERS_BLOCKED << open_array << close_array

            << user_account_keys::NUMBER_TIMES_SWIPED_YES << INT_ZERO
            << user_account_keys::NUMBER_TIMES_SWIPED_NO << INT_ZERO
            << user_account_keys::NUMBER_TIMES_SWIPED_BLOCK << INT_ZERO
            << user_account_keys::NUMBER_TIMES_SWIPED_REPORT << INT_ZERO

            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES << INT_ZERO
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO << INT_ZERO
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK << INT_ZERO
            << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT << INT_ZERO

            << user_account_keys::NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION << INT_ZERO
            << user_account_keys::NUMBER_TIMES_SENT_BUG_REPORT << INT_ZERO
            << user_account_keys::NUMBER_TIMES_SENT_OTHER_SUGGESTION << INT_ZERO

            << user_account_keys::NUMBER_TIMES_SPAM_FEEDBACK_SENT << INT_ZERO
            << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << INT_ZERO

            << user_account_keys::NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM << INT_ZERO
            << user_account_keys::NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM << INT_ZERO
            << user_account_keys::NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM << INT_ZERO
            << user_account_keys::NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM << INT_ZERO

            << user_account_keys::DISCIPLINARY_RECORD << open_array << close_array;

    std::optional<std::string> exception_string;
    bsoncxx::stdx::optional<mongocxx::result::insert_one> insert_success;
    try {
        //insert new user account
        insert_success = insert_one_optional_session(
                session,
                user_accounts_collection,
                user_account_doc_builder.view()
        );
    }
    catch (const mongocxx::logic_error& e) {
        exception_string = std::string(e.what());
    }

    if (!insert_success || insert_success->result().inserted_count() != 1) { //upsert failed
        const std::string error_string = "Insert user account failed.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "verifiedAccountDocument", user_account_doc_builder.view()
        );
        return false;
    }

    bsoncxx::builder::basic::array phone_numbers_array;
    phone_numbers_array.append(
            document{}
                    << user_account_statistics_keys::phone_numbers::PHONE_NUMBER << parameter_pack.phone_number
                    << user_account_statistics_keys::phone_numbers::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
            << finalize
    );

    mongocxx::collection user_account_statistics_collection = accounts_db[collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME];
    const bsoncxx::document::value user_account_statistics_doc = buildUserStatisticsDocument(
            parameter_pack.user_account_oid,
            phone_numbers_array
    );

    bsoncxx::stdx::optional<mongocxx::result::insert_one> insert_statistics_doc_success;
    try {
        //insert new user statistics document
        insert_statistics_doc_success = insert_one_optional_session(
                session,
                user_account_statistics_collection,
                user_account_statistics_doc.view()
        );
    }
    catch (const mongocxx::logic_error& e) {
        exception_string = std::string(e.what());
    }

    if (!insert_statistics_doc_success || insert_statistics_doc_success->result().inserted_count() != 1) { //upsert failed
        const std::string error_string = "Insert user statistics document failed.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "userStatisticsDocument", user_account_statistics_doc.view()
        );
        return false;
    }

    return true;
}
