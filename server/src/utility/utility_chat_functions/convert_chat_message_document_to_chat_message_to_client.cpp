//
// Created by jeremiah on 4/7/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <utility_testing_functions.h>
#include <sstream>

#include "utility_chat_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "server_parameter_restrictions.h"
#include "chat_stream_container.h"
#include "general_values.h"
#include "chat_room_message_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

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
        bool internal_force_send_message_to_current_user
) {

    if (extractUserInfoObjects == nullptr &&
        (amountOfMessage == AmountOfMessage::COMPLETE_MESSAGE_INFO ||
         differentUserJoinedChatRoomAmount != DifferentUserJoinedChatRoomAmount::SKELETON)) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string("convertChatMessageDocumentToChatMessageToClient() was called with invalid parameters."),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME,
                "amountOfMessage", AmountOfMessage_Name(amountOfMessage),
                "differentUserJoinedChatRoomAmount",convertDifferentUserJoinedChatRoomAmountToString(differentUserJoinedChatRoomAmount)
        );

        return false;
    }

    std::string messageUUIDFromIDField;
    std::chrono::milliseconds messageCreatedTime;
    std::string messageSentByOID;
    MessageSpecifics::MessageBodyCase messageType;
    bsoncxx::document::view specificsDocument;

    //NOTE: for USER_ACTIVITY_DETECTED this is actually MESSAGE_SENT_BY because of the $group stage
    //NOTE: for MESSAGE_EDITED and MESSAGE_DELETED this is actually CHAT_ROOM_MESSAGE_OID_VALUE because of the $group stage
    auto messageOIDElement = messageDoc["_id"];
    if (messageOIDElement &&
        messageOIDElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        messageUUIDFromIDField = messageOIDElement.get_string().value.to_string();
    } else { //if element does not exist or is not type oid
        logElementError(__LINE__, __FILE__, messageOIDElement,
                        messageDoc, bsoncxx::type::k_utf8, "_id",
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return false;
    }

    auto messageCreatedTimeElement = messageDoc[chat_room_shared_keys::TIMESTAMP_CREATED];
    if (messageCreatedTimeElement &&
        messageCreatedTimeElement.type() == bsoncxx::type::k_date) { //if element exists and is type date
        messageCreatedTime = messageCreatedTimeElement.get_date().value;
    } else { //if element does not exist or is not type date
        logElementError(__LINE__, __FILE__, messageCreatedTimeElement,
                        messageDoc, bsoncxx::type::k_date, chat_room_shared_keys::TIMESTAMP_CREATED,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return false;
    }

    auto messageSentByOIDElement = messageDoc[chat_room_message_keys::MESSAGE_SENT_BY];
    if (messageSentByOIDElement &&
        messageSentByOIDElement.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
        messageSentByOID = messageSentByOIDElement.get_oid().value.to_string();
    } else { //if element does not exist or is not type oid
        logElementError(__LINE__, __FILE__, messageSentByOIDElement,
                        messageDoc, bsoncxx::type::k_oid, chat_room_message_keys::MESSAGE_SENT_BY,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return false;
    }

    auto messageTypeElement = messageDoc[chat_room_message_keys::MESSAGE_TYPE];
    if (messageTypeElement &&
        messageTypeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        messageType = MessageSpecifics::MessageBodyCase(messageTypeElement.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, messageTypeElement,
                        messageDoc, bsoncxx::type::k_int32, chat_room_message_keys::MESSAGE_TYPE,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return false;
    }

    auto specificsDocumentElement = messageDoc[chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT];
    if (specificsDocumentElement &&
        specificsDocumentElement.type() == bsoncxx::type::k_document) { //if element exists and is type document
        specificsDocument = specificsDocumentElement.get_document().value;
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, specificsDocumentElement,
                        messageDoc, bsoncxx::type::k_document, chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return false;
    }

    responseMsg->set_message_uuid(messageUUIDFromIDField);
    responseMsg->set_sent_by_account_id(messageSentByOID);
    responseMsg->set_timestamp_stored(messageCreatedTime.count());

    //This will tell the client if this message is for a newly added chat room (such as join chat room).
    responseMsg->set_only_store_message(onlyStoreMessageBoolValue);

    StandardChatMessageInfo* standardChatMessageInfo = responseMsg->mutable_message()->mutable_standard_message_info();
    standardChatMessageInfo->set_chat_room_id_message_sent_from(chatRoomId);
    standardChatMessageInfo->set_do_not_update_user_state(do_not_update_user_state);
    standardChatMessageInfo->set_internal_force_send_message_to_current_user(internal_force_send_message_to_current_user);

    switch (messageType) {
        case MessageSpecifics::kTextMessage: {
            TextChatMessage* chatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_text_message();

            bsoncxx::document::view activeMessageInfoDocView;

            auto activeMessageInfoDocViewElement = specificsDocument[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT];
            if (activeMessageInfoDocViewElement &&
                activeMessageInfoDocViewElement.type() ==
                bsoncxx::type::k_document) { //if element exists and is type document
                activeMessageInfoDocView = activeMessageInfoDocViewElement.get_document().value;
            } else { //if element does not exist or is not type document
                logElementError(__LINE__, __FILE__, activeMessageInfoDocViewElement,
                                specificsDocument, bsoncxx::type::k_document,
                                chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!extractMessageDeleted(
                    chatRoomId,
                    userAccountOIDStr,
                    activeMessageInfoDocView,
                    chatMessage->mutable_active_message_info())
                    ) {
                //error was already stored
                return false;
            }

            if (userAccountOIDStr != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT && chatMessage->mutable_active_message_info()->is_deleted()) {
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                standardChatMessageInfo->set_message_has_complete_info(true);
                break;
            }

            standardChatMessageInfo->set_amount_of_message(amountOfMessage);

            if (!extractReplyFromMessage(
                    chatRoomId,
                    activeMessageInfoDocView,
                    amountOfMessage,
                    chatMessage->mutable_active_message_info())
            ) {
                return false;
            }

            if (amountOfMessage == AmountOfMessage::ONLY_SKELETON) {
                break;
            }

            auto messageIsEditedElement = specificsDocument[chat_room_message_keys::message_specifics::TEXT_IS_EDITED];
            if (messageIsEditedElement &&
                messageIsEditedElement.type() == bsoncxx::type::k_bool) { //if element exists and is type bool
                chatMessage->set_is_edited(messageIsEditedElement.get_bool().value);
            } else { //if element does not exist or is not type bool
                logElementError(__LINE__, __FILE__, messageIsEditedElement,
                                specificsDocument, bsoncxx::type::k_bool, chat_room_message_keys::message_specifics::TEXT_IS_EDITED,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto messageEditedTimeElement = specificsDocument[chat_room_message_keys::message_specifics::TEXT_EDITED_TIME];
            if (messageEditedTimeElement &&
                messageEditedTimeElement.type() == bsoncxx::type::k_date) { //if element exists and is type date
                chatMessage->set_edited_time(messageEditedTimeElement.get_date().value.count());
            } else { //if element does not exist or is not type date
                logElementError(__LINE__, __FILE__, messageEditedTimeElement,
                                specificsDocument, bsoncxx::type::k_date, chat_room_message_keys::message_specifics::TEXT_EDITED_TIME,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            std::string textMessage;

            auto chatMessageTextElement = specificsDocument[chat_room_message_keys::message_specifics::TEXT_MESSAGE];
            if (chatMessageTextElement &&
                chatMessageTextElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                textMessage = chatMessageTextElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, chatMessageTextElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::TEXT_MESSAGE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (amountOfMessage == AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE) {

                if(textMessage.empty()) {
                    const std::string error_string = "A text message was stored with no text. This should never happen.";
                    const bsoncxx::document::view message_view = messageDoc;

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                            "message_document", message_view,
                            "messageUUIDFromIDField", messageUUIDFromIDField
                    );

                    chatMessage->set_message_text("");
                } else {
                    chatMessage->set_message_text(textMessage.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE));
                }

                if (textMessage.size() <= server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE
                    && !chatMessage->active_message_info().is_reply()
                ) {
                    standardChatMessageInfo->set_message_has_complete_info(true);
                }

                break;
            }

            chatMessage->set_message_text(textMessage);

            standardChatMessageInfo->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kPictureMessage: {
            PictureChatMessage* pictureChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_picture_message();

            bsoncxx::document::view activeMessageInfoDocView;

            auto activeMessageInfoDocViewElement = specificsDocument[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT];
            if (activeMessageInfoDocViewElement &&
                activeMessageInfoDocViewElement.type() ==
                bsoncxx::type::k_document) { //if element exists and is type document
                activeMessageInfoDocView = activeMessageInfoDocViewElement.get_document().value;
            } else { //if element does not exist or is not type document
                logElementError(__LINE__, __FILE__, activeMessageInfoDocViewElement,
                                specificsDocument, bsoncxx::type::k_document,
                                chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!extractMessageDeleted(
                    chatRoomId,
                    userAccountOIDStr,
                    activeMessageInfoDocView,
                    pictureChatMessage->mutable_active_message_info()
            )) {
                //error was already stored
                return false;
            }

            if (userAccountOIDStr != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT && pictureChatMessage->mutable_active_message_info()->is_deleted()) {
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
                break;
            }

            if (!extractReplyFromMessage(
                    chatRoomId,
                    activeMessageInfoDocView,
                    amountOfMessage,
                    pictureChatMessage->mutable_active_message_info())
            ) {
                return false;
            }

            std::string pictureOIDString;

            auto pictureOIDElement = specificsDocument[chat_room_message_keys::message_specifics::PICTURE_OID];
            if (pictureOIDElement &&
                pictureOIDElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                pictureOIDString = pictureOIDElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, pictureOIDElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::PICTURE_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            switch (amountOfMessage) {
                case AmountOfMessage_INT_MIN_SENTINEL_DO_NOT_USE_:
                case AmountOfMessage_INT_MAX_SENTINEL_DO_NOT_USE_:
                case ONLY_SKELETON:
                case ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE: {

                    standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

                    auto pictureWidthElement = specificsDocument[chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH];
                    if (pictureWidthElement &&
                        pictureWidthElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                        pictureChatMessage->set_image_width(pictureWidthElement.get_int32().value);
                    } else { //if element does not exist or is not type int32
                        logElementError(__LINE__, __FILE__, pictureWidthElement,
                                        specificsDocument, bsoncxx::type::k_int32,
                                        chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                        return false;
                    }

                    auto pictureHeightElement = specificsDocument[chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT];
                    if (pictureHeightElement &&
                        pictureHeightElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                        pictureChatMessage->set_image_height(pictureHeightElement.get_int32().value);
                    } else { //if element does not exist or is not type int32
                        logElementError(__LINE__, __FILE__, pictureHeightElement,
                                        specificsDocument, bsoncxx::type::k_int32,
                                        chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                        return false;
                    }

                    break;
                }
                case COMPLETE_MESSAGE_INFO: {

                    standardChatMessageInfo->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);

                    if (extractUserInfoObjects != nullptr) {

                        auto successful = [&](const int pictureSize,
                                              const int pictureHeight, const int pictureWidth,
                                              std::string& pictureByteString
                        ) {
                            pictureChatMessage->set_image_height(pictureHeight);
                            pictureChatMessage->set_image_width(pictureWidth);
                            pictureChatMessage->set_picture_file_size(pictureSize);
                            pictureChatMessage->set_picture_file_in_bytes(std::move(pictureByteString));
                        };

                        auto pictureCorrupt = [&]() {
                            pictureChatMessage->set_picture_file_size(-1);
                        };

                        if (!extractChatPicture(
                                extractUserInfoObjects->mongoCppClient,
                                pictureOIDString,
                                chatRoomId,
                                userAccountOIDStr,
                                messageUUIDFromIDField,
                                successful,
                                pictureCorrupt)
                        ) { //if error occurred
                            return false;
                        }

                        responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(
                                true);

                    }

                    break;
                }
            }
            break;
        }
        case MessageSpecifics::kLocationMessage: {
            LocationChatMessage* locationChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_location_message();

            bsoncxx::document::view activeMessageInfoDocView;

            auto activeMessageInfoDocViewElement = specificsDocument[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT];
            if (activeMessageInfoDocViewElement
                && activeMessageInfoDocViewElement.type() == bsoncxx::type::k_document) { //if element exists and is type document
                activeMessageInfoDocView = activeMessageInfoDocViewElement.get_document().value;
            } else { //if element does not exist or is not type document
                logElementError(__LINE__, __FILE__, activeMessageInfoDocViewElement,
                                specificsDocument, bsoncxx::type::k_document,
                                chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!extractMessageDeleted(
                    chatRoomId,
                    userAccountOIDStr,
                    activeMessageInfoDocView,
                    locationChatMessage->mutable_active_message_info()
            )) {
                //error was already stored
                return false;
            }

            if (userAccountOIDStr != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT && locationChatMessage->mutable_active_message_info()->is_deleted()) {
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
                break;
            }

            if (!extractReplyFromMessage(
                    chatRoomId,
                    activeMessageInfoDocView,
                    amountOfMessage,
                    locationChatMessage->mutable_active_message_info())
            ) {
                return false;
            }

            auto longitudeElement = specificsDocument[chat_room_message_keys::message_specifics::LOCATION_LONGITUDE];
            if (longitudeElement &&
                longitudeElement.type() == bsoncxx::type::k_double) { //if element exists and is type double
                locationChatMessage->set_longitude(longitudeElement.get_double().value);
            } else { //if element does not exist or is not type double
                logElementError(__LINE__, __FILE__, longitudeElement,
                                specificsDocument, bsoncxx::type::k_double, chat_room_message_keys::message_specifics::LOCATION_LONGITUDE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto latitudeElement = specificsDocument[chat_room_message_keys::message_specifics::LOCATION_LATITUDE];
            if (latitudeElement &&
                latitudeElement.type() == bsoncxx::type::k_double) { //if element exists and is type double
                locationChatMessage->set_latitude(latitudeElement.get_double().value);
            } else { //if element does not exist or is not type double
                logElementError(__LINE__, __FILE__, latitudeElement,
                                specificsDocument, bsoncxx::type::k_double, chat_room_message_keys::message_specifics::LOCATION_LATITUDE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!locationChatMessage->active_message_info().is_reply()) { //if message is not a reply
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            } else if (amountOfMessage !=
                       AmountOfMessage::COMPLETE_MESSAGE_INFO) { //if message is a reply and does not have complete info
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
            } else { //if message is a reply and has complete info
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            }

            break;
        }
        case MessageSpecifics::kMimeTypeMessage: {
            MimeTypeChatMessage* mimeTypeChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_mime_type_message();

            bsoncxx::document::view activeMessageInfoDocView;

            auto activeMessageInfoDocViewElement = specificsDocument[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT];
            if (activeMessageInfoDocViewElement
                && activeMessageInfoDocViewElement.type() == bsoncxx::type::k_document) { //if element exists and is type document
                activeMessageInfoDocView = activeMessageInfoDocViewElement.get_document().value;
            } else { //if element does not exist or is not type document
                logElementError(__LINE__, __FILE__, activeMessageInfoDocViewElement,
                                specificsDocument, bsoncxx::type::k_document,
                                chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!extractMessageDeleted(
                    chatRoomId,
                    userAccountOIDStr,
                    activeMessageInfoDocView,
                    mimeTypeChatMessage->mutable_active_message_info())
                    ) {
                //error was already stored
                return false;
            }

            if (userAccountOIDStr != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT && mimeTypeChatMessage->mutable_active_message_info()->is_deleted()) {
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
                break;
            }

            if (!extractReplyFromMessage(
                    chatRoomId,
                    activeMessageInfoDocView,
                    amountOfMessage,
                    mimeTypeChatMessage->mutable_active_message_info())
            ) {
                return false;
            }

            auto gifWidthElement = specificsDocument[chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH];
            if (gifWidthElement &&
                gifWidthElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                mimeTypeChatMessage->set_image_width(gifWidthElement.get_int32().value);
            } else { //if element does not exist or is not type int32
                logElementError(__LINE__, __FILE__, gifWidthElement,
                                specificsDocument, bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto gifHeightElement = specificsDocument[chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT];
            if (gifHeightElement &&
                gifHeightElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                mimeTypeChatMessage->set_image_height(gifHeightElement.get_int32().value);
            } else { //if element does not exist or is not type int32
                logElementError(__LINE__, __FILE__, gifHeightElement,
                                specificsDocument, bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            /** It is important to the client to pass back both the mime type and url no matter what AmountOfMessage
             * is set to in order to updated a reference count. **/

            auto messageMimeTypeElement = specificsDocument[chat_room_message_keys::message_specifics::MIME_TYPE_TYPE];
            if (messageMimeTypeElement && messageMimeTypeElement.type() ==
                                          bsoncxx::type::k_utf8) { //if element exists and is type utf8
                mimeTypeChatMessage->set_mime_type(messageMimeTypeElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, messageMimeTypeElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::MIME_TYPE_TYPE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto urlElement = specificsDocument[chat_room_message_keys::message_specifics::MIME_TYPE_URL];
            if (urlElement && urlElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                mimeTypeChatMessage->set_url_of_download(urlElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, urlElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::MIME_TYPE_URL,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!mimeTypeChatMessage->active_message_info().is_reply()) { //if message is not a reply
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            } else if (amountOfMessage !=
                       AmountOfMessage::COMPLETE_MESSAGE_INFO) { //if message is a reply and does not have complete info
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
            } else { //if message is a reply and has complete info
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            }

            break;
        }
        case MessageSpecifics::kInviteMessage: {
            InviteChatMessage* inviteChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_invite_message();

            bsoncxx::document::view activeMessageInfoDocView;

            auto activeMessageInfoDocViewElement = specificsDocument[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT];
            if (activeMessageInfoDocViewElement
                && activeMessageInfoDocViewElement.type() == bsoncxx::type::k_document) { //if element exists and is type document
                activeMessageInfoDocView = activeMessageInfoDocViewElement.get_document().value;
            } else { //if element does not exist or is not type document
                logElementError(__LINE__, __FILE__, activeMessageInfoDocViewElement,
                                specificsDocument, bsoncxx::type::k_document,
                                chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!extractMessageDeleted(
                    chatRoomId,
                    userAccountOIDStr,
                    activeMessageInfoDocView,
                    inviteChatMessage->mutable_active_message_info())
                    ) {
                //error was already stored
                return false;
            }

            if (
                userAccountOIDStr != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT
                && inviteChatMessage->mutable_active_message_info()->is_deleted()
            ) {
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
                break;
            }

            if (!extractReplyFromMessage(
                    chatRoomId,
                    activeMessageInfoDocView,
                    amountOfMessage,
                    inviteChatMessage->mutable_active_message_info())
            ) {
                return false;
            }

            std::string invite_user_account_oid;

            auto invitedUserAccountOIDElement = specificsDocument[chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID];
            if (invitedUserAccountOIDElement &&
                invitedUserAccountOIDElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                invite_user_account_oid = invitedUserAccountOIDElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                logElementError(
                        __LINE__, __FILE__,
                        invitedUserAccountOIDElement,specificsDocument,
                        bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId
                );

                return false;
            }

            //this could happen if user somehow got an incorrect oid and requested the full message from
            // the chat stream
            if (userAccountOIDStr != chat_stream_container::CHAT_CHANGE_STREAM_PASSED_STRING_TO_CONVERT
                && userAccountOIDStr != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT
                && invite_user_account_oid != userAccountOIDStr
                && messageSentByOID != userAccountOIDStr) {

                //set message to deleted if this message is not meant for the current user
                inviteChatMessage->mutable_active_message_info()->set_is_deleted(true);
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
                break;
            }

            inviteChatMessage->set_invited_user_account_oid(invite_user_account_oid);

            auto invitedUserNameElement = specificsDocument[chat_room_message_keys::message_specifics::INVITED_USER_NAME];
            if (invitedUserNameElement &&
                invitedUserNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                inviteChatMessage->set_invited_user_name(invitedUserNameElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, invitedUserNameElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::INVITED_USER_NAME,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto invitedChatRoomIdElement = specificsDocument[chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID];
            if (invitedChatRoomIdElement &&
                invitedChatRoomIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                inviteChatMessage->set_chat_room_id(invitedChatRoomIdElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, invitedChatRoomIdElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto invitedChatRoomNameElement = specificsDocument[chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME];
            if (invitedChatRoomNameElement &&
                invitedChatRoomNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                inviteChatMessage->set_chat_room_name(invitedChatRoomNameElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, invitedChatRoomNameElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto invitedChatRoomPasswordElement = specificsDocument[chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD];
            if (invitedChatRoomPasswordElement &&
                invitedChatRoomPasswordElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                inviteChatMessage->set_chat_room_password(invitedChatRoomPasswordElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, invitedChatRoomPasswordElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (!inviteChatMessage->active_message_info().is_reply()) { //if message is not a reply
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            } else if (amountOfMessage !=
                       AmountOfMessage::COMPLETE_MESSAGE_INFO) { //if message is a reply and does not have complete info
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
            } else { //if message is a reply and has complete info
                standardChatMessageInfo->set_amount_of_message(AmountOfMessage::COMPLETE_MESSAGE_INFO);
                responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            }

            break;
        }
        case MessageSpecifics::kUserKickedMessage: {
            UserKickedChatMessage* userKickedChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_user_kicked_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto userKickedOidElement = specificsDocument[chat_room_message_keys::message_specifics::USER_KICKED_KICKED_OID];
            if (userKickedOidElement &&
                userKickedOidElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                userKickedChatMessage->set_kicked_account_oid(userKickedOidElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, userKickedOidElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::USER_KICKED_KICKED_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kUserBannedMessage: {
            UserBannedChatMessage* userBannedChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_user_banned_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto userBannedOidElement = specificsDocument[chat_room_message_keys::message_specifics::USER_BANNED_BANNED_OID];
            if (userBannedOidElement &&
                userBannedOidElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                userBannedChatMessage->set_banned_account_oid(userBannedOidElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, userBannedOidElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::USER_BANNED_BANNED_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kDifferentUserJoinedMessage: {
            DifferentUserJoinedChatRoomChatMessage* differentUserJoinedChatRoomChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_different_user_joined_message();

            //The kDifferentUserJoinedMessage itself only consists of the USER_JOINED_ACCOUNT_STATE, the rest of the user info will
            // be associated with the member and optionally sent back
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
            standardChatMessageInfo->set_message_has_complete_info(true);

            //NOTE: don't cancel this based on amountOfMessage
            //NOTE: don't need to extract CHAT_ROOM_MESSAGE_ACCOUNT_OID because MESSAGE_SENT_BY will be the same

            //Cases where this function will be called:
            //1) on initial login joining new chat room
            //2) on initial login updating chat room present on device
            //3) user manually joins chat room
            //4) request message info from chat stream is called
            //5) updateChatRoom
            //6) updateChatRoomMember

            auto memberInfo = differentUserJoinedChatRoomChatMessage->mutable_member_info();

            auto accountStateElement = specificsDocument[chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE];
            if (accountStateElement &&
                accountStateElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                memberInfo->set_account_state(AccountStateInChatRoom(accountStateElement.get_int32().value));
            } else { //if element does not exist or is not type int32
                logElementError(__LINE__, __FILE__, accountStateElement,
                                specificsDocument, bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            //the created time is the same time as the 'last active time' of this member
            memberInfo->set_account_last_activity_time(messageCreatedTime.count());

            if(differentUserJoinedChatRoomAmount == DifferentUserJoinedChatRoomAmount::SKELETON) {
                //send no user info back in this case
                break;
            }

            HowToHandleMemberPictures howToHandleMemberPictures;

            switch (differentUserJoinedChatRoomAmount) {
                case DifferentUserJoinedChatRoomAmount::SKELETON:
                case DifferentUserJoinedChatRoomAmount::INFO_WITHOUT_IMAGES: {
                    howToHandleMemberPictures = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;
                    break;
                }
                case DifferentUserJoinedChatRoomAmount::INFO_WITH_THUMBNAIL_NO_PICTURES: {
                    howToHandleMemberPictures = HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL;
                    break;
                }
                case DifferentUserJoinedChatRoomAmount::ALL_INFO_AND_IMAGES: {
                    howToHandleMemberPictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO;
                    break;
                }
                default: {
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(),
                            std::string("An invalid value of DifferentUserJoinedChatRoomAmount was passed."),
                            "differentUserJoinedChatRoomAmount", std::to_string((int)differentUserJoinedChatRoomAmount));

                    return false;
                }
            }

            auto userMemberInfo = memberInfo->mutable_user_info();

            if (!getUserAccountInfoForUserJoinedChatRoom(
                    extractUserInfoObjects->mongoCppClient,
                    extractUserInfoObjects->accountsDB,
                    extractUserInfoObjects->userAccountsCollection,
                    chatRoomId,
                    messageSentByOID,
                    howToHandleMemberPictures,
                    userMemberInfo)
            ) {
                return false;
            }

            break;
        }
        case MessageSpecifics::kDifferentUserLeftMessage: {
            DifferentUserLeftChatRoomChatMessage* differentUserLeftChatRoomChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_different_user_left_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto differentUserLeftElement = specificsDocument[chat_room_message_keys::message_specifics::USER_NEW_ACCOUNT_ADMIN_OID];
            if (differentUserLeftElement
                && differentUserLeftElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                differentUserLeftChatRoomChatMessage->set_new_admin_account_oid(differentUserLeftElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, differentUserLeftElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::USER_NEW_ACCOUNT_ADMIN_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kChatRoomNameUpdatedMessage: {
            ChatRoomNameUpdatedChatMessage* chatRoomNameUpdatedChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_chat_room_name_updated_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto newNameElement = specificsDocument[chat_room_message_keys::message_specifics::NAME_NEW_NAME];
            if (newNameElement && newNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                chatRoomNameUpdatedChatMessage->set_new_chat_room_name(newNameElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, newNameElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::NAME_NEW_NAME,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage: {
            ChatRoomPasswordUpdatedChatMessage* chatRoomPasswordUpdatedChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_chat_room_password_updated_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto newPasswordElement = specificsDocument[chat_room_message_keys::message_specifics::PASSWORD_NEW_PASSWORD];
            if (newPasswordElement &&
                newPasswordElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                chatRoomPasswordUpdatedChatMessage->set_new_chat_room_password(
                        newPasswordElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, newPasswordElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::PASSWORD_NEW_PASSWORD,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kNewAdminPromotedMessage: {
            NewAdminPromotedChatMessage* newAdminPromotedChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_new_admin_promoted_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto newAdminPromotedElement = specificsDocument[chat_room_message_keys::message_specifics::NEW_ADMIN_ADMIN_ACCOUNT_OID];
            if (newAdminPromotedElement &&
                newAdminPromotedElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                newAdminPromotedChatMessage->set_promoted_account_oid(
                        newAdminPromotedElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, newAdminPromotedElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::NEW_ADMIN_ADMIN_ACCOUNT_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kNewPinnedLocationMessage: {
            NewPinnedLocationMessage* newPinnedLocationChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_new_pinned_location_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto longitudeElement = specificsDocument[chat_room_message_keys::message_specifics::PINNED_LOCATION_LONGITUDE];
            if (longitudeElement
                && longitudeElement.type() == bsoncxx::type::k_double) { //if element exists and is type double
                newPinnedLocationChatMessage->set_longitude(longitudeElement.get_double().value);
            } else { //if element does not exist or is not type double
                logElementError(__LINE__, __FILE__, longitudeElement,
                                specificsDocument, bsoncxx::type::k_double,
                                chat_room_message_keys::message_specifics::PINNED_LOCATION_LONGITUDE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto latitudeElement = specificsDocument[chat_room_message_keys::message_specifics::PINNED_LOCATION_LATITUDE];
            if (latitudeElement
                && latitudeElement.type() == bsoncxx::type::k_double) { //if element exists and is type double
                newPinnedLocationChatMessage->set_latitude(latitudeElement.get_double().value);
            } else { //if element does not exist or is not type double
                logElementError(__LINE__, __FILE__, latitudeElement,
                                specificsDocument, bsoncxx::type::k_double,
                                chat_room_message_keys::message_specifics::PINNED_LOCATION_LATITUDE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kMatchCanceledMessage: {
            MatchCanceledChatMessage* matchCanceledChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_match_canceled_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            auto matchCanceledOidElement = specificsDocument[chat_room_message_keys::message_specifics::MATCH_CANCELED_ACCOUNT_OID];
            if (matchCanceledOidElement &&
                matchCanceledOidElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                matchCanceledChatMessage->set_matched_account_oid(matchCanceledOidElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, matchCanceledOidElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::MATCH_CANCELED_ACCOUNT_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kEditedMessage: {
            EditedMessageChatMessage* editedMessageChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_edited_message();
            standardChatMessageInfo->set_amount_of_message(amountOfMessage);

            //NOTE: In some cases the _id will be sent back (ex: handle_messages_to_be_sent) as the modified message _id
            // in others (ex: chat_change_stream) it will be the legitimate _id for the message, so both of these need
            // extracted each time.

            //NOTE: The messageUUIDFromIDField (the "_id" of the message extracted and stored above) is incorrect for
            // kEditedMessage, is has an 'e' appended to the end.

            auto messageIdElement = specificsDocument[chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID];
            if (messageIdElement &&
                    messageIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                responseMsg->set_message_uuid(messageIdElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, messageIdElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto modifiedMessageIdElement = specificsDocument[chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID];
            if (modifiedMessageIdElement &&
                modifiedMessageIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                editedMessageChatMessage->set_message_uuid(modifiedMessageIdElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, modifiedMessageIdElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            //this will update the text message on the client to ONLY_SKELETON, and it will need to
            // download the full info
            if (amountOfMessage == AmountOfMessage::ONLY_SKELETON) {
                break;
            }

            std::string textMessage;

            auto editedTextElement = specificsDocument[chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT];
            if (editedTextElement &&
                editedTextElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                textMessage = editedTextElement.get_string().value.to_string();
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, editedTextElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            if (amountOfMessage == AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE) {

                if(textMessage.empty()) {
                    const std::string error_string = "An edited message was stored with no text. This should never happen.";
                    const bsoncxx::document::view message_view = messageDoc;

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                            "message_document", message_view,
                            "messageUUIDFromIDField", messageUUIDFromIDField
                    );

                    editedMessageChatMessage->set_new_message("");
                } else {
                    editedMessageChatMessage->set_new_message(
                            textMessage.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE)
                    );
                }

                if (textMessage.size() <= server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE) {
                    responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(
                            true);
                }

                break;
            }

            editedMessageChatMessage->set_new_message(textMessage);

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kDeletedMessage: {
            DeletedMessageChatMessage* deletedMessageChatMessage = responseMsg->mutable_message()->mutable_message_specifics()->mutable_deleted_message();
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);

            //NOTE: in some cases the _id will be sent back (ex: handle_messages_to_be_sent) as the modified message _id
            // in others (ex: chat_change_stream) it will be the legitimate _id for the message, so both of these need
            // extracted each time

            //NOTE: The messageUUIDFromIDField (the "_id" of the message extracted and stored above) is incorrect for
            // kDeletedMessage, is has a 'd' appended to the end.

            auto messageIdElement = specificsDocument[chat_room_message_keys::message_specifics::DELETED_THIS_MESSAGE_UUID];
            if (messageIdElement &&
                messageIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                responseMsg->set_message_uuid(messageIdElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, messageIdElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::DELETED_THIS_MESSAGE_UUID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto modifiedMessageIdElement = specificsDocument[chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID];
            if (modifiedMessageIdElement &&
                    modifiedMessageIdElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                deletedMessageChatMessage->set_message_uuid(modifiedMessageIdElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, modifiedMessageIdElement,
                                specificsDocument, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            auto deletedTypeElement = specificsDocument[chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE];
            if (deletedTypeElement &&
                deletedTypeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                deletedMessageChatMessage->set_delete_type(DeleteType(deletedTypeElement.get_int32().value));
            } else { //if element does not exist or is not type int32
                logElementError(__LINE__, __FILE__, deletedTypeElement,
                                specificsDocument, bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

                return false;
            }

            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kUserActivityDetectedMessage: {

            //NOTE: the responseMsg->message_uuid() (copied from _id field above) is actually the account OID this message was sent by
            // if this is from messages_to_client, clearing it for consistency
            responseMsg->clear_message_uuid();

            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            responseMsg->mutable_message()->mutable_message_specifics()->mutable_user_activity_detected_message();
            break;
        }
        case MessageSpecifics::kChatRoomCapMessage: {
            standardChatMessageInfo->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
            responseMsg->mutable_message()->mutable_standard_message_info()->set_message_has_complete_info(true);
            responseMsg->mutable_message()->mutable_message_specifics()->mutable_chat_room_cap_message();
            break;
        }
        case MessageSpecifics::kUpdateObservedTimeMessage: //generate never stored
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage: //generate never stored
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage: //generate never stored
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage: //generate never stored
        case MessageSpecifics::kThisUserLeftChatRoomMessage: //generate never stored
        case MessageSpecifics::kHistoryClearedMessage: //client specific
        case MessageSpecifics::kNewUpdateTimeMessage: //generate never stored
        case MessageSpecifics::kLoadingMessage: //client specific
        case MessageSpecifics::MESSAGE_BODY_NOT_SET:

            std::stringstream errorString;
            errorString
                    << "An enum MessageInstruction was invalid passed of '"
                    << convertMessageBodyTypeToString(messageType)
                    << "'.\n";

            bsoncxx::document::view messageView = messageDoc;

            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), errorString.str(),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                "message_document", messageView,
                "messageUUIDFromIDField", messageUUIDFromIDField
            );

            return false; //do not send this message back to the client

    }

    return true;
}

bool extractReplyFromMessage(
        const std::string& chatRoomID,
        const bsoncxx::document::view& activeMessageInfoDocView,
        AmountOfMessage amountOfMessage,
        ActiveMessageInfo* activeMessageInfo
) {

    auto replyDocumentElement = activeMessageInfoDocView[chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT];
    if (replyDocumentElement && replyDocumentElement.type() == bsoncxx::type::k_document) { //if element exists and is type document

        activeMessageInfo->set_is_reply(true);

        if (amountOfMessage == AmountOfMessage::COMPLETE_MESSAGE_INFO) {

            const bsoncxx::document::view replyDocView = replyDocumentElement.get_document().value;

            /** It is important to only call mutable_reply_info() if a reply message is fully set. This is because the
             * client uses hasReplyInfo() to determine if a message has a reply. **/
            ReplyChatMessageInfo* replyElement = activeMessageInfo->mutable_reply_info();

            auto replyIsSentFromAccountOIDElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING];
            if (replyIsSentFromAccountOIDElement &&
            replyIsSentFromAccountOIDElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                replyElement->set_reply_is_sent_from_user_oid(
                        replyIsSentFromAccountOIDElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, replyIsSentFromAccountOIDElement,
                                replyDocView, bsoncxx::type::k_utf8,
                                chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                return false;
            }

            auto replyMessageOidElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID];
            if (replyMessageOidElement &&
            replyMessageOidElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                replyElement->set_reply_is_to_message_uuid(replyMessageOidElement.get_string().value.to_string());
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, replyMessageOidElement,
                                replyDocView, bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                return false;
            }

            ReplySpecifics::ReplyBodyCase replyBodyCase;

            auto replyMessageTypeElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE];
            if (replyMessageTypeElement &&
            replyMessageTypeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type utf8
                replyBodyCase = ReplySpecifics::ReplyBodyCase(replyMessageTypeElement.get_int32().value);
            } else { //if element does not exist or is not type utf8
                logElementError(__LINE__, __FILE__, replyMessageTypeElement,
                                replyDocView, bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                return false;
            }

            switch (replyBodyCase) {
                case ReplySpecifics::kTextReply: {

                    auto replyChatMessageTextElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT];
                    if (replyChatMessageTextElement &&
                    replyChatMessageTextElement.type() ==
                    bsoncxx::type::k_utf8) { //if element exists and is type utf8
                        replyElement->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
                                replyChatMessageTextElement.get_string().value.to_string());
                    } else { //if element does not exist or is not type utf8
                        logElementError(__LINE__, __FILE__, replyChatMessageTextElement,
                                        replyDocView, bsoncxx::type::k_utf8,
                                        chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                        return false;
                    }
                    break;
                }
                case ReplySpecifics::kPictureReply: {

                    auto replyThumbnailByteArrayElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES];
                    if (replyThumbnailByteArrayElement &&
                    replyThumbnailByteArrayElement.type() ==
                    bsoncxx::type::k_utf8) { //if element exists and is type utf8
                        replyElement->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_in_bytes(
                                replyThumbnailByteArrayElement.get_string().value.to_string());
                    } else { //if element does not exist or is not type utf8
                        logElementError(__LINE__, __FILE__, replyThumbnailByteArrayElement,
                                        replyDocView, bsoncxx::type::k_utf8,
                                        chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                        return false;
                    }

                    auto replyThumbnailByteArraySizeElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE];
                    if (replyThumbnailByteArraySizeElement &&
                    replyThumbnailByteArraySizeElement.type() ==
                    bsoncxx::type::k_int32) { //if element exists and is type int32'
                        replyElement->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_file_size(
                                replyThumbnailByteArraySizeElement.get_int32().value);
                    } else { //if element does not exist or is not type int32
                        logElementError(__LINE__, __FILE__, replyThumbnailByteArraySizeElement,
                                        replyDocView, bsoncxx::type::k_int32,
                                        chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                        return false;
                    }
                    break;
                }
                case ReplySpecifics::kMimeReply: {

                    auto replyThumbnailByteArrayElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES];
                    if (replyThumbnailByteArrayElement &&
                    replyThumbnailByteArrayElement.type() ==
                    bsoncxx::type::k_utf8) { //if element exists and is type utf8
                        replyElement->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_in_bytes(
                                replyThumbnailByteArrayElement.get_string().value.to_string());
                    } else { //if element does not exist or is not type utf8
                        logElementError(__LINE__, __FILE__, replyThumbnailByteArrayElement,
                                        replyDocView, bsoncxx::type::k_utf8,
                                        chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                        return false;
                    }

                    auto replyThumbnailByteArraySizeElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE];
                    if (replyThumbnailByteArraySizeElement &&
                    replyThumbnailByteArraySizeElement.type() ==
                    bsoncxx::type::k_int32) { //if element exists and is type int32'
                        replyElement->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_file_size(
                                replyThumbnailByteArraySizeElement.get_int32().value);
                    } else { //if element does not exist or is not type int32
                        logElementError(__LINE__, __FILE__, replyThumbnailByteArraySizeElement,
                                        replyDocView, bsoncxx::type::k_int32,
                                        chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                        return false;
                    }

                    auto replyMessageMimeTypeElement = replyDocView[chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE];
                    if (replyMessageMimeTypeElement &&
                    replyMessageMimeTypeElement.type() ==
                    bsoncxx::type::k_utf8) { //if element exists and is type utf8
                        replyElement->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_mime_type(
                                replyMessageMimeTypeElement.get_string().value.to_string());
                    } else { //if element does not exist or is not type utf8
                        logElementError(__LINE__, __FILE__, replyMessageMimeTypeElement,
                                        replyDocView, bsoncxx::type::k_utf8,
                                        chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE,
                                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

                        return false;
                    }

                    break;
                }
                case ReplySpecifics::kLocationReply:
                    replyElement->mutable_reply_specifics()->mutable_location_reply();
                    break;
                case ReplySpecifics::kInviteReply:
                    replyElement->mutable_reply_specifics()->mutable_invite_reply();
                    break;
                case ReplySpecifics::REPLY_BODY_NOT_SET: {
                    const std::string errorString = "Message set as a reply however invalid type for replying, this should never happen here it should be checked for inside client_message_to_server as well as send_message_to_chat_room.\n";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), errorString,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chatRoomID,
                            "reply_is_to_message_uuid", replyElement->reply_is_to_message_uuid(),
                            "message_type", convertReplyBodyCaseToString(replyElement->reply_specifics().reply_body_case())
                    );
                    return false;
                }
            }
        }

    } else if (replyDocumentElement && replyDocumentElement.type() == bsoncxx::type::k_null) { //if element exists and is type null
        //not a reply, continue
        activeMessageInfo->set_is_reply(false);
    } else { //if element does not exist or is not type date
        logElementError(__LINE__, __FILE__, replyDocumentElement,
                        activeMessageInfoDocView, bsoncxx::type::k_document, chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomID);

        return false;
    }

    return true;
}

bool extractMessageDeleted(
        const std::string& chatRoomId,
        const std::string& userAccountOIDStr,
        const bsoncxx::document::view& activeMessageInfoDocView,
        ActiveMessageInfo* activeMessageInfo
) {

    DeleteType deleteType;

    auto deletedTypeElement = activeMessageInfoDocView[chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY];
    if (deletedTypeElement
        && deletedTypeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        deleteType = DeleteType(deletedTypeElement.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, deletedTypeElement,
                        activeMessageInfoDocView, bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId);

        return false;
    }

    //NOTE: This should NEVER be set for chat_change_stream, it only checks newly inserted messages, and so
    // they should never already be deleted
    if (deleteType == DeleteType::DELETE_FOR_ALL_USERS) {
        activeMessageInfo->set_is_deleted(true);
        return true;
    }
    else if (deleteType == DeleteType::DELETE_FOR_SINGLE_USER) {

        //this should never happen, the chat change stream will handle the error
        if(userAccountOIDStr == chat_stream_container::CHAT_CHANGE_STREAM_PASSED_STRING_TO_CONVERT) { //if requested from begin_chat_change_stream
            activeMessageInfo->set_is_deleted(true);
            return true;
        } else if(userAccountOIDStr == general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT) {
            return true;
        }

        bsoncxx::array::view message_deleted_for_accounts;

        auto messageDeletedForAccountsElement = activeMessageInfoDocView[chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY];
        if (messageDeletedForAccountsElement
            && messageDeletedForAccountsElement.type() == bsoncxx::type::k_array) { //if element exists and is type int32
            message_deleted_for_accounts = messageDeletedForAccountsElement.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(
                    __LINE__, __FILE__,
                    messageDeletedForAccountsElement, activeMessageInfoDocView,
                    bsoncxx::type::k_array, chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId
            );

            return false;
        }

        for (const auto& ele : message_deleted_for_accounts) {
            if (ele.type() == bsoncxx::type::k_utf8) {
                if (ele.get_string().value.to_string() == userAccountOIDStr) {
                    activeMessageInfo->set_is_deleted(true);
                    break;
                }
            } else {
                logElementError(
                        __LINE__, __FILE__,
                        messageDeletedForAccountsElement, activeMessageInfoDocView,
                        bsoncxx::type::k_utf8, chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY,
                        database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chatRoomId
                );

                //NOTE: can continue here
            }
        }
    }

    return true;
}
