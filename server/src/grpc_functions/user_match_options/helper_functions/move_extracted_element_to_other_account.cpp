//
// Created by jeremiah on 3/19/21.
//

#include <AccountState.grpc.pb.h>
#include <create_chat_room_helper.h>
#include <build_validate_match_document.h>
#include <sstream>
#include <global_bsoncxx_docs.h>
#include <store_info_to_user_statistics.h>

#include "user_match_options_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "extract_thumbnail_from_verified_doc.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "matching_algorithm.h"
#include "chat_room_commands_helper_functions.h"
#include "utility_find_matches_functions.h"
#include "check_for_no_elements_in_array.h"
#include "MatchTypeEnum.grpc.pb.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool moveExtractedElementToOtherAccount(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const std::optional<bsoncxx::document::view>& user_extracted_array_document,
        const bsoncxx::document::view& user_account_document_view,
        const bsoncxx::oid& match_account_oid,
        const bsoncxx::oid& user_account_oid,
        const std::chrono::milliseconds& current_timestamp
#ifdef LG_TESTING
        , int& testing_result
#endif // TESTING
) {

    if (session == nullptr) {
        const std::string error_string = "moveExtractedElementToOtherAccount() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "userAccountOID", user_account_oid,
                "matchAccountOID", match_account_oid
        );

        return false;
    }

    try {

        const std::chrono::milliseconds expiration_time = extractFromBsoncxx_k_date(
                *user_extracted_array_document,
                user_account_keys::accounts_list::EXPIRATION_TIME
        ).value;

        if (expiration_time <= current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED) { //match has expired
#ifdef LG_TESTING
                testing_result = 1;
#endif
            return true;
        }

        const double point_value_of_match = extractFromBsoncxx_k_double(
                *user_extracted_array_document,
                user_account_keys::accounts_list::POINT_VALUE
        );

        const double match_distance = extractFromBsoncxx_k_double(
                *user_extracted_array_document,
                user_account_keys::accounts_list::DISTANCE
        );

        const std::chrono::milliseconds time_match_occurred = extractFromBsoncxx_k_date(
                *user_extracted_array_document,
                user_account_keys::accounts_list::MATCH_TIMESTAMP
        ).value;

        const bool from_match_algorithm_list = extractFromBsoncxx_k_bool(
                *user_extracted_array_document,
                user_account_keys::accounts_list::FROM_MATCH_ALGORITHM_LIST
        );

        const bsoncxx::oid statistics_doc_oid = extractFromBsoncxx_k_oid(
                *user_extracted_array_document,
                user_account_keys::accounts_list::SAVED_STATISTICS_OID
        );

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
        generateSwipedYesOnUserMatchDocument(
                match_matching_account_doc,
                match_account_oid,
                user_age,
                min_age_range,
                max_age_range,
                user_gender,
                genders_to_match_array_view,
                user_matches_with_everyone,
                user_account_oid,
                current_timestamp,
                from_match_algorithm_list
        );

        mongocxx::pipeline pipe;
        bsoncxx::builder::stream::document project_reduced_arrays_document;

        pipe.match(match_matching_account_doc.view());

        addFilterForProjectionStage(
                project_reduced_arrays_document,
                user_account_oid,
                user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
                user_account_keys::accounts_list::OID
        );

        addFilterForProjectionStage(
                project_reduced_arrays_document,
                user_account_oid,
                user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
                user_account_keys::accounts_list::OID
        );

        addFilterForProjectionStage(
                project_reduced_arrays_document,
                user_account_oid,
                user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
                user_account_keys::accounts_list::OID
        );

        pipe.project(project_reduced_arrays_document.view());

        bsoncxx::stdx::optional<mongocxx::cursor> matched_doc_cursor;
        try {
            //find the matches' matching account document
            matched_doc_cursor = user_accounts_collection.aggregate(*session, pipe);
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

        std::optional<bsoncxx::document::value> projected_match_doc_value;
        bsoncxx::document::view projected_match_doc_view;
        unsigned int num_docs_in_cursor = 0;

        for (const auto& doc : *matched_doc_cursor) {
            projected_match_doc_value = bsoncxx::document::value(doc);
            projected_match_doc_view = projected_match_doc_value->view();
            num_docs_in_cursor++;
        }

        //1 should be the maximum number because "_id" is a part of the match criteria
        if (num_docs_in_cursor != 1) {
#ifdef LG_TESTING
            testing_result = 2;
#endif
            //If there were no documents found then some criteria did not match; no longer a valid match.
            // However, it did not have an error.
            return true;
        }

        const int num_algorithm_matched_elements = checkForNoElementsInArray<false>(
                &projected_match_doc_view,
                user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST
        );

        const int num_other_user_said_yes_matched_elements = checkForNoElementsInArray<false>(
                &projected_match_doc_view,
                user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST
        );

        const int num_extracted_elements = checkForNoElementsInArray<false>(
                &projected_match_doc_view,
                user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST
        );

        if (num_algorithm_matched_elements != 0 ||
            num_other_user_said_yes_matched_elements != 0 ||
            num_extracted_elements != 0) { //if current user exists inside a match 'accounts list'.

            if (
                    (num_algorithm_matched_elements > 1 && num_other_user_said_yes_matched_elements > 1)
                    || (num_algorithm_matched_elements > 1 && num_extracted_elements > 1)
                    || (num_other_user_said_yes_matched_elements > 1 && num_extracted_elements > 1)
                    ) { //if this element exists in more than one place, store error

                const std::string error_string = "An _id field exists in multiple places across the matching account lists in.";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "oid_used", match_account_oid,
                        "projected_document", projected_match_doc_view
                );
            }

            bsoncxx::builder::stream::document arrays_to_clear;

            addPullArrayToQuery(
                    user_account_oid,
                    projected_match_doc_view,
                    arrays_to_clear,
                    user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST,
                    user_account_keys::accounts_list::OID,
                    num_algorithm_matched_elements
            );

            //NOTE: this is possible if the other user is inactive for some time
            addPullArrayToQuery(
                    user_account_oid,
                    projected_match_doc_view,
                    arrays_to_clear,
                    user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST,
                    user_account_keys::accounts_list::OID,
                    num_other_user_said_yes_matched_elements
            );

            addPullArrayToQuery(
                    user_account_oid,
                    projected_match_doc_view,
                    arrays_to_clear,
                    user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST,
                    user_account_keys::accounts_list::OID,
                    num_extracted_elements
            );

            const bsoncxx::document::view arrays_to_clear_view = arrays_to_clear.view();

            //If document is empty, do not let it update the user account to empty.
            if(!arrays_to_clear_view.empty()) {
                bsoncxx::stdx::optional<mongocxx::result::update> update_matching_doc;
                std::optional<std::string> update_matching_doc_extracted_string;
                try {

                    //remove all elements in the lists
                    update_matching_doc = user_accounts_collection.update_one(
                            *session,
                            document{}
                                    << "_id" << bsoncxx::types::b_oid{match_account_oid}
                            << finalize,
                            arrays_to_clear.view()
                    );
                }
                catch (const mongocxx::logic_error& e) {
                    update_matching_doc_extracted_string = std::string(e.what());
                }

                if (!update_matching_doc || update_matching_doc->modified_count() != 1) {
                    //The lists were checked to have elements inside them. So modified_count() should always be one.
                    const std::string error_string = "Updating an 'accounts list' for matching user account document failed.";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            update_matching_doc_extracted_string, error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "oid_used", match_account_oid
                    );

                    return false;
                }
            }

        }

        if (from_match_algorithm_list) { //if this 'extracted' array element was initially from 'algorithm match' list

            const bsoncxx::document::value document_to_be_inserted = buildMatchedAccountDoc<false>(
                    user_account_oid,
                    point_value_of_match,
                    match_distance,
                    expiration_time,
                    time_match_occurred,
                    false,
                    statistics_doc_oid
            );

            if (num_extracted_elements != 0) { //if this element was found in the matching users 'extracted' list

                //If the element is from the current users' 'algorithm_matched' list AND it already exists inside the
                // matched users' 'extracted' list, then simply update the element to this element. Then, if the matched
                // user swipes 'yes' then it can be returned and the match can be made instantly.

                bsoncxx::stdx::optional<mongocxx::result::update> update_matching_doc;
                try {

                    //push the element into 'extracted' list (it should have already been removed)
                    update_matching_doc = user_accounts_collection.update_one(
                        *session,
                        document{}
                            << "_id" << match_account_oid //matches account ID
                        << finalize,
                        document{}
                            << "$push" << open_document
                                << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << document_to_be_inserted.view()
                            << close_document
                        << finalize
                    );

                } catch (const mongocxx::logic_error& e) {
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), e.what(),
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "oid_used", match_account_oid);

                    return false;
                }

                if (!update_matching_doc || update_matching_doc->modified_count() != 1) {

                    const std::string error_string = "Push 'extracted' list for document failed in.";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "oid_used", match_account_oid);

                    return false;
                }

            }
            else { //if this element was not found in matching users 'extracted' list

                bsoncxx::stdx::optional<mongocxx::result::update> update_matching_doc;
                try {

                    //$push the element into 'other users said yes' and sort by points.
                    update_matching_doc = user_accounts_collection.update_one(
                        *session,
                        document{}
                            << "_id" << match_account_oid //matches account ID
                        << finalize,
                        document{}
                            << "$push" << open_document
                                << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_document
                                    << "$each" << open_array //NOTE: each must be used to use an array sort
                                        << document_to_be_inserted.view()
                                    << close_array
                                    << "$sort" << open_document
                                        << user_account_keys::accounts_list::POINT_VALUE << bsoncxx::types::b_int32{-1}
                                    << close_document
                                << close_document
                            << close_document
                        << finalize
                    );
                } catch (const mongocxx::logic_error& e) {
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), e.what(),
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "oid_used", match_account_oid);

                    return false;
                }

                if (!update_matching_doc || update_matching_doc->modified_count() != 1) {

                    const std::string error_string = "Push 'other user swiped yes' list for document failed in.";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "oid_used", match_account_oid);

                    return false;
                }
            }
        }
        else { //if this 'extracted' array element is from 'other user said yes' list

            bsoncxx::stdx::optional<mongocxx::cursor> find_user_account_doc;
            try {

                mongocxx::options::find opts;
                opts.projection(
                    document{}
                        << user_account_keys::FIRST_NAME << 1
                        << user_account_keys::PICTURES << 1
                    << finalize
                );

                //NOTE: the previous array values should have been removed in handleUserExtractedAccount()
                find_user_account_doc = user_accounts_collection.find(
                    *session,
                    document{}
                        << "_id" << open_document
                            << "$in" << open_array
                                << user_account_oid
                                << match_account_oid
                            << close_array
                        << close_document
                    << finalize,
                    opts
                );

            }
            catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(),std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "userAccountOID", user_account_oid,
                        "matchAccountOID", match_account_oid
                );

                return false;
            }

            std::optional<bsoncxx::document::value> user_account_value;
            bsoncxx::document::view user_account_view;
            std::optional<bsoncxx::document::value> match_account_value;
            bsoncxx::document::view match_account_view;
            bool user_account_set = false;
            bool match_account_set = false;

            if (find_user_account_doc) { //if user account documents were found

                //save results to their respective views, there should be maximum 2 results (only 2 _ids are searched for)
                for (const auto& document: *find_user_account_doc) {

                    const bsoncxx::oid account_oid = extractFromBsoncxx_k_oid(
                            document,
                            "_id"
                    );

                    if (account_oid == user_account_oid) { //if it is this user verified account doc
                        user_account_value = bsoncxx::document::value(document);
                        user_account_view = (*user_account_value).view();
                        user_account_set = true;
                    } else if (account_oid == match_account_oid) { //if it is the match verified account doc
                        match_account_value = bsoncxx::document::value(document);
                        match_account_view = (*match_account_value).view();
                        match_account_set = true;
                    } else { //if it is neither

                        const std::string error_string = "User account found was neither user account OID, or match account OID\n.";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection",collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "user_account_document", document,
                                "user_account_oid", user_account_oid,
                                "match_account_oid", match_account_oid
                        );

                        return false;
                    }

                }
            }
            //else //if user account docs were not found this means an exception was thrown, the error is stored above

            if (!user_account_set ||
                !match_account_set) { //if at least one of the verified accounts was not found

                //NOTE: When this returns the client will NOT be able to try again for this match
                // because it was removed during handleUserExtractedAccount() from the relevant arrays
                // so the error will be stored once then it will be gone.
                //NOTE: If this is reached it will be odd because both the user_account AND the match_account
                // should have been found at a previous stage of this OTHER_USER_SWIPED_YES being received.

                const std::string error_string = "User account OID, or match account OID was not found in server\n.";
                storeMongoDBErrorAndException(
                        __LINE__,__FILE__,
                        std::optional<std::string>(),error_string,
                        "user_account_set", user_account_set?"true":"false",
                        "match_account_set", match_account_set?"true":"false",
                        "user_account_oid", user_account_oid,
                        "match_account_oid", match_account_oid
                );

                //NOTE: if either account is not found, this has done everything it can, return success to client
                return true;
            }

            bsoncxx::builder::basic::array header_accounts_array_builder;

            std::string chat_room_id;
            mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

            //generates a chat room iD and stores it inside chatRoomId
            if (!generateChatRoomId(
                    chat_room_db,
                    chat_room_id)
            ) {
                //NOTE: errors already stored here
                return false;
            }

            const std::string user_name = extractFromBsoncxx_k_utf8(
                    user_account_view,
                    user_account_keys::FIRST_NAME
            );

            int user_thumbnail_size = 0;
            std::string user_thumbnail_reference_oid;

            auto set_user_thumbnail_value = [&](
                    std::string& /*thumbnail*/,
                    int _thumbnail_size,
                    const std::string& _thumbnail_reference_oid,
                    const int /*index*/,
                    const std::chrono::milliseconds& /*timestamp*/
            ) {
                user_thumbnail_size = _thumbnail_size;
                user_thumbnail_reference_oid = _thumbnail_reference_oid;
            };

            ExtractThumbnailAdditionalCommands extract_thumbnail_commands;
            extract_thumbnail_commands.setupForAddToSet(chat_room_id);
            extract_thumbnail_commands.setOnlyRequestThumbnailSize();

            //extract thumbnail requires PICTURES projection
            if (!extractThumbnailFromUserAccountDoc(
                    accounts_db,
                    user_account_view,
                    user_account_oid,
                    session,
                    set_user_thumbnail_value,
                    extract_thumbnail_commands)
            ) { //failed to extract thumbnail

                //errors are already stored
                return false;
            }

            const std::string match_name = extractFromBsoncxx_k_utf8(
                    match_account_view,
                    user_account_keys::FIRST_NAME
            );

            int match_thumbnail_size = 0;
            std::string match_thumbnail_reference_oid;

            auto set_match_thumbnail_value = [&](
                    std::string& /*thumbnail*/,
                    int thumbnail_size,
                    const std::string& _thumbnail_reference_oid,
                    const int /*index*/,
                    const std::chrono::milliseconds& /*_thumbnail_timestamp*/
            ) {
                match_thumbnail_size = thumbnail_size;
                match_thumbnail_reference_oid = _thumbnail_reference_oid;
            };

            //extract thumbnail requires PICTURES projection
            if (!extractThumbnailFromUserAccountDoc(
                    accounts_db,
                    match_account_view,
                    match_account_oid,
                    session,
                    set_match_thumbnail_value,
                    extract_thumbnail_commands)
            ) { //failed to extract thumbnail

                //errors are already stored
                return false;
            }

            const GenerateNewChatRoomTimes chat_room_times(current_timestamp);

            auto chat_room_last_active_time_date = bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time};

            //append current user to chat room header
            header_accounts_array_builder.append(
                    createChatRoomHeaderUserDoc(
                            user_account_oid,
                            AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
                            user_name,
                            user_thumbnail_size,
                            user_thumbnail_reference_oid,
                            chat_room_last_active_time_date,
                            chat_room_times.chat_room_last_active_time
                    )
            );

            //append matched user to chat room header
            header_accounts_array_builder.append(
                    createChatRoomHeaderUserDoc(
                            match_account_oid,
                            AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
                            match_name,
                            match_thumbnail_size,
                            match_thumbnail_reference_oid,
                            chat_room_last_active_time_date,
                            chat_room_times.chat_room_last_active_time
                    )
            );

            std::string chat_room_password;
            const std::string chat_room_name = generateEmptyChatRoomName(user_name + " & " + match_name);

            auto set_return_status = [&chat_room_password]
                    (const std::string& /*_chatRoomId*/, const std::string& _chatRoomPassword) {
                chat_room_password = _chatRoomPassword;
            };

            bsoncxx::builder::basic::array match_oid_strings{};

            match_oid_strings.append(user_account_oid.to_string());
            match_oid_strings.append(match_account_oid.to_string());

            bsoncxx::builder::basic::array account_oid_values;

            account_oid_values.append(user_account_oid);
            account_oid_values.append(match_account_oid);

            mongocxx::pipeline update_users_pipeline;

            const bsoncxx::document::value update_users_to_matched_doc = buildUpdateAccountsToMatchMade(
                    chat_room_id,
                    chat_room_last_active_time_date,
                    user_account_oid,
                    match_account_oid
            );

            update_users_pipeline.add_fields(update_users_to_matched_doc.view());

            bsoncxx::stdx::optional<mongocxx::result::update> update_user_account_doc;
            try {
                update_user_account_doc = user_accounts_collection.update_many(
                        *session,
                        document{}
                            << "_id" << open_document
                                << "$in" << open_array
                                    << user_account_oid
                                    << match_account_oid
                                << close_array
                            << close_document
                        << finalize,
                        update_users_pipeline);
            } catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), e.what(),
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", collection_names::CHAT_ROOM_INFO,
                        "ObjectID_used", user_account_oid);

                return false;
            }

            //NOTE: No reason to remove the chat room header here, every chat room id is unique and so this will never come up again.
            if (!update_user_account_doc || update_user_account_doc->modified_count() != 2) {

                bool update_found = true;
                int matched_count = 0;
                int modified_count = 0;

                if (!update_user_account_doc) {
                    update_found = false;
                } else {
                    matched_count = update_user_account_doc->matched_count();
                    modified_count = update_user_account_doc->modified_count();
                }

                std::stringstream error_string_stream;
                error_string_stream
                        << "One of the user accounts being updated failed."
                        << "\nUpdate Found: " << update_found
                        << "\nMatched Count: " << matched_count
                        << "\nModified Count: " << modified_count;

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string_stream.str(),
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", collection_names::CHAT_ROOM_INFO,
                        "ObjectID_used", user_account_oid,
                        "Match_OID_used", match_account_oid);

                return false;
            }

            {
                std::string cap_message_uuid;

                //NOTE: The reason this is set up so oddly (with the chat room Id generated, then the chat room created
                // from it afterwards) is because the match made message is sent when this header is inserted into the
                // new collection. This occurs while this is inside a transaction and all operations should work at
                // the same time. It isn't relevant in terms of database calls and so it's better to be safe than sorry
                // here.
                if (!createChatRoomFromId(
                        header_accounts_array_builder,
                        chat_room_name,
                        cap_message_uuid,
                        user_account_oid,
                        chat_room_times,
                        session,
                        set_return_status,
                        match_oid_strings.view(),
                        chat_room_db,
                        chat_room_id)
                        ) {
                    //NOTE: errors already stored here
                    return false;
                }
            }

            //NOTE: It would be nice to update both of these simultaneously however it comes with the problem
            // that a document could be too large. This will cause all document in the query to be individually checked
            // to see which ones were NOT updated. Then they will need to each be checked for which ones
            // are too large, then the ones that are too large and the ones that were simply not updated will
            // need to be handled separately.
            auto user_push_update_doc =
                    document{}
                        << user_account_statistics_keys::TIMES_MATCH_OCCURRED << open_document
                            << user_account_statistics_keys::times_match_occurred::MATCHED_OID << match_account_oid
                            << user_account_statistics_keys::times_match_occurred::CHAT_ROOM_ID << chat_room_id
                            << user_account_statistics_keys::times_match_occurred::MATCH_TYPE << MatchType::USER_ACCOUNT_MATCH
                            << user_account_statistics_keys::times_match_occurred::TIMESTAMP << chat_room_last_active_time_date
                    << close_document
                << finalize;

            storeInfoToUserStatistics(
                    mongo_cpp_client,
                    accounts_db,
                    user_account_oid,
                    user_push_update_doc,
                    chat_room_times.chat_room_last_active_time,
                    session
            );

            auto match_push_update_doc =
                    document{}
                        << user_account_statistics_keys::TIMES_MATCH_OCCURRED << open_document
                        << user_account_statistics_keys::times_match_occurred::MATCHED_OID << user_account_oid
                        << user_account_statistics_keys::times_match_occurred::CHAT_ROOM_ID << chat_room_id
                            << user_account_statistics_keys::times_match_occurred::MATCH_TYPE << MatchType::USER_ACCOUNT_MATCH
                        << user_account_statistics_keys::times_match_occurred::TIMESTAMP << chat_room_last_active_time_date
                    << close_document
                << finalize;

            storeInfoToUserStatistics(
                    mongo_cpp_client,
                    accounts_db,
                    match_account_oid,
                    match_push_update_doc,
                    chat_room_times.chat_room_last_active_time,
                    session
            );

#ifdef LG_TESTING
            testing_result = 3;
#endif
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Any errors have already been stored
        return false;
    }

    return true;
}