//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/concatenate.hpp>
#include <global_bsoncxx_docs.h>

#include "find_matches_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "matching_algorithm.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

template <typename T>
void saveMatchedAccountsArrayInReverse(
        const bsoncxx::document::view& user_account_doc_view,
        const std::string& document_key,
        std::vector<T>& matched_accounts_list
) {
    bsoncxx::array::view matches_accounts_arr = extractFromBsoncxx_k_array(
            user_account_doc_view,
            document_key
    );

    matched_accounts_list.clear();
    for(const auto& other_user_ele : matches_accounts_arr) {
        matched_accounts_list.emplace_back(
                extractFromBsoncxxArrayElement_k_document(other_user_ele)
        );
    }
    std::reverse(matched_accounts_list.begin(), matched_accounts_list.end());
}

bool extractAndSaveUserInfo(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::function<void(
                const ReturnStatus&,
                const findmatches::FindMatchesCapMessage::SuccessTypes&
            )>& send_error_message_to_client,
        bsoncxx::stdx::optional<bsoncxx::document::value>& find_user_account_value
) {

    if(session == nullptr) {
        std::string error_string = "extractAndSaveUserInfo() was called when session was set to nullptr.";

        std::optional <std::string> dummyExceptionString;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                dummyExceptionString, error_string,
                "user_OID", user_account_values.user_account_oid
        );

        //Error already stored.
        send_error_message_to_client(
                ReturnStatus::LG_ERROR,
                findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN
        );
        return false;
    }

    const long next_time_to_be_updated = user_account_values.current_timestamp.count() / matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED.count();
    const bsoncxx::types::b_date earliest_start_time_mongo_db_date = bsoncxx::types::b_date{user_account_values.earliest_time_frame_start_timestamp};

    const bsoncxx::document::value age_calculator_document = buildDocumentToCalculateAge(user_account_values.current_timestamp);

    bsoncxx::document::value update_swipes_condition_document = document{}
        << "$gt" << open_array
            << bsoncxx::types::b_int64{next_time_to_be_updated} << "$" + user_account_keys::SWIPES_LAST_UPDATED_TIME
        << close_array
    << finalize;

    bsoncxx::document::view update_swipes_condition_document_view = update_swipes_condition_document.view();

    bsoncxx::document::value remove_expired_timeframes_doc = getCategoriesDocWithChecks(user_account_values.earliest_time_frame_start_timestamp);

    auto update_matches_arrays_lambda = [&earliest_start_time_mongo_db_date]
            (const std::string& matching_array_name) -> bsoncxx::document::value {
        return document{}
            << "$filter" << open_document
                << "input" << "$" + matching_array_name
                << "as" << "match"
                << "cond" << open_document
                    << "$gt" << open_array
                        << "$$match." + user_account_keys::accounts_list::EXPIRATION_TIME << earliest_start_time_mongo_db_date
                    << close_array
                << close_document
            << close_document
        << finalize;
    };

    bsoncxx::document::value update_document = document{}

        //calculate and update age
        << user_account_keys::AGE << age_calculator_document.view()

        //update swipes last time updated
        << user_account_keys::SWIPES_LAST_UPDATED_TIME << open_document
            << "$cond" << open_array
                << update_swipes_condition_document_view
                << bsoncxx::types::b_int64{next_time_to_be_updated} //this is supposed to be int64 not date type
                << "$" + user_account_keys::SWIPES_LAST_UPDATED_TIME
            << close_array
        << close_document

        //update number swipes remaining
        << user_account_keys::NUMBER_SWIPES_REMAINING << open_document
            << "$cond" << open_array
                << update_swipes_condition_document_view
                << bsoncxx::types::b_int32{matching_algorithm::MAXIMUM_NUMBER_SWIPES}
                << "$" + user_account_keys::NUMBER_SWIPES_REMAINING
            << close_array
        << close_document

        //update the last active time for the account to now
        << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << bsoncxx::types::b_date{user_account_values.current_timestamp}

        //update the location to the passed location
        << user_account_keys::LOCATION << open_document
            << "type" << "Point"
            << "coordinates" << open_array
                << bsoncxx::types::b_double{user_account_values.longitude}
                << bsoncxx::types::b_double{user_account_values.latitude}
            << close_array
        << close_document

        //remove categories array values for out of date time stamps
        << user_account_keys::CATEGORIES << remove_expired_timeframes_doc

        //clean the 'extracted' list out of outdated elements
        << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
        << update_matches_arrays_lambda(user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST).view()

        //clean the 'other users said yes' list out of outdated elements
        << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST
        << update_matches_arrays_lambda(user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST).view()

        //if algorithm has not been run in the last TIME_BETWEEN_ALGORITHM_MATCH_INVALIDATION clear the array
        // otherwise clean the 'algorithm matches' list out of outdated elements
        << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << open_document
            << "$cond" << open_document
                << "if" << open_document
                    << "$gt" << open_array
                        << bsoncxx::types::b_date{user_account_values.current_timestamp - matching_algorithm::TIME_BETWEEN_ALGORITHM_MATCH_INVALIDATION}
                        << "$" + user_account_keys::LAST_TIME_MATCH_ALGORITHM_RAN
                    << close_array
                << close_document
                << "then" << open_array
                << close_array
                << "else" << update_matches_arrays_lambda(user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST).view()
            << close_document
        << close_document

    << finalize;

    //NOTE: It is possible with the aggregation pipeline to also implement the next step of the
    // algorithm (extracting the proper array elements representing matches and moving them to
    // the 'matches' list). However, some problems occur when trying to calculate things such as
    // {take all values from 'other users said yes' if the algorithm fails to get enough results}
    // It is also significantly more complex (and harder to test) than doing it in C++.

    std::optional<std::string> find_and_update_exception_string;
    try {
        //NOTE: projecting most of the fields so that statistics can be stored
        bsoncxx::document::value projection_document = projectUserAccountFieldsOutForStatistics();

        mongocxx::options::find_one_and_update opts;
        opts.projection(projection_document.view());
        opts.return_document(mongocxx::options::return_document::k_after);

        mongocxx::pipeline pipeline;
        pipeline.add_fields(update_document.view());

        //find user account document
        find_user_account_value = user_accounts_collection.find_one_and_update(
                *session,
                document{}
                    << "_id" << user_account_values.user_account_oid
                << finalize,
                pipeline,
                opts
        );
    } catch (const mongocxx::logic_error& e) {
        find_and_update_exception_string = e.what();
    }

    if(!find_user_account_value) {
        const std::string error_string = "Failed to update user document inside find_matches.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                find_and_update_exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", user_account_values.user_account_oid,
                "Document_passed", update_document
        );

        send_error_message_to_client(
                find_and_update_exception_string ? ReturnStatus::LG_ERROR : ReturnStatus::NO_VERIFIED_ACCOUNT,
                findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN
                );
        return false;
    }

    user_account_values.user_account_doc_view = *find_user_account_value;
    user_account_values.number_swipes_remaining = 0;

    try {

        user_account_values.number_swipes_remaining = extractFromBsoncxx_k_int32(
                user_account_values.user_account_doc_view,
                user_account_keys::NUMBER_SWIPES_REMAINING
        );

        if (user_account_values.number_swipes_remaining < 1) { //if the swipes are at 0
            send_error_message_to_client(
                    ReturnStatus::SUCCESS,
                    findmatches::FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING
            );
            return false;
        }

        user_account_values.draw_from_list_value = extractFromBsoncxx_k_int32(
                user_account_values.user_account_doc_view,
                user_account_keys::INT_FOR_MATCH_LIST_TO_DRAW_FROM
        );

        //NOTE: A user can ONLY have 1 gender themselves, but can select multiple genders or all
        // genders to match with.
        user_account_values.genders_to_match_with_bson_array = extractFromBsoncxx_k_array(
                user_account_values.user_account_doc_view,
                user_account_keys::GENDERS_RANGE
        );

        MatchesWithEveryoneReturnValue matches_with_everyone_return = checkIfUserMatchesWithEveryone(
                user_account_values.genders_to_match_with_bson_array,
                user_account_values.user_account_doc_view
        );

        switch(matches_with_everyone_return) {
            case USER_MATCHES_WITH_EVERYONE:
                user_account_values.user_matches_with_everyone = true;
                break;
            case USER_DOES_NOT_MATCH_WITH_EVERYONE:
                user_account_values.user_matches_with_everyone = false;
                break;
            case MATCHES_WITH_EVERYONE_ERROR_OCCURRED:
                send_error_message_to_client(
                        ReturnStatus::LG_ERROR,
                        findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN
                        );
                return false;
        }

        user_account_values.gender = extractFromBsoncxx_k_utf8(
                user_account_values.user_account_doc_view,
                user_account_keys::GENDER
        );

        user_account_values.age = extractFromBsoncxx_k_int32(
                user_account_values.user_account_doc_view,
                user_account_keys::AGE
        );

        if (!extractMinMaxAgeRange(
                user_account_values.user_account_doc_view,
                user_account_values.min_age_range,
                user_account_values.max_age_range)
        ) {
            send_error_message_to_client(
                    ReturnStatus::LG_ERROR,
                    findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN
            );
            return false;
        }

        saveMatchedAccountsArrayInReverse<bsoncxx::document::view>(
                user_account_values.user_account_doc_view,
                user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
                user_account_values.other_users_swiped_yes_list
        );

        saveMatchedAccountsArrayInReverse<bsoncxx::document::view_or_value>(
                user_account_values.user_account_doc_view,
                user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
                user_account_values.algorithm_matched_account_list
        );

    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored.
        send_error_message_to_client(
                ReturnStatus::LG_ERROR,
                findmatches::FindMatchesCapMessage_SuccessTypes_UNKNOWN
        );
        return false;
    }

    return true;
}