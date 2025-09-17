//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <utility>

#include <ChatMessageStream.grpc.pb.h>

#include "store_and_send_messages.h"
#include "how_to_handle_member_pictures.h"
#include "messages_waiting_to_be_sent.h"

enum class DifferentUserJoinedChatRoomAmount {
    SKELETON,
    INFO_WITHOUT_IMAGES,
    INFO_WITH_THUMBNAIL_NO_PICTURES,
    ALL_INFO_AND_IMAGES
};

struct ExtractUserInfoObjects {
    ExtractUserInfoObjects() = delete;

    ExtractUserInfoObjects(
            mongocxx::client& _mongoCppClient,
            mongocxx::database& _accountsDB,
            mongocxx::collection& _userAccountsCollection
    ) :
            mongoCppClient(_mongoCppClient),
            accountsDB(_accountsDB),
            userAccountsCollection(_userAccountsCollection) {}

    mongocxx::client& mongoCppClient;
    mongocxx::database& accountsDB;
    mongocxx::collection& userAccountsCollection;
};

//writes the chat room and all messages to the passed lambda writeResponseToClient
//requestFinalFewMessagesInFull will write ONLY the final few messages (chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST) with
// AmountOfMessage::COMPLETE_MESSAGE_INFO, all others will be written back as AmountOfMessage::ONLY_SKELETON.
//do_not_update_user_state sets a boolean in each message sent back as well as a boolean inside the
// this_user_joined_chat_room_finished_message message.
//requestPictures is used to determine how much info from each member is requested to be sent back
//amountOfMessageInfoToRequest is passed through to streamInitializationMessagesToClient(), it WILL BE IGNORED if
// requestFinalFewMessagesInFull==true
bool sendNewChatRoomAndMessages(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        mongocxx::collection& chatRoomCollection,
        const bsoncxx::document::view& userAccountDocView,
        const std::string& chatRoomId,
        const std::chrono::milliseconds& timeChatRoomLastObserved,
        const std::chrono::milliseconds& currentTimestamp,
        const std::string& userAccountOID,
        StoreAndSendMessagesVirtual* storeAndSendMessagesToClient,
        HowToHandleMemberPictures requestPictures,
        AmountOfMessage amountOfMessageInfoToRequest,
        bool requestFinalFewMessagesInFull,
        bool do_not_update_user_state,
        bool from_user_swiped_yes_on_event = false,
        const std::chrono::milliseconds& time_to_request_before_or_equal = std::chrono::milliseconds{general_values::NUMBER_BIGGER_THAN_UNIX_TIMESTAMP_MS},
        const std::string& message_uuid_to_exclude = "",
        const std::function<bool()>& run_after_messages_requested = nullptr
);

//saves the info from user_account_doc to the MemberSharedInfoMessage value
//NOTE: does not save current_object_oid (requires it as a parameter)
bool saveUserInfoToMemberSharedInfoMessage(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& user_account_collection,
        const bsoncxx::document::view& user_account_doc,
        const bsoncxx::oid& memberOID,
        MemberSharedInfoMessage* userInfo,
        HowToHandleMemberPictures requestPictures,
        const std::chrono::milliseconds& currentTimestamp
);

//takes a chat message document passed as messageDoc and stores it inside the ChatMessageToClient responseMsg
//if ExtractUserInfoObjects is not set and the type is DIFFERENT_USER_JOINED_CHAT_ROOM then the other user account info
// will not be stored in responseMsg
/** If amountOfMessage != ONLY_SKELETON then extractUserInfoObjects must be set. **/
bool convertChatMessageDocumentToChatMessageToClient(
        const bsoncxx::document::view& messageDoc,
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        bool onlyStoreMessageBoolValue,
        ChatMessageToClient* responseMsg,
        AmountOfMessage amountOfMessage,
        DifferentUserJoinedChatRoomAmount differentUserJoinedChatRoomAmount,
        bool do_not_update_user_state,
        ExtractUserInfoObjects* extractUserInfoObjects,
        bool internal_force_send_message_to_current_user = false
);

//Used with convertChatMessageDocumentToChatMessageToClient(), visible for testing.
bool extractReplyFromMessage(
        const std::string& chatRoomID,
        const bsoncxx::document::view& activeMessageInfoDocView,
        AmountOfMessage amountOfMessage,
        ActiveMessageInfo* activeMessageInfo
);

//Used with convertChatMessageDocumentToChatMessageToClient(), visible for testing.
bool extractMessageDeleted(
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        const bsoncxx::document::view& activeMessageInfoDocView,
        ActiveMessageInfo* activeMessageInfo
);

//extracts the picture and runs the passed success lambda if successful
// and corrupt lambda if fails
bool extractChatPicture(
        const mongocxx::client& mongoCppClient,
        const std::string& pictureOIDString,
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        const std::string& message_uuid,
        const std::function<void(const int /*pictureSize*/,
                                 const int /*pictureHeight*/, const int /*pictureWidth*/,
                                 std::string& /*pictureByteString*/)>& pictureSuccessfullyExtracted,
        const std::function<void()>& pictureNotFoundOrCorrupt
);

//stores other user info for a DIFFERENT_USER_JOINED_CHAT_ROOM message inside ChatRoomMemberInfoMessage* memberInfo
bool getUserAccountInfoForUserJoinedChatRoom(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& userAccountsCollection,
        const std::string& chatRoomID,
        const std::string& messageSentByOID,
        HowToHandleMemberPictures howToHandleMemberPictures,
        MemberSharedInfoMessage* userMemberInfo
);

//returns true if invalid chat room Id false if not
bool isInvalidChatRoomId(const std::string& chatRoomID);

//leaves the chatRoomId for userAccountOID, returns false if fails and runs setReturnStatus
//user_account_keys::PICTURES must be projected inside user_account_doc
//If user_pictures_keys::THUMBNAIL_REFERENCES is expected to be updated for the thumbnail, it should be done beforehand
// under the same transaction.
bool leaveChatRoom(
        mongocxx::database& chatRoomDB,
        mongocxx::database& accounts_db,
        mongocxx::collection& userAccountsCollection,
        mongocxx::client_session* session,
        const bsoncxx::document::view& user_account_doc,
        const std::string& chatRoomId,
        const bsoncxx::oid& userAccountOID,
        std::chrono::milliseconds& currentTimestamp,
        const std::function<void(
               const ReturnStatus&,
               const std::chrono::milliseconds&
        )>& setReturnStatus = [](const ReturnStatus&, const std::chrono::milliseconds&){}
);

//Will extract a thumbnail from a chat room header document. It requires the document
// element of chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM. It will also require that the
// THUMBNAIL_SIZE and THUMBNAIL_TIMESTAMP are already extracted.
bool extractThumbnailFromHeaderAccountDocument(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& chat_room_collection,
        const bsoncxx::document::view& user_from_chat_room_header_doc_view,
        const std::chrono::milliseconds& current_timestamp,
        MemberSharedInfoMessage* mutable_user_info,
        const std::string& member_account_oid_string,
        int thumbnail_size_from_header,
        const std::chrono::milliseconds& thumbnail_timestamp_from_header
);

inline grpc_chat_commands::ClientMessageToServerRequest generateUserActivityDetectedMessage(
        const std::string& message_uuid,
        const std::string& chat_room_id
        ) {
    grpc_chat_commands::ClientMessageToServerRequest request;
    request.set_message_uuid(message_uuid);
    request.mutable_message()->mutable_message_specifics()->mutable_user_activity_detected_message();
    request.mutable_message()->mutable_standard_message_info()->set_chat_room_id_message_sent_from(
            chat_room_id
    );

    return request;
}

