//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <mongocxx/database.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <ChatRoomCommands.grpc.pb.h>
#include <UpdateOtherUserMessages.grpc.pb.h>
#include <how_to_handle_member_pictures.h>
#include <thread>
#include "../helper_objects/chat_room_commands_helper_objects.h"
#include "server_initialization_functions.h"
#include "utility_general_functions.h"

bool removeMessageOnError(
        const std::string& message_uuid,
        mongocxx::collection& chatRoomCollection,
        mongocxx::client_session* session,
        const bsoncxx::oid& user_account_oid
);

SendMessageToChatRoomReturn handleMessageToBeSent(
        mongocxx::client_session* session,
        mongocxx::collection& chatRoomCollection,
        const grpc_chat_commands::ClientMessageToServerRequest* request,
        bsoncxx::builder::stream::document& insertDocBuilder,
        const bsoncxx::oid& user_account_oid
);

//Stores a new message inside the passed chat room collection.
//TypeOfChatMessage extracted from ClientMessageToServerRequest
//It is possible that the message is already stored when this is running, if that occurs the result will
// be sent back using SendMessageToChatRoomReturn::SuccessfulReturn. The transaction can then be aborted
// by the calling function.
SendMessageToChatRoomReturn sendMessageToChatRoom(
        const grpc_chat_commands::ClientMessageToServerRequest* request,
        mongocxx::collection& chatRoomCollection,
        const bsoncxx::oid& userAccountOID,
        mongocxx::client_session* session = nullptr,
        const std::string& insertedPictureOID = "",
        const std::string& previousMessage = "",
        const std::chrono::milliseconds& modifiedMessageCreatedTime = std::chrono::milliseconds{0},
        ReplyChatMessageInfo* replyChatMessageInfo = nullptr
);

//this function will return true if the message type can reply to other messages (the types listed inside checkIfChatRoomMessageCanBeRepliedTo) and false otherwise
bool chatRoomMessageIsAbleToReply(MessageSpecifics::MessageBodyCase messageType);

//will un-match userAccountOID and matchedAccountOID and set the return status through the lambda setReturnStatus
//requires user_account_keys::PICTURES be projected inside current_user_account_doc_view
//session is mandatory here
bool unMatchHelper(
        mongocxx::database& accounts_db,
        mongocxx::database& chat_room_db,
        mongocxx::client_session* session,
        mongocxx::collection& user_accounts_collection,
        const bsoncxx::document::view& current_user_account_doc_view,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::oid& matched_account_oid,
        const std::string& chat_room_id,
        std::chrono::milliseconds& current_timestamp,
        const std::function<void(const ReturnStatus&, const std::chrono::milliseconds&)>& set_return_status
);

//update a single chat room member (member is expected to NOT be IN_CHAT_ROOM or CHAT_ROOM_ADMIN), passed parameters are in
// memberFromClient, responseFunction will be called when user data has been compiled
void updateSingleChatRoomMemberNotInChatRoom(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& chat_room_collection,
        const std::string& memberAccountOIDString,
        const bsoncxx::document::view& userFromChatRoomHeaderDocView,
        const OtherUserInfoForUpdates& memberFromClient,
        const std::chrono::milliseconds& currentTimestamp,
        bool alwaysSetResponseMessage,
        AccountStateInChatRoom accountStateFromChatRoom,
        const std::chrono::milliseconds& userLastActivityTime,
        UpdateOtherUserResponse* streamResponse,
        const std::function<void()>& responseFunction
);

//Generate a message document for kDifferentUserJoinedChatRoom, stores inside builder, returns message_uuid.
//Pass event_oid_str if the field USER_JOINED_FROM_EVENT should be set, if event_oid_str is set to chat_room_values::EVENT_ID_DEFAULT, it
// will be ignored.
std::string generateDifferentUserJoinedChatRoomMessage(
        bsoncxx::builder::stream::document& builder,
        const bsoncxx::oid& userAccountOID,
        AccountStateInChatRoom currentUserAccountState,
        const std::string& event_oid_str,
        const std::chrono::milliseconds& currentTimestamp = std::chrono::milliseconds{-1}
);

void sendDifferentUserJoinedChatRoom(
        const std::string& chatRoomId,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::builder::stream::document& different_user_joined_chat_room_message_doc,
        const std::string& message_uuid
);

bool updateUserLastViewedTime(
        const std::string& chat_room_id,
        const bsoncxx::oid& user_account_oid,
        const bsoncxx::types::b_date& mongodb_current_date,
        mongocxx::collection& user_accounts_collection,
        mongocxx::client_session* session
);

//Will error check an OtherUserInfoForUpdates (member) for any problems and save the resulting (and
// possibly modified) info to the other OtherUserInfoForUpdates, error_checked_member.
//NOTE: This is not explicitly tested, instead it is tested through filterAndStoreListOfUsersToBeUpdated().
//account_state_in_chat_room is simply extracted from the member. The reason it is separate is that not all
// calling function update from inside a chat room.
bool filterAndStoreSingleUserToBeUpdated(
        const OtherUserInfoForUpdates& member,
        AccountStateInChatRoom account_state_in_chat_room,
        const std::chrono::milliseconds& current_timestamp,
        OtherUserInfoForUpdates& error_checked_member,
        bool insert_result = true
);

std::vector<OtherUserInfoForUpdates> filterAndStoreListOfUsersToBeUpdated(
        const google::protobuf::RepeatedPtrField<OtherUserInfoForUpdates>& chat_room_member_info,
        const std::chrono::milliseconds& currentTimestamp
);

inline std::string generateEmptyChatRoomName(
        const std::string& user_name
) {
    return user_name + "'s chat room";
}

//This function is set up to provide the fields that are 'guaranteed' to be returned to the client when a chat room
// member is updated. See under 'General idea' header inside
// [grpc_functions/chat_room_commands/request_information/_documentation.md].
inline void buildBasicUpdateOtherUserResponse(
        UpdateOtherUserResponse* members_response_for_current_user,
        AccountStateInChatRoom user_account_state_in_chat_room,
        const std::string& user_account_oid_str,
        const std::chrono::milliseconds& current_timestamp,
        const long user_last_activity_time
) {
    members_response_for_current_user->Clear();
    members_response_for_current_user->set_return_status(ReturnStatus::SUCCESS);
    members_response_for_current_user->set_account_state(user_account_state_in_chat_room);
    members_response_for_current_user->mutable_user_info()->set_account_oid(user_account_oid_str);
    members_response_for_current_user->set_timestamp_returned(current_timestamp.count());
    members_response_for_current_user->set_account_last_activity_time(user_last_activity_time);
}

#ifdef LG_TESTING

//set this to -1 to use $$NOW
inline std::chrono::milliseconds testing_delay_for_messages{-1};

//Simulate gaps between the messages being stored.
inline std::chrono::milliseconds generateTestingTimestamp() {
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    long sleep_time = rand() % testing_delay_for_messages.count();

    //Want smaller times more often.
    if (sleep_time > 0 && sleep_time % 2 == 0) {
        sleep_time /= 2;
    }
    if (sleep_time > 0 && sleep_time % 3 == 0) {
        sleep_time /= 2;
    }

    if (sleep_time > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds{sleep_time});
    }

    return current_timestamp;
}
#endif