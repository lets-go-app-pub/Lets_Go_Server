//
// Created by jeremiah on 3/20/21.
//

#include <optional>

#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <build_validate_match_document.h>

#include "find_matches_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "extract_data_from_bsoncxx.h"
#include "utility_find_matches_functions.h"
#include "matching_algorithm.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

ValidateArrayElementReturnEnum validateArrayElement(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const FindMatchesArrayNameEnum array_to_pop_from
) {

    if (session == nullptr) {
        const std::string error_string = "validateArrayElement() was called when session was set to nullptr.";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(),error_string,
                "user_OID", user_account_values.user_account_oid
        );

        return ValidateArrayElementReturnEnum::unhandleable_error;
    }

    //Add a match array element.
    //NOTE: The try catch loop will pop this on failure, so adding it before the block starts for safety.
    user_account_values.saved_matches.emplace_back();

    try {

        //Used to store the value when accessed from algorithmMatchedAccountList so that the view can still exist.
        bsoncxx::document::view_or_value algorithm_matched_reference;
        bsoncxx::document::view first_matching_document_view;

        /// THESE ARRAYS ARE STORED BACKWARDS SO back() IS THE FIRST ELEMENT
        switch(array_to_pop_from) {
            case FindMatchesArrayNameEnum::other_users_matched_list: //from other users swiped yes list
                if (!user_account_values.other_users_swiped_yes_list.empty()) {
                    //These views are extracted from userAccountDocView. This means the view can be passed around
                    // instead of making copies or moving it.
                    first_matching_document_view = user_account_values.other_users_swiped_yes_list.back();
                    user_account_values.other_users_swiped_yes_list.pop_back();
                } else {
                    user_account_values.saved_matches.pop_back();
                    return ValidateArrayElementReturnEnum::empty_list;
                }
                break;
            case FindMatchesArrayNameEnum::algorithm_matched_list: //from algorithm matched list
                if (!user_account_values.algorithm_matched_account_list.empty()) {
                    algorithm_matched_reference = std::move(user_account_values.algorithm_matched_account_list.back());
                    first_matching_document_view = algorithm_matched_reference.view();
                    user_account_values.algorithm_matched_account_list.pop_back();
                } else {
                    user_account_values.saved_matches.pop_back();
                    return ValidateArrayElementReturnEnum::empty_list;
                }
                break;
            default: {
                const std::string error_string = "An invalid value of FindMatchesArrayNameEnum was passed into validateArrayElement().";

                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              std::optional<std::string>(),
                                              error_string,
                                              "array_to_pop_from", std::to_string((int)array_to_pop_from),
                                              "user_account_oid", user_account_values.user_account_oid);

                user_account_values.saved_matches.pop_back();
                return ValidateArrayElementReturnEnum::unhandleable_error;
            }
        }

        user_account_values.saved_matches.back().array_element_is_from = array_to_pop_from;
        user_account_values.saved_matches.back().number_swipes_after_extracted = user_account_values.number_swipes_remaining - 1;

        //NOTE: Arrays are updated by expiration time when matching account is initially found.

        std::chrono::milliseconds expiration_time = extractFromBsoncxx_k_date(
                first_matching_document_view,
                user_account_keys::accounts_list::EXPIRATION_TIME
        ).value;

        const bsoncxx::oid match_account_oid = extractFromBsoncxx_k_oid(
                first_matching_document_view,
                user_account_keys::accounts_list::OID
        );

        const std::chrono::milliseconds time_match_occurred = extractFromBsoncxx_k_date(
                first_matching_document_view,
                user_account_keys::accounts_list::MATCH_TIMESTAMP
        ).value;

        bsoncxx::builder::stream::document query_document{};

        //Generate document to guarantee match is still valid.
        generateValidateArrayElementMatchDocument(
                query_document,
                match_account_oid,
                user_account_values.current_timestamp,
                user_account_values.age,
                user_account_values.min_age_range,
                user_account_values.max_age_range,
                user_account_values.gender,
                user_account_values.genders_to_match_with_bson_array,
                user_account_values.user_matches_with_everyone
        );

        //NOTE: This is not projected so that the entire document can be stored when saving statistics.
        mongocxx::options::find find_options;
        find_options.projection(projectUserAccountFieldsOutForStatistics());

        try {
            //Find the matches' user account document.
            //NOTE: Still making this call to find the other user, it will get complex if I don't do this
            // because there is the chance that some matches are invalid, and so I would have to go through the entire process again
            // if I try to extract them in batches (a lot of this is happening because I want them to still be ordered).
            user_account_values.saved_matches.back().match_user_account_doc = user_accounts_collection.find_one(
                    *session,
                    query_document.view(),
                    find_options
            );
        }
        catch (mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(),
                    std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );

            user_account_values.saved_matches.pop_back();
            return ValidateArrayElementReturnEnum::extraction_error;
        }

        if (user_account_values.saved_matches.back().match_user_account_doc) { //Document was found.

            const double distance_at_match_time = extractFromBsoncxx_k_double(
                    first_matching_document_view,
                    user_account_keys::accounts_list::DISTANCE
            );

            const double match_point_value = extractFromBsoncxx_k_double(
                    first_matching_document_view,
                    user_account_keys::accounts_list::POINT_VALUE
            );

            const bsoncxx::oid saved_statistics_oid = extractFromBsoncxx_k_oid(
                    first_matching_document_view,
                    user_account_keys::accounts_list::SAVED_STATISTICS_OID
            );

            switch (array_to_pop_from) {
                case FindMatchesArrayNameEnum::other_users_matched_list: {

                    //expiration_time should only be set when popping it from OTHER_USERS_MATCHED_ACCOUNTS_LIST. This
                    // is because when it is popped from ALGORITHM_MATCHED_ACCOUNTS_LIST it must go through the process
                    // of being inserted into the matched users' OTHER_USERS_MATCHED_ACCOUNTS_LIST. Which means it will
                    // potentially need more time than the maximum time it sits on one device. The reason it is set at
                    // all is to attempt to remove the matches from the user lists as soon as they are irrelevant in
                    // order to allow the match to happen again. This is also set inside
                    // extractMatchAccountInfoForWriteToClient() when written to the client.

                    //Added a little extra for latency (more than latency, but it won't hurt anything).
                    const auto latest_expiration_time = matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE + user_account_values.current_timestamp + std::chrono::milliseconds{10L * 1000L};

                    if (expiration_time >= latest_expiration_time) { //if the expiration time is larger than the max time allowed on android then change the expiration time
                        expiration_time = latest_expiration_time;
                    }
                    break;
                }
                case FindMatchesArrayNameEnum::algorithm_matched_list: {
                    bsoncxx::array::view activity_statistics = extractFromBsoncxx_k_array(
                            first_matching_document_view,
                            user_account_keys::accounts_list::ACTIVITY_STATISTICS
                    );

                    for (const auto& ele : activity_statistics) {
                        user_account_values.saved_matches.back().activity_statistics.append(ele.get_value());
                    }
                    break;
                }
            }

            //Rebuild the extracted array element representing the match with an OID for the statistics document.
            user_account_values.saved_matches.back().matching_element_doc = buildMatchedAccountDoc<false>(
                    match_account_oid,
                    match_point_value,
                    distance_at_match_time,
                    expiration_time,
                    time_match_occurred,
                    array_to_pop_from == FindMatchesArrayNameEnum::algorithm_matched_list,
                    saved_statistics_oid
            );

            return ValidateArrayElementReturnEnum::success;
        }
        else { //No match was found.

            user_account_values.saved_matches.pop_back();

            //This means that the other account was deleted, inactivated or no longer matches the users search
            // criteria and can not be returned to the user
            return ValidateArrayElementReturnEnum::no_longer_valid;
        }

    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        user_account_values.saved_matches.pop_back();
        return ValidateArrayElementReturnEnum::extraction_error;
    }

}