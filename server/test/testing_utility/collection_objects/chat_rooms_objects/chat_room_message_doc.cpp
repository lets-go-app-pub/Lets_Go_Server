//
// Created by jeremiah on 6/1/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <chat_room_message_keys.h>

#include "chat_rooms_objects.h"
#include "chat_stream_container.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void extractActiveMessageToDocument(const ChatRoomMessageDoc::ActiveMessageInfo& active_message_info,
                                    bsoncxx::builder::stream::document& document_result) {
    bsoncxx::builder::basic::array deleted_accounts_arr;

    for (const std::string& deleted_account : active_message_info.chat_room_message_deleted_accounts) {
        deleted_accounts_arr.append(deleted_account);
    }

    document_result
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY << active_message_info.chat_room_message_deleted_type_key
            << chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY << deleted_accounts_arr;

    if (active_message_info.reply_document == nullptr) {
        document_result
                << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << bsoncxx::types::b_null{};
    } else {

        bsoncxx::builder::stream::document reply_message_doc;

        reply_message_doc
                << chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING << active_message_info.reply_document->sent_from_account_oid_string
                << chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID << active_message_info.reply_document->message_uuid
                << chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE << active_message_info.reply_document->reply_body_case;

        switch (active_message_info.reply_document->reply_body_case) {
            case ReplySpecifics::kTextReply: {
                auto reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::TextReplyDocument>(
                        active_message_info.reply_document);

                reply_message_doc
                        << chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT << reply_to_message->chat_message_text;
                break;
            }
            case ReplySpecifics::kPictureReply: {
                auto reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::PictureReplyDocument>(
                        active_message_info.reply_document);

                reply_message_doc
                        << chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES << reply_to_message->picture_thumbnail_in_bytes
                        << chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE << reply_to_message->picture_thumbnail_size;
                break;
            }
            case ReplySpecifics::kMimeReply: {
                auto reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::MimeTypeReplyDocument>(
                        active_message_info.reply_document);

                reply_message_doc
                        << chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES << reply_to_message->mime_type_thumbnail_in_bytes
                        << chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE << reply_to_message->mime_type_thumbnail_size
                        << chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE << reply_to_message->reply_mime_type;
                break;
            }
            case ReplySpecifics::kLocationReply:
            case ReplySpecifics::kInviteReply:
            case ReplySpecifics::REPLY_BODY_NOT_SET:
                break;
        }

        document_result
                << chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT << reply_message_doc;
    }
}

//converts this UserPictureDoc object to a document and saves it to the passed builder
void ChatRoomMessageDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::stream::document specifics_doc;

    switch (message_type) {
        case MessageSpecifics::kTextMessage: {
            auto message = std::static_pointer_cast<ChatTextMessageSpecifics>(message_specifics_document);

            bsoncxx::builder::stream::document active_message_doc;

            extractActiveMessageToDocument(
                    message->active_message_info,
                    active_message_doc
            );

            specifics_doc
                    << chat_room_message_keys::message_specifics::TEXT_MESSAGE << message->text_message
                    << chat_room_message_keys::message_specifics::TEXT_IS_EDITED << message->text_is_edited
                    << chat_room_message_keys::message_specifics::TEXT_EDITED_TIME << message->text_edited_time
                    << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << active_message_doc;

            break;
        }
        case MessageSpecifics::kPictureMessage: {
            auto message = std::static_pointer_cast<PictureMessageSpecifics>(message_specifics_document);

            bsoncxx::builder::stream::document active_message_doc;

            extractActiveMessageToDocument(
                    message->active_message_info,
                    active_message_doc
            );

            specifics_doc
                    << chat_room_message_keys::message_specifics::PICTURE_OID << message->picture_oid
                    << chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH << message->picture_image_width
                    << chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT << message->picture_image_height
                    << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << active_message_doc;

            break;
        }
        case MessageSpecifics::kLocationMessage: {
            auto message = std::static_pointer_cast<LocationMessageSpecifics>(message_specifics_document);

            bsoncxx::builder::stream::document active_message_doc;

            extractActiveMessageToDocument(
                    message->active_message_info,
                    active_message_doc
            );

            specifics_doc
                    << chat_room_message_keys::message_specifics::LOCATION_LONGITUDE << message->location_longitude
                    << chat_room_message_keys::message_specifics::LOCATION_LATITUDE << message->location_latitude
                    << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << active_message_doc;

            break;
        }
        case MessageSpecifics::kMimeTypeMessage: {
            auto message = std::static_pointer_cast<MimeTypeMessageSpecifics>(message_specifics_document);

            bsoncxx::builder::stream::document active_message_doc;

            extractActiveMessageToDocument(
                    message->active_message_info,
                    active_message_doc
            );

            specifics_doc
                    << chat_room_message_keys::message_specifics::MIME_TYPE_URL << message->mime_type_url
                    << chat_room_message_keys::message_specifics::MIME_TYPE_TYPE << message->mime_type_type
                    << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH << message->mime_type_image_width
                    << chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT << message->mime_type_image_height
                    << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << active_message_doc;

            break;
        }
        case MessageSpecifics::kInviteMessage: {
            auto message = std::static_pointer_cast<InviteMessageSpecifics>(message_specifics_document);

            bsoncxx::builder::stream::document active_message_doc;

            extractActiveMessageToDocument(
                    message->active_message_info,
                    active_message_doc
            );

            specifics_doc
                    << chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID << message->invited_user_account_oid
                    << chat_room_message_keys::message_specifics::INVITED_USER_NAME << message->invited_user_name
                    << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID << message->invited_chat_room_id
                    << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME << message->invited_chat_room_name
                    << chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD << message->invited_chat_room_password
                    << chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT << active_message_doc;

            break;
        }
        case MessageSpecifics::kEditedMessage: {
            auto message = std::static_pointer_cast<EditedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID << message->edited_this_message_uuid
                    << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID << message->edited_modified_message_uuid
                    << chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT << message->edited_new_message_text
                    << chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT << message->edited_previous_message_text
                    << chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME << message->edited_modified_message_created_time;

            break;
        }
        case MessageSpecifics::kDeletedMessage: {
            auto message = std::static_pointer_cast<DeletedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::DELETED_THIS_MESSAGE_UUID << message->deleted_this_message_uuid
                    << chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID << message->deleted_modified_message_uuid
                    << chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE << message->deleted_deleted_type
                    << chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_CREATED_TIME << message->deleted_modified_message_created_time;

            break;
        }
        case MessageSpecifics::kUserKickedMessage: {
            auto message = std::static_pointer_cast<UserKickedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::USER_KICKED_KICKED_OID << message->user_kicked_kicked_oid;

            break;
        }
        case MessageSpecifics::kUserBannedMessage: {
            auto message = std::static_pointer_cast<UserBannedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::USER_BANNED_BANNED_OID << message->user_banned_banned_oid;

            break;
        }
        case MessageSpecifics::kDifferentUserJoinedMessage: {
            auto message = std::static_pointer_cast<DifferentUserJoinedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE << message->user_joined_account_state;

            if(!message->user_joined_from_event.empty()) {
                specifics_doc
                        << chat_room_message_keys::message_specifics::USER_JOINED_FROM_EVENT << message->user_joined_from_event;
            }
            break;
        }
        case MessageSpecifics::kDifferentUserLeftMessage: {
            auto message = std::static_pointer_cast<DifferentUserLeftMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::USER_NEW_ACCOUNT_ADMIN_OID << message->user_new_account_admin_oid;

            break;
        }
        case MessageSpecifics::kChatRoomNameUpdatedMessage: {
            auto message = std::static_pointer_cast<ChatRoomNameUpdatedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::NAME_NEW_NAME << message->name_new_name;

            break;
        }
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage: {
            auto message = std::static_pointer_cast<ChatRoomPasswordUpdatedMessageSpecifics>(
                    message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::PASSWORD_NEW_PASSWORD << message->password_new_password;

            break;
        }
        case MessageSpecifics::kNewAdminPromotedMessage: {
            auto message = std::static_pointer_cast<NewAdminPromotedMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::NEW_ADMIN_ADMIN_ACCOUNT_OID << message->new_admin_admin_account_oid;

            break;
        }
        case MessageSpecifics::kNewPinnedLocationMessage: {
            auto message = std::static_pointer_cast<NewPinnedLocationMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::PINNED_LOCATION_LONGITUDE << message->pinned_location_longitude
                    << chat_room_message_keys::message_specifics::PINNED_LOCATION_LATITUDE << message->pinned_location_latitude;

            break;
        }
        case MessageSpecifics::kMatchCanceledMessage: {
            auto message = std::static_pointer_cast<MatchCanceledMessageSpecifics>(message_specifics_document);

            specifics_doc
                    << chat_room_message_keys::message_specifics::MATCH_CANCELED_ACCOUNT_OID << message->match_canceled_account_oid;

            break;
        }

        //empty document types (although they each have a type defined)
        case MessageSpecifics::kUpdateObservedTimeMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
        case MessageSpecifics::kThisUserLeftChatRoomMessage:
        case MessageSpecifics::kUserActivityDetectedMessage:
        case MessageSpecifics::kChatRoomCapMessage:
        case MessageSpecifics::kHistoryClearedMessage:
        case MessageSpecifics::kLoadingMessage:
        case MessageSpecifics::kNewUpdateTimeMessage:
        case MessageSpecifics::MESSAGE_BODY_NOT_SET:
            break;
    }

    document_result
            << "_id" << id
            << chat_room_shared_keys::TIMESTAMP_CREATED << shared_properties.timestamp
            << chat_room_message_keys::MESSAGE_SENT_BY << message_sent_by
            << chat_room_message_keys::MESSAGE_TYPE << message_type
            << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << specifics_doc;

}

void ChatRoomMessageDoc::extractDeleteFromMessage(
        const std::string& requesting_user_account_oid,
        const ActiveMessageInfo& active_message_info,
        const std::function<void()>& set_is_deleted_to_true
        ) {
    if(active_message_info.chat_room_message_deleted_type_key == DeleteType::DELETE_FOR_ALL_USERS) {
        set_is_deleted_to_true();
    } else if(active_message_info.chat_room_message_deleted_type_key == DeleteType::DELETE_FOR_ALL_USERS) {
        if(requesting_user_account_oid == chat_stream_container::CHAT_CHANGE_STREAM_PASSED_STRING_TO_CONVERT) { //if requested from begin_chat_change_stream
            set_is_deleted_to_true();
            return;
        } else if(requesting_user_account_oid == general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT) {
            return;
        }

        for(const auto& acct : active_message_info.chat_room_message_deleted_accounts) {
            if(acct == requesting_user_account_oid) {
                set_is_deleted_to_true();
                break;
            }
        }
    }
}

void ChatRoomMessageDoc::extractReplyFromMessage(
        const std::shared_ptr<ActiveMessageInfo::ReplyDocument>& reply_document,
        const AmountOfMessage amount_of_message,
        const std::function<void(bool /*is_reply*/)>& set_is_reply,
        const std::function<ReplyChatMessageInfo*()>& get_mutable_reply_info
        ) {

    if(reply_document) {

        set_is_reply(true);

        if(amount_of_message == AmountOfMessage::COMPLETE_MESSAGE_INFO) {

            auto* mutable_reply_info = get_mutable_reply_info();

            mutable_reply_info->set_reply_is_sent_from_user_oid(
                    reply_document->sent_from_account_oid_string
            );

            mutable_reply_info->set_reply_is_to_message_uuid(
                    reply_document->message_uuid
            );

            switch(reply_document->reply_body_case) {
                case ReplySpecifics::kTextReply: {
                    const auto text_reply = std::static_pointer_cast<ActiveMessageInfo::TextReplyDocument>(reply_document);
                    mutable_reply_info->mutable_reply_specifics()->mutable_text_reply()->set_message_text(
                            text_reply->chat_message_text
                    );
                    break;
                }
                case ReplySpecifics::kPictureReply: {
                    const auto picture_reply = std::static_pointer_cast<ActiveMessageInfo::PictureReplyDocument>(reply_document);
                    mutable_reply_info->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_in_bytes(
                            picture_reply->picture_thumbnail_in_bytes
                    );
                    mutable_reply_info->mutable_reply_specifics()->mutable_picture_reply()->set_thumbnail_file_size(
                            picture_reply->picture_thumbnail_size
                    );
                    break;
                }
                case ReplySpecifics::kLocationReply: {
                    mutable_reply_info->mutable_reply_specifics()->mutable_location_reply();
                    break;
                }
                case ReplySpecifics::kMimeReply: {
                    const auto mime_type_reply = std::static_pointer_cast<ActiveMessageInfo::MimeTypeReplyDocument>(reply_document);
                    mutable_reply_info->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_in_bytes(
                            mime_type_reply->mime_type_thumbnail_in_bytes
                    );
                    mutable_reply_info->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_file_size(
                            mime_type_reply->mime_type_thumbnail_size
                    );
                    mutable_reply_info->mutable_reply_specifics()->mutable_mime_reply()->set_thumbnail_mime_type(
                            mime_type_reply->reply_mime_type
                    );
                    break;
                }
                case ReplySpecifics::kInviteReply: {
                    mutable_reply_info->mutable_reply_specifics()->mutable_location_reply();
                    break;
                }
                case ReplySpecifics::REPLY_BODY_NOT_SET: {
                    break;
                }
            }
        }

    } else {
        set_is_reply(false);
    }
}

ChatMessageToClient ChatRoomMessageDoc::convertToChatMessageToClient(
        const std::string& requesting_user_account_oid,
        AmountOfMessage amount_of_message,
        const bool only_store_message,
        const bool do_not_update_user_state
) const {
    ChatMessageToClient return_value;

    return_value.set_message_uuid(id);
    return_value.set_sent_by_account_id(message_sent_by.to_string());
    return_value.set_timestamp_stored(shared_properties.timestamp.value.count());

    return_value.set_only_store_message(only_store_message); //this will tell the client if this message is for a newly added chat room

    StandardChatMessageInfo* standard_chat_message_info = return_value.mutable_message()->mutable_standard_message_info();
    standard_chat_message_info->set_chat_room_id_message_sent_from(chat_room_id);
    standard_chat_message_info->set_do_not_update_user_state(do_not_update_user_state);

    switch(message_type) {
        case MessageSpecifics::kTextMessage: {
            TextChatMessage* text_message_response = return_value.mutable_message()->mutable_message_specifics()->mutable_text_message();
            const auto text_message = std::static_pointer_cast<ChatRoomMessageDoc::ChatTextMessageSpecifics>(message_specifics_document);

            extractDeleteFromMessage(
                    requesting_user_account_oid,
                    text_message->active_message_info,
                    [&]{
                        text_message_response->mutable_active_message_info()->set_is_deleted(true);
                    }
            );

            if (requesting_user_account_oid != general_values::GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT
                && text_message_response->active_message_info().is_deleted()
            ) {
                standard_chat_message_info->set_amount_of_message(AmountOfMessage::ONLY_SKELETON);
                standard_chat_message_info->set_message_has_complete_info(true);
                break;
            }

            standard_chat_message_info->set_amount_of_message(amount_of_message);

            extractReplyFromMessage(
                    text_message->active_message_info.reply_document,
                    amount_of_message,
                    [&](bool is_reply){
                        text_message_response->mutable_active_message_info()->set_is_reply(is_reply);
                    },
                    [&] {
                        return text_message_response->mutable_active_message_info()->mutable_reply_info();
                    }
            );

            if(amount_of_message == ONLY_SKELETON) {
                break;
            }

            text_message_response->set_is_edited(text_message->text_is_edited);
            text_message_response->set_edited_time(text_message->text_edited_time.value.count());

            if (amount_of_message == AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE) {
                text_message_response->set_message_text(text_message->text_message.substr(0, server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE));

                if (text_message->text_message.size() <= server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE
                    && !text_message_response->active_message_info().is_reply()
                        ) {
                    standard_chat_message_info->set_message_has_complete_info(true);
                }

                break;
            }

            text_message_response->set_message_text(text_message->text_message);

            standard_chat_message_info->set_message_has_complete_info(true);
            break;
        }
        case MessageSpecifics::kPictureMessage:
            break;
        case MessageSpecifics::kLocationMessage:
            break;
        case MessageSpecifics::kMimeTypeMessage:
            break;
        case MessageSpecifics::kInviteMessage:
            break;
        case MessageSpecifics::kUserKickedMessage:
            break;
        case MessageSpecifics::kUserBannedMessage:
            break;
        case MessageSpecifics::kDifferentUserJoinedMessage:
            break;
        case MessageSpecifics::kDifferentUserLeftMessage:
            break;
        case MessageSpecifics::kChatRoomNameUpdatedMessage:
            break;
        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
            break;
        case MessageSpecifics::kNewAdminPromotedMessage:
            break;
        case MessageSpecifics::kNewPinnedLocationMessage:
            break;
        case MessageSpecifics::kMatchCanceledMessage:
            break;
        case MessageSpecifics::kEditedMessage:
            break;
        case MessageSpecifics::kDeletedMessage:
            break;
        case MessageSpecifics::kUserActivityDetectedMessage:
            break;
        case MessageSpecifics::kChatRoomCapMessage:
            break;

        //None of these are stored
        case MessageSpecifics::kUpdateObservedTimeMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
        case MessageSpecifics::kThisUserLeftChatRoomMessage:
        case MessageSpecifics::kHistoryClearedMessage:
        case MessageSpecifics::kNewUpdateTimeMessage:
        case MessageSpecifics::kLoadingMessage:
        case MessageSpecifics::MESSAGE_BODY_NOT_SET:
            break;
    }

    return return_value;
}

bool ChatRoomMessageDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (isInvalidUUID(id)) id = generateUUID();

        mongocxx::options::update updateOptions;
        updateOptions.upsert(true);

        chat_room_collection.update_one(
                document{}
                        << "_id" << id
                        << finalize,
                document{}
                        << "$set" << insertDocument.view()
                        << finalize,
                updateOptions);

    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in ChatRoomMessageDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ChatRoomMessageDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ChatRoomMessageDoc::getFromCollection(
        const std::string& uuid,
        const std::string& _chat_room_id
        ) {
    chat_room_id = _chat_room_id;
    id = uuid;
    return getFromCollection(bsoncxx::oid{});
}

bool ChatRoomMessageDoc::getFromCollection(const bsoncxx::oid&) {

    assert(!chat_room_id.empty());
    assert(!isInvalidUUID(id));

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_room_collection = chat_rooms_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = chat_room_collection.find_one(document{}
                                                                << "_id" << id
                                                                << finalize);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in ChatRoomMessageDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ChatRoomMessageDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal, chat_room_id);
}

bool ChatRoomMessageDoc::saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val,
                                            const std::string& updated_chat_room_id) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view, updated_chat_room_id);
    } else {
        //clear this if failed to set
        id = "";
        chat_room_id = "";

        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function ChatRoomMessageDoc::saveInfoToDocument\n";
        return false;
    }
}

ChatRoomMessageDoc::ActiveMessageInfo
saveActiveMessageInfoToClass(const bsoncxx::document::view& active_message_info_doc) {

    auto delete_type = DeleteType(
            active_message_info_doc[chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY].get_int32().value
    );

    bsoncxx::array::view chat_room_message_deleted_accounts_arr =
            active_message_info_doc[chat_room_message_keys::message_specifics::active_info::CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY].get_array().value;

    std::vector<std::string> chat_room_message_deleted_accounts;

    for (const auto& deleted_account : chat_room_message_deleted_accounts_arr) {
        chat_room_message_deleted_accounts.emplace_back(deleted_account.get_string().value.to_string());
    }

    auto reply_document_element = active_message_info_doc[chat_room_message_keys::message_specifics::active_info::REPLY_DOCUMENT];

    std::shared_ptr<ChatRoomMessageDoc::ActiveMessageInfo::ReplyDocument> reply_document = nullptr;

    if (reply_document_element.type() != bsoncxx::type::k_null) {
        bsoncxx::document::view reply_document_doc = reply_document_element.get_document().value;

        std::string sent_from_account_oid =
                reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::SENT_FROM_ACCOUNT_OID_STRING].get_string().value.to_string();

        std::string message_uuid =
                reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::MESSAGE_UUID].get_string().value.to_string();

        ReplySpecifics::ReplyBodyCase reply_case =
                ReplySpecifics::ReplyBodyCase(
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::REPLY_BODY_CASE].get_int32().value
                );

        switch (reply_case) {
            case ReplySpecifics::kTextReply: {
                reply_document = std::make_shared<ChatRoomMessageDoc::ActiveMessageInfo::TextReplyDocument>(
                        sent_from_account_oid,
                        message_uuid,
                        reply_case,
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::CHAT_MESSAGE_TEXT].get_string().value.to_string()
                );
                break;
            }
            case ReplySpecifics::kPictureReply: {
                reply_document = std::make_shared<ChatRoomMessageDoc::ActiveMessageInfo::PictureReplyDocument>(
                        sent_from_account_oid,
                        message_uuid,
                        reply_case,
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_IN_BYTES].get_string().value.to_string(),
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::PICTURE_THUMBNAIL_SIZE].get_int32().value
                );
                break;
            }
            case ReplySpecifics::kLocationReply: {
                reply_document = std::make_shared<ChatRoomMessageDoc::ActiveMessageInfo::LocationReplyDocument>(
                        sent_from_account_oid,
                        message_uuid,
                        reply_case
                );
                break;
            }
            case ReplySpecifics::kMimeReply: {
                reply_document = std::make_shared<ChatRoomMessageDoc::ActiveMessageInfo::MimeTypeReplyDocument>(
                        sent_from_account_oid,
                        message_uuid,
                        reply_case,
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_IN_BYTES].get_string().value.to_string(),
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::MIME_TYPE_THUMBNAIL_SIZE].get_int32().value,
                        reply_document_doc[chat_room_message_keys::message_specifics::active_info::reply::REPLY_MIME_TYPE].get_string().value.to_string()
                );
                break;
            }
            case ReplySpecifics::kInviteReply: {
                reply_document = std::make_shared<ChatRoomMessageDoc::ActiveMessageInfo::InvitedReplyDocument>(
                        sent_from_account_oid,
                        message_uuid,
                        reply_case
                );
                break;
            }
            case ReplySpecifics::REPLY_BODY_NOT_SET: {
                reply_document = std::make_shared<ChatRoomMessageDoc::ActiveMessageInfo::ReplyDocument>(
                        sent_from_account_oid,
                        message_uuid,
                        reply_case
                );
                break;
            }
        }

    }

    return {
        delete_type,
        chat_room_message_deleted_accounts,
        reply_document
    };
}

bool ChatRoomMessageDoc::convertDocumentToClass(
        const bsoncxx::v_noabi::document::view& user_account_document,
        const std::string& updated_chat_room_id
) {

    chat_room_id = updated_chat_room_id;
    try {
        id = extractFromBsoncxx_k_utf8(
                user_account_document,
                "_id"
        );

        shared_properties =
                ChatRoomShared(
                        extractFromBsoncxx_k_date(
                                user_account_document,
                                chat_room_shared_keys::TIMESTAMP_CREATED
                        )
                );

        message_sent_by = extractFromBsoncxx_k_oid(
                user_account_document,
                chat_room_message_keys::MESSAGE_SENT_BY
        );


        message_type = MessageSpecifics::MessageBodyCase(
                extractFromBsoncxx_k_int32(
                        user_account_document,
                        chat_room_message_keys::MESSAGE_TYPE
                )
        );

        bsoncxx::document::view message_specifics_doc = extractFromBsoncxx_k_document(
                user_account_document,
                chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT
        );

        switch (message_type) {
            case MessageSpecifics::kTextMessage: {
                bsoncxx::document::view active_message_info_doc =
                        message_specifics_doc[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT].get_document().value;

                message_specifics_document = std::make_shared<ChatTextMessageSpecifics>(
                        saveActiveMessageInfoToClass(active_message_info_doc),
                        message_specifics_doc[chat_room_message_keys::message_specifics::TEXT_MESSAGE].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::TEXT_IS_EDITED].get_bool().value,
                        message_specifics_doc[chat_room_message_keys::message_specifics::TEXT_EDITED_TIME].get_date()
                );

                break;
            }
            case MessageSpecifics::kPictureMessage: {
                bsoncxx::document::view active_message_info_doc =
                        message_specifics_doc[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT].get_document().value;

                message_specifics_document = std::make_shared<PictureMessageSpecifics>(
                        saveActiveMessageInfoToClass(active_message_info_doc),
                        message_specifics_doc[chat_room_message_keys::message_specifics::PICTURE_OID].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH].get_int32().value,
                        message_specifics_doc[chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT].get_int32().value
                );
                break;
            }
            case MessageSpecifics::kLocationMessage: {
                bsoncxx::document::view active_message_info_doc =
                        message_specifics_doc[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT].get_document().value;

                message_specifics_document = std::make_shared<LocationMessageSpecifics>(
                        saveActiveMessageInfoToClass(active_message_info_doc),
                        message_specifics_doc[chat_room_message_keys::message_specifics::LOCATION_LONGITUDE].get_double().value,
                        message_specifics_doc[chat_room_message_keys::message_specifics::LOCATION_LATITUDE].get_double().value
                );
                break;
            }
            case MessageSpecifics::kMimeTypeMessage: {
                bsoncxx::document::view active_message_info_doc =
                        message_specifics_doc[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT].get_document().value;

                message_specifics_document = std::make_shared<MimeTypeMessageSpecifics>(
                        saveActiveMessageInfoToClass(active_message_info_doc),
                        message_specifics_doc[chat_room_message_keys::message_specifics::MIME_TYPE_URL].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::MIME_TYPE_TYPE].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_WIDTH].get_int32().value,
                        message_specifics_doc[chat_room_message_keys::message_specifics::MIME_TYPE_IMAGE_HEIGHT].get_int32().value
                );
                break;
            }
            case MessageSpecifics::kInviteMessage: {
                bsoncxx::document::view active_message_info_doc =
                        message_specifics_doc[chat_room_message_keys::message_specifics::ACTIVE_MESSAGE_INFO_DOCUMENT].get_document().value;

                message_specifics_document = std::make_shared<InviteMessageSpecifics>(
                        saveActiveMessageInfoToClass(active_message_info_doc),
                        message_specifics_doc[chat_room_message_keys::message_specifics::INVITED_USER_ACCOUNT_OID].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::INVITED_USER_NAME].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_ID].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_NAME].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::INVITED_CHAT_ROOM_PASSWORD].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kEditedMessage: {
                message_specifics_document = std::make_shared<EditedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::EDITED_THIS_MESSAGE_UUID].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_UUID].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::EDITED_NEW_MESSAGE_TEXT].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::EDITED_PREVIOUS_MESSAGE_TEXT].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::EDITED_MODIFIED_MESSAGE_CREATED_TIME].get_date()
                );
                break;
            }
            case MessageSpecifics::kDeletedMessage: {
                message_specifics_document = std::make_shared<DeletedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::DELETED_THIS_MESSAGE_UUID].get_string().value.to_string(),
                        message_specifics_doc[chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_UUID].get_string().value.to_string(),
                        DeleteType(
                                message_specifics_doc[chat_room_message_keys::message_specifics::DELETED_DELETED_TYPE].get_int32().value),
                        message_specifics_doc[chat_room_message_keys::message_specifics::DELETED_MODIFIED_MESSAGE_CREATED_TIME].get_date()
                );
                break;
            }
            case MessageSpecifics::kUserKickedMessage: {
                message_specifics_document = std::make_shared<UserKickedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::USER_KICKED_KICKED_OID].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kUserBannedMessage: {
                message_specifics_document = std::make_shared<UserBannedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::USER_BANNED_BANNED_OID].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kDifferentUserJoinedMessage: {
                auto user_joined_from_event_element = message_specifics_doc[chat_room_message_keys::message_specifics::USER_JOINED_FROM_EVENT];

                message_specifics_document = std::make_shared<DifferentUserJoinedMessageSpecifics>(
                        AccountStateInChatRoom(
                                message_specifics_doc[chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE].get_int32().value
                        ),
                        user_joined_from_event_element ? user_joined_from_event_element.get_string().value.to_string() : ""
                );
                break;
            }
            case MessageSpecifics::kDifferentUserLeftMessage: {
                message_specifics_document = std::make_shared<DifferentUserLeftMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::USER_NEW_ACCOUNT_ADMIN_OID].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kUpdateObservedTimeMessage: {
                message_specifics_document = std::make_shared<UpdateObservedTimeMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kThisUserJoinedChatRoomStartMessage: {
                message_specifics_document = std::make_shared<ThisUserJoinedChatRoomStartMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage: {
                message_specifics_document = std::make_shared<ThisUserJoinedChatRoomMemberMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage: {
                message_specifics_document = std::make_shared<ThisUserJoinedChatRoomFinishedMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kThisUserLeftChatRoomMessage: {
                message_specifics_document = std::make_shared<ThisLeftJoinedChatRoomMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kUserActivityDetectedMessage: {
                message_specifics_document = std::make_shared<UserActivityDetectedMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kChatRoomNameUpdatedMessage: {
                message_specifics_document = std::make_shared<ChatRoomNameUpdatedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::NAME_NEW_NAME].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kChatRoomPasswordUpdatedMessage: {
                message_specifics_document = std::make_shared<ChatRoomPasswordUpdatedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::PASSWORD_NEW_PASSWORD].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kNewAdminPromotedMessage: {
                message_specifics_document = std::make_shared<NewAdminPromotedMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::NEW_ADMIN_ADMIN_ACCOUNT_OID].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kNewPinnedLocationMessage: {
                message_specifics_document = std::make_shared<NewPinnedLocationMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::PINNED_LOCATION_LONGITUDE].get_double().value,
                        message_specifics_doc[chat_room_message_keys::message_specifics::PINNED_LOCATION_LATITUDE].get_double().value
                );
                break;
            }
            case MessageSpecifics::kChatRoomCapMessage: {
                message_specifics_document = std::make_shared<ChatRoomCapMessageMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kMatchCanceledMessage: {
                message_specifics_document = std::make_shared<MatchCanceledMessageSpecifics>(
                        message_specifics_doc[chat_room_message_keys::message_specifics::MATCH_CANCELED_ACCOUNT_OID].get_string().value.to_string()
                );
                break;
            }
            case MessageSpecifics::kNewUpdateTimeMessage: {
                message_specifics_document = std::make_shared<NewUpdateTimeMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kHistoryClearedMessage: {
                message_specifics_document = std::make_shared<HistoryClearedMessageMessageSpecifics>();
                break;
            }
            case MessageSpecifics::kLoadingMessage: {
                message_specifics_document = std::make_shared<LoadingMessageMessageSpecifics>();
                break;
            }
            case MessageSpecifics::MESSAGE_BODY_NOT_SET: {
                message_specifics_document = std::make_shared<MessageSpecificsDoc>();
                break;
            }
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in ChatRoomMessageDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool ChatRoomMessageDoc::operator==(const ChatRoomMessageDoc& other) const {
    bool return_value = true;

    checkForEquality(
            chat_room_id,
            other.chat_room_id,
            "CHAT_ROOM_ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            id,
            other.id,
            "ID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            message_sent_by.to_string(),
            other.message_sent_by.to_string(),
            "MESSAGE_SENT_BY",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            message_type,
            other.message_type,
            "MESSAGE_SENT_BY",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            shared_properties.timestamp,
            other.shared_properties.timestamp,
            "SHARED_PROPERTIES.shared_properties",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (message_specifics_document == nullptr
        || other.message_specifics_document == nullptr) {
        if (message_type == other.message_type) {

            ActiveMessageInfo* current_active_message_doc = nullptr;
            ActiveMessageInfo* other_active_message_doc = nullptr;

            switch (message_type) {
                case MessageSpecifics::kTextMessage: {
                    auto current_user_message = std::static_pointer_cast<ChatTextMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<ChatTextMessageSpecifics>(
                            other.message_specifics_document);

                    current_active_message_doc = &current_user_message->active_message_info;
                    other_active_message_doc = &other_user_message->active_message_info;

                    checkForEquality(
                            current_user_message->text_message,
                            other_user_message->text_message,
                            "MESSAGE_SPECIFICS.text_message",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->text_is_edited,
                            other_user_message->text_is_edited,
                            "MESSAGE_SPECIFICS.text_is_edited",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->text_edited_time.value.count(),
                            other_user_message->text_edited_time.value.count(),
                            "MESSAGE_SPECIFICS.text_edited_time",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kPictureMessage: {
                    auto current_user_message = std::static_pointer_cast<PictureMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<PictureMessageSpecifics>(
                            other.message_specifics_document);

                    current_active_message_doc = &current_user_message->active_message_info;
                    other_active_message_doc = &other_user_message->active_message_info;

                    checkForEquality(
                            current_user_message->picture_oid,
                            other_user_message->picture_oid,
                            "MESSAGE_SPECIFICS.picture_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->picture_image_width,
                            other_user_message->picture_image_width,
                            "MESSAGE_SPECIFICS.picture_image_width",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->picture_image_height,
                            other_user_message->picture_image_height,
                            "MESSAGE_SPECIFICS.picture_image_height",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kLocationMessage: {
                    auto current_user_message = std::static_pointer_cast<LocationMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<LocationMessageSpecifics>(
                            other.message_specifics_document);

                    current_active_message_doc = &current_user_message->active_message_info;
                    other_active_message_doc = &other_user_message->active_message_info;

                    checkForEquality(
                            current_user_message->location_longitude,
                            other_user_message->location_longitude,
                            "MESSAGE_SPECIFICS.location_longitude",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->location_latitude,
                            other_user_message->location_latitude,
                            "MESSAGE_SPECIFICS.location_latitude",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kMimeTypeMessage: {
                    auto current_user_message = std::static_pointer_cast<MimeTypeMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<MimeTypeMessageSpecifics>(
                            other.message_specifics_document);

                    current_active_message_doc = &current_user_message->active_message_info;
                    other_active_message_doc = &other_user_message->active_message_info;

                    checkForEquality(
                            current_user_message->mime_type_url,
                            other_user_message->mime_type_url,
                            "MESSAGE_SPECIFICS.mime_type_url",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->mime_type_type,
                            other_user_message->mime_type_type,
                            "MESSAGE_SPECIFICS.mime_type_type",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->mime_type_image_width,
                            other_user_message->mime_type_image_width,
                            "MESSAGE_SPECIFICS.mime_type_image_width",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->mime_type_image_height,
                            other_user_message->mime_type_image_height,
                            "MESSAGE_SPECIFICS.mime_type_image_height",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kInviteMessage: {
                    auto current_user_message = std::static_pointer_cast<InviteMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<InviteMessageSpecifics>(
                            other.message_specifics_document);

                    current_active_message_doc = &current_user_message->active_message_info;
                    other_active_message_doc = &other_user_message->active_message_info;

                    checkForEquality(
                            current_user_message->invited_user_account_oid,
                            other_user_message->invited_user_account_oid,
                            "MESSAGE_SPECIFICS.invited_user_account_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->invited_user_name,
                            other_user_message->invited_user_name,
                            "MESSAGE_SPECIFICS.invited_user_name",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->invited_chat_room_id,
                            other_user_message->invited_chat_room_id,
                            "MESSAGE_SPECIFICS.invited_chat_room_id",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->invited_chat_room_name,
                            other_user_message->invited_chat_room_name,
                            "MESSAGE_SPECIFICS.invited_chat_room_name",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->invited_chat_room_password,
                            other_user_message->invited_chat_room_password,
                            "MESSAGE_SPECIFICS.invited_chat_room_password",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                    break;
                }
                case MessageSpecifics::kEditedMessage: {
                    auto current_user_message = std::static_pointer_cast<EditedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<EditedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->edited_this_message_uuid,
                            other_user_message->edited_this_message_uuid,
                            "MESSAGE_SPECIFICS.edited_this_message_uuid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->edited_modified_message_uuid,
                            other_user_message->edited_modified_message_uuid,
                            "MESSAGE_SPECIFICS.edited_modified_message_uuid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->edited_new_message_text,
                            other_user_message->edited_new_message_text,
                            "MESSAGE_SPECIFICS.edited_new_message_text",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->edited_previous_message_text,
                            other_user_message->edited_previous_message_text,
                            "MESSAGE_SPECIFICS.edited_previous_message_text",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->edited_modified_message_created_time.value.count(),
                            other_user_message->edited_modified_message_created_time.value.count(),
                            "MESSAGE_SPECIFICS.edited_modified_message_created_time",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                    break;
                }
                case MessageSpecifics::kDeletedMessage: {
                    auto current_user_message = std::static_pointer_cast<DeletedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<DeletedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->deleted_this_message_uuid,
                            other_user_message->deleted_this_message_uuid,
                            "MESSAGE_SPECIFICS.deleted_this_message_uuid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->deleted_modified_message_uuid,
                            other_user_message->deleted_modified_message_uuid,
                            "MESSAGE_SPECIFICS.deleted_modified_message_uuid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->deleted_deleted_type,
                            other_user_message->deleted_deleted_type,
                            "MESSAGE_SPECIFICS.deleted_deleted_type",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->deleted_modified_message_created_time.value.count(),
                            other_user_message->deleted_modified_message_created_time.value.count(),
                            "MESSAGE_SPECIFICS.deleted_modified_message_created_time",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kUserKickedMessage: {
                    auto current_user_message = std::static_pointer_cast<UserKickedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<UserKickedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->user_kicked_kicked_oid,
                            other_user_message->user_kicked_kicked_oid,
                            "MESSAGE_SPECIFICS.user_kicked_kicked_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kUserBannedMessage: {
                    auto current_user_message = std::static_pointer_cast<UserBannedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<UserBannedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->user_banned_banned_oid,
                            other_user_message->user_banned_banned_oid,
                            "MESSAGE_SPECIFICS.user_banned_banned_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kDifferentUserJoinedMessage: {
                    auto current_user_message = std::static_pointer_cast<DifferentUserJoinedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<DifferentUserJoinedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->user_joined_account_state,
                            other_user_message->user_joined_account_state,
                            "MESSAGE_SPECIFICS.user_joined_account_state",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->user_joined_from_event,
                            other_user_message->user_joined_from_event,
                            "MESSAGE_SPECIFICS.user_joined_from_event",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kDifferentUserLeftMessage: {
                    auto current_user_message = std::static_pointer_cast<DifferentUserLeftMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<DifferentUserLeftMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->user_new_account_admin_oid,
                            other_user_message->user_new_account_admin_oid,
                            "MESSAGE_SPECIFICS.user_new_account_admin_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kChatRoomNameUpdatedMessage: {
                    auto current_user_message = std::static_pointer_cast<ChatRoomNameUpdatedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<ChatRoomNameUpdatedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->name_new_name,
                            other_user_message->name_new_name,
                            "MESSAGE_SPECIFICS.user_new_account_admin_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kChatRoomPasswordUpdatedMessage: {
                    auto current_user_message = std::static_pointer_cast<ChatRoomPasswordUpdatedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<ChatRoomPasswordUpdatedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->password_new_password,
                            other_user_message->password_new_password,
                            "MESSAGE_SPECIFICS.password_new_password",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                    break;
                }
                case MessageSpecifics::kNewAdminPromotedMessage: {
                    auto current_user_message = std::static_pointer_cast<NewAdminPromotedMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<NewAdminPromotedMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->new_admin_admin_account_oid,
                            other_user_message->new_admin_admin_account_oid,
                            "MESSAGE_SPECIFICS.new_admin_admin_account_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kNewPinnedLocationMessage: {
                    auto current_user_message = std::static_pointer_cast<NewPinnedLocationMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<NewPinnedLocationMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->pinned_location_longitude,
                            other_user_message->pinned_location_longitude,
                            "MESSAGE_SPECIFICS.pinned_location_longitude",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_user_message->pinned_location_latitude,
                            other_user_message->pinned_location_latitude,
                            "MESSAGE_SPECIFICS.pinned_location_latitude",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }
                case MessageSpecifics::kMatchCanceledMessage: {
                    auto current_user_message = std::static_pointer_cast<MatchCanceledMessageSpecifics>(
                            message_specifics_document);
                    auto other_user_message = std::static_pointer_cast<MatchCanceledMessageSpecifics>(
                            other.message_specifics_document);

                    checkForEquality(
                            current_user_message->match_canceled_account_oid,
                            other_user_message->match_canceled_account_oid,
                            "MESSAGE_SPECIFICS.match_canceled_account_oid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    break;
                }

                    //empty document types (although they each have a type defined)
                case MessageSpecifics::kUpdateObservedTimeMessage:
                case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
                case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
                case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
                case MessageSpecifics::kThisUserLeftChatRoomMessage:
                case MessageSpecifics::kUserActivityDetectedMessage:
                case MessageSpecifics::kChatRoomCapMessage:
                case MessageSpecifics::kHistoryClearedMessage:
                case MessageSpecifics::kNewUpdateTimeMessage:
                case MessageSpecifics::kLoadingMessage:
                case MessageSpecifics::MESSAGE_BODY_NOT_SET:
                    break;
            }

            if (current_active_message_doc != nullptr
                && other_active_message_doc != nullptr) {

                checkForEquality(
                        current_active_message_doc->chat_room_message_deleted_type_key,
                        other_active_message_doc->chat_room_message_deleted_type_key,
                        "ACTIVE_MESSAGE_DOC.chat_room_message_deleted_type_key",
                        OBJECT_CLASS_NAME,
                        return_value
                );

                if (current_active_message_doc->chat_room_message_deleted_accounts.size() ==
                    other_active_message_doc->chat_room_message_deleted_accounts.size()) {
                    for (int i = 0; i < (int)current_active_message_doc->chat_room_message_deleted_accounts.size(); i++) {
                        checkForEquality(
                                current_active_message_doc->chat_room_message_deleted_accounts[i],
                                other_active_message_doc->chat_room_message_deleted_accounts[i],
                                "ACTIVE_MESSAGE_DOC.chat_room_message_deleted_accounts.oid",
                                OBJECT_CLASS_NAME,
                                return_value
                        );
                    }
                } else {
                    checkForEquality(
                            current_active_message_doc->chat_room_message_deleted_accounts.size(),
                            other_active_message_doc->chat_room_message_deleted_accounts.size(),
                            "ACTIVE_MESSAGE_DOC.chat_room_message_deleted_accounts",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                }

                if (current_active_message_doc->reply_document != nullptr
                    && other_active_message_doc->reply_document != nullptr) {

                    checkForEquality(
                            current_active_message_doc->reply_document->sent_from_account_oid_string,
                            other_active_message_doc->reply_document->sent_from_account_oid_string,
                            "ACTIVE_MESSAGE_DOC.reply_document.sent_from_account_oid_string",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_active_message_doc->reply_document->message_uuid,
                            other_active_message_doc->reply_document->message_uuid,
                            "ACTIVE_MESSAGE_DOC.reply_document.message_uuid",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    checkForEquality(
                            current_active_message_doc->reply_document->reply_body_case,
                            other_active_message_doc->reply_document->reply_body_case,
                            "ACTIVE_MESSAGE_DOC.reply_document.reply_body_case",
                            OBJECT_CLASS_NAME,
                            return_value
                    );

                    if (current_active_message_doc->reply_document->reply_body_case ==
                        other_active_message_doc->reply_document->reply_body_case) {
                        switch (other_active_message_doc->reply_document->reply_body_case) {
                            case ReplySpecifics::kTextReply: {
                                auto current_reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::TextReplyDocument>(
                                        current_active_message_doc->reply_document);
                                auto other_reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::TextReplyDocument>(
                                        other_active_message_doc->reply_document);

                                checkForEquality(
                                        current_reply_to_message->chat_message_text,
                                        other_reply_to_message->chat_message_text,
                                        "ACTIVE_MESSAGE_DOC.reply_document.chat_message_text",
                                        OBJECT_CLASS_NAME,
                                        return_value
                                        );
                                break;
                            }
                            case ReplySpecifics::kPictureReply: {
                                auto current_reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::PictureReplyDocument>(
                                        current_active_message_doc->reply_document);
                                auto other_reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::PictureReplyDocument>(
                                        other_active_message_doc->reply_document);

                                checkForEquality(
                                        current_reply_to_message->picture_thumbnail_in_bytes,
                                        other_reply_to_message->picture_thumbnail_in_bytes,
                                        "ACTIVE_MESSAGE_DOC.reply_document.picture_thumbnail_in_bytes",
                                        OBJECT_CLASS_NAME,
                                        return_value
                                        );

                                checkForEquality(
                                        current_reply_to_message->picture_thumbnail_size,
                                        other_reply_to_message->picture_thumbnail_size,
                                        "ACTIVE_MESSAGE_DOC.reply_document.picture_thumbnail_size",
                                        OBJECT_CLASS_NAME,
                                        return_value
                                        );
                                break;
                            }
                            case ReplySpecifics::kMimeReply: {
                                auto current_reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::MimeTypeReplyDocument>(
                                        current_active_message_doc->reply_document);
                                auto other_reply_to_message = std::static_pointer_cast<ChatRoomMessageDoc::ActiveMessageInfo::MimeTypeReplyDocument>(
                                        other_active_message_doc->reply_document);

                                checkForEquality(
                                        current_reply_to_message->mime_type_thumbnail_in_bytes,
                                        other_reply_to_message->mime_type_thumbnail_in_bytes,
                                        "ACTIVE_MESSAGE_DOC.reply_document.mime_type_thumbnail_in_bytes",
                                        OBJECT_CLASS_NAME,
                                        return_value
                                        );

                                checkForEquality(
                                        current_reply_to_message->mime_type_thumbnail_size,
                                        other_reply_to_message->mime_type_thumbnail_size,
                                        "ACTIVE_MESSAGE_DOC.reply_document.mime_type_thumbnail_size",
                                        OBJECT_CLASS_NAME,
                                        return_value
                                        );

                                checkForEquality(
                                        current_reply_to_message->reply_mime_type,
                                        other_reply_to_message->reply_mime_type,
                                        "ACTIVE_MESSAGE_DOC.reply_document.reply_mime_type",
                                        OBJECT_CLASS_NAME,
                                        return_value
                                        );
                                break;
                            }

                            case ReplySpecifics::kLocationReply:
                            case ReplySpecifics::kInviteReply:
                            case ReplySpecifics::REPLY_BODY_NOT_SET:
                                break;
                        }
                    } //no need to add an else here, was checked above

                } else {
                    checkForEquality(
                            current_active_message_doc->reply_document,
                            other_active_message_doc->reply_document,
                            "ACTIVE_MESSAGE_DOC.reply_document",
                            OBJECT_CLASS_NAME,
                            return_value
                    );
                }

            } else {
                checkForEquality(
                        current_active_message_doc,
                        other_active_message_doc,
                        "ACTIVE_MESSAGE_DOC",
                        OBJECT_CLASS_NAME,
                        return_value
                );
            }

        } //no need to add an else here, was checked above
    } else {
        checkForEquality(
                message_specifics_document,
                other.message_specifics_document,
                "MESSAGE_SPECIFICS_DOCUMENT",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const ChatRoomMessageDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}