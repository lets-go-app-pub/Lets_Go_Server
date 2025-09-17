//
// Created by jeremiah on 3/20/21.
//

#include <build_debug_string_response.h>
#include "chat_room_commands_helper_functions.h"

#include "utility_testing_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "chat_room_message_keys.h"

#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bsoncxx::document::value appendBasicActiveMessageInfo(
        bsoncxx::builder::stream::document& messageReplyDocument
) {
    bsoncxx::builder::stream::document activeMessageInfoDocument{};

    activeMessageInfoDocument
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << DeleteType::DELETE_TYPE_NOT_SET
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << open_array << close_array;

    bsoncxx::document::view messageReplyDocView = messageReplyDocument.view();

    if (messageReplyDocView.empty()) {
        activeMessageInfoDocument
                << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{};
    } else {
        activeMessageInfoDocument
                << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << messageReplyDocView;
    }

    return activeMessageInfoDocument << finalize;
}

//SendMessageToChatRoomReturn returns empty (default constructor) if fails, otherwise fills in at least the messageOid
SendMessageToChatRoomReturn sendMessageToChatRoom(
        const grpc_chat_commands::ClientMessageToServerRequest* request,
        mongocxx::collection& chatRoomCollection,
        const bsoncxx::oid& userAccountOID,
        mongocxx::client_session* const session,
        const std::string& insertedPictureOID,
        const std::string& previousMessage,
        const std::chrono::milliseconds& modifiedMessageCreatedTime,
        ReplyChatMessageInfo* replyChatMessageInfo) {

    bsoncxx::builder::stream::document insertDocBuilder{};
    bsoncxx::builder::stream::document messageReplyDocument{};

    if (replyChatMessageInfo != nullptr) { //message is type reply
        //NOTE: replyChatMessageInfo was checked to be valid inside client_message_to_server.cpp

        // request->message().is_reply() is saved to the document inside HandleMessage() function
        messageReplyDocument
                << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << open_document
                    << "$literal" << replyChatMessageInfo->reply_is_sent_from_user_oid()
                << close_document
                << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << open_document
                    << "$literal" << replyChatMessageInfo->reply_is_to_message_uuid()
                << close_document
                << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << replyChatMessageInfo->reply_specifics().reply_body_case();

        switch (replyChatMessageInfo->reply_specifics().reply_body_case()) {
            case ReplySpecifics::kTextReply: {

                //trim message if too long
                std::string replyText;
                if (replyChatMessageInfo->reply_specifics().text_reply().message_text().size() >
                server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY) {
                    replyText = replyChatMessageInfo->reply_specifics().text_reply().message_text().substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY);
                } else {
                    replyText = replyChatMessageInfo->reply_specifics().text_reply().message_text();
                }

                trimTrailingWhitespace(replyText);

                if(replyText.empty()) {
                    replyText = " ";
                }

                messageReplyDocument
                        << chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT << open_document
                            << "$literal" << replyText
                        << close_document;
                break;
            }
            case ReplySpecifics::kPictureReply: {

                std::string* thumbnail_in_bytes = replyChatMessageInfo->mutable_reply_specifics()->mutable_picture_reply()->release_thumbnail_in_bytes();

                if(thumbnail_in_bytes == nullptr) {
                    std::string errorString = "A chat room picture thumbnail value was null inside the request.\n";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), errorString,
                            "request", buildDebugStringResponse(request),
                            "user_oid", userAccountOID);

                    return {};
                }

                messageReplyDocument
                        << chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES << open_document
                            << "$literal" << std::move(*thumbnail_in_bytes)
                        << close_document
                        << chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE << replyChatMessageInfo->reply_specifics().picture_reply().thumbnail_file_size();

                delete thumbnail_in_bytes;
                break;
            }
            case ReplySpecifics::kMimeReply: {
                std::string* thumbnail_in_bytes = replyChatMessageInfo->mutable_reply_specifics()->mutable_mime_reply()->release_thumbnail_in_bytes();

                if(thumbnail_in_bytes == nullptr) {

                    std::string errorString = "A chat room picture thumbnail value was null inside the request.\n";

                    std::optional<std::string> dummy_exception_string;
                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  dummy_exception_string,
                                                  errorString,
                                                  "request", buildDebugStringResponse(request),
                                                  "user_oid", userAccountOID);

                    return {};
                }

                messageReplyDocument
                        << chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES << open_document
                            << "$literal" << std::move(*thumbnail_in_bytes)
                        << close_document
                        << chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE << replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_file_size()
                        << chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE << open_document
                            << "$literal" << replyChatMessageInfo->reply_specifics().mime_reply().thumbnail_mime_type()
                        << close_document;

                delete thumbnail_in_bytes;
                break;
            }
            case ReplySpecifics::kLocationReply:
            case ReplySpecifics::kInviteReply: {
                break;
            }
            case ReplySpecifics::REPLY_BODY_NOT_SET:
                std::string errorString = "Message set as a reply however invalid type for replying, this should never happen here it should be checked for before this function is called.\n";

                storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), errorString,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chatRoomCollection.name().to_string(),
                    "ObjectID_used", userAccountOID.to_string(),
                    "message_type",
                    convertMessageBodyTypeToString(request->message().message_specifics().message_body_case()),
                    "replying_to_type",
                    convertReplyBodyCaseToString(replyChatMessageInfo->reply_specifics().reply_body_case())
                );

                return {};
        }

    }

    switch (request->message().message_specifics().message_body_case()) {
        case MessageSpecifics::kTextMessage: {

//            std::unique_ptr<bsoncxx::types::b_date> editedTimeDate;
//
//            //the edited time will need to be set to -1 if it is not a 'real' timestamp
//            if (request->message().message_specifics().text_message().edited_time() <
//                general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count()) {
//                editedTimeDate = std::make_unique<bsoncxx::types::b_date>(
//                        bsoncxx::types::b_date{std::chrono::milliseconds{-1}});
//            } else {
//                editedTimeDate = std::make_unique<bsoncxx::types::b_date>(bsoncxx::types::b_date{
//                        std::chrono::milliseconds{
//                                request->message().message_specifics().text_message().edited_time()}});
//            }

            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::TEXT_MESSAGE << open_document
                            << "$literal" << request->message().message_specifics().text_message().message_text()
                        << close_document
                        << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << false
                        << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
                        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << appendBasicActiveMessageInfo(messageReplyDocument)
                    << close_document;

            break;
        }
        case MessageSpecifics::kPictureMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::PICTURE_OID << open_document
                            << "$literal" << insertedPictureOID
                        << close_document
                        << chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH << request->message().message_specifics().picture_message().image_width()
                        << chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT << request->message().message_specifics().picture_message().image_height()
                        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << appendBasicActiveMessageInfo(messageReplyDocument)
                    << close_document;

            break;
        case MessageSpecifics::kLocationMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::LOCATION_LONGITUDE << request->message().message_specifics().location_message().longitude()
                        << chat_room_message_keys::message_specifics::LOCATION_LATITUDE << request->message().message_specifics().location_message().latitude()
                        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << appendBasicActiveMessageInfo(messageReplyDocument)
                    << close_document;

            break;
        case MessageSpecifics::kMimeTypeMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::MIME_TYPE_URL << open_document
                            << "$literal" << request->message().message_specifics().mime_type_message().url_of_download()
                        << close_document
                        << chat_room_message_keys::message_specifics::MIME_TYPE_TYPE <<  open_document
                            << "$literal" << request->message().message_specifics().mime_type_message().mime_type()
                        << close_document
                        << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH << request->message().message_specifics().mime_type_message().image_width()
                        << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT << request->message().message_specifics().mime_type_message().image_height()
                        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << appendBasicActiveMessageInfo(messageReplyDocument)
                    << close_document;
            break;
        case MessageSpecifics::kInviteMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << open_document
                            << "$literal" << request->message().message_specifics().invite_message().invited_user_account_oid()
                        << close_document
                        << chat_room_message_keys::message_specifics::INVITED_USER_NAME << open_document
                            << "$literal" << request->message().message_specifics().invite_message().invited_user_name()
                        << close_document
                        << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID <<open_document
                            << "$literal" << request->message().message_specifics().invite_message().chat_room_id()
                        << close_document
                        << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME <<open_document
                            << "$literal" << request->message().message_specifics().invite_message().chat_room_name()
                        << close_document
                        << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD <<open_document
                            << "$literal" << request->message().message_specifics().invite_message().chat_room_password()
                        << close_document
                        << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << appendBasicActiveMessageInfo(messageReplyDocument)
                    << close_document;

            break;
        case MessageSpecifics::kEditedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                    << chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID << open_document
                        << "$literal" << request->message_uuid()
                    << close_document
                    << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << open_document
                        << "$literal" << request->message().message_specifics().edited_message().message_uuid()
                    << close_document
                    << chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << open_document
                        << "$literal" << request->message().message_specifics().edited_message().new_message()
                    << close_document
                    << chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << open_document
                        << "$literal" << previousMessage
                    << close_document
                    << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
                    modifiedMessageCreatedTime}
                    << close_document;
            break;
        case MessageSpecifics::kDeletedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                    << chat_room_message_keys::message_specifics::DELETED_THIS_MESSAGE_UUID << open_document
                        << "$literal" << request->message_uuid()
                    << close_document
                    << chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID << open_document
                        << "$literal" << request->message().message_specifics().deleted_message().message_uuid()
                    << close_document
                    << chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE << request->message().message_specifics().deleted_message().delete_type()
                    << chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_CREATED_TIME << bsoncxx::types::b_date{
                    modifiedMessageCreatedTime}
                    << close_document;
            break;
        case MessageSpecifics::kUserKickedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::USER_KICKED_KICKED_OID << open_document
                            << "$literal" << request->message().message_specifics().user_kicked_message().kicked_account_oid()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kUserBannedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::USER_BANNED_BANNED_OID << open_document
                            << "$literal" << request->message().message_specifics().user_banned_message().banned_account_oid()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kDifferentUserLeftMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::USER_NEW_ACCOUNT_ADMIN_OID << open_document
                            << "$literal" << request->message().message_specifics().different_user_left_message().new_admin_account_oid()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kChatRoomCapMessage:
        case MessageSpecifics::kUserActivityDetectedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                    << close_document;
            break;
        case MessageSpecifics::kChatRoomNameUpdatedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::NAME_NEW_NAME << open_document
                            << "$literal" << request->message().message_specifics().chat_room_name_updated_message().new_chat_room_name()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::PASSWORD_NEW_PASSWORD << open_document
                            << "$literal" << request->message().message_specifics().chat_room_password_updated_message().new_chat_room_password()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kNewAdminPromotedMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::NEW_ADMIN_ADMIN_ACCOUNT_OID << open_document
                            << "$literal" << request->message().message_specifics().new_admin_promoted_message().promoted_account_oid()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kNewPinnedLocationMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::PINNED_LOCATION_LONGITUDE << request->message().message_specifics().new_pinned_location_message().longitude()
                        << chat_room_message_keys::message_specifics::PINNED_LOCATION_LATITUDE << request->message().message_specifics().new_pinned_location_message().latitude()
                    << close_document;
            break;
        case MessageSpecifics::kMatchCanceledMessage:
            insertDocBuilder
                    << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << open_document
                        << chat_room_message_keys::message_specifics::MATCH_CANCELED_ACCOUNT_OID << open_document
                            << "$literal" << request->message().message_specifics().match_canceled_message().matched_account_oid()
                        << close_document
                    << close_document;
            break;
        case MessageSpecifics::kUpdateObservedTimeMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
        case MessageSpecifics::kThisUserLeftChatRoomMessage:
        case MessageSpecifics::kHistoryClearedMessage:
        case MessageSpecifics::kNewUpdateTimeMessage:
        case MessageSpecifics::kLoadingMessage:
        case MessageSpecifics::kDifferentUserJoinedMessage:
        case MessageSpecifics::MESSAGE_BODY_NOT_SET: {
            std::string errorString = "Invalid message type passed to sendMessageToChatRoom() it should be checked for before this function is called.\n";

            std::optional<std::string> dummy_exception_string;
            storeMongoDBErrorAndException(__LINE__,
                                          __FILE__, dummy_exception_string, errorString,
                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                          "collection", chatRoomCollection.name().to_string(),
                                          "ObjectID_used", userAccountOID.to_string(),
                                          "message_type",
                                          convertMessageBodyTypeToString(
                                                  request->message().message_specifics().message_body_case()));

            return {};
        }
    }

    SendMessageToChatRoomReturn message_sent_result = handleMessageToBeSent(
        session,
        chatRoomCollection,
        request,
        insertDocBuilder,
        userAccountOID
    );

    return message_sent_result;

}