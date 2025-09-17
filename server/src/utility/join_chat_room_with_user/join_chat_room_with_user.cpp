//
// Created by jeremiah on 3/4/23.
//

#include "join_chat_room_with_user.h"

#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <extract_thumbnail_from_verified_doc.h>
#include <global_bsoncxx_docs.h>
#include <helper_functions/chat_room_commands_helper_functions.h>

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "chat_room_header_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;
using grpc_chat_commands::ChatRoomStatus;

JoinChatRoomWithUserReturn joinChatRoomWithUser(
        std::string& different_user_joined_message_uuid,
        bsoncxx::stdx::optional<bsoncxx::document::value>& different_user_joined_chat_room_value,
        std::chrono::milliseconds& current_timestamp,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& chat_room_collection,
        mongocxx::client_session* const session,
        const bsoncxx::document::view& user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        const std::string& chat_room_id,
        const std::string& chat_room_password,
        const bool user_joined_event_from_swiping,
        const std::function<void(std::string& error_string)>& append_info_to_error_string
        ) {

    if (session == nullptr) {
        std::string error_string = "joinChatRoomWithUser() was called when session was set to nullptr.";

        append_info_to_error_string(error_string);

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "user_account_oid", user_account_oid,
                "user_account_doc_view", user_account_doc_view,
                "chat_room_id", chat_room_id,
                "chat_room_password", chat_room_password
        );

        return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
    }

    try {

        const int user_age = extractFromBsoncxx_k_int32(
                user_account_doc_view,
                user_account_keys::AGE
        );

        mongocxx::pipeline stages;
        const std::string USER_INFO = "uIf"; //this will default to null if it does not exist
        const std::string CORRECT_PASSWORD_PASSED = "cP";
        const std::string USER_IS_VALID_AGE_TO_JOIN = "vA";

        //find the collections' header as long as it is not a 'match made' type chat room
        stages.match(
            document{}
                << "_id" << chat_room_header_keys::ID
                << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
            << finalize
        );

        //project the account state to STATE_IN_CHAT_ROOM and true or false to CORRECT_PASSWORD_PASSED if correct password was sent
        stages.project(
            document{}
                << USER_INFO << open_document
                    << "$reduce" << open_document
                        << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                        << "initialValue" << bsoncxx::types::b_null{}
                        << "in" << open_document
                            << "$cond" << open_document
                                << "if" << open_document
                                    << "$eq" << open_array
                                         << user_account_oid << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                    << close_array
                                << close_document
                                << "then" << open_document
                                    << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                    << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << "$$this." + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE
                                << close_document
                                << "else" << "$$value"
                            << close_document
                        << close_document
                    << close_document
               << close_document
               << CORRECT_PASSWORD_PASSED << open_document
                   << "$eq" << open_array
                        << chat_room_password << "$" + chat_room_header_keys::CHAT_ROOM_PASSWORD
                   << close_array
               << close_document
               << USER_IS_VALID_AGE_TO_JOIN << open_document

                    << "$gte" << open_array

                        << user_age

                        << open_document
                            << "$ifNull" << open_array
                                << "$" + chat_room_header_keys::MIN_AGE
                                << server_parameter_restrictions::LOWEST_ALLOWED_AGE
                            << close_array
                        << close_document

                    << close_array

               << close_document
               << finalize
           );

        bsoncxx::stdx::optional<mongocxx::cursor> findChatRoom;
        try {
            findChatRoom = chat_room_collection.aggregate(*session, stages);
        }
        catch (const mongocxx::logic_error& e) {

            std::string error_string;

            append_info_to_error_string(error_string);

            error_string += "\n";
            error_string += e.what();

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "user_account_oid", user_account_oid
            );

            session->abort_transaction();
            return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
        }

        bool chatRoomFound = false;
        bool idExists = false;
        AccountStateInChatRoom accountState;
        std::string existing_thumbnail_reference_oid;
        bool correctPassword = false;
        bool user_valid_age_to_join = false;
        if (findChatRoom) { //if this is not set it means an exception occurred

            //this is searched for by _id so there can only be one document in the cursor
            for (auto& cursor_doc : *findChatRoom) {
                chatRoomFound = true;

                correctPassword = extractFromBsoncxx_k_bool(
                        cursor_doc,
                        CORRECT_PASSWORD_PASSED
                );

                user_valid_age_to_join = extractFromBsoncxx_k_bool(
                        cursor_doc,
                        USER_IS_VALID_AGE_TO_JOIN
                );

                auto userInfoElement = cursor_doc[USER_INFO];
                if (userInfoElement
                    && userInfoElement.type() == bsoncxx::type::k_document) { //if element exists and is type int32
                    const bsoncxx::document::view userInfoValue = userInfoElement.get_document().value;

                    accountState = AccountStateInChatRoom(
                        extractFromBsoncxx_k_int32(
                            userInfoValue,
                            chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                        )
                    );

                    existing_thumbnail_reference_oid = extractFromBsoncxx_k_utf8(
                                    userInfoValue,
                                    chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE
                            );

                    idExists = true;

                } else if(!userInfoElement
                    || userInfoElement.type() != bsoncxx::type::k_null) { //if element does not exist or is not type bool
                    logElementError(
                            __LINE__, __FILE__,
                            userInfoElement, cursor_doc,
                            bsoncxx::type::k_null, USER_INFO,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                    );
                    session->abort_transaction();
                    return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
                }
            }
        }

        if (!chatRoomFound) { //chat room not found
            return {ReturnStatus::SUCCESS, ChatRoomStatus::INVALID_CHAT_ROOM_ID};
        } else if (!user_joined_event_from_swiping && !correctPassword) { //incorrect chat room password sent
            return {ReturnStatus::SUCCESS, ChatRoomStatus::INVALID_CHAT_ROOM_PASSWORD};
        } else if (idExists && accountState == ACCOUNT_STATE_BANNED) { //account was banned
            return {ReturnStatus::SUCCESS, ChatRoomStatus::ACCOUNT_WAS_BANNED};
        } else if (idExists
                   && (accountState == ACCOUNT_STATE_IS_ADMIN
                       || accountState == ACCOUNT_STATE_IN_CHAT_ROOM)) { //account already exists in chatroom
            return {ReturnStatus::SUCCESS, ChatRoomStatus::ALREADY_IN_CHAT_ROOM};
        } else if(!user_valid_age_to_join) {
            return {ReturnStatus::SUCCESS, ChatRoomStatus::USER_TOO_YOUNG_FOR_CHAT_ROOM};
        }

        const std::string first_name = extractFromBsoncxx_k_utf8(
                        user_account_doc_view,
                        user_account_keys::FIRST_NAME
                );

        bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_chat_room_account_result;
        std::optional<std::string> find_and_update_chat_room_account_exception_string;

        //project only the header account OIDs and account states
        const bsoncxx::document::value update_user_account_projection_doc =
            document{}
                << "_id" << 0
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << 1
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + "." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << 1
                << chat_room_header_keys::EVENT_ID << 1
            << finalize;

        if (!idExists) { //account has never been in chat room before and info must be inserted

            int thumbnail_size = 0;
            std::string updated_thumbnail_reference_oid;

            auto setThumbnailValue = [&](
                    [[maybe_unused]] std::string& thumbnail,
                    int _thumbnail_size,
                    const std::string& _thumbnail_reference_oid,
                    [[maybe_unused]] const int index,
                    [[maybe_unused]] const std::chrono::milliseconds& thumbnail_timestamp
                    ) {
                thumbnail_size = _thumbnail_size;
                updated_thumbnail_reference_oid = _thumbnail_reference_oid;
            };

            ExtractThumbnailAdditionalCommands extractThumbnailAdditionalCommands;
            extractThumbnailAdditionalCommands.setupForAddToSet(chat_room_id);
            extractThumbnailAdditionalCommands.setOnlyRequestThumbnailSize();

            //extract thumbnail requires PICTURES
            if (!extractThumbnailFromUserAccountDoc(
                    accounts_db,
                    user_account_doc_view,
                    user_account_oid,
                    session,
                    setThumbnailValue,
                    extractThumbnailAdditionalCommands)
            ) { //failed to extract thumbnail

                //errors are already stored inside function
                session->abort_transaction();
                return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
            }

            auto mongoDBDateObject = bsoncxx::types::b_date{current_timestamp};

            auto userChatRoomInfo = createChatRoomHeaderUserDoc(
                    user_account_oid,
                    AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM,
                    first_name,
                    thumbnail_size,
                    updated_thumbnail_reference_oid,
                    mongoDBDateObject,
                    current_timestamp
            );

            try {

                mongocxx::options::find_one_and_update opts;

                opts.projection(update_user_account_projection_doc.view());

                opts.return_document(mongocxx::options::return_document::k_after);

                find_and_update_chat_room_account_result = chat_room_collection.find_one_and_update(
                    *session,
                    document{}
                          << "_id" << chat_room_header_keys::ID
                    << finalize,
                    document{}
                        << "$push" << open_document
                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << userChatRoomInfo.view()
                        << close_document
                        << "$max" << open_document
                            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongoDBDateObject
                        << close_document
                    << finalize,
                    opts
                );

            }
            catch (const mongocxx::logic_error& e) {
                std::string error_string;

                append_info_to_error_string(error_string);

                error_string += "\n";
                error_string += e.what();

                find_and_update_chat_room_account_exception_string = error_string;
            }

        }
        else { //account has been in chat room before and info must be updated

            int thumbnail_size;
            std::string updated_thumbnail_reference_oid;

            auto set_thumbnail_value = [&](
                    [[maybe_unused]] std::string& thumbnail,
                    int _thumbnail_size,
                    const std::string& _thumbnail_reference_oid,
                    [[maybe_unused]] const int index,
                    [[maybe_unused]] const std::chrono::milliseconds& thumbnail_timestamp
                    ) {
                thumbnail_size = _thumbnail_size;
                updated_thumbnail_reference_oid = _thumbnail_reference_oid;
            };

            ExtractThumbnailAdditionalCommands extract_thumbnail_additional_commands;
            extract_thumbnail_additional_commands.setupForUpdate(chat_room_id, existing_thumbnail_reference_oid);
            extract_thumbnail_additional_commands.setOnlyRequestThumbnailSize();

            //extract thumbnail requires PICTURES
            if (!extractThumbnailFromUserAccountDoc(
                    accounts_db,
                    user_account_doc_view,
                    user_account_oid,
                    session,
                    set_thumbnail_value,
                    extract_thumbnail_additional_commands)
            ) { //failed to extract thumbnail

                //errors are already stored inside function
                session->abort_transaction();
                return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
            }

            try {

                bsoncxx::types::b_date mongo_db_current_date_object{current_timestamp};

                mongocxx::options::find_one_and_update opts;

                opts.projection(update_user_account_projection_doc.view());

                const std::string ELEM = "e";
                bsoncxx::builder::basic::array array_builder{};
                array_builder.append(
                    document{}
                        << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << bsoncxx::types::b_oid{user_account_oid}
                    << finalize
                );

                opts.array_filters(array_builder.view());

                opts.return_document(mongocxx::options::return_document::k_after);

                const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";
                find_and_update_chat_room_account_result = chat_room_collection.find_one_and_update(
                    *session,
                    document{}
                          << "_id" << chat_room_header_keys::ID
                          << finalize,
                    document{}
                        << "$set" << open_document
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM}
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << bsoncxx::types::b_string{first_name}
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << thumbnail_size
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << mongo_db_current_date_object
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::types::b_string{updated_thumbnail_reference_oid}
                        << close_document
                        << "$push" << open_document
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << mongo_db_current_date_object
                        << close_document
                        << "$max" << open_document
                            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongo_db_current_date_object
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongo_db_current_date_object
                        << close_document
                    << finalize,
                    opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                std::string error_string;

                append_info_to_error_string(error_string);

                error_string += "\n";
                error_string += e.what();

                find_and_update_chat_room_account_exception_string = error_string;
            }
        }

        if (!find_and_update_chat_room_account_result) {  //find and update failed
            std::string error_string = "Update user info in chat room header failed.\n";

            append_info_to_error_string(error_string);

            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                find_and_update_chat_room_account_exception_string, error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "user_account_oid", user_account_oid
            );

            session->abort_transaction();
            return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
        }

        const bsoncxx::document::view chat_room_header_doc = find_and_update_chat_room_account_result->view();

        const bsoncxx::array::view accounts_in_chat_room = extractFromBsoncxx_k_array(
                        chat_room_header_doc,
                        chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                );

        //if the chat room is empty, (or a bug occurred that there is no admin) set this account as admin
        bool an_admin_exists_in_chat = false;
        for (const auto& account_ele : accounts_in_chat_room) {

            const bsoncxx::document::view account_doc = extractFromBsoncxxArrayElement_k_document(
                    account_ele
            );

            const auto account_state_in_chat_room = AccountStateInChatRoom(
                extractFromBsoncxx_k_int32(
                    account_doc,
                    chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                )
            );

            //if at least one account exists inside this chat room before the user is added, no
            //need to make this user admin
            if (account_state_in_chat_room ==
                AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN) {
                an_admin_exists_in_chat = true;
                break;
            }
        }

        std::string event_id;

        //Element may not exist, see variable declaration for more info.
        auto event_id_element = chat_room_header_doc[chat_room_header_keys::EVENT_ID];
        if (event_id_element && event_id_element.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
            event_id = event_id_element.get_oid().value.to_string();
        } else if(!event_id_element) { //element does not exist
            event_id = chat_room_values::EVENT_ID_DEFAULT;
        } else { //if element not type utf
            logElementError(
                    __LINE__, __FILE__,
                    event_id_element, chat_room_header_doc,
                    bsoncxx::type::k_oid, chat_room_header_keys::EVENT_ID,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );

            throw ErrorExtractingFromBsoncxx("Error requesting user info value " + chat_room_header_keys::EVENT_ID + ".");
        }

        bsoncxx::builder::stream::document different_user_joined_chat_room_message_doc;

        //The message must be sent THEN the chat room active times are updated to the timestamp_stored.
        different_user_joined_message_uuid = generateDifferentUserJoinedChatRoomMessage(
                different_user_joined_chat_room_message_doc,
                user_account_oid,
                an_admin_exists_in_chat ? AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM : AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
                user_joined_event_from_swiping ? event_id : chat_room_values::EVENT_ID_DEFAULT
        );

        std::optional<std::string> insert_chat_room_account_exception_string;
        try {
            mongocxx::options::find_one_and_update opts;
            opts.upsert(true);
            opts.return_document(mongocxx::options::return_document::k_after);

            mongocxx::pipeline pipeline = buildPipelineForInsertingDocument(different_user_joined_chat_room_message_doc.view());

            //This is stored outside the transaction, so it could have a value already stored if the transaction
            // had to retry.
            different_user_joined_chat_room_value = bsoncxx::stdx::optional<bsoncxx::document::value>();
            //This is actually an insert not an update. It is just a different way of doing it.
            different_user_joined_chat_room_value = chat_room_collection.find_one_and_update(
                    *session,
                    document{}
                        << "_id" << different_user_joined_message_uuid
                    << finalize,
                    pipeline,
                    opts
                );
        }
        catch (const mongocxx::logic_error& e) {
            insert_chat_room_account_exception_string = std::string(e.what());
        }

        //NOTE: The message uuid was freshly (randomly) generated. It should never be a repeat. So if it
        // was found, then it was inserted.
        if (!different_user_joined_chat_room_value) {
            std::string error_string = "Update failed when adding OID to chat room list.\n";

            append_info_to_error_string(error_string);

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    insert_chat_room_account_exception_string, error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "user_account_oid", user_account_oid
            );

            return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
        }

        const bsoncxx::document::view upsert_chat_room_account_result_view = different_user_joined_chat_room_value->view();

        current_timestamp = extractFromBsoncxx_k_date(
                upsert_chat_room_account_result_view,
                chat_room_shared_keys::TIMESTAMP_CREATED
        ).value;

        const bsoncxx::types::b_date mongo_db_current_date{current_timestamp};

        //NOTE: Updating the last time viewed for user account.
        bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
        std::optional<std::string> user_account_exception_string;
        try {

            bsoncxx::builder::stream::document update_doc;
            bsoncxx::builder::stream::document new_chat_room_doc;
            new_chat_room_doc
                    << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                    << user_account_keys::chat_rooms::LAST_TIME_VIEWED << mongo_db_current_date;

            if(event_id != chat_room_values::EVENT_ID_DEFAULT
                && !isInvalidOIDString(event_id)
            ) {
                const bsoncxx::oid event_oid{event_id};
                //If this is an event chat room, need the EVENT_OID stored as well.
                new_chat_room_doc
                    << user_account_keys::chat_rooms::EVENT_OID << event_oid;

                update_doc
                    << "$pull" << open_document
                        << user_account_keys::HAS_BEEN_EXTRACTED_ACCOUNTS_LIST << open_document
                            << user_account_keys::accounts_list::OID << event_oid
                        << close_document
                        << user_account_keys::ALGORITHM_MATCHED_ACCOUNTS_LIST << open_document
                            << user_account_keys::accounts_list::OID << event_oid
                        << close_document
                        << user_account_keys::OTHER_USERS_MATCHED_ACCOUNTS_LIST << open_document
                            << user_account_keys::accounts_list::OID << event_oid
                        << close_document
                    << close_document;
            }

            update_doc
                << "$push" << open_document
                    << user_account_keys::CHAT_ROOMS << new_chat_room_doc.view()
                << close_document;

            update_user_account = user_accounts_collection.update_one(
                *session,
                document{}
                    << "_id" << user_account_oid
                << finalize,
                update_doc.view()
            );
        }
        catch (const mongocxx::logic_error& e) {
            std::string error_string;

            append_info_to_error_string(error_string);

            error_string += "\n";
            error_string += e.what();

            user_account_exception_string = error_string;
        }

        if (!update_user_account || update_user_account->matched_count() == 0) {
            std::string error_string = "Updating user account failed after successfully logged in.\n";

            append_info_to_error_string(error_string);

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    user_account_exception_string, error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "user_account_oid", user_account_oid);

            session->abort_transaction();
            return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
        }

        //This is updated afterward in order to get TIMESTAMP_CREATED from the message.
        if (!an_admin_exists_in_chat) { //if no admin is present inside this chat before the user joined

            //make this user admin
            bsoncxx::stdx::optional<mongocxx::result::update> update_user_to_admin_result;
            std::optional<std::string> update_user_to_admin_exception_string;
            try {
                mongocxx::options::update opts;

                const std::string ELEM = "e";
                bsoncxx::builder::basic::array array_builder{};
                array_builder.append(
                    document{}
                        << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << bsoncxx::types::b_oid{user_account_oid}
                    << finalize
                );

                opts.array_filters(array_builder.view());

                const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";
                update_user_to_admin_result = chat_room_collection.update_one(
                    *session,
                    document{}
                        << "_id" << chat_room_header_keys::ID
                    << finalize,
                    document{}
                        << "$max" << open_document
                            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongo_db_current_date
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongo_db_current_date
                        << close_document
                        << "$set" << open_document
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                        << close_document
                    << finalize,
                    opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                update_user_to_admin_exception_string = e.what();
            }

            if (!update_user_to_admin_result || update_user_to_admin_result->modified_count() == 0) { //if update failed
                std::string error_string = "Failed to update user to admin after user was already found in chat room.\n";

                if (update_user_to_admin_result) {
                    error_string +=
                            "modified count: " + std::to_string(update_user_to_admin_result->modified_count()) +
                            "\n";
                    error_string +=
                            "matched count: " + std::to_string(update_user_to_admin_result->matched_count()) +
                            "\n";
                } else {
                    error_string += "updateUserToAdmin was false\n";
                }

                append_info_to_error_string(error_string);
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        update_user_to_admin_exception_string, error_string,
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chat_room_collection.name().to_string(),
                        "user_account_oid", user_account_oid,
                        "user_account_doc_view", user_account_doc_view,
                        "chat_room_header_doc", find_and_update_chat_room_account_result->view());

                session->abort_transaction();
                return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
            }
        }
        else { //if an admin was present in chat room before this user joined

            //update timestamps
            bsoncxx::stdx::optional<mongocxx::result::update> update_user_last_active_time;
            std::optional<std::string> update_user_to_admin_exception_string;
            try {
                mongocxx::options::update opts;

                const std::string ELEM = "e";
                bsoncxx::builder::basic::array array_builder{};
                array_builder.append(
                    document{}
                        << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << bsoncxx::types::b_oid{user_account_oid}
                    << finalize
                );

                opts.array_filters(array_builder.view());

                const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";
                update_user_last_active_time = chat_room_collection.update_one(
                    *session,
                    document{}
                        << "_id" << chat_room_header_keys::ID
                    << finalize,
                    document{}
                        << "$max" << open_document
                            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongo_db_current_date
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongo_db_current_date
                        << close_document
                    << finalize,
                    opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                update_user_to_admin_exception_string = e.what();
            }

            if (!update_user_last_active_time || update_user_last_active_time->matched_count() == 0) { //if update failed
                std::string error_string = "Failed to find activity times after chat room was already found.\n";

                if (update_user_last_active_time) {
                    error_string +=
                            "modified count: " + std::to_string(update_user_last_active_time->modified_count()) +
                            "\n";
                    error_string +=
                            "matched count: " + std::to_string(update_user_last_active_time->matched_count()) +
                            "\n";
                } else {
                    error_string += "update_user_last_active_time was false\n";
                }

                append_info_to_error_string(error_string);
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        update_user_to_admin_exception_string, error_string,
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chat_room_collection.name().to_string(),
                        "user_account_oid", user_account_oid,
                        "user_account_doc_view", user_account_doc_view,
                        "chat_room_header_doc", (*find_and_update_chat_room_account_result).view()
                );

                session->abort_transaction();
                return {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
            }
        }

        //Success!
        return {ReturnStatus::SUCCESS, ChatRoomStatus::SUCCESSFULLY_JOINED};
    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        session->abort_transaction();
        return{ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
    }

}