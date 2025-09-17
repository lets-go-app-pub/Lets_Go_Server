//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <utility_testing_functions.h>
#include <utility_chat_functions.h>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>
#include <accepted_mime_types.h>
#include <extract_data_from_bsoncxx.h>
#include <build_debug_string_response.h>

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "helper_objects/chat_room_commands_helper_objects.h"
#include "helper_functions/chat_room_commands_helper_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "user_account_keys.h"
#include "chat_message_pictures_keys.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "chat_room_header_keys.h"
#include "chat_room_message_keys.h"
#include "client_message_to_server_helper_functions/client_message_to_server_helper_functions.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void clientMessageToServerImplementation(grpc_chat_commands::ClientMessageToServerRequest* request,
                                         grpc_chat_commands::ClientMessageToServerResponse* response);

enum EditedOrDeletedMessagePassed {
    TYPE_OF_MESSAGE_PASSED_EDITED,
    TYPE_OF_MESSAGE_PASSED_DELETED
};

/** This function is set up to be called from inside a transaction. **/
ReturnStatus runEditedMessageOrDeleteOneMessage(
        const bsoncxx::oid& userAccountOID,
        const std::string& uuidOfModifiedMessage,
        mongocxx::client_session* session,
        const grpc_chat_commands::ClientMessageToServerRequest* request,
        mongocxx::collection& chatRoomCollection,
        const std::string& chatRoomId,
        SendMessageToChatRoomReturn& messageReturnVal,
        std::chrono::milliseconds& current_timestamp,
        ReplyChatMessageInfo* replyChatMessageInfo,
        EditedOrDeletedMessagePassed edited_or_deleted,
        const bsoncxx::document::view& findMessageDocument,
        const std::function<bsoncxx::document::value(
                const std::chrono::milliseconds& timestamp_stored)>& get_update_message_document,
        std::unique_ptr<bsoncxx::document::value>& chatRoomHeaderDocValue,
        bsoncxx::document::view& chatRoomHeaderDocView
);

inline void setTransactionSuccess(
        grpc_chat_commands::ClientMessageToServerResponse* response,
        const std::string& picture_oid,
        const std::chrono::milliseconds time_message_stored_on_server,
        bool& transaction_successful
        ) {

    response->set_return_status(ReturnStatus::SUCCESS);

    response->set_picture_oid(picture_oid); //picture_oid will be empty if not picture type oid
    response->set_timestamp_stored(time_message_stored_on_server.count());
    transaction_successful = true;
}

inline void setTransactionError(
        grpc_chat_commands::ClientMessageToServerResponse* response,
        mongocxx::client_session* session,
        ReturnStatus return_status,
        bool& transaction_successful
        ) {
    //Abort will not return from the lambda, it will simply cancel all pending operations for
    // the database.
    session->abort_transaction();
    response->set_return_status(return_status);
    transaction_successful = false;
}

//primary function for ClientMessageToServerRPC, called from gRPC server implementation
void clientMessageToServer(grpc_chat_commands::ClientMessageToServerRequest* request,
                           grpc_chat_commands::ClientMessageToServerResponse* response) {

    handleFunctionOperationException(
            [&] {
                clientMessageToServerImplementation(
                        request,
                        response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);

}

void clientMessageToServerImplementation(grpc_chat_commands::ClientMessageToServerRequest* request,
                                         grpc_chat_commands::ClientMessageToServerResponse* response) {

    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationID;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            userAccountOIDStr,
            loginTokenStr,
            installationID
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(basicInfoReturnStatus);
        return;
    }

    std::string chatRoomId = request->message().standard_message_info().chat_room_id_message_sent_from();

    if (isInvalidChatRoomId(chatRoomId)
        ||
        (request->message().message_specifics().message_body_case() != MessageSpecifics::kUpdateObservedTimeMessage
         && isInvalidUUID(request->message_uuid()))
            ) {
        response->set_timestamp_stored(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    ReplyChatMessageInfo* replyChatMessageInfo = nullptr;

    switch (request->message().message_specifics().message_body_case()) {
        case MessageSpecifics::kTextMessage: {

            //trim trailing whitespace to be consistent with client
            trimTrailingWhitespace(
                    *request->mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_message_text());

            if (request->message().message_specifics().text_message().message_text().empty()
                || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_MESSAGE <
                   request->message().message_specifics().text_message().message_text().size()
                    ) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }

            if (request->message().message_specifics().text_message().active_message_info().is_reply()
                    ) { //if this is a reply and the reply is invalid
                replyChatMessageInfo = request->mutable_message()->mutable_message_specifics()->mutable_text_message()->mutable_active_message_info()->mutable_reply_info();
            }

            break;
        }
        case MessageSpecifics::kPictureMessage:
            if (request->message().message_specifics().picture_message().picture_file_size() !=
                (int) request->message().message_specifics().picture_message().picture_file_in_bytes().size()) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::CORRUPTED_FILE);
                return;
            }

            if (
                    request->message().message_specifics().picture_message().image_width() == 0
                    || request->message().message_specifics().picture_message().image_height() == 0
                    || request->message().message_specifics().picture_message().picture_file_size() == 0
                    || server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_PICTURE_SIZE_IN_BYTES <
                       request->message().message_specifics().picture_message().picture_file_size()
                    ) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }

            if (request->message().message_specifics().picture_message().active_message_info().is_reply()
                    ) { //if this is a reply and the reply is invalid
                replyChatMessageInfo = request->mutable_message()->mutable_message_specifics()->mutable_picture_message()->mutable_active_message_info()->mutable_reply_info();
            }

            break;
        case MessageSpecifics::kLocationMessage:
            if (isInvalidLocation(request->message().message_specifics().location_message().longitude(),
                                  request->message().message_specifics().location_message().latitude())) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }

            if (request->message().message_specifics().location_message().active_message_info().is_reply()
                    ) { //if this is a reply and the reply is invalid
                replyChatMessageInfo = request->mutable_message()->mutable_message_specifics()->mutable_location_message()->mutable_active_message_info()->mutable_reply_info();
            }

            break;
        case MessageSpecifics::kMimeTypeMessage:

            if (
                    request->message().message_specifics().mime_type_message().image_width() == 0 ||
                    request->message().message_specifics().mime_type_message().image_height() == 0 ||

                    largest_mime_type_size <
                    request->message().message_specifics().mime_type_message().mime_type().size() ||
                    accepted_mime_types.find(request->message().message_specifics().mime_type_message().mime_type()) ==
                    accepted_mime_types.end() ||

                    request->message().message_specifics().mime_type_message().url_of_download().empty() ||
                    server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES <
                    request->message().message_specifics().mime_type_message().url_of_download().size()
                    ) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }

            if (request->message().message_specifics().mime_type_message().active_message_info().is_reply()
                    ) { //if this is a reply and the reply is invalid
                replyChatMessageInfo = request->mutable_message()->mutable_message_specifics()->mutable_mime_type_message()->mutable_active_message_info()->mutable_reply_info();
            }

            break;
        case MessageSpecifics::kInviteMessage:
            if (
                    isInvalidOIDString(
                            request->message().message_specifics().invite_message().invited_user_account_oid())

                    || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES <
                       request->message().message_specifics().invite_message().invited_user_name().size() ||

                    isInvalidChatRoomId(request->message().message_specifics().invite_message().chat_room_id())

                    || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES <
                       request->message().message_specifics().invite_message().chat_room_name().size()

                    || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES <
                       request->message().message_specifics().invite_message().chat_room_password().size()
                    ) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }

            if (request->message().message_specifics().invite_message().active_message_info().is_reply()
                    ) { //if this is a reply and the reply is invalid
                replyChatMessageInfo = request->mutable_message()->mutable_message_specifics()->mutable_invite_message()->mutable_active_message_info()->mutable_reply_info();
            }

            break;
        case MessageSpecifics::kEditedMessage:
            if (
                    request->message().message_specifics().edited_message().new_message().empty()
                    || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES <
                       request->message().message_specifics().edited_message().new_message().size() ||

                    isInvalidUUID(request->message().message_specifics().edited_message().message_uuid())
                    ) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }
            break;
        case MessageSpecifics::kDeletedMessage:
            if (
                    (request->message().message_specifics().deleted_message().delete_type() !=
                     DeleteType::DELETE_FOR_ALL_USERS
                     && request->message().message_specifics().deleted_message().delete_type() !=
                        DeleteType::DELETE_FOR_SINGLE_USER) ||

                    isInvalidUUID(request->message().message_specifics().deleted_message().message_uuid())
                    ) {
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }
            break;
        case MessageSpecifics::kUpdateObservedTimeMessage:
        case MessageSpecifics::kUserActivityDetectedMessage: //this type is currently not used, however it is valid
            break;

        case MessageSpecifics::kUserKickedMessage:
        case MessageSpecifics::kUserBannedMessage:
        case MessageSpecifics::kDifferentUserJoinedMessage:
        case MessageSpecifics::kDifferentUserLeftMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
        case MessageSpecifics::kThisUserLeftChatRoomMessage:
        case MessageSpecifics::kChatRoomNameUpdatedMessage:
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
        case MessageSpecifics::kNewAdminPromotedMessage:
        case MessageSpecifics::kNewPinnedLocationMessage:
        case MessageSpecifics::kMatchCanceledMessage:
        case MessageSpecifics::kNewUpdateTimeMessage:
        case MessageSpecifics::kHistoryClearedMessage:
        case MessageSpecifics::kLoadingMessage:
        case MessageSpecifics::kChatRoomCapMessage:
        case MessageSpecifics::MESSAGE_BODY_NOT_SET:
        default: //NOTE: default must be here, otherwise a message case that is not any of the above could be passed.
            response->set_timestamp_stored(-2L);
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
    }

    if (replyChatMessageInfo != nullptr) {

        if (
            isInvalidOIDString(replyChatMessageInfo->reply_is_sent_from_user_oid()) ||

            isInvalidUUID(replyChatMessageInfo->reply_is_to_message_uuid()) ||

            !chatRoomMessageIsAbleToReply(request->message().message_specifics().message_body_case())
        ) {
            response->set_timestamp_stored(-2L);
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
        }

        switch (replyChatMessageInfo->reply_specifics().reply_body_case()) {
            case ReplySpecifics::kPictureReply:
                if ((int) replyChatMessageInfo->reply_specifics().picture_reply().thumbnail_in_bytes().size() !=
                    replyChatMessageInfo->reply_specifics().picture_reply().thumbnail_file_size()) {
                    response->set_return_status(ReturnStatus::CORRUPTED_FILE);
                    return;
                }

                if (replyChatMessageInfo->reply_specifics().picture_reply().thumbnail_file_size() == 0
                    || server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES <
                       replyChatMessageInfo->reply_specifics().picture_reply().thumbnail_file_size()) {
                    response->set_timestamp_stored(-2L);
                    response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                    return;
                }
                break;
            case ReplySpecifics::kMimeReply:
                if ((int) replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_in_bytes().size() !=
                    replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_file_size()) {
                    response->set_return_status(ReturnStatus::CORRUPTED_FILE);
                    return;
                }

                if (replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_file_size() == 0
                    || server_parameter_restrictions::MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES <
                       replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_file_size()
                    //NOTE: This could be checked for the known mime types, but it doesn't actually matter much. This
                    // is because the client will simply display the passed thumbnail regardless of the type. The only
                    // thing it is user for is showing little messages like 'sticker type'. However, 'image type' is
                    // always the backup.
                    || replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_mime_type().empty()
                    || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES <
                       replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_mime_type().size()
                        ) {
                    response->set_timestamp_stored(-2L);
                    response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                    return;
                }

                break;
            case ReplySpecifics::kLocationReply:
            case ReplySpecifics::kInviteReply:
            case ReplySpecifics::kTextReply: //NOTE this will be trimmed if it is too long
                break;
            case ReplySpecifics::REPLY_BODY_NOT_SET:
            default: //NOTE: default must be here, otherwise a reply case that is not any of the above could be passed.
                response->set_timestamp_stored(-2L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
        }
    }

    response->set_timestamp_stored(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chatRoomCollection = chatRoomDB[collection_names::CHAT_ROOM_ID_ + chatRoomId];

    const bsoncxx::oid userAccountOID(userAccountOIDStr); //the OID of the verified account

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::chrono::milliseconds timestampObserved;

    if (general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count() < request->timestamp_observed()
        && request->timestamp_observed() <= current_timestamp.count()
            ) { //if timestamp is valid
        timestampObserved = std::chrono::milliseconds{request->timestamp_observed()};
    } else {
        timestampObserved = current_timestamp;
    }

    std::shared_ptr<std::vector<bsoncxx::document::value>> appendToAndStatementDoc = std::make_shared<std::vector<bsoncxx::document::value>>();

    //checks if user is a member of chat room
    bsoncxx::document::value userRequiredInChatRoomCondition = buildUserRequiredInChatRoomCondition(chatRoomId);
    appendToAndStatementDoc->emplace_back(userRequiredInChatRoomCondition.view());

    //update the observed time of the passed chat room
    const bsoncxx::document::value mergeDocument = document{}
            << user_account_keys::CHAT_ROOMS << open_document
                << "$map" << open_document
                    << "input" << "$" + user_account_keys::CHAT_ROOMS
                    << "as" << "chatRoom"
                    << "in" << open_document
                        << "$cond" << open_document

                            //if the chat room is the chat room ID
                            << "if" << open_document
                                << "$eq" << open_array
                                    << "$$chatRoom." + user_account_keys::chat_rooms::CHAT_ROOM_ID
                                    << chatRoomId
                                << close_array
                            << close_document

                            //update the chat room
                            << "then" << open_document
                                << "$mergeObjects" << open_array
                                    << "$$chatRoom"
                                    << open_document
                                        << user_account_keys::chat_rooms::LAST_TIME_VIEWED << open_document
                                            << "$max" << open_array
                                                << bsoncxx::types::b_date{timestampObserved}
                                                << "$$chatRoom." + user_account_keys::chat_rooms::LAST_TIME_VIEWED
                                            << close_array
                                        << close_document
                                    << close_document
                                << close_array
                            << close_document

                            //return the same document
                            << "else" << "$$chatRoom"

                        << close_document
                    << close_document
                << close_document
            << close_document
        << finalize;

    const bsoncxx::document::value projectionDocument = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << 1
        << finalize;

    const bsoncxx::document::value loginDocument = getLoginDocument<false>(
        loginTokenStr,
        installationID,
        mergeDocument,
        current_timestamp,
        appendToAndStatementDoc
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccount;

    if (!runInitialLoginOperation(
            findUserAccount,
            userAccountsCollection,
            userAccountOID,
            loginDocument,
            projectionDocument)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, userAccountOIDStr);

    if (returnStatus == ReturnStatus::UNKNOWN) { //this means user was not part of chat room
        //NOTE: this could easily happen, because the messages are saved to be stored later a user could
        // 1) not be connected to the internet and send a message
        // 2) gets kicked (or leaves on a different device)
        // 3) reconnects to the internet

        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_user_not_in_chat_room(true);
    }
    else if (returnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(returnStatus);
    }
    else {

        //NOTE: this does not need to be inside a transaction because it does not connect 2 pieces of this being
        // the account and transaction, even if it sets up a match, when the chat room header is updated the matching
        // array is always set to null

        if (request->message().message_specifics().message_body_case() ==
            MessageSpecifics::MessageBodyCase::kUpdateObservedTimeMessage) {

            //for kUpdateObservedTimeMessage timestamp_stored is request->timestamp_observed() (unless it was invalid)
            response->set_timestamp_stored(timestampObserved.count());
            response->set_return_status(ReturnStatus::SUCCESS);
        }
        else { //user account exists and not a message to only update observed time

            //parameters that may be set depending on the message type
            std::string insertedPictureOID;
            std::string previousMessage;
            std::chrono::milliseconds modifiedMessageCreatedTime = std::chrono::milliseconds{0};

            if (replyChatMessageInfo != nullptr) { //if this is a reply to another message

                MessageSpecifics::MessageBodyCase messageType = MessageSpecifics::MessageBodyCase::MESSAGE_BODY_NOT_SET;

                switch (replyChatMessageInfo->reply_specifics().reply_body_case()) {
                    case ReplySpecifics::kTextReply:
                        messageType = MessageSpecifics::MessageBodyCase::kTextMessage;
                        break;
                    case ReplySpecifics::kPictureReply:
                        messageType = MessageSpecifics::MessageBodyCase::kPictureMessage;
                        break;
                    case ReplySpecifics::kLocationReply:
                        messageType = MessageSpecifics::MessageBodyCase::kLocationMessage;
                        break;
                    case ReplySpecifics::kMimeReply:
                        messageType = MessageSpecifics::MessageBodyCase::kMimeTypeMessage;
                        break;
                    case ReplySpecifics::kInviteReply:
                        messageType = MessageSpecifics::MessageBodyCase::kInviteMessage;
                        break;
                    case ReplySpecifics::REPLY_BODY_NOT_SET:
                        response->set_timestamp_stored(-2L);
                        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                        return;
                }

                std::optional<std::string> checkMessageFromAccountExceptionString;
                long num_messages = -1;
                try {

                    //make sure the message that is being replied to exists inside the database and is the correct type
                    num_messages = chatRoomCollection.count_documents(
                        document{}
                            << "_id" << replyChatMessageInfo->reply_is_to_message_uuid()
                            << chat_room_message_keys::MESSAGE_SENT_BY << bsoncxx::oid{replyChatMessageInfo->reply_is_sent_from_user_oid()}
                            << chat_room_message_keys::MESSAGE_TYPE << messageType
                        << finalize
                    );
                }
                catch (const mongocxx::logic_error& e) {
                    checkMessageFromAccountExceptionString = std::string(e.what());
                }

                //if message was NOT found, do not store this reply
                if(num_messages < 1) {

                    const std::string errorString = std::string("A reply was passed into the server which was not found in the database.")
                            .append("_id: ").append(replyChatMessageInfo->reply_is_to_message_uuid()).append("\n")
                            .append(chat_room_message_keys::MESSAGE_SENT_BY).append(": ").append(replyChatMessageInfo->reply_is_sent_from_user_oid()).append("\n")
                            .append(chat_room_message_keys::MESSAGE_TYPE).append(": ").append(convertMessageBodyTypeToString(messageType)).append("\n");

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), errorString,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ +
                            request->message().message_specifics().invite_message().chat_room_id(),
                            "chat_Room_ID", chatRoomId,
                            "num_messages", std::to_string(num_messages)
                    );

                    //accessed for storing error, set to null AFTER the error is stored
                    replyChatMessageInfo = nullptr;

                    //can continue here, will ignore reply information
                }
            }

            //Make sure invited message chat room exists.
            if (request->message().message_specifics().message_body_case() == MessageSpecifics::kInviteMessage) {
                mongocxx::collection inviteChatRoomCollection = chatRoomDB[collection_names::CHAT_ROOM_ID_ +
                                                                           request->message().message_specifics().invite_message().chat_room_id()];

                unsigned long numDocs = 0;
                std::optional<std::string> countDocumentsExceptionString;
                try {
                    //make sure the chat room info is correct
                    numDocs = inviteChatRoomCollection.count_documents(
                        buildFilterForCheckingInviteHeader(request)
                    );
                }
                catch (const mongocxx::logic_error& e) {
                    countDocumentsExceptionString = e.what();
                }

                if (numDocs <= 0) { //if chat room was not found

                    //NOTE: If the chat room name or password was changed as this message was sent this could happen.

                    const std::string errorString = std::string("An INVITED_TO_CHAT_ROOM type chat message was sent with incorrect info.\nmessage_value_chat_room_id: ")
                        .append(request->message().message_specifics().invite_message().chat_room_id())
                        .append("\nmessage_value_chat_room_name: ")
                        .append(request->message().message_specifics().invite_message().chat_room_name())
                        .append("\nmessage_value_chat_room_password: ")
                        .append(request->message().message_specifics().invite_message().chat_room_password())
                        .append("\n");

                    storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        countDocumentsExceptionString, errorString,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::CHAT_ROOM_ID_ + request->message().message_specifics().invite_message().chat_room_id(),
                        "chat_Room_ID", chatRoomId,
                        "document_count", std::to_string(numDocs)
                    );

                    //can still respond with success, the invite is as successful as it can be
                    response->set_return_status(ReturnStatus::SUCCESS);
                    response->set_timestamp_stored(current_timestamp.count());
                    return;
                }
            }

            bool transaction_successful = false;

            //NOTE: this transaction is here to avoid inconsistencies between the chat room header and the user account info
            mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

                //upsert picture if picture message
                if (request->message().message_specifics().message_body_case() == MessageSpecifics::kPictureMessage) {
                    mongocxx::collection chatRoomPicturesCollection = chatRoomDB[collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

                    //NOTE: was checked above for corrupted file
                    std::optional<std::string> findMessagesExceptionString;
                    bsoncxx::stdx::optional<mongocxx::result::insert_one> insertDocument;
                    try {

                        std::string* picture_in_bytes = request->mutable_message()->mutable_message_specifics()->mutable_picture_message()->release_picture_file_in_bytes();

                        if(picture_in_bytes == nullptr) {

                            const std::string errorString = "A chat room picture value was null inside the request.\n";

                            storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), errorString,
                                "request", buildDebugStringResponse(request),
                                "Searched_OID", userAccountOID
                            );

                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );
                            return;
                        }

                        document insert_doc{};
                        insert_doc
                            << chat_message_pictures_keys::CHAT_ROOM_ID << chatRoomId
                            << chat_message_pictures_keys::PICTURE_SIZE_IN_BYTES << request->message().message_specifics().picture_message().picture_file_size()
                            << chat_message_pictures_keys::PICTURE_IN_BYTES << std::move(*picture_in_bytes)
                            << chat_message_pictures_keys::WIDTH << request->message().message_specifics().picture_message().image_width()
                            << chat_message_pictures_keys::HEIGHT << request->message().message_specifics().picture_message().image_height()
                            << chat_message_pictures_keys::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp};

                        delete picture_in_bytes;

                        insertDocument = chatRoomPicturesCollection.insert_one(*session, insert_doc.view());
                    }
                    catch (const mongocxx::logic_error& e) {
                        findMessagesExceptionString = e.what();
                    }

                    if (insertDocument &&
                        insertDocument->result().inserted_count() == 1) { //if document was successfully inserted
                        insertedPictureOID = insertDocument->inserted_id().get_oid().value.to_string();
                    } else { //if document failed to be inserted

                        std::string errorString = "A PICTURE_MESSAGE type chat message failed when inserted.\n";
                        errorString += "inserted_count: ";
                        errorString += std::to_string(insertDocument->result().inserted_count());
                        errorString += '\n';

                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                findMessagesExceptionString, errorString,
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME,
                                "chat_Room_ID", chatRoomId,
                                "picture_file_size", std::to_string(
                                        request->message().message_specifics().picture_message().picture_file_in_bytes().size()));

                        //return error if picture was not saved

                        setTransactionError(
                                response,
                                session,
                                ReturnStatus::LG_ERROR,
                                transaction_successful
                                );
                        return;
                    }
                }

                std::unique_ptr<bsoncxx::document::value> chatRoomHeaderDocValue;
                bsoncxx::document::view chatRoomHeaderDocView;
                SendMessageToChatRoomReturn messageReturnVal;

                bsoncxx::stdx::optional<bsoncxx::document::value> findAndUpdateChatRoomTimes;
                if (request->message().message_specifics().message_body_case() ==
                    MessageSpecifics::kEditedMessage) { //edited message type

                    auto get_update_message_doc = [&request]
                            (const std::chrono::milliseconds& timestamp_stored)->bsoncxx::document::value {
                        return document{}
                            << chat_room_message_keys::message_specifics::TEXT_MESSAGE << open_document
                                << "$literal" << request->message().message_specifics().edited_message().new_message()
                            << close_document
                            << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << true
                            << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{timestamp_stored}
                        << finalize;
                    };

                    bsoncxx::builder::stream::document findMessageDocument;
                    findMessageDocument
                            << "_id" << request->message().message_specifics().edited_message().message_uuid()
                            << chat_room_message_keys::MESSAGE_TYPE << (int) MessageSpecifics::MessageBodyCase::kTextMessage
                            << chat_room_message_keys::MESSAGE_SENT_BY << userAccountOID;

                    ReturnStatus status = runEditedMessageOrDeleteOneMessage(
                            userAccountOID,
                            request->message().message_specifics().edited_message().message_uuid(),
                            session,
                            request,
                            chatRoomCollection,
                            chatRoomId,
                            messageReturnVal,
                            current_timestamp,
                            replyChatMessageInfo,
                            EditedOrDeletedMessagePassed::TYPE_OF_MESSAGE_PASSED_EDITED,
                            findMessageDocument.view(),
                            get_update_message_doc,
                            chatRoomHeaderDocValue,
                            chatRoomHeaderDocView
                    );

                    if(status == ReturnStatus::SUCCESS) { //For some reason (see function), there is nothing more that can be done.

                        //Want to return a timestamp (current_timestamp) because the client will use it as the edited time.
                        setTransactionSuccess(
                                response,
                                messageReturnVal.picture_oid,
                                current_timestamp,
                                transaction_successful
                                );
                        return;
                    } else if (status == ReturnStatus::INVALID_PARAMETER_PASSED) { //duplicate message uuid passed
                        //Abort the transaction because the message that was set up was not actually stored inside
                        // the database. Return the relevant info extracted from the 'original' message.
                        //Abort will not return from the lambda, it will simply cancel all pending operations for
                        // the database.
                        session->abort_transaction();

                        setTransactionSuccess(
                                response,
                                messageReturnVal.picture_oid,
                                messageReturnVal.time_stored_on_server,
                                transaction_successful
                                );

                        return;
                    } else if (status != ReturnStatus::VALUE_NOT_SET) { //error

                        response->set_timestamp_stored(current_timestamp.count());
                        setTransactionError(
                                response,
                                session,
                                status,
                                transaction_successful
                                );

                        return;
                    }
                    //else {} //continue

                }
                else if (request->message().message_specifics().message_body_case() ==
                           MessageSpecifics::kDeletedMessage) { //deleted message type

                    if (request->message().message_specifics().deleted_message().delete_type() ==
                        DeleteType::DELETE_FOR_ALL_USERS) { //if message is meant to be deleted for everyone

                        mongocxx::pipeline findDocsPipeline;

                        findDocsPipeline.match(
                            document{}
                                << "$or" << open_array

                                    //find header document
                                    << matchChatRoomUserInside(userAccountOID)

                                    //find message document
                                    << open_document
                                        << "_id" << request->message().message_specifics().deleted_message().message_uuid()
                                        << chat_room_message_keys::MESSAGE_TYPE << buildMessageIsDeleteTypeDocument()
                                    << close_document

                                << close_array
                            << finalize
                        );

                        findDocsPipeline.replace_root(
                                buildClientMessageToServerDeleteForEveryoneProjectionDocument(userAccountOID));

                        //get the chat room header and edited message
                        bsoncxx::stdx::optional<mongocxx::cursor> findHeaderAndMessageDocs;
                        try {
                            //if user exists in chat room, return chat room header
                            //if message exists with proper credentials, return messages
                            //NOTE: if the collection does not exist findHeaderAndMessageDocs will not be set, so it will return false
                            findHeaderAndMessageDocs = chatRoomCollection.aggregate(*session, findDocsPipeline);
                        }
                        catch (const mongocxx::logic_error& e) {
                            std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());

                            std::optional<std::string> exceptionString = e.what();
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          exceptionString, errorString,
                                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                          "ObjectID_used", userAccountOID,
                                                          "key", userAccountOID,
                                                          "chatRoomId", chatRoomId);

                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );

                            return;
                        }

                        if (!findHeaderAndMessageDocs) {
                            std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());

                            std::optional<std::string> dummyExceptionString;
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          dummyExceptionString, errorString,
                                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                          "ObjectID_used", userAccountOID,
                                                          "key", userAccountOID,
                                                          "chatRoomId", chatRoomId);

                            //returning success because it has done as much as it can
                            setTransactionSuccess(
                                    response,
                                    "",
                                    current_timestamp,
                                    transaction_successful
                                    );
                            return;
                        }

                        std::unique_ptr<bsoncxx::document::value> messageValue;
                        bsoncxx::document::view messageView;

                        for (auto cursorDoc : *findHeaderAndMessageDocs) {
                            auto messageTextElement = cursorDoc["_id"];
                            if (messageTextElement && messageTextElement.type() ==
                                                      bsoncxx::type::k_utf8) { //if element exists and is type utf8
                                std::string doc_id = messageTextElement.get_string().value.to_string();
                                if (doc_id == chat_room_header_keys::ID) {
                                    chatRoomHeaderDocValue = std::make_unique<bsoncxx::document::value>(cursorDoc);
                                    chatRoomHeaderDocView = (*chatRoomHeaderDocValue).view();
                                } else {
                                    messageValue = std::make_unique<bsoncxx::document::value>(cursorDoc);
                                    messageView = (*messageValue).view();
                                }
                            } else { //if element does not exist or is not type oid or utf8
                                logElementError(__LINE__, __FILE__,
                                                messageTextElement,
                                                cursorDoc, bsoncxx::type::k_oid, "_id",
                                                database_names::CHAT_ROOMS_DATABASE_NAME,
                                                collection_names::CHAT_ROOM_ID_ + chatRoomId);

                                setTransactionError(
                                        response,
                                        session,
                                        ReturnStatus::LG_ERROR,
                                        transaction_successful
                                        );
                                return;
                            }
                        }

                        if (chatRoomHeaderDocView.empty()) {
                            std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());

                            std::optional<std::string> dummyExceptionString;
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          dummyExceptionString, errorString,
                                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                          "ObjectID_used", userAccountOID,
                                                          "key", userAccountOID,
                                                          "chatRoomId", chatRoomId);

                            //returning success because it has done as much as it can
                            setTransactionSuccess(
                                    response,
                                    "",
                                    current_timestamp,
                                    transaction_successful
                                    );
                            return;
                        }
                        else if (messageView.empty()) {
                            std::string errorString = "User is attempting to delete a message when message does not exist, incorrect message oid, or invalid message type.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());

                            std::optional<std::string> dummyExceptionString;
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          dummyExceptionString, errorString,
                                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                          "ObjectID_used", userAccountOID,
                                                          "key", userAccountOID,
                                                          "chatRoomId", chatRoomId);

                            //returning success because it has done as much as it can
                            setTransactionSuccess(
                                    response,
                                    "",
                                    current_timestamp,
                                    transaction_successful
                                    );
                            return;
                        }
                        else { //messageView and headerDoc found

                            bool canUpdateMessage = false;
                            AccountStateInChatRoom userAccountState;

                            auto userAccountStateInChatRoomElement = chatRoomHeaderDocView[chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM];
                            if (userAccountStateInChatRoomElement && userAccountStateInChatRoomElement.type() ==
                                                                     bsoncxx::type::k_int32) { //if element exists and is type utf8
                                userAccountState = AccountStateInChatRoom(
                                        userAccountStateInChatRoomElement.get_int32().value);
                            } else { //if element does not exist or is not type oid
                                logElementError(__LINE__, __FILE__,
                                                userAccountStateInChatRoomElement,
                                                chatRoomHeaderDocView, bsoncxx::type::k_int32,
                                                chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM,
                                                database_names::CHAT_ROOMS_DATABASE_NAME,
                                                collection_names::CHAT_ROOM_ID_ + chatRoomId);

                                setTransactionError(
                                        response,
                                        session,
                                        ReturnStatus::LG_ERROR,
                                        transaction_successful
                                        );
                                return;
                            }

                            //if user is admin or sent the message, allow 'delete' to occur for all users
                            if (userAccountState == AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN) { //user is admin
                                canUpdateMessage = true;
                            } else { //user is not admin

                                auto messageSentByOidElement = messageView[chat_room_message_keys::MESSAGE_SENT_BY];
                                if (messageSentByOidElement && messageSentByOidElement.type() ==
                                                               bsoncxx::type::k_oid) { //if element exists and is type utf8

                                    //message was sent by this user
                                    if (messageSentByOidElement.get_oid().value == userAccountOID) {
                                        canUpdateMessage = true;
                                    }
                                } else { //if element does not exist or is not type oid
                                    logElementError(__LINE__, __FILE__,
                                                    messageSentByOidElement,
                                                    messageView, bsoncxx::type::k_oid,
                                                    chat_room_message_keys::MESSAGE_SENT_BY,
                                                    database_names::CHAT_ROOMS_DATABASE_NAME,
                                                    collection_names::CHAT_ROOM_ID_ + chatRoomId);

                                    setTransactionError(
                                            response,
                                            session,
                                            ReturnStatus::LG_ERROR,
                                            transaction_successful
                                            );
                                    return;
                                }
                            }

                            auto createdTimeElement = messageView[chat_room_shared_keys::TIMESTAMP_CREATED];
                            if (createdTimeElement &&
                                createdTimeElement.type() ==
                                bsoncxx::type::k_date) { //if element exists and is type date
                                modifiedMessageCreatedTime = createdTimeElement.get_date().value;
                            } else { //if element does not exist or is not type oid
                                logElementError(__LINE__, __FILE__,
                                                createdTimeElement,
                                                messageView, bsoncxx::type::k_date,
                                                chat_room_shared_keys::TIMESTAMP_CREATED,
                                                database_names::CHAT_ROOMS_DATABASE_NAME,
                                                collection_names::CHAT_ROOM_ID_ + chatRoomId);

                                setTransactionError(
                                        response,
                                        session,
                                        ReturnStatus::LG_ERROR,
                                        transaction_successful
                                        );
                                return;
                            }

                            if (canUpdateMessage == false) {
                                std::string errorString = "User is attempting to delete a message when they are not admin and they did not send it.\n";
                                errorString += "Message Type: ";
                                errorString += convertMessageBodyTypeToString(
                                        request->message().message_specifics().message_body_case());

                                std::optional<std::string> dummyExceptionString;
                                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                              dummyExceptionString, errorString,
                                                              "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                              "collection",
                                                              collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                              "ObjectID_used", userAccountOID,
                                                              "key", userAccountOID,
                                                              "chatRoomId", chatRoomId);

                                //returning success because it has done as much as it can
                                setTransactionSuccess(
                                        response,
                                        "",
                                        current_timestamp,
                                        transaction_successful
                                        );
                                return;
                            }
                        }

                        messageReturnVal = sendMessageToChatRoom(
                                request,
                                chatRoomCollection,
                                userAccountOID,
                                session,
                                insertedPictureOID,
                                previousMessage,
                                modifiedMessageCreatedTime,
                                replyChatMessageInfo
                                );

                        switch (messageReturnVal.successful) {
                            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED: {
                                //error was already stored
                                setTransactionError(
                                        response,
                                        session,
                                        ReturnStatus::LG_ERROR,
                                        transaction_successful
                                        );

                                return;
                            }
                            case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                                //Abort the transaction because the message that was set up was not actually stored inside
                                // the database. Return the relevant info extracted from the 'original' message.
                                //Abort will not return from the lambda, it will simply cancel all pending operations for
                                // the database.
                                session->abort_transaction();

                                setTransactionSuccess(
                                        response,
                                        messageReturnVal.picture_oid,
                                        messageReturnVal.time_stored_on_server,
                                        transaction_successful
                                        );

                                return;
                            }
                            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL: {
                                //continue on if it reaches this point
                                current_timestamp = messageReturnVal.time_stored_on_server;
                                break;
                            }
                        }

                        mongocxx::pipeline deletePipeline;

                        //update the header to not be a matching chat room & update the current users last active time
                        //update the deleted message to have deleteType = DELETE_FOR_ALL_USERS
                        deletePipeline.replace_root(
                            buildClientMessageToServerDeleteForEveryoneUpdateDocument(
                                userAccountOID,
                                current_timestamp
                            )
                        );

                        //update the chat room header and deleted message
                        bsoncxx::stdx::optional<mongocxx::result::update> updateHeaderAndMessageDocs;
                        try {

                            updateHeaderAndMessageDocs = chatRoomCollection.update_many(
                                *session,
                                document{}
                                    << "_id" << open_document
                                        << "$in" << open_array
                                            << request->message().message_specifics().deleted_message().message_uuid()
                                            << chat_room_header_keys::ID
                                        << close_array
                                    << close_document
                                << finalize,
                                deletePipeline
                            );

                        }
                        catch (const mongocxx::logic_error& e) {
                            std::string errorString = "Failed to update chat room or member is not a part of chat room when they were already found.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());
                            request->AppendToString(&errorString);

                            std::optional<std::string> exceptionString = e.what();
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          exceptionString, errorString,
                                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                          "ObjectID_used", userAccountOID,
                                                          "key", userAccountOID,
                                                          "chatRoomId", chatRoomId);

                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );
                            return;
                        }

                        if (!updateHeaderAndMessageDocs) {
                            std::string errorString = "Failed to update chat room or member is not a part of chat room when they were already found.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());
                            request->AppendToString(&errorString);

                            std::optional<std::string> dummyExceptionString;
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          dummyExceptionString, errorString,
                                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                                          "ObjectID_used", userAccountOID,
                                                          "key", userAccountOID,
                                                          "chatRoomId", chatRoomId);

                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );
                            return;
                        }

                    }
                    else if (request->message().message_specifics().deleted_message().delete_type() ==
                               DeleteType::DELETE_FOR_SINGLE_USER) { //if message is only meant to be deleted for sender

                        bsoncxx::builder::stream::document findMessageDocument;
                        findMessageDocument
                                << "_id" << request->message().message_specifics().deleted_message().message_uuid()

                                //type can be deleted
                                << chat_room_message_keys::MESSAGE_TYPE << buildMessageIsDeleteTypeDocument();

                        //NOTE: delete for single user can be done by anyone, not just the sender
                        //<< MESSAGE_SENT_BY << userAccountOID;

                        bsoncxx::document::value updateMessageDocument = buildClientMessageToServerDeleteSingleUserUpdateDocument(
                                userAccountOIDStr);

                        auto get_update_message_doc = [&userAccountOIDStr]
                                (const std::chrono::milliseconds&)->bsoncxx::document::value {
                            return buildClientMessageToServerDeleteSingleUserUpdateDocument(userAccountOIDStr);
                        };

                        ReturnStatus status = runEditedMessageOrDeleteOneMessage(
                                userAccountOID,
                                request->message().message_specifics().deleted_message().message_uuid(),
                                session,
                                request,
                                chatRoomCollection,
                                chatRoomId,
                                messageReturnVal,
                                current_timestamp,
                                replyChatMessageInfo,
                                EditedOrDeletedMessagePassed::TYPE_OF_MESSAGE_PASSED_DELETED,
                                findMessageDocument.view(),
                                get_update_message_doc,
                                chatRoomHeaderDocValue,
                                chatRoomHeaderDocView
                        );

                        if (status == ReturnStatus::SUCCESS) { //For some reason (see function), there is nothing more that can be done.

                            //Want to return a timestamp (current_timestamp) because the client will use it as the deleted time.
                            setTransactionSuccess(
                                    response,
                                    messageReturnVal.picture_oid,
                                    current_timestamp,
                                    transaction_successful
                                    );
                            return;
                        } else if (status == ReturnStatus::INVALID_PARAMETER_PASSED) { //duplicate message uuid passed
                            //Abort the transaction because the message that was set up was not actually stored inside
                            // the database. Return the relevant info extracted from the 'original' message.
                            //Abort will not return from the lambda, it will simply cancel all pending operations for
                            // the database.
                            session->abort_transaction();

                            setTransactionSuccess(
                                    response,
                                    messageReturnVal.picture_oid,
                                    messageReturnVal.time_stored_on_server,
                                    transaction_successful
                                    );

                            return;
                        } else if (status != ReturnStatus::VALUE_NOT_SET) { //error
                            setTransactionError(
                                    response,
                                    session,
                                    status,
                                    transaction_successful
                                    );

                            response->set_timestamp_stored(current_timestamp.count());

                            return;
                        }
                        //else {} //continue

                    }
                    else { //delete type was not set
                        //this means either the message oid does not exist inside the chat room, or the wrong account attempted to edit it
                        std::string errorString = "A MESSAGE_DELETED message was received by the server however delete_type was not set to a valid value."
                                                  "Note that this should have been checked above as well.";

                        std::optional<std::string> exceptionString;
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                exceptionString, errorString,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                "ObjectID_used", userAccountOID,
                                "message_uuid",
                                request->message().message_specifics().deleted_message().message_uuid(),
                                "delete_type",
                                convertDeleteTypeToString(
                                        request->message().message_specifics().deleted_message().delete_type()));

                        setTransactionError(
                                response,
                                session,
                                ReturnStatus::LG_ERROR,
                                transaction_successful
                                );
                        return;
                    }

                }
                else { //not deleted or edited message type

                    messageReturnVal = sendMessageToChatRoom(
                            request,
                            chatRoomCollection,
                            userAccountOID,
                            session,
                            insertedPictureOID,
                            previousMessage,
                            modifiedMessageCreatedTime,
                            replyChatMessageInfo
                            );

                    switch (messageReturnVal.successful) {
                        case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED: {
                            //error was already stored
                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );

                            return;
                        }
                        case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                            //Abort the transaction because the message that was set up was not actually stored inside
                            // the database. Return the relevant info extracted from the 'original' message.
                            //Abort will not return from the lambda, it will simply cancel all pending operations for
                            // the database.
                            session->abort_transaction();

                            setTransactionSuccess(
                                    response,
                                    messageReturnVal.picture_oid,
                                    messageReturnVal.time_stored_on_server,
                                    transaction_successful
                                    );

                            return;
                        }
                        case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL: {
                            //continue on if it reaches this point
                            current_timestamp = messageReturnVal.time_stored_on_server;
                            break;
                        }
                    }

                    const std::string ELEM = "e";
                    const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";

                    bsoncxx::builder::stream::document updateDocument{};

                    //save times to messages
                    switch (request->message().message_specifics().message_body_case()) {
                        //message types user can directly send as 'first contact' for a match
                        case MessageSpecifics::kTextMessage:
                        case MessageSpecifics::kPictureMessage:
                        case MessageSpecifics::kLocationMessage:
                        case MessageSpecifics::kMimeTypeMessage: {

                            auto mongoDBDateObject = bsoncxx::types::b_date{
                                    std::chrono::milliseconds(current_timestamp)};

                            //Update the chat room activity time for these message types
                            updateDocument
                                    << "$max" << open_document
                                        << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongoDBDateObject
                                        << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongoDBDateObject
                                    << close_document;

                            //NOTE: this could be done only when first contact is made, however it is reliant on the client and so some chat rooms
                            // could stay in a state that no one else could join with a bug
                            updateDocument
                                    << "$set" << open_document
                                        << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
                                    << close_document;

                            break;
                        }
                        case MessageSpecifics::kInviteMessage:
                            //NOTE: Invite cannot update chat room activity time because it is not an activity for all users.
                            //NOTE: This could be done only when first contact is made. However, it is reliant
                            // on the client and so some chat rooms could stay in a state that no one else
                            // could join with a bug.
                            //Do not allow the matching oid to be canceled when kUserActivityDetectedMessage is returned.
                            updateDocument
                                    << "$set" << open_document
                                        << chat_room_header_keys::MATCHING_OID_STRINGS << bsoncxx::types::b_null{}
                                    << close_document;
                            [[fallthrough]];
                        case MessageSpecifics::kUserActivityDetectedMessage: {
                            //Do NOT update the chat room activity time for these message types
                            updateDocument
                                    << "$max" << open_document
                                        << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << bsoncxx::types::b_date{current_timestamp}
                                    << close_document;
                            break;
                        }

                        //These message types should never be reached
                        case MessageSpecifics::kEditedMessage:
                        case MessageSpecifics::kDeletedMessage:
                        case MessageSpecifics::kUserKickedMessage:
                        case MessageSpecifics::kUserBannedMessage:
                        case MessageSpecifics::kDifferentUserJoinedMessage:
                        case MessageSpecifics::kDifferentUserLeftMessage:
                        case MessageSpecifics::kUpdateObservedTimeMessage:
                        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
                        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
                        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
                        case MessageSpecifics::kThisUserLeftChatRoomMessage:
                        case MessageSpecifics::kChatRoomNameUpdatedMessage:
                        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
                        case MessageSpecifics::kNewAdminPromotedMessage:
                        case MessageSpecifics::kNewPinnedLocationMessage:
                        case MessageSpecifics::kMatchCanceledMessage:
                        case MessageSpecifics::kNewUpdateTimeMessage:
                        case MessageSpecifics::kHistoryClearedMessage:
                        case MessageSpecifics::kLoadingMessage:
                        case MessageSpecifics::kChatRoomCapMessage:
                        case MessageSpecifics::MESSAGE_BODY_NOT_SET: {

                            const std::string error_string = "Incorrect message type sent to clientMessageToServer.\n";

                            storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                "ObjectID_used", userAccountOID,
                                "message_type", convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case()
                                ),
                                "chatRoomId", chatRoomId
                            );

                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );
                            return;
                        }
                    }

                    //this document will check if user is 'inside' chat room
                    const bsoncxx::document::value findDocumentValue = matchChatRoomUserInside(userAccountOID);

                    mongocxx::options::find_one_and_update opts;

                    opts.projection(
                        document{}
                            << chat_room_header_keys::MATCHING_OID_STRINGS << 1
                        << finalize
                    );

                    bsoncxx::builder::basic::array arrayBuilder{};
                    arrayBuilder.append(
                        document{}
                            << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                        << finalize
                    );

                    opts.array_filters(arrayBuilder.view());

                    //get the chat room header
                    try {
                        //if user exists in chat room, return chat room header
                        //NOTE: if the collection does not exist findAndUpdateChatRoomTimes will not be set, so it will return false
                        findAndUpdateChatRoomTimes = chatRoomCollection.find_one_and_update(
                            *session,
                            findDocumentValue.view(),
                            updateDocument.view(),
                            opts
                        );
                    }
                    catch (const mongocxx::logic_error& e) {
                        std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
                        errorString += "Message Type: ";
                        errorString += convertMessageBodyTypeToString(
                                request->message().message_specifics().message_body_case());

                        std::optional<std::string> exceptionString = e.what();
                        storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            exceptionString, errorString,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                            "ObjectID_used", userAccountOID,
                            "key", userAccountOID,
                            "chatRoomId", chatRoomId
                        );

                        setTransactionError(
                                response,
                                session,
                                ReturnStatus::LG_ERROR,
                                transaction_successful
                                );
                        return;
                    }

                    if (!findAndUpdateChatRoomTimes) { //failed to find or update chat room
                        std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in user account chat room array.\n";
                        errorString += "Message Type: ";
                        errorString += convertMessageBodyTypeToString(
                                request->message().message_specifics().message_body_case());

                        storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), errorString,
                            "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                            "ObjectID_used", userAccountOID,
                            "key", userAccountOID,
                            "chatRoomId", chatRoomId,
                            "message_uuid", request->message_uuid()
                        );

                        //NOTE: This can be possible if the user was removed from the chat room 'in between' the above check and this one.
                        session->abort_transaction();
                        transaction_successful = false;
                        response->set_return_status(ReturnStatus::SUCCESS);
                        response->set_user_not_in_chat_room(true);
                        return;
                    }

                    chatRoomHeaderDocValue = std::make_unique<bsoncxx::document::value>(*findAndUpdateChatRoomTimes);
                    chatRoomHeaderDocView = (*chatRoomHeaderDocValue).view();
                }

                bsoncxx::array::view matching_chat_array;

                auto isMatchingOIDElement = chatRoomHeaderDocView[chat_room_header_keys::MATCHING_OID_STRINGS];
                if (isMatchingOIDElement && isMatchingOIDElement.type() ==
                                            bsoncxx::type::k_array) { //if element exists and is type array
                    matching_chat_array = isMatchingOIDElement.get_array().value;
                }
                else if (!isMatchingOIDElement || isMatchingOIDElement.type() !=
                                                    bsoncxx::type::k_null) { //if element does not exist or is not type null or array
                    logElementError(
                        __LINE__, __FILE__,
                        isMatchingOIDElement, chatRoomHeaderDocView,
                        bsoncxx::type::k_null, chat_room_header_keys::MATCHING_OID_STRINGS,
                        database_names::CHAT_ROOMS_DATABASE_NAME,collection_names::CHAT_ROOM_ID_ + chatRoomId
                    );

                    setTransactionError(
                            response,
                            session,
                            ReturnStatus::LG_ERROR,
                            transaction_successful
                            );
                    return;
                }

                //NOTE: kUpdateObservedTimeMessage will not update this to a 'match made' either, however
                // kUpdateObservedTimeMessage should be returned before it reaches this point
                if (!matching_chat_array.empty()
                    && request->message().message_specifics().message_body_case() !=
                       MessageSpecifics::kUserActivityDetectedMessage) {

                    size_t num_user_in_matching_chat_array = std::distance(
                        matching_chat_array.begin(),
                        matching_chat_array.end()
                    );

                    if (num_user_in_matching_chat_array < 2) {
                        //NOTE: this means that the userAccount OTHER_ACCOUNTS_MATCHED_WITH was not removed
                        std::string errorString = "MATCHING_OID_STRINGS was size 1, should be an array of size 2 or null\n";
                        errorString += "Message Type: ";
                        errorString += convertMessageBodyTypeToString(
                                request->message().message_specifics().message_body_case());

                        storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), errorString,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                            "ObjectID_used", userAccountOID,
                            "chatRoomId", chatRoomId
                        );

                        //NOTE: nothing to remove from user account, however the chat room was already set up so can continue
                    }
                    else { //num_user_in_matching_chat_array >= 2

                        if (num_user_in_matching_chat_array > 2) {
                            std::string errorString = "MATCHING_OID_STRINGS larger than size 2.\n";
                            errorString += "Message Type: ";
                            errorString += convertMessageBodyTypeToString(
                                    request->message().message_specifics().message_body_case());

                            storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), errorString,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                "ObjectID_used", userAccountOID,
                                "matching_chat_array", makePrettyJson(matching_chat_array),
                                "chatRoomId", chatRoomId
                            );

                            //NOTE: the update_many function below will remove them all, can continue
                        }

                        bsoncxx::builder::basic::array matching_chat_array_OIDs;

                        for (const auto& matching_chat_string_element : matching_chat_array) {
                            if (matching_chat_string_element.type() == bsoncxx::type::k_utf8) {
                                matching_chat_array_OIDs.append(
                                        bsoncxx::oid{matching_chat_string_element.get_string().value});
                            } else { //element

                                storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(),
                                        std::string("Incorrect type for matching_chat_array element"),
                                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                    "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                    "ObjectID_used", userAccountOID,
                                    "matching_chat_array", makePrettyJson(matching_chat_array),
                                    "chatRoomId", chatRoomId
                                );

                                //NOTE: can continue it just won't find anything for this specific element
                            }
                        }

                        try {

                            //remove relevant matches inside MATCHING_OID_STRINGS
                            userAccountsCollection.update_many(
                                *session,
                                document{}
                                    << "_id" << open_document
                                        << "$in" << matching_chat_array_OIDs
                                    << close_document
                                << finalize,
                                document{}
                                    << "$pull" << open_document
                                        << user_account_keys::OTHER_ACCOUNTS_MATCHED_WITH << open_document
                                            << user_account_keys::other_accounts_matched_with::OID_STRING << open_document
                                                << "$in" << matching_chat_array
                                            << close_document
                                        << close_document
                                    << close_document
                                << finalize
                            );

                        } catch (const mongocxx::logic_error& e) {

                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), e.what(),
                                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                    "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                    "ObjectID_used", userAccountOID,
                                    "key", userAccountOID,
                                    "chatRoomId", chatRoomId);

                            setTransactionError(
                                    response,
                                    session,
                                    ReturnStatus::LG_ERROR,
                                    transaction_successful
                                    );
                            return;
                        }
                    }
                }

                setTransactionSuccess(
                        response,
                        messageReturnVal.picture_oid,
                        current_timestamp,
                        transaction_successful
                        );
            };

            mongocxx::client_session session = mongoCppClient.start_session();

            //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
            // more 'generic' errors. Can look here for more info
            // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
            try {
                session.with_transaction(transactionCallback);
            }
            catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
                std::cout << "Exception calling clientMessageToServerImplementation() transaction.\n" << e.what() << '\n';
#endif
                storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(),std::string(e.what()),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                    "user_OID", userAccountOID,
                    "chat_room_id", chatRoomId);

                transaction_successful = false;
            }

            //error was already stored
            if (!transaction_successful) {
                if (response->return_status() == ReturnStatus::UNKNOWN) {
                    response->set_return_status(ReturnStatus::LG_ERROR);
                }
                return;
            }

        }

    }
}

ReturnStatus runEditedMessageOrDeleteOneMessage(
        const bsoncxx::oid& userAccountOID,
        const std::string& uuidOfModifiedMessage,
        mongocxx::client_session* session,
        const grpc_chat_commands::ClientMessageToServerRequest* request,
        mongocxx::collection& chatRoomCollection,
        const std::string& chatRoomId,
        SendMessageToChatRoomReturn& messageReturnVal,
        std::chrono::milliseconds& current_timestamp,
        ReplyChatMessageInfo* replyChatMessageInfo,
        EditedOrDeletedMessagePassed edited_or_deleted,
        const bsoncxx::document::view& findMessageDocument,
        const std::function<bsoncxx::document::value(
            const std::chrono::milliseconds& timestamp_stored)>& get_update_message_document,
        std::unique_ptr<bsoncxx::document::value>& chatRoomHeaderDocValue,
        bsoncxx::document::view& chatRoomHeaderDocView
) {

    if (session == nullptr) {
        std::string error_string = "handleMessageToBeSent() was called when session was set to nullptr.";

        std::optional<std::string> dummyExceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__, dummyExceptionString,
                                      error_string,
                                      "userAccountOID", userAccountOID,
                                      "chatRoomId", chatRoomId,
                                      "uuidOfModifiedMessage", uuidOfModifiedMessage,
                                      "findMessageDocument", findMessageDocument,
                                      "updateMessageDocument", get_update_message_document(current_timestamp)
        );

        return ReturnStatus::LG_ERROR;
    }

    mongocxx::options::find opts;

    bsoncxx::builder::stream::document projection_doc_builder;

    projection_doc_builder
            << chat_room_header_keys::MATCHING_OID_STRINGS << 1
            << chat_room_shared_keys::TIMESTAMP_CREATED << 1;

    if(edited_or_deleted == EditedOrDeletedMessagePassed::TYPE_OF_MESSAGE_PASSED_EDITED) {
        projection_doc_builder
                << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::TEXT_MESSAGE << 1;
    }

    //MATCHING_OID_STRINGS field is used after this function ends
    opts.projection(projection_doc_builder.view());

    //get the chat room header and edited message
    mongocxx::stdx::optional<mongocxx::cursor> findHeaderAndMessageDocs;
    try {
        //if user exists in chat room, return chat room header
        //if message exists with proper credentials, return messages
        //NOTE: if the collection does not exist findHeaderAndMessageDocs will not be set, so it will return false
        findHeaderAndMessageDocs = chatRoomCollection.find(
            *session,
            document{}
                << "$or" << open_array

                    //find header document
                    << matchChatRoomUserInside(userAccountOID)

                    //find message document
                    << findMessageDocument

                << close_array
            << finalize
        );
    }
    catch (const mongocxx::logic_error& e) {
        std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
        errorString += "Message Type: ";
        errorString += convertMessageBodyTypeToString(request->message().message_specifics().message_body_case());

        std::optional<std::string> exceptionString = e.what();
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "ObjectID_used", userAccountOID,
                                      "key", userAccountOID,
                                      "chatRoomId", chatRoomId);

        return ReturnStatus::LG_ERROR;
    }

    if (!findHeaderAndMessageDocs) {
        std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
        errorString += "Message Type: ";
        errorString += convertMessageBodyTypeToString(request->message().message_specifics().message_body_case());

        std::optional<std::string> dummyExceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummyExceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "ObjectID_used", userAccountOID,
                                      "key", userAccountOID,
                                      "chatRoomId", chatRoomId);

        //Returning success because it has done as much as it can.
        return ReturnStatus::SUCCESS;
    }

    std::unique_ptr<bsoncxx::document::value> messageValue;
    bsoncxx::document::view messageView;

    for (auto cursorDoc : *findHeaderAndMessageDocs) {
        auto messageTextElement = cursorDoc["_id"];
        if (messageTextElement
            && messageTextElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
            std::string doc_id = messageTextElement.get_string().value.to_string();
            if (doc_id == chat_room_header_keys::ID) {
                chatRoomHeaderDocValue = std::make_unique<bsoncxx::document::value>(cursorDoc);
                chatRoomHeaderDocView = (*chatRoomHeaderDocValue).view();
            } else {
                messageValue = std::make_unique<bsoncxx::document::value>(cursorDoc);
                messageView = (*messageValue).view();
            }
        }
        else { //if element does not exist or is not type oid or utf8
            logElementError(__LINE__, __FILE__, messageTextElement,
                            cursorDoc, bsoncxx::type::k_oid, "_id",
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

            return ReturnStatus::LG_ERROR;
        }
    }

    if (chatRoomHeaderDocView.empty()) {
        std::string errorString = "Failed to find chat room or member is not a part of chat room when they were in verified chat room.\n";
        errorString += "Message Type: ";
        errorString += convertMessageBodyTypeToString(request->message().message_specifics().message_body_case());

        std::optional<std::string> dummyExceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummyExceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "ObjectID_used", userAccountOID,
                                      "key", userAccountOID,
                                      "chatRoomId", chatRoomId);

        //Returning success because it has done as much as it can.
        return ReturnStatus::SUCCESS;
    }
    else if (messageView.empty()) {
        std::string errorString = "User is attempting to edit or delete a message when message does not exist, incorrect message oid, or invalid message type.\n";
        errorString += "Message Type: ";
        errorString += convertMessageBodyTypeToString(request->message().message_specifics().message_body_case());

        std::optional<std::string> dummyExceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummyExceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "ObjectID_used", userAccountOID,
                                      "key", userAccountOID,
                                      "chatRoomId", chatRoomId);

        //Returning success because it has done as much as it can.
        return ReturnStatus::SUCCESS;
    }

    std::string previous_message;
    std::chrono::milliseconds modified_message_created_time;

    if(edited_or_deleted == EditedOrDeletedMessagePassed::TYPE_OF_MESSAGE_PASSED_EDITED) {

        auto messageSpecificsDocElement = messageView[chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT];
        if (messageSpecificsDocElement
            && messageSpecificsDocElement.type() == bsoncxx::type::k_document) { //if element exists and is type utf8
            bsoncxx::document::view messageSpecificsDoc = messageSpecificsDocElement.get_document().value;

            auto messageTextElement = messageSpecificsDoc[chat_room_message_keys::message_specifics::TEXT_MESSAGE];
            if (messageTextElement &&
            messageTextElement.type() ==
            bsoncxx::type::k_utf8) { //if element exists and is type utf8
                previous_message = messageTextElement.get_string().value.to_string();
            } else { //if element does not exist or is not type oid
                logElementError(__LINE__, __FILE__,
                                messageTextElement,
                                messageSpecificsDoc, bsoncxx::type::k_utf8,
                                chat_room_message_keys::message_specifics::TEXT_MESSAGE,
                                database_names::CHAT_ROOMS_DATABASE_NAME,
                                collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return ReturnStatus::LG_ERROR;
            }

        }
        else { //if element does not exist or is not type oid
            logElementError(__LINE__, __FILE__,
                            messageSpecificsDocElement,
                            messageView, bsoncxx::type::k_document,
                            chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT,
                            database_names::CHAT_ROOMS_DATABASE_NAME,
                            collection_names::CHAT_ROOM_ID_ + chatRoomId);

            return ReturnStatus::LG_ERROR;
        }
    }

    auto createdTimeElement = messageView[chat_room_shared_keys::TIMESTAMP_CREATED];
    if (createdTimeElement
        && createdTimeElement.type() == bsoncxx::type::k_date) { //if element exists and is type date
        modified_message_created_time = createdTimeElement.get_date().value;
    }
    else { //if element does not exist or is not type oid
        logElementError(__LINE__, __FILE__,
                        createdTimeElement,
                        messageView, bsoncxx::type::k_date,
                        chat_room_shared_keys::TIMESTAMP_CREATED,
                        database_names::CHAT_ROOMS_DATABASE_NAME,
                        collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return ReturnStatus::LG_ERROR;
    }

    messageReturnVal = sendMessageToChatRoom(
            request,
            chatRoomCollection,
            userAccountOID,
            session,
            "",
            previous_message,
            modified_message_created_time,
            replyChatMessageInfo
            );

    switch (messageReturnVal.successful) {
        case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED: {
            //error was already stored
            return ReturnStatus::LG_ERROR;
        }
        case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
            //This will be translated into SUCCESS after it is returned.
            return ReturnStatus::INVALID_PARAMETER_PASSED;
        }
        case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL: {
            //continue on if it reaches this point
            current_timestamp = messageReturnVal.time_stored_on_server;
            break;
        }
    }

    mongocxx::pipeline edited_deleted_pipeline;

    //update the header to not be a matching chat room & update the current users last active time
    //update the edited message to have isEdited = true, the edited time, and the new message
    edited_deleted_pipeline.replace_root(
        buildClientMessageToServerUpdateModifiedMessage(
                userAccountOID,
                current_timestamp,
                get_update_message_document(current_timestamp)
        )
    );

    //get the chat room header and edited message
    bsoncxx::stdx::optional<mongocxx::result::update> updateHeaderAndMessageDocs;
    try {

        updateHeaderAndMessageDocs = chatRoomCollection.update_many(
                *session,
                document{}
                << "_id" << open_document
                    << "$in" << open_array
                        << chat_room_header_keys::ID
                        << uuidOfModifiedMessage
                    << close_array
                << close_document
            << finalize,
                edited_deleted_pipeline
        );

    }
    catch (const mongocxx::logic_error& e) {
        std::string errorString = "Failed to update chat room or member is not a part of chat room when they were already found.\n";
        errorString += "Message Type: ";
        errorString += convertMessageBodyTypeToString(request->message().message_specifics().message_body_case());
        request->AppendToString(&errorString);

        std::optional<std::string> exceptionString = e.what();
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "ObjectID_used", userAccountOID,
                                      "key", userAccountOID,
                                      "chatRoomId", chatRoomId);

        return ReturnStatus::LG_ERROR;
    }

    if (!updateHeaderAndMessageDocs) {
        std::string errorString = "Failed to update chat room or member is not a part of chat room when they were already found.\n";
        errorString += "Message Type: ";
        errorString += convertMessageBodyTypeToString(request->message().message_specifics().message_body_case());
        request->AppendToString(&errorString);

        std::optional<std::string> dummyExceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummyExceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "ObjectID_used", userAccountOID,
                                      "key", userAccountOID,
                                      "chatRoomId", chatRoomId);

        return ReturnStatus::LG_ERROR;
    }

    return ReturnStatus::VALUE_NOT_SET;
}