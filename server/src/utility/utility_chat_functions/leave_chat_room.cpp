//
// Created by jeremiah on 7/12/21.
//

#include <global_bsoncxx_docs.h>
#include <store_mongoDB_error_and_exception.h>
#include <helper_functions/chat_room_commands_helper_functions.h>


#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "chat_room_header_keys.h"
#include "extract_thumbnail_from_verified_doc.h"

bool leaveChatRoom(
        mongocxx::database& chat_room_db,
        mongocxx::database& accounts_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session,
        const bsoncxx::document::view& user_account_doc,
        const std::string& chat_room_id,
        const bsoncxx::oid& user_account_oid,
        std::chrono::milliseconds& current_timestamp,
        const std::function<void(
                const ReturnStatus&,
                const std::chrono::milliseconds&
        )>& set_return_status
) {

    if (session == nullptr) {
        const std::string error_string = "leaveChatRoom() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "user_OID", user_account_oid,
                "chatRoomId", chat_room_id
        );
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    mongocxx::stdx::optional<mongocxx::cursor> find_users_cursor;
    try {
        const mongocxx::pipeline pipeline = buildUserLeftChatRoomExtractAccountsAggregation(
                user_account_oid
        );

        find_users_cursor = chat_room_collection.aggregate(
                    *session,
                    pipeline
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), e.what(),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                "chatRoomId", chat_room_id,
                "user_account_OID", user_account_oid
        );
        session->abort_transaction();
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    if(!find_users_cursor) {
        const std::string error_message = "Error running pipeline to leave chat room. A cursor value of some sort should always be set.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_message,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                "chatRoomId", chat_room_id,
                "user_account_OID", user_account_oid
        );
        session->abort_transaction();
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    std::optional<bsoncxx::document::value> chat_room_header_pipeline_result;

    for(auto& doc : *find_users_cursor) {
        //make a copy, the document should be small
        chat_room_header_pipeline_result.emplace(doc);
    }

    if (chat_room_header_pipeline_result) { //if chat room is found

        int thumbnail_size;
        std::string thumbnail_reference_oid;
        auto setThumbnailValue = [&](
                std::string& /*thumbnail*/,
                const int _thumbnail_size,
                const std::string& _thumbnail_reference_oid,
                const int /*index*/,
                const std::chrono::milliseconds& /*_thumbnail_timestamp*/
        ) {
            thumbnail_size = _thumbnail_size;
            thumbnail_reference_oid = _thumbnail_reference_oid;
        };

        //the thumbnail reference inside the chat room header will be updated later
        ExtractThumbnailAdditionalCommands extractThumbnailAdditionalCommands;
        extractThumbnailAdditionalCommands.setupForAddToSet(chat_room_id);
        extractThumbnailAdditionalCommands.setOnlyRequestThumbnailSize();

        //extract thumbnail requires PICTURES projection
        if (!extractThumbnailFromUserAccountDoc(
                accounts_db,
                user_account_doc,
                user_account_oid,
                session,
                setThumbnailValue,
                extractThumbnailAdditionalCommands)
                ) { //failed to extract thumbnail
            //errors are already stored
            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        const bsoncxx::document::view chat_room_header_pipeline_result_view = *chat_room_header_pipeline_result;
        bsoncxx::array::view header_user_accounts_array;
        std::string new_account_admin;

        auto header_user_accounts_array_element = chat_room_header_pipeline_result_view[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM];
        if (header_user_accounts_array_element
            && header_user_accounts_array_element.type() == bsoncxx::type::k_array
        ) { //if element exists and is type array
            header_user_accounts_array = header_user_accounts_array_element.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(
                    __LINE__, __FILE__,
                    header_user_accounts_array_element, chat_room_header_pipeline_result_view,
                    bsoncxx::type::k_array, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
            );

            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        bool user_account_found = false;
        int num_users_found = 0;
        //At most this array will have two elements, it should ALWAYS have the current user, then it
        // will contain the admin if a new admin will be promoted.
        for (const auto& member : header_user_accounts_array) {
            num_users_found++;
            if(num_users_found > 2) {
                const std::string error_message = "Too many users were extracted from header document. At most 2 should be extracted.";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_message,
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                        "chatRoomId", chat_room_id,
                        "user_account_OID", user_account_oid,
                        "chat_room_header_pipeline_result_view", chat_room_header_pipeline_result_view
                );

                //can continue here
                break;
            }

            if (member.type() == bsoncxx::type::k_document) {

                const bsoncxx::document::view member_doc = member.get_document().value;

                auto account_oid_element = member_doc[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID];
                if (account_oid_element
                    && account_oid_element.type() == bsoncxx::type::k_oid
                ) { //if element exists and is type oid
                    const bsoncxx::oid account_oid = account_oid_element.get_oid().value;

                    if (account_oid == user_account_oid) {
                        user_account_found = true;
                    } else {
                        new_account_admin = account_oid.to_string();
                    }
                }
                else { //if element does not exist or is not type oid
                    logElementError(
                            __LINE__, __FILE__,
                            account_oid_element, member_doc,
                            bsoncxx::type::k_oid, chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                    );
                    //NOTE: can continue here (see note below)
                }
            }
            else {
                logElementError(
                        __LINE__, __FILE__,
                        header_user_accounts_array_element, chat_room_header_pipeline_result_view,
                        bsoncxx::type::k_document, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                );
            }
        }

        if(!user_account_found) {
            const std::string error_message = "Error user was not found in passed chat room header during leave_chat_room.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_message,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    "chatRoomId", chat_room_id,
                    "user_account_OID", user_account_oid,
                    "chat_room_header_pipeline_result_view", chat_room_header_pipeline_result_view
            );
            //User was not found in chat room. Return success, nothing more to be done here.
            set_return_status(ReturnStatus::SUCCESS, std::chrono::milliseconds{-1L});
            return true;
        }

        grpc_chat_commands::ClientMessageToServerRequest generatedRequest;

        generatedRequest.set_message_uuid(generateUUID());
        auto message = generatedRequest.mutable_message();
        message->mutable_message_specifics()->mutable_different_user_left_message()->set_new_admin_account_oid(
                new_account_admin
                );

        const SendMessageToChatRoomReturn returnValue = sendMessageToChatRoom(
                &generatedRequest,
                chat_room_collection,
                user_account_oid,
                session
        );

        switch (returnValue.successful) {
            case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                const std::string error_string = "Message UUID already existed when leave chat room attempted to send it.\n";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "ObjectID_used", user_account_oid,
                        "chatRoomId", chat_room_id,
                        "user_account_OID", user_account_oid,
                        "message", generatedRequest.DebugString()
                );
                //can still continue
                session->abort_transaction();
                set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
                return false;
            }
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                current_timestamp = returnValue.time_stored_on_server;
                break;
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED:
                session->abort_transaction();
                set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
                return false;
        }

        mongocxx::stdx::optional<mongocxx::result::update> find_and_update_chat_room_account;
        std::optional<std::string> update_chat_room_exception_string;
        try {
            mongocxx::pipeline update_chat_room_pipeline;

            const bsoncxx::document::value leave_chat_room_pipeline_doc = buildUserLeftChatRoomHeader(
                    user_account_oid,
                    thumbnail_size,
                    thumbnail_reference_oid,
                    returnValue.time_stored_on_server
                    );

            update_chat_room_pipeline.add_fields(leave_chat_room_pipeline_doc.view());

            //run update on chat room
            // update user info, update user account state to not in chat room
            // set up a new admin if user was admin and another valid user inside chat room
            find_and_update_chat_room_account = chat_room_collection.update_one(
                    *session,
                    document{}
                        << "_id" << chat_room_header_keys::ID
                        << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
                    << finalize,
                    update_chat_room_pipeline
            );
        } catch (const mongocxx::logic_error& e) {
            update_chat_room_exception_string = std::string(e.what());
        }

        if(!find_and_update_chat_room_account
           || find_and_update_chat_room_account->matched_count() == 0) {
            const std::string error_string = "An error occurred when attempting to update the chat room.\n";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_chat_room_exception_string, error_string,
                    "ObjectID_used", user_account_oid,
                    "chatRoomId", chat_room_id,
                    "user_account_OID", user_account_oid,
                    "message", generatedRequest.DebugString()
           );

            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        if (!extractUserAndRemoveChatRoomIdFromUserPicturesReference(
                accounts_db,
                chat_room_header_pipeline_result_view,
                user_account_oid,
                thumbnail_reference_oid,
                chat_room_id,
                session)
        ) {
            //error is already stored here
            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        set_return_status(ReturnStatus::SUCCESS, returnValue.time_stored_on_server);
    }
    else { //chat room was not found

        //This block will check and see if it is a 'matching chat room' and if it is it will run unMatch.
        //This is done because there could be a memory leak inside the user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH
        // if leaveChatRoom is called while it is a matching chat room.
        //NOTE: inside ChatRoomFragment.kt (in setUpChatRoomViews()) if an error occurs, it relies
        // on this functionality to work correctly.
        mongocxx::stdx::optional<bsoncxx::document::value> find_single_chat_room;
        std::optional<std::string> find_single_exception_string;
        try {
            mongocxx::options::find opts;

            opts.projection(
                    document{}
                            << chat_room_header_keys::MATCHING_OID_STRINGS << 1
                    << finalize
            );

            //run update on chat room
            // update user info, update user account state to not in chat room
            // set up a new admin if user was admin and another valid user inside chat room
            find_single_chat_room = chat_room_collection.find_one(
                *session,
                document{}
                        << "_id" << chat_room_header_keys::ID
                << finalize,
                opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            find_single_exception_string = e.what();
        }

        if (!find_single_chat_room) {
            const std::string error_message = "Chat rooms was not found. Chat rooms are never removed and so"
                                              " this chat room should always exist.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    find_single_exception_string, error_message,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    "chatRoomId", chat_room_id,
                    "user_account_OID", user_account_oid
            );
            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        bsoncxx::document::view chat_room_header_view = find_single_chat_room->view();
        bsoncxx::array::view matching_oid_array;
        auto matchingOidElement = chat_room_header_view[chat_room_header_keys::MATCHING_OID_STRINGS];
        if (matchingOidElement
            && matchingOidElement.type() == bsoncxx::type::k_array
        ) { //if element exists and is type array
            matching_oid_array = matchingOidElement.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(
                    __LINE__, __FILE__,
                    matchingOidElement, chat_room_header_view,
                    bsoncxx::type::k_array, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
            );
            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        std::string matching_account_oid;
        for (const auto& val : matching_oid_array) {
            if (val.type() == bsoncxx::type::k_utf8) {
                const std::string account_oid = val.get_string().value.to_string();

                if (user_account_oid.to_string() != account_oid) {
                    matching_account_oid = account_oid;
                    break;
                }
            } else {
                logElementError(
                        __LINE__, __FILE__,
                        matchingOidElement, chat_room_header_view,
                        bsoncxx::type::k_array, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                );
                session->abort_transaction();
                set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
                return false;
            }
        }

        if (matching_account_oid.empty()) {
            const std::string error_message = "Matching OID array did not contain a valid oid.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    find_single_exception_string, error_message,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    "chatRoomId", chat_room_id,
                    "user_account_OID", user_account_oid,
                    "matching_oid_array", matching_oid_array
            );
            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        //leaveChatRoom() is already inside a transaction.
        //This will set the return status and timestamp has already been set
        // (these are the only 2 fields inside the response at the moment).
        return unMatchHelper(
                accounts_db,
                chat_room_db,
                session,
                user_accounts_collection,
                user_account_doc,
                user_account_oid,
                bsoncxx::oid(matching_account_oid),
                chat_room_id,
                current_timestamp,
                set_return_status
        );
    }

    return true;
}