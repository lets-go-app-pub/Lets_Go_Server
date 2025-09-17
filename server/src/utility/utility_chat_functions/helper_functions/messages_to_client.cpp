//
// Created by jeremiah on 5/2/21.
//

#include "messages_to_client.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <global_bsoncxx_docs.h>


#include "store_and_send_messages.h"
#include "utility_chat_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "chat_stream_container.h"
#include "chat_room_header_keys.h"
#include "chat_room_message_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//extracts all messages before timeOfMessageToCompare from the passed chat room collection and streams them back to the client
//amountOfMessage will determine the amount of messages that will be sent back, if requestFinalFewMessagesInFull==true, this value
// will be ignored
//differentUserJoinedChatRoomAmount will return different amounts of info for message type kDifferentUserJoinedChatRoom
//do_not_update_user_state will set a bool (in certain message types) if set to true
//onlyStoreMessageBoolValue will skip edited and deleted messages if set to true, it will also set a bool in each message sent back
//requestFinalFewMessagesInFull will override amountOfMessage and request ONLY the final MAX_NUMBER_MESSAGES_USER_CAN_REQUEST number
// of messages for COMPLETE_MESSAGE_INFO then everything else for ONLY_SKELETON.
bool streamInitializationMessagesToClient(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& chatRoomCollection,
        mongocxx::collection& userAccountsCollection,
        const bsoncxx::document::view& userAccountDocView,
        const std::string& chatRoomId,
        const std::string& currentUserAccountOIDStr,
        StoreAndSendMessagesVirtual* storeAndSendMessagesToClient,
        AmountOfMessage amountOfMessage,
        DifferentUserJoinedChatRoomAmount differentUserJoinedChatRoomAmount,
        bool do_not_update_user_state,
        bool onlyStoreMessageBoolValue,
        bool requestFinalFewMessagesInFull,
        const std::chrono::milliseconds& greater_than_or_equal_to_message_time,
        bsoncxx::builder::basic::array& message_uuids_to_exclude,
        const std::chrono::milliseconds& time_to_request_before_or_equal
) {

    mongocxx::pipeline stages;
    bsoncxx::oid currentUserAccountOID = bsoncxx::oid{currentUserAccountOIDStr};
    //checks if
    //document is not the header for the collection
    //document is past the required time
    //if document is INVITED type then make sure it is an 'invite message' that this user received or sent
    //if document is MESSAGE_DELETED type then make sure it is a 'delete message' that is specific for this user or is general to all users

    //NOTE: kMatchCanceled should not be needed here, when it is called the chat room will be removed from the users list of chat
    // rooms, so even if the chat stream initialization occurs it will just remove the chat room w/o actually requesting this message
    // meaning no one should ever need this

    bsoncxx::builder::basic::array match_branches_array{};

    //active message outside invite
    match_branches_array.append(
        document{}
            << "case" << open_document
                << "$in" << open_array
                    << "$" + chat_room_message_keys::MESSAGE_TYPE
                    << open_array
                        << (int)MessageSpecifics::MessageBodyCase::kTextMessage
                        << (int)MessageSpecifics::MessageBodyCase::kPictureMessage
                        << (int)MessageSpecifics::MessageBodyCase::kLocationMessage
                        << (int)MessageSpecifics::MessageBodyCase::kMimeTypeMessage
                    << close_array
                << close_array
            << close_document

            //return true if account is accessible, false if deleted
            << "then" << buildCheckIfAccountDeletedDocument(currentUserAccountOIDStr)
        << finalize
    );

    //invite message
    match_branches_array.append(
        document{}
            << "case" << open_document
                << "$eq" << open_array
                    << "$" + chat_room_message_keys::MESSAGE_TYPE
                    << (int)MessageSpecifics::MessageBodyCase::kInviteMessage
                << close_array
            << close_document
            << "then" << open_document
                << "$and" << open_array

                    //return true if account is accessible, false if deleted
                    << buildCheckIfAccountDeletedDocument(currentUserAccountOIDStr)

                    //return true if invite is from or to this account
                    << open_document
                        << "$or" << open_array

                            //message is sent FROM current user
                            << open_document
                                << "$eq" << open_array
                                    << "$" + chat_room_message_keys::MESSAGE_SENT_BY
                                    << currentUserAccountOID
                                << close_array
                            << close_document

                            //message is sent TO current user
                            << open_document
                                << "$eq" << open_array
                                    << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID
                                    << currentUserAccountOIDStr
                                << close_array
                            << close_document

                        << close_array
                    << close_document

                << close_array
            << close_document
        << finalize
    );

    if(!onlyStoreMessageBoolValue) {

        //delete message
        match_branches_array.append(
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                        << "$" + chat_room_message_keys::MESSAGE_TYPE
                        << (int)MessageSpecifics::MessageBodyCase::kDeletedMessage
                    << close_array
                << close_document
                << "then" << open_document

                    //make sure delete message is relevant to this user
                    << "$and" << open_array

                        //this will make sure the message is only extracted if the original message
                        // was NOT extracted as part of this batch of messages, the reason for this is
                        // that the original message has the modification already attached to it and so
                        // this message is not only redundant but will make the client do unnecessary
                        // server calls in many cases
                        << open_document
                            << "$lt" << open_array
                                << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_CREATED_TIME
                                << bsoncxx::types::b_date{greater_than_or_equal_to_message_time}
                            << close_array
                        << close_document

                        //Only extract deleted messages specifically for this user.
                        << open_document

                            << "$or" << open_array

                                //message is to delete all
                                << open_document
                                    << "$eq" << open_array
                                        << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY
                                        << DeleteType::DELETE_FOR_ALL_USERS
                                    << close_array
                                << close_document

                                //message is to delete for single user and for this user
                                << open_document
                                    << "$and" << open_array

                                        //message type is a 'delete for single user'
                                        << open_document
                                            << "$eq" << open_array
                                                << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE
                                                << DeleteType::DELETE_FOR_SINGLE_USER
                                            << close_array
                                        << close_document

                                        //message was sent by the current user
                                        << open_document
                                            << "$eq" << open_array
                                                << "$" + chat_room_message_keys::MESSAGE_SENT_BY
                                                << currentUserAccountOID
                                            << close_array
                                        << close_document

                                    << close_array
                                << close_document
                            << close_array
                        << close_document

                    << close_array

                << close_document
            << finalize
        );

        //edited message
        match_branches_array.append(
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                        << "$" + chat_room_message_keys::MESSAGE_TYPE
                        << (int)MessageSpecifics::MessageBodyCase::kEditedMessage
                    << close_array
                << close_document
                << "then" << open_document

                    //this will make sure the message is only extracted if the original message
                    // was NOT extracted as part of this batch of messages, the reason for this is
                    // that the original message has the modification already attached to it and so
                    // this message is not only redundant but will make the client do unnecessary
                    // server calls in many cases
                    << "$lt" << open_array
                        << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME
                        << bsoncxx::types::b_date{greater_than_or_equal_to_message_time}
                    << close_array

                << close_document
            << finalize
        );

    }
    else { //onlyStoreMessageBoolValue == true

        //skip edited and deleted messages when onlyStoreMessages is true

        //delete message
        match_branches_array.append(
            document{}
                << "case" << open_document
                    << "$or" << open_array
                        << open_document
                            << "$eq" << open_array
                                << "$" + chat_room_message_keys::MESSAGE_TYPE
                                << (int)MessageSpecifics::MessageBodyCase::kDeletedMessage
                            << close_array
                        << close_document
                        << open_document
                            << "$eq" << open_array
                                << "$" + chat_room_message_keys::MESSAGE_TYPE
                                << (int)MessageSpecifics::MessageBodyCase::kEditedMessage
                            << close_array
                        << close_document
                    << close_array
                << close_document
                << "then" << false
            << finalize
        );
    }

    document match_document{};

    message_uuids_to_exclude.append(chat_room_header_keys::ID);

    match_document
        //There is a rare situation where two messages can have the same timestamp. If this happens and for
        // whatever reason only one of the messages is sent back to the user, then they can miss a message
        // if $gt is used with TIMESTAMP_CREATED. However, in a majority of cases using $gte will send back a
        // duplicate message. So the solution used here is that the most_recent_message_uuid is passed in and
        // explicitly excluded. It should be pointed out that if most_recent_message_uuid is invalid (empty, not
        // a valid uuid, etc.) then this will still work just fine. At worst, it should send back a duplicate
        // message which the client will ignore.
        << "_id" << open_document
            << "$nin" << message_uuids_to_exclude.view()
        << close_document

        //find the value between these two timestamps (inclusive)
        << chat_room_shared_keys::TIMESTAMP_CREATED << open_document
           << "$gte" << bsoncxx::types::b_date{greater_than_or_equal_to_message_time}
        << close_document
        << chat_room_shared_keys::TIMESTAMP_CREATED << open_document
            << "$lte" << bsoncxx::types::b_date{time_to_request_before_or_equal}
        << close_document

        //valid message type
        << chat_room_message_keys::MESSAGE_TYPE << open_document
           << "$gte" << (int)MessageSpecifics::MessageBodyCase::kTextMessage
        << close_document
        << chat_room_message_keys::MESSAGE_TYPE << open_document
           << "$lt" << (int)MessageSpecifics::MessageBodyCase::kMatchCanceledMessage
        << close_document

        //previously setup conditions
        << "$expr" << open_document
           << "$switch" << open_document
               << "branches" << match_branches_array.view()

               //extract any other messages
               << "default" << true
           << close_document
        << close_document;

    stages.match(match_document.view());

    stages.sort(
        document{}
            << chat_room_shared_keys::TIMESTAMP_CREATED << 1
            << "_id"<< 1
        << finalize
    );

    //project fields out
    if(!requestFinalFewMessagesInFull && amountOfMessage != AmountOfMessage::COMPLETE_MESSAGE_INFO) {

        bsoncxx::builder::stream::document projection_doc;

        const std::string message_specifics_doc_starter = std::string(chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT)
                .append(".");

        const std::string reply_doc_starter = message_specifics_doc_starter +
            std::string(chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT)
            .append(".")
            .append(chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT)
            .append(".");

        //Reply info will not be extracted, however REPLY_DOCUMENT itself will be checked if the reply exists, so
        // cannot simply project out REPLY_DOCUMENT, must project out each field
        projection_doc
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE << 0
            << reply_doc_starter + chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE << 0;

        if(amountOfMessage == AmountOfMessage::ONLY_SKELETON) {
            projection_doc

                //CHAT_TEXT_MESSAGE
                << message_specifics_doc_starter + chat_room_message_keys::message_specifics::TEXT_MESSAGE << 0
                << message_specifics_doc_starter + chat_room_message_keys::message_specifics::TEXT_IS_EDITED << 0
                << message_specifics_doc_starter + chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << 0

                //MESSAGE_EDITED
                << message_specifics_doc_starter + chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << 0
                << message_specifics_doc_starter + chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << 0
                << message_specifics_doc_starter + chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << 0;
        }

        //project out un-necessary potentially large fields
        stages.project(projection_doc.view());
    }

    bsoncxx::builder::basic::array group_branches_array{};

    group_branches_array.append(
        document{}
            << "case" << open_document
                << "$eq" << open_array
                   << "$" + chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kUserActivityDetectedMessage
                << close_array
            << close_document
            << "then" << open_document
               << "$toString" << "$" + chat_room_message_keys::MESSAGE_SENT_BY
            << close_document
        << finalize
    );

    if(!onlyStoreMessageBoolValue) {
        //edited and deleted messages are not requested when onlyStoreMessageBoolValue == true

        group_branches_array.append(
            document{}
                << "case" << open_document
                    << "$eq" << open_array
                       << "$" + chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kEditedMessage
                    << close_array
                << close_document
                << "then" << open_document
                    //delete must be separate from edited messages, otherwise in a case where an edited message is sent AFTER a message
                    // is deleted, the edited message will be sent back instead of the deleted message
                    << "$concat" << open_array
                        << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID
                        << "e"
                    << close_array
                << close_document
             << finalize
         );

        //Only the deleted messages relevant to this user were extracted above. It IS possible that the user deletes the message
        // personally, then it is deleted for all users. This would mean that there are two deleted messages for this user that
        // do the exact same thing.
        group_branches_array.append(
            document{}
                 << "case" << open_document
                     << "$eq" << open_array
                        << "$" + chat_room_message_keys::MESSAGE_TYPE << (int)MessageSpecifics::MessageBodyCase::kDeletedMessage
                     << close_array
                 << close_document
                 << "then" << open_document

                    //delete must be separate from edited messages, otherwise in a case where an edited message is sent AFTER a message
                    // is deleted, the edited message will be sent back instead of the deleted message
                    << "$concat" << open_array
                        << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT + "." + chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID
                        << "d"
                    << close_array

                 << close_document
             << finalize
         );
    }

    //group these so
    //1) only the last USER_ACTIVITY_DETECTED message is returned for each user
    //2) only the last MESSAGE_EDITED and MESSAGE_DELETED message types are returned for each message
    //3) all the other messages after timeLastMessageReceived
    stages.group(
        document{}
            << "_id" << open_document
                << "$switch" << open_document
                    << "branches" << group_branches_array.view()
                    << "default" << "$_id"
                << close_document
            << close_document

            // NOTE: these were sorted before this, so $last will be the largest time; TIMESTAMP_CREATED
            << chat_room_shared_keys::TIMESTAMP_CREATED << open_document
               << "$last" << "$" + chat_room_shared_keys::TIMESTAMP_CREATED
            << close_document
            << chat_room_message_keys::MESSAGE_SENT_BY << open_document
               << "$last" << "$" + chat_room_message_keys::MESSAGE_SENT_BY
            << close_document
            << chat_room_message_keys::MESSAGE_TYPE << open_document
               << "$last" << "$" + chat_room_message_keys::MESSAGE_TYPE
            << close_document
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
               << "$last" << "$" + chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT
            << close_document
        << finalize
    );

    //NOTE: These are sorted above, however if things are grouped it is very likely they will be out of order.

    //sort these in ascending order, that way if the stream is interrupted in some way the
    // client will not have a gap between messages. Also using _id as second sort parameter
    // to guarantee a repeatable order (the same is done in begin_chat_change_stream).
    stages.sort(
        document{}
            << chat_room_shared_keys::TIMESTAMP_CREATED << 1
            << "_id"<< 1
        << finalize
    );

    std::optional<std::string> findMessagesExceptionString;
    bsoncxx::stdx::optional<mongocxx::cursor> findMessages;
    try {
        findMessages = chatRoomCollection.aggregate(
            stages
        );
    }
    catch (const mongocxx::logic_error& e) {
        findMessagesExceptionString = e.what();
    }

    if(!findMessages) {
        const std::string errorString = "Failed to find chat room messages.\n";

        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      findMessagesExceptionString, errorString,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                      "verified_document", userAccountDocView);

        return false;
    }

    ExtractUserInfoObjects extractUserInfoObjects(
        mongoCppClient, accountsDB, userAccountsCollection
    );

    if(requestFinalFewMessagesInFull) {
        //NOTE: Do not use a view here, the cursor seems to remove them after they are iterated through occasionally(?)
        // not sure what the criteria are for removal before they are used, however if enough messages are returned
        // some will be removed.
        std::vector<bsoncxx::document::value> documents;
        for (auto& messageDoc : *findMessages) {
            documents.emplace_back(messageDoc);
        }

        for(size_t i = 0; i < documents.size(); i++) {
            unsigned int number_remaining_messages = documents.size() - i;
            if(number_remaining_messages <= chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST) { //if one of the last few messages

                //request full message
                ChatMessageToClient responseMsg;

                if (convertChatMessageDocumentToChatMessageToClient(
                        documents[i],
                        chatRoomId,
                        currentUserAccountOIDStr,
                        onlyStoreMessageBoolValue,
                        &responseMsg,
                        AmountOfMessage::COMPLETE_MESSAGE_INFO,
                        differentUserJoinedChatRoomAmount,
                        do_not_update_user_state,
                        &extractUserInfoObjects,
                        true)
                        ) {

                    storeAndSendMessagesToClient->sendMessage(std::move(responseMsg));
                }
            }
            else {

                //request skeleton of message
                ChatMessageToClient responseMsg;

                if (convertChatMessageDocumentToChatMessageToClient(
                        documents[i],
                        chatRoomId,
                        currentUserAccountOIDStr,
                        onlyStoreMessageBoolValue,
                        &responseMsg,
                        AmountOfMessage::ONLY_SKELETON,
                        differentUserJoinedChatRoomAmount,
                        do_not_update_user_state,
                        &extractUserInfoObjects,
                        true)
                        ) {

                    storeAndSendMessagesToClient->sendMessage(std::move(responseMsg));
                }
            }
        }
    }
    else { //requestFinalFewMessagesInFull == false

        for (const auto& messageDoc : *findMessages) {

            ChatMessageToClient responseMsg;

            if (convertChatMessageDocumentToChatMessageToClient(
                    messageDoc,
                    chatRoomId,
                    currentUserAccountOIDStr,
                    onlyStoreMessageBoolValue,
                    &responseMsg,
                    amountOfMessage,
                    differentUserJoinedChatRoomAmount,
                    do_not_update_user_state,
                    &extractUserInfoObjects,
                    true)
                    ) {
                storeAndSendMessagesToClient->sendMessage(std::move(responseMsg));
            }
        }
    }

    return true;
}


