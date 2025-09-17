//
// Created by jeremiah on 3/6/23.
//

#include <create_chat_room_helper.h>
#include <build_validate_match_document.h>
#include <store_info_to_user_statistics.h>

#include "user_match_options_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "matching_algorithm.h"
#include "chat_room_commands_helper_functions.h"
#include "check_for_no_elements_in_array.h"
#include "join_chat_room_with_user.h"
#include "MatchTypeEnum.grpc.pb.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool handleUserSwipedYesOnEvent(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& chat_room_collection,
        mongocxx::client_session* session,
        const bsoncxx::document::view& user_account_document_view,
        const std::string& chat_room_id,
        const bsoncxx::oid& event_account_oid,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp
#ifdef LG_TESTING
        , int& testing_result
#endif // TESTING
) {
    if (session == nullptr) {
        const std::string error_string = "handleUserSwipedYesOnEvent() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "user_account_oid", user_account_oid,
                "event_account_oid", event_account_oid
        );

        return false;
    }

    try {

        const int user_age = extractFromBsoncxx_k_int32(
                user_account_document_view,
                user_account_keys::AGE
        );

        //NOTE: You can only have one gender yourself, but you can select multiple genders to match with.
        const bsoncxx::array::view genders_to_match_array_view = extractFromBsoncxx_k_array(
                user_account_document_view,
                user_account_keys::GENDERS_RANGE
        );

        const MatchesWithEveryoneReturnValue matches_with_everyone_return = checkIfUserMatchesWithEveryone(
                genders_to_match_array_view,
                user_account_document_view
        );

        const bool user_matches_with_everyone = matches_with_everyone_return == USER_MATCHES_WITH_EVERYONE;
        if(matches_with_everyone_return == MATCHES_WITH_EVERYONE_ERROR_OCCURRED) {
            //error already stored
            return false;
        }

        const std::string user_gender = extractFromBsoncxx_k_utf8(
                user_account_document_view,
                user_account_keys::GENDER
        );

        int min_age_range;
        int max_age_range;

        if (!extractMinMaxAgeRange(
                user_account_document_view,
                min_age_range,
                max_age_range)
        ) {
            return false;
        }

        bsoncxx::builder::stream::document match_matching_account_doc;

        //generate document to guarantee match is still valid
        generateSwipedYesOnEventMatchDocument(
                match_matching_account_doc,
                event_account_oid,
                user_age,
                min_age_range,
                max_age_range,
                user_gender,
                genders_to_match_array_view,
                user_matches_with_everyone,
                current_timestamp
        );

        std::int64_t num_matching_docs;
        try {
            //check if matching account exists
            num_matching_docs = user_accounts_collection.count_documents(
                match_matching_account_doc.view()
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            return false;
        }

        //1 should be the maximum number because "_id" is a part of the match criteria
        if (num_matching_docs != 1) {
#ifdef LG_TESTING
            testing_result = 2;
#endif
            //If there were no documents found then some criteria did not match; no longer a valid match (event could
            // be expired).
            // However, it did not have an error.
            return true;
        }


        std::string different_user_joined_message_uuid;
        bsoncxx::stdx::optional<bsoncxx::document::value> different_user_joined_chat_room_value;
        std::chrono::milliseconds updated_current_timestamp = current_timestamp;

        const JoinChatRoomWithUserReturn join_chat_room_return_value = joinChatRoomWithUser(
            different_user_joined_message_uuid,
            different_user_joined_chat_room_value,
            updated_current_timestamp,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            session,
            user_account_document_view,
            user_account_oid,
            chat_room_id,
            "", //password will be ignored because user_joined_event_from_swiping==true
            true
        );

        if(join_chat_room_return_value.return_status != ReturnStatus::SUCCESS) {
            //Error has already been stored
            return false;
        } else if(join_chat_room_return_value.chat_room_status != grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED) {
            const std::string error_string = "joinChatRoomWithUser() was called when an event was swiped yes on. However,"
                                             " the function was unable to properly join the chat room.";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "user_account_oid", user_account_oid,
                    "event_account_oid", event_account_oid,
                    "chat_room_id", chat_room_id,
                    "chat_room_status", grpc_chat_commands::ChatRoomStatus_Name(join_chat_room_return_value.chat_room_status)
            );

#ifdef LG_TESTING
            testing_result = 3;
#endif
            //There are a few situations where this could happen (such as user already inside chat room), they do
            // not need to return an error to the client.
            return true;
        }

        //NOTE: It would be nice to update both of these simultaneously however it comes with the problem
        // that a document could be too large. This will cause all document in the query to be individually checked
        // to see which ones were NOT updated. Then they will need to each be checked for which ones
        // are too large, then the ones that are too large and the ones that were simply not updated will
        // need to be handled separately.
        auto user_push_update_doc =
            document{}
                << user_account_statistics_keys::TIMES_MATCH_OCCURRED << open_document
                    << user_account_statistics_keys::times_match_occurred::MATCHED_OID << event_account_oid
                    << user_account_statistics_keys::times_match_occurred::CHAT_ROOM_ID << chat_room_id
                    << user_account_statistics_keys::times_match_occurred::MATCH_TYPE << MatchType::EVENT_MATCH
                    << user_account_statistics_keys::times_match_occurred::TIMESTAMP << bsoncxx::types::b_date{updated_current_timestamp}
                << close_document
            << finalize;

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                user_account_oid,
                user_push_update_doc,
                updated_current_timestamp,
                session
        );

        auto event_push_update_doc =
            document{}
                << user_account_statistics_keys::TIMES_MATCH_OCCURRED << open_document
                    << user_account_statistics_keys::times_match_occurred::MATCHED_OID << user_account_oid
                    << user_account_statistics_keys::times_match_occurred::CHAT_ROOM_ID << chat_room_id
                    << user_account_statistics_keys::times_match_occurred::MATCH_TYPE << MatchType::EVENT_MATCH
                    << user_account_statistics_keys::times_match_occurred::TIMESTAMP << bsoncxx::types::b_date{updated_current_timestamp}
                << close_document
            << finalize;

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                event_account_oid,
                event_push_update_doc,
                updated_current_timestamp,
                session
        );

#ifdef LG_TESTING
            testing_result = 4;
#endif
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Any errors have already been stored
        return false;
    }

    return true;
}