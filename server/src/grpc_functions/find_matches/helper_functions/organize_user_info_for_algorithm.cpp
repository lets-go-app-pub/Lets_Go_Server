//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/exception/operation_exception.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <AccountCategoryEnum.grpc.pb.h>

#include "find_matches_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "utility_testing_functions.h"
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

bool clearOidArrayElement(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::string& arrayKey
) {

    if(session == nullptr) {
        std::string error_string = "session passed to clearArrayElement() null. Updating match list '";
        error_string += arrayKey;
        error_string += "' failed '";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "matchAccountDocument", user_account_values.user_account_doc_view,
                "oID_used", user_account_values.user_account_oid
              );

        return false;
    }

    bsoncxx::stdx::optional<mongocxx::result::update> update_array;
    std::optional<std::string> update_array_string;
    try {
        update_array = user_accounts_collection.update_one(
            *session,
            document{}
                << "_id" << user_account_values.user_account_oid
            << finalize,
            document{}
                << "$set" << open_document
                    << arrayKey << open_array << close_array
                << close_document
            << finalize
        );
    }
    catch (const mongocxx::logic_error& e) {
        update_array_string = e.what();
    }

    if (!update_array || update_array->matched_count() != 1) { // || updateArray->modified_count() != 1) { //if update failed, not checking modified count array could have been cleared elsewhere

        std::string error_string = "updating match list '";
        error_string += arrayKey;
        error_string += "' failed '";

        storeMongoDBErrorAndException(__LINE__, __FILE__, update_array_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "matchAccountDocument", user_account_values.user_account_doc_view,
                                      "oID_used", user_account_values.user_account_oid);

        return false;
    }

    return true;
}

bool clearStringArrayElement(
        UserAccountValues& user_account_values,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::string& arrayOfOID,
        const std::string& oidKey
) {

    if(session == nullptr) {
        std::string error_string = "session passed to clearArrayElement() null. Updating match list '";
        error_string += arrayOfOID;
        error_string += "' failed '";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "matchAccountDocument", user_account_values.user_account_doc_view,
                "oID_used", user_account_values.user_account_oid
        );

        return false;
    }

    bsoncxx::stdx::optional<mongocxx::result::update> update_array;
    std::optional<std::string> update_array_exception_string;
    try {
        update_array = user_accounts_collection.update_one(
            *session,
            document{}
                << "_id" << user_account_values.user_account_oid
            << finalize,
            document{}
                << "$pull" << open_document
                    << arrayOfOID << open_document
                        << oidKey << open_document
                            << "$not" << open_document
                                << "$type" << (int)bsoncxx::type::k_utf8
                            << close_document
                        << close_document
                    << close_document
                << close_document
            << finalize
        );
    }
    catch (mongocxx::logic_error& e) {
        update_array_exception_string = std::string(e.what());
    }

    if (!update_array || update_array->matched_count() != 1) { //if update failed; no need to check modified count here could be something else that caused the error

        std::string error_string = "updating match list '";
        error_string += arrayOfOID;
        error_string += "' with key '";
        error_string += oidKey;
        error_string += "' failed '";

        storeMongoDBErrorAndException(__LINE__, __FILE__, update_array_exception_string,
                                      error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "matchAccountDocument", user_account_values.user_account_doc_view,
                                      "oID_used", user_account_values.user_account_oid);

        return false;
    }

    return true;
}

bool storeRestrictedAccounts(
        UserAccountValues& user_account_values,
        mongocxx::client_session* session,
        mongocxx::collection& user_accounts_collection
        ) {

    //This could be directly inserted into the array inside the query function allowing this to be bypassed.
    // However, keeping it here allows the entire restricted accounts vector to be built in one place.
    user_account_values.restricted_accounts.insert(
            user_account_values.user_account_oid
            );

    auto save_array_containing_oids = [&](
            const std::string& array_of_oid,
            const std::string& oid_key,
            bool field_must_exist = true
    ) {

        //save oid from 'has been extracted' list
        if (!saveRestrictedOIDToArray<false>(
                user_account_values,
                array_of_oid,
                oid_key,
                user_account_values.user_account_doc_view,
                field_must_exist
                )
        ) { //if saving oid was unsuccessful
            //clear the array
            if (!clearOidArrayElement(
                    user_account_values,
                    user_accounts_collection,
                    session,
                    array_of_oid)
            ) { //if clearing the array failed
                return false;
            }
        }

        return true;
    };

    auto save_array_containing_strings = [&](
            const std::string& array_of_strings,
            const std::string& string_key,
            bool field_must_exist = true
    ) {

        //save oid from 'accounts matched with' list
        if (!saveRestrictedOIDToArray<true>(
                user_account_values,
                array_of_strings,
                string_key,
                user_account_values.user_account_doc_view,
                field_must_exist)
        ) {
            if(!clearStringArrayElement(
                    user_account_values,
                    user_accounts_collection,
                    session,
                    array_of_strings,
                    string_key)
            ) {
                return false;
            }
        }

        return true;
    };

    //save oid from 'has been extracted' list
    if(!save_array_containing_oids(
            user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID)
    ) {
        return false;
    }

    //save oid from 'other users matched' list
    if(!save_array_containing_oids(
            user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID)
            ) {
        return false;
    }

    //save oid from 'algorithm matched' list
    if(!save_array_containing_oids(
            user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
            user_account_keys::accounts_list::OID)
            ) {
        return false;
    }

    //save oid(string) from 'matched with' list
    if(!save_array_containing_strings(
            user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH,
            user_account_keys::other_accounts_matched_with::OID_STRING)
    ) {
        return false;
    }

    //save oid(string) from 'blocked with' list
    if(!save_array_containing_strings(
            user_account_keys::OTHER_USERS_BLOCKED,
            user_account_keys::other_users_blocked::OID_STRING)
            ) {
        return false;
    }

    //save oid from 'user created events' list
    if(!save_array_containing_oids(
            user_account_keys::USER_CREATED_EVENTS,
            user_account_keys::user_created_events::EVENT_OID)
            ) {
        return false;
    }

    //save oid from 'chat rooms' list, if event_oid does NOT exist, then this is not an event chat room that the user is already a part of
    if(!save_array_containing_oids(
            user_account_keys::CHAT_ROOMS,
            user_account_keys::chat_rooms::EVENT_OID,
            false)
    ) {
        return false;
    }

    try {
        const bsoncxx::array::view previously_matched_accounts = extractFromBsoncxx_k_array(
                user_account_values.user_account_doc_view,
                user_account_keys::PREVIOUSLY_MATCHED_ACCOUNTS
        );

        //save oid from 'previously matched' list that have not been matched in the last TIME_BETWEEN_SAME_ACCOUNT_MATCHES
        for (const auto& ele : previously_matched_accounts) {

            const bsoncxx::document::view previously_matched_doc = extractFromBsoncxxArrayElement_k_document(
                    ele
            );

            const std::chrono::milliseconds previously_matched_timestamp = extractFromBsoncxx_k_date(
                    previously_matched_doc,
                    user_account_keys::previously_matched_accounts::TIMESTAMP
            ).value;

            //if the account was found within TIME_BETWEEN_SAME_ACCOUNT_MATCHES then don't allow it to be matched with this account
            if ((previously_matched_timestamp + matching_algorithm::TIME_BETWEEN_SAME_ACCOUNT_MATCHES) >
                user_account_values.current_timestamp) {

                //add it to the restricted account list
                user_account_values.restricted_accounts.insert(
                        extractFromBsoncxx_k_oid(
                                previously_matched_doc,
                                user_account_keys::previously_matched_accounts::OID
                        )
                );
            }
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return false;
    }

    return true;
}

void setTimeframesToAnytime(
        const UserAccountValues& user_account_values,
        ActivityStruct& activity
) {

    activity.timeFrames.clear();
    activity.timeFrames.emplace_back(
            user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1}, 1
    );

    activity.timeFrames.emplace_back(
            user_account_values.end_of_time_frame_timestamp, -1
    );

    activity.totalTime =
            user_account_values.end_of_time_frame_timestamp - user_account_values.earliest_time_frame_start_timestamp - std::chrono::milliseconds{1};
}

bool buildCategoriesArrayForCurrentUser(
        UserAccountValues& user_account_values
) {

    try {

        bsoncxx::array::view categories_array = extractFromBsoncxx_k_array(
                user_account_values.user_account_doc_view,
                user_account_keys::CATEGORIES
        );

        user_account_values.user_activities.clear();
        user_account_values.user_categories.clear();

        //iterate through categories and save to the vector,
        //the checking for if the time_frames are within the time frame is done above
        //timeframes coming into this are expected to be
        //1) ordered
        //2) no overlapping timeframes
        int number_activities = 0;
        for (const auto& ele : categories_array) {

            bsoncxx::document::view category_doc = extractFromBsoncxxArrayElement_k_document(
                    ele
            );

            auto activity_type = AccountCategoryType(
                    extractFromBsoncxx_k_int32(
                            category_doc,
                            user_account_keys::categories::TYPE
                    )
            );

            //no need to store categories if only matching by activities
            if(user_account_values.algorithm_search_options == AlgorithmSearchOptions::USER_MATCHING_BY_ACTIVITY
               && activity_type == AccountCategoryType::CATEGORY_TYPE) {
                continue;
            }

            ActivityStruct current_activity;

            bsoncxx::array::view time_frames_arr = extractFromBsoncxx_k_array(
                    category_doc,
                    user_account_keys::categories::TIMEFRAMES
            );

            //iterate through time frames
            //NOTE: more time frame checks are done above in generateAddFieldsMergeTimeFramesPipelineStage
            int time_frame_index = 0;
            for (const auto& time_frame_ele : time_frames_arr) {

                bsoncxx::document::view time_frame_doc = extractFromBsoncxxArrayElement_k_document(
                        time_frame_ele
                );

                std::chrono::milliseconds time =
                        std::chrono::milliseconds{
                                extractFromBsoncxx_k_int64(
                                        time_frame_doc,
                                        user_account_keys::categories::timeframes::TIME
                                )
                        };

                int start_stop_time = extractFromBsoncxx_k_int32(
                        time_frame_doc,
                        user_account_keys::categories::timeframes::START_STOP_VALUE
                );

                //if the first element is a stop time then upsert a start time of now first
                if (time_frame_index == 0 && start_stop_time == -1) {
                    //upsert time frame to vector of now
                    current_activity.timeFrames.emplace_back(
                            user_account_values.earliest_time_frame_start_timestamp + std::chrono::milliseconds{1}, 1
                    );
                }

                //upsert time frame to vector
                current_activity.timeFrames.emplace_back(
                        time, start_stop_time
                );

                if (start_stop_time == -1) { //if it is a stop time
                    int final_index = (int) current_activity.timeFrames.size() - 1;
                    if (final_index > 0) { //if size is greater than 1
                        //calculate total time
                        current_activity.totalTime +=
                                current_activity.timeFrames[final_index].time -
                                current_activity.timeFrames[final_index - 1].time;
                    } else { //if it is a stop time and the size is less than 1
                        const std::string error_string = "In categories time frames a stop time is found without a start time '";

                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection",
                                collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "document",
                                user_account_values.user_account_doc_view,
                                "oID_Used", user_account_values.user_account_oid
                        );

                        return false;
                    }
                }

                time_frame_index++;
            }

            int nested_value = 0;
            for(const auto& time_frame : current_activity.timeFrames) {
                nested_value += time_frame.startStopValue;
            }

            if (time_frame_index == 0) { //if time frame is empty then calculate it as 'anytime'
                setTimeframesToAnytime(user_account_values, current_activity);
            }
            else if (nested_value != 0) { //if each start times does not have a stop time
                const std::string error_string = "time frame size was not a factor of 2 so a start stop is not lined up\n";

                std::string timeFrameValues;

                for (unsigned int k = 0; k < current_activity.timeFrames.size(); k++) {
                    timeFrameValues += "Index: " + std::to_string(k);
                    timeFrameValues +=
                            " StartStopValue: " + std::to_string(current_activity.timeFrames[k].startStopValue);
                    timeFrameValues += " Time: " + std::to_string(current_activity.timeFrames[k].time.count());
                    timeFrameValues += '\n';
                }

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "time_frames", timeFrameValues,
                        "document", user_account_values.user_account_doc_view,
                        "oID_Used", user_account_values.user_account_oid
                );

                //NOTE: not ending here because recoverable
                setTimeframesToAnytime(user_account_values, current_activity);
            }

            current_activity.activityIndex = extractFromBsoncxx_k_int32(
                    category_doc,
                    user_account_keys::categories::INDEX_VALUE
            );

            switch(activity_type) {
                case AccountCategoryType::ACTIVITY_TYPE: {
                    user_account_values.user_activities.push_back(current_activity);
                    break;
                }
                case AccountCategoryType::CATEGORY_TYPE: {
                    user_account_values.user_categories.push_back(current_activity);
                    break;
                }
                default: {
                    const std::string error_string = "An TYPE did not match an AccountCategoryType enum value";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "document", user_account_values.user_account_doc_view,
                            "oID_Used", user_account_values.user_account_oid,
                            "activityType", std::to_string(activity_type)
                    );

                    //can continue here
                }
            }

            number_activities++;
        }

        //if no activities were found
        if (number_activities == 0) {
            std::string errorString = "activities(categories) array came back empty '";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), errorString,
                    "document", user_account_values.user_account_doc_view,
                    "oID_Used", user_account_values.user_account_oid,
                    "userActivities.size()", std::to_string(user_account_values.user_activities.size()),
                    "userCategories.size()", std::to_string(user_account_values.user_categories.size())
            );

            return false;
        }

    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return false;
    }

    return true;
}

AlgorithmReturnValues organizeUserInfoForAlgorithm(
        UserAccountValues& user_account_values,
        mongocxx::client_session* session,
        mongocxx::collection& user_accounts_collection
) {

    //this must be extracted before buildCategoriesArrayForCurrentUser() is run
    auto search_by_options_element = user_account_values.user_account_doc_view[user_account_keys::SEARCH_BY_OPTIONS];
    if (search_by_options_element
        && search_by_options_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        user_account_values.algorithm_search_options = AlgorithmSearchOptions(search_by_options_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, search_by_options_element,
                        user_account_values.user_account_doc_view, bsoncxx::type::k_int32,
                        user_account_keys::SEARCH_BY_OPTIONS,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return {};
    }

    //build the categories array for this user
    if (!buildCategoriesArrayForCurrentUser(user_account_values)) {
        return {};
    }

    //If the only activity is the 'Unknown' activity (index 0) or no activities, don't run algorithm.
    if(user_account_values.user_activities.empty()
       || (user_account_values.user_activities.size() == 1 && user_account_values.user_activities[0].activityIndex == 0)) {
        const std::string errorString = "Only activity is the 'Unknown' activity (index 0).";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), errorString,
                "matchAccountDocument", user_account_values.user_account_doc_view,
                "oID_used", user_account_values.user_account_oid,
                "userAccountValues.userActivities", std::to_string(user_account_values.user_activities.size())
        );

        return {
            match_algorithm_ran_recently,
            matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND
        };
    }

    auto max_distance_element = user_account_values.user_account_doc_view[user_account_keys::MAX_DISTANCE];
    if (max_distance_element
        && max_distance_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        user_account_values.max_distance = max_distance_element.get_int32().value;
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, max_distance_element,
                        user_account_values.user_account_doc_view, bsoncxx::type::k_int32,
                        user_account_keys::MAX_DISTANCE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return {};
    }

    //fill the restricted accounts array
    if (!storeRestrictedAccounts(
            user_account_values,
            session,
            user_accounts_collection
        )
    ) {
        return {};
    }

    return runMatchingAlgorithmAndReturnResults(
            user_account_values,
            user_accounts_collection,
            false
    );
}