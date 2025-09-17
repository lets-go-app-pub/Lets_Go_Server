//
// Created by jeremiah on 3/27/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <utility_chat_functions.h>
#include <extract_data_from_bsoncxx.h>

#include "find_matches_helper_functions.h"

#include "user_account_keys.h"
#include "matching_algorithm.h"
#include "request_statistics_values.h"
#include "utility_find_matches_functions.h"
#include "individual_match_statistics_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool extractMatchAccountInfoForWriteToClient(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        const MatchInfoStruct& match_info,
        const std::chrono::milliseconds& swipes_time_before_reset,
        const std::chrono::milliseconds& current_timestamp,
        findmatches::SingleMatchMessage* match_response_message,
        std::vector<bsoncxx::document::value>& insert_statistics_documents,
        bsoncxx::builder::basic::array& update_statistics_documents_query
) {

    const bsoncxx::document::view matching_element_document_view = match_info.matching_element_doc.view();

    //NOTE: Arrays are updated by expiration time when matching account is initially found.
    std::chrono::milliseconds expiration_time;
    bsoncxx::oid match_account_oid;
    bsoncxx::oid statistics_document_oid;

    double distance_at_match_time;
    double match_point_value;

    try {
        distance_at_match_time = extractFromBsoncxx_k_double(
                matching_element_document_view,
                user_account_keys::accounts_list::DISTANCE
        );

        match_point_value = extractFromBsoncxx_k_double(
                matching_element_document_view,
                user_account_keys::accounts_list::POINT_VALUE
        );

        expiration_time = extractFromBsoncxx_k_date(
                matching_element_document_view,
                user_account_keys::accounts_list::EXPIRATION_TIME
        ).value;

        match_account_oid = extractFromBsoncxx_k_oid(
                matching_element_document_view,
                user_account_keys::accounts_list::OID
        );

        statistics_document_oid = extractFromBsoncxx_k_oid(
                matching_element_document_view,
                user_account_keys::accounts_list::SAVED_STATISTICS_OID
        );
    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return false;
    }

    //NOTE: This should be set here as well as inside validateArrayElement(). This is because the match could
    // still sit inside the OTHER_USERS_MATCHED_ACCOUNTS_LIST and then potentially be sent back to the matched
    // user.
    if (expiration_time >= matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE + current_timestamp
            ) { //if the expiration time is larger than the max time allowed on android then change the expiration time
        expiration_time = matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE + current_timestamp;
    }

    if (!setupSuccessfulSingleMatchMessage(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            match_info.match_user_account_doc->view(),
            match_account_oid,
            distance_at_match_time,
            match_point_value,
            expiration_time,
            swipes_time_before_reset,
            match_info.number_swipes_after_extracted,
            current_timestamp,
            match_info.array_element_is_from,
            match_response_message
        )
    ) {
        return false;
    }

    switch (match_info.array_element_is_from) {
        case FindMatchesArrayNameEnum::other_users_matched_list: {
            update_statistics_documents_query.append(statistics_document_oid);
            break;
        }
        case FindMatchesArrayNameEnum::algorithm_matched_list: {

            std::chrono::milliseconds match_timestamp;
            bool from_match_algorithm_list;

            try {
                match_timestamp = extractFromBsoncxx_k_date(
                        matching_element_document_view,
                        user_account_keys::accounts_list::MATCH_TIMESTAMP
                ).value;

                from_match_algorithm_list = extractFromBsoncxx_k_bool(
                        matching_element_document_view,
                        user_account_keys::accounts_list::FROM_MATCH_ALGORITHM_LIST
                );
            } catch (const ErrorExtractingFromBsoncxx& e) {
                //Error already stored
                return false;
            }

            bsoncxx::array::view activity_statistics_array = match_info.activity_statistics.view();

            insert_statistics_documents.emplace_back(
                document{}
                    << "_id" << statistics_document_oid
                    << individual_match_statistics_keys::STATUS_ARRAY << open_array << close_array
                    << individual_match_statistics_keys::SENT_TIMESTAMP << open_array
                        << bsoncxx::types::b_date{current_timestamp}
                    << close_array
                    << individual_match_statistics_keys::DAY_TIMESTAMP << bsoncxx::types::b_int64{current_timestamp.count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY}
                    << individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT <<
                        buildMatchedAccountDoc<true>(
                            match_account_oid,
                            match_point_value,
                            distance_at_match_time,
                            expiration_time,
                            match_timestamp,
                            from_match_algorithm_list,
                            statistics_document_oid,
                            &activity_statistics_array
                        )
                    << individual_match_statistics_keys::MATCHED_USER_ACCOUNT_DOCUMENT << match_info.match_user_account_doc->view()
                << finalize
            );
            break;
        }
    }

    return true;
}
