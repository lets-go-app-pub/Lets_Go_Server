// Created by jeremiah on 3/20/21.
//
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <extract_thumbnail_from_verified_doc.h>

#include "chat_room_commands_helper_functions.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "user_account_keys.h"
#include "chat_room_header_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//will un-match userAccountOID and matchedAccountOID and set the return status through the lambda setReturnStatus
bool unMatchHelper(
        mongocxx::database& accounts_db,
        mongocxx::database& chat_room_db,
        mongocxx::client_session* const session,
        mongocxx::collection& user_accounts_collection,
        const bsoncxx::document::view& current_user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::oid& matched_account_oid,
        const std::string& chat_room_id,
        std::chrono::milliseconds& current_timestamp,
        const std::function<void(const ReturnStatus&, const std::chrono::milliseconds&)>& set_return_status
) {

    if(session == nullptr) {
        const std::string error_string = "unMatchHelper() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "user_OID", user_account_oid,
                "matched_OID", matched_account_oid
        );
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    //NOTE: Cannot do this in 1 command because a picture needs to be extracted for the account thumbnails,
    // and they need to set those to update the account when they leave.

    bsoncxx::stdx::optional<bsoncxx::document::value> matched_user_account_doc_value;
    try {
        mongocxx::options::find opts;

        opts.projection(
            document{}
                << user_account_keys::PICTURES << 1
            << finalize
        );

        //NOTE: this should not check and see if the users exist inside the 'match made' array
        // this is because I want to extract them either way to see if they exist and extract the thumbnail, a check comes in the chat room lookup
        matched_user_account_doc_value = user_accounts_collection.find_one(
            *session,
            document{}
                << "_id" << bsoncxx::types::b_oid{matched_account_oid}
            << finalize
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), e.what(),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid,
                "matched_OID", matched_account_oid
        );
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    //NOTE: If the matched_user_account_doc_view is empty then it will handle it below.

    int user_thumbnail_size = 0;
    std::string user_thumbnail_reference_oid;
    int match_thumbnail_size = 0;
    std::string match_thumbnail_reference_oid;

    ExtractThumbnailAdditionalCommands extract_thumbnail_additional_commands;
    extract_thumbnail_additional_commands.setupForAddToSet(chat_room_id);
    extract_thumbnail_additional_commands.getOnlyRequestThumbnailSize();

    const auto set_user_thumbnail_value = [&](
            std::string& /*thumbnail*/,
            int _thumbnail_size,
            const std::string& _thumbnail_reference_oid,
            const int /*index*/,
            const std::chrono::milliseconds& /*thumbnail_timestamp*/
    ) {
        user_thumbnail_size = _thumbnail_size;
        user_thumbnail_reference_oid = _thumbnail_reference_oid;
    };

    //extract user thumbnail requires PICTURES projection
    if (!extractThumbnailFromUserAccountDoc(
            accounts_db,
            current_user_account_doc_view,
            user_account_oid,
            session,
            set_user_thumbnail_value,
            extract_thumbnail_additional_commands)
    ) { //failed to extract thumbnail
        //errors are already stored
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    if (matched_user_account_doc_value) { //if the matched document was found

        const auto set_match_thumbnail_value = [&](
                std::string& /*thumbnail*/,
                int _thumbnail_size,
                const std::string& _thumbnail_reference_oid,
                const int /*index*/,
                const std::chrono::milliseconds& /*thumbnail_timestamp*/
        ) {
            match_thumbnail_size = _thumbnail_size;
            match_thumbnail_reference_oid = _thumbnail_reference_oid;
        };

        const bsoncxx::document::view matched_user_account_doc_view = matched_user_account_doc_value->view();

        //extract match thumbnail requires PICTURES projection
        if (!extractThumbnailFromUserAccountDoc(
                accounts_db,
                matched_user_account_doc_view,
                matched_account_oid,
                session,
                set_match_thumbnail_value,
                extract_thumbnail_additional_commands)
        ) { //failed to extract thumbnail
            //errors are already stored
            session->abort_transaction();
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return false;
        }

        grpc_chat_commands::ClientMessageToServerRequest generated_request;

        generated_request.set_message_uuid(generateUUID());
        generated_request.mutable_message()->mutable_message_specifics()->mutable_match_canceled_message()->set_matched_account_oid(matched_account_oid.to_string());

        const SendMessageToChatRoomReturn send_msg_return_val = sendMessageToChatRoom(
                &generated_request,
                chat_room_collection,
                user_account_oid,
                session);

        switch (send_msg_return_val.successful) {
            case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                const std::string error_string = "Message UUID already existed when un match attempted to send it.\n";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "ObjectID_used", user_account_oid,
                        "message", generated_request.DebugString()
                );
                //The message uuid was randomly generated above, this should never happen.
                session->abort_transaction();
                set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
                return false;
            }
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                current_timestamp = send_msg_return_val.time_stored_on_server;
                break;
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED:
                session->abort_transaction();
                set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
                return false;
        }
    }
    else { //if the matched document was not found
        //This shouldn't really happen, (unless by chance where 2 people un-match each other simultaneously)
        const std::string error_string = "Matched account was not found.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid,
                "matched_OID", matched_account_oid
        );

        //NOTE: this is set up to continue here
    }

    bsoncxx::stdx::optional<bsoncxx::document::value> findAndUpdateChatRoomAccount;
    try {

        mongocxx::options::find_one_and_update opts;

        const std::string USER_ELEM = "e";
        const std::string MATCH_ELEM = "m";

        //check if the user current_object_oid is inside the chat room
        bsoncxx::builder::basic::array arrayBuilder{};
        arrayBuilder.append(
            document{}
                << USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                << "$or" << open_array
                    << open_document
                        << USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                        << bsoncxx::types::b_int32{ (int) AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM }
                    << close_document
                    << open_document
                        << USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                        << bsoncxx::types::b_int32{ (int) AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN }
                    << close_document
                << close_array
            << finalize
        );

        //check if the matched current_object_oid is inside the chat room
        arrayBuilder.append(
            document{}
                << MATCH_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << matched_account_oid
                << "$or" << open_array
                    << open_document
                        << MATCH_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                        << bsoncxx::types::b_int32{ (int) AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM }
                    << close_document
                    << open_document
                        << MATCH_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                        << bsoncxx::types::b_int32{ (int) AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN }
                    << close_document
                << close_array
            << finalize
        );

        opts.array_filters(arrayBuilder.view());

        //project the admin
        //NOTE: would prefer to project out the single element inside the array AND only the header account OID, however
        // starting in MongoDB 4.4 I need an aggregation pipeline to do that (mentioned inside the $slice documentation).
        // so I need to either take a single element which matches the parameters (which would extract the user thumbnail)
        // OR take all array elements however only the account OID and account state
        opts.projection(
            document{}
                << "_id" << 0
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << 1
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + "." + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << 1
            << finalize
        );

        const auto mongocxxDate = bsoncxx::types::b_date{current_timestamp};

        //if chat room exists AND this chat room is a match still (the part where CHAT_ROOM_HEADER_IS_MADE_MATCH
        // is there on purpose so people can't abuse this function to kick others) remove the user that sent
        // the message and the other user from the chat room
        //NOTE: $[<identifier>] types don't actually seem to throw an error (in C++ at least) if the identifier is not found
        const std::string USER_ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + USER_ELEM + "].";
        const std::string MATCH_ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + MATCH_ELEM + "].";
        findAndUpdateChatRoomAccount = chat_room_collection.find_one_and_update(
                 *session,
                 document{}
                     << "_id" << chat_room_header_keys::ID
                     << chat_room_header_keys::MATCHING_OID_STRINGS << user_account_oid.to_string()
                 << finalize,
                 document{}
                     << "$push" << open_document
                         << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << mongocxxDate
                         << MATCH_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << mongocxxDate
                     << close_document
                     << "$max" << open_document
                         << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongocxxDate
                         << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongocxxDate
                     << close_document
                     << "$set" << open_document
                         << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
                         << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << user_thumbnail_size
                         << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << mongocxxDate
                         << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::types::b_string{user_thumbnail_reference_oid}
                         << MATCH_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
                         << MATCH_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << match_thumbnail_size
                         << MATCH_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << mongocxxDate
                         << MATCH_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::types::b_string{match_thumbnail_reference_oid}
                         << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
                     << close_document
                 << finalize,
                 opts
            );

    }
    catch (const mongocxx::logic_error& e) {
        const std::string error_string = "Could not find chat room to update, either chat room was not a 'match made' type or chat room did not exist.\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(e.what()), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "user_OID", user_account_oid,
                "matched_OID", matched_account_oid
        );
        session->abort_transaction();
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    //Want this block to go before anything happens to user accounts, if header was not updated do not update user account.
    if(!findAndUpdateChatRoomAccount) {
        const std::string error_string = "Could not find chat room to update, either chat room was not a 'match made' type or chat room did not exist.\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "user_OID", user_account_oid,
                "matched_OID", matched_account_oid
        );
        set_return_status(ReturnStatus::SUCCESS, current_timestamp);
        return true;
    }

    bsoncxx::stdx::optional<mongocxx::result::update> update_user_account;
    try {
        //clear both user accounts of any chat room references and 'match made' references
        update_user_account = user_accounts_collection.update_many(
            *session,
            document{}
                << "_id" << open_document
                    << "$in" << open_array
                        << user_account_oid
                        << matched_account_oid
                    << close_array
                << close_document
            << finalize,
            document{}
                << "$pull" << open_document
                    << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << open_document
                        << user_account_keys::other_accounts_matched_with::OID_STRING << open_document
                            << "$in" << open_array
                                << bsoncxx::types::b_string{user_account_oid.to_string() }
                                << bsoncxx::types::b_string{matched_account_oid.to_string() }
                            << close_array
                        << close_document
                    << close_document
                    << user_account_keys::CHAT_ROOMS << open_document
                        << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                    << close_document
                << close_document
           << finalize
       );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "user_OID", user_account_oid,
                "matched_OID", matched_account_oid
        );

        session->abort_transaction();
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    const bsoncxx::document::view chat_room_header_doc_view = findAndUpdateChatRoomAccount->view();

    if(!extractUserAndRemoveChatRoomIdFromUserPicturesReference(
            accounts_db,
            chat_room_header_doc_view,
            user_account_oid,
            user_thumbnail_reference_oid,
            chat_room_id,
            session)
    ) {
        session->abort_transaction();
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    //NOTE: This is set up to continue even if the match is not found (should be rare). This function
    // should still be run, the matched user element will still exist inside the chat room header.
    if (!extractUserAndRemoveChatRoomIdFromUserPicturesReference(
            accounts_db,
            chat_room_header_doc_view,
            matched_account_oid,
            match_thumbnail_reference_oid,
            chat_room_id,
            session)
    ) {

        session->abort_transaction();
        set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
        return false;
    }

    //It is possible for the user account to not exist (if this is called from delete) or for
    // the match account to not exist or for neither to exist (should be very rare).
    if (!update_user_account) {
        const std::string error_string = std::string("Chat room or 'match made' list item was found inside chat room but")
                                              .append(" not inside user account.\nmatched_user_account_doc_view: ")
                                              .append((matched_user_account_doc_value?"true\n":"false\n"));
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection_name", chat_room_collection.name().to_string(),
                "ObjectID_used", user_account_oid,
                "matched_count", std::to_string((*update_user_account).matched_count()),
                "modified_count", std::to_string((*update_user_account).modified_count())
        );
    }

    set_return_status(ReturnStatus::SUCCESS, current_timestamp);

    return true;
}
