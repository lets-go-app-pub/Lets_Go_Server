//
// Created by jeremiah on 6/1/22.
//

#pragma once

#include <boost/optional.hpp>
#include <bsoncxx/oid.hpp>
#include <iostream>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <utility>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <AlgorithmSearchOptions.grpc.pb.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <server_parameter_restrictions.h>
#include <matching_algorithm.h>
#include <database_names.h>
#include <collection_names.h>
#include <DisciplinaryActionType.grpc.pb.h>
#include <base_collection_object.h>
#include <ChatRoomCommands.grpc.pb.h>
#include "chat_room_header_keys.h"

struct ChatRoomShared {
    bsoncxx::types::b_date timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{-1L}};

    ChatRoomShared() = default;

    explicit ChatRoomShared(bsoncxx::types::b_date _timestamp) : timestamp(_timestamp) {}
};

class ChatMessagePictureDoc : public BaseCollectionObject {
public:

    std::string chat_room_id; //"cR"; //utf8; ID of chat room this message was sent from
    std::string picture_in_bytes; //"pI"; //utf8; picture itself in bytes
    int picture_size_in_bytes = 0; //"pS"; //int32; total size in bytes of this picture
    int height = 0; //"pH"; //int32; height of the picture in pixels
    int width = 0; //"pW"; //int32; width of the picture in pixels
    bsoncxx::types::b_date timestamp_stored = DEFAULT_DATE; //"tS"; //mongocxx date type; timestamp picture was stored

    virtual bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    virtual void convertToDocument(
            bsoncxx::builder::stream::document& document_result,
            bool skip_long_strings
    ) const;

    virtual bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    ChatMessagePictureDoc() = default;

    explicit ChatMessagePictureDoc(bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const ChatMessagePictureDoc& v);

    bool operator==(const ChatMessagePictureDoc& other) const;

    bool operator!=(const ChatMessagePictureDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Chat Message Picture";
};

class ChatRoomHeaderDoc : public BaseCollectionObject {
public:

    std::string chat_room_id;
    std::string chat_room_name; //"nAm"; //utf8; name of the chat room
    std::string chat_room_password; //"pWo"; //utf8; password of the chat room
    bsoncxx::types::b_date chat_room_last_active_time = DEFAULT_DATE; //"lAt"; //mongo Date; last time this chat room was active; NOTE: this is NOT updated on every message, only messages user will actively see
    std::unique_ptr<std::vector<std::string>> matching_oid_strings = nullptr; //"mMm"; //null OR array of utf8; if this is a chat room for 2 people that swiped 'yes' on each other and no contact has been made yet will be an array of 2 strings, the OIDs of the user accounts; null otherwise

    std::optional<bsoncxx::oid> event_id{}; //"eId"; //oid or does not exist; If this is an event chat room, this will be set to the oid of the event inside user accounts collection. If it is not an event type chat room, the field will not exist.
    std::optional<std::string> qr_code{}; //"qRc"; //string or does not exist; If this is an event chat room, it will be the qr code (or QR_CODE_DEFAULT if there is no qr code). If it is not an event type chat room, the field will not exist.
    std::optional<std::string> qr_code_message{}; //"qCm"; //string or does not exist; If this is an event chat room, it will be the timestamp (or QR_CODE_TIME_UPDATED_DEFAULT of there is no qr code). If it is not an event type chat room, the field will not exist.
    std::optional<bsoncxx::types::b_date> qr_code_time_updated{}; //"qRt"; //mongodb date or does not exist; If this is an event chat room, it will be the timestamp (or QR_CODE_TIME_UPDATED_DEFAULT of there is no qr code). If it is not an event type chat room, the field will not exist.

    struct PinnedLocation {
        double longitude;
        double latitude;

        PinnedLocation() = delete;

        PinnedLocation(
                double _longitude,
                double _latitude) :
                longitude(_longitude),
                latitude(_latitude) {}
    };

    std::optional<PinnedLocation> pinned_location{}; //"pIl"; //document or does not exist; This field will not exist if there is no PINNED_LOCATION.

    std::optional<int> min_age{};//int32 or does not exist; If this is an event chat room, this will be set to the minimum age of the activity that was used to create it. If it does not exist, the value is assumed to be server_parameter_restrictions::LOWEST_ALLOWED_AGE.

    struct AccountsInChatRoom {
        bsoncxx::oid account_oid; //"aO"; //OID: account OID
        AccountStateInChatRoom state_in_chat_room; //"aS"; //int32; uses enum AccountStateInChatRoom in ChatRoomCommands.proto
        std::string first_name; //"fN"; //utf8; the account first name (used after the account is no longer inside the chat room)
        std::string thumbnail_reference; //"tR"; //string; the user current thumbnail reference (used after the account is no longer inside the chat room, can be empty if picture was inappropriate)
        bsoncxx::types::b_date thumbnail_timestamp; //"tA"; //mongodb Date; the last time the user thumbnail was set (this will be set when for example a user joins the chat room, however it will be updated EVEN IF THE THUMBNAIL_IN_BYTES DOES NOT CHANGE)
        int thumbnail_size; //"tS"; //int32; the user current thumbnail size (used for comparison purposes after account is no longer inside the chat room to see if the thumbnail requires an update, can be zero if picture was inappropriate)
        bsoncxx::types::b_date last_activity_time; //"aT"; //mongo Date; the last time this user performed any action inside or viewed this chat room
        std::vector<bsoncxx::types::b_date> times_joined_left; // = "tJ"; //array of mongoDB Dates; will be ordered as [ joinedTime, leftTime, joinedTime ...]

        AccountsInChatRoom() = delete;

        AccountsInChatRoom(
                const bsoncxx::oid& _account_oid,
                AccountStateInChatRoom _state_in_chat_room,
                std::string _first_name,
                std::string _thumbnail_reference,
                bsoncxx::types::b_date _thumbnail_timestamp,
                int _thumbnail_size,
                bsoncxx::types::b_date _last_activity_time,
                const std::vector<bsoncxx::types::b_date>& _times_joined_left
        ) :
                account_oid(_account_oid),
                state_in_chat_room(_state_in_chat_room),
                first_name(std::move(_first_name)),
                thumbnail_reference(std::move(_thumbnail_reference)),
                thumbnail_timestamp(_thumbnail_timestamp),
                thumbnail_size(_thumbnail_size),
                last_activity_time(_last_activity_time) {
            std::copy(_times_joined_left.begin(), _times_joined_left.end(), std::back_inserter(times_joined_left));
        }

        //converts this UserAccountDoc object to a document and saves it to the passed builder
        void convertToDocument(
                bsoncxx::builder::stream::document& document_result
        ) const {

            bsoncxx::builder::basic::array times_joined_left_arr;

            for (const bsoncxx::types::b_date& time_joined: times_joined_left) {
                times_joined_left_arr.append(time_joined);
            }

            document_result
                    << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << account_oid
                    << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << state_in_chat_room
                    << chat_room_header_keys::accounts_in_chat_room::FIRST_NAME << first_name
                    << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << thumbnail_reference
                    << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << thumbnail_timestamp
                    << chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << thumbnail_size
                    << chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << last_activity_time
                    << chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << times_joined_left_arr;
        }
    };

    std::vector<AccountsInChatRoom> accounts_in_chat_room; //"aCc"; //array of documents; document elements are the variables until the empty line

    ChatRoomShared shared_properties;

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val,
                            const std::string& updated_chat_room_id);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(
            bsoncxx::builder::stream::document& document_result
    ) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document,
                                const std::string& updated_chat_room_id);

    bool setIntoCollection() override;

    void generateHeaderForEventChatRoom(
            const std::string& _chat_room_id,
            const std::string& _chat_room_name,
            const std::string& _chat_room_password,
            const std::chrono::milliseconds& _chat_room_last_active_time,
            const bsoncxx::oid _event_oid,
            const std::string& _qr_code,
            const std::string& _qr_code_message,
            const std::chrono::milliseconds & _qr_code_time_updated,
            double longitude,
            double latitude,
            int _min_age,
            const std::chrono::milliseconds & _created_time,
            const bsoncxx::oid& account_oid
    );

    bool getFromCollection(const std::string& _chat_room_id);

    bool getFromCollection(const bsoncxx::oid& _) override;

    ChatRoomHeaderDoc() = default;

    explicit ChatRoomHeaderDoc(const std::string& _chat_room_id) {
        getFromCollection(_chat_room_id);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const ChatRoomHeaderDoc& v);

    bool operator==(const ChatRoomHeaderDoc& other) const;

    bool operator!=(const ChatRoomHeaderDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Chat Room Header";
};

class ChatRoomInfoDoc : public BaseCollectionObject {
public:

    //this is the collection that holds the number to convert into a chat room ID
    long previously_used_chat_room_number = 0; //"pN"; //int64; the number used to convert into a chat room ID

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    ChatRoomInfoDoc() = default;

    explicit ChatRoomInfoDoc(bool) {
        getFromCollection(bsoncxx::oid{});
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const ChatRoomInfoDoc& v);

    bool operator==(const ChatRoomInfoDoc& other) const;

    bool operator!=(const ChatRoomInfoDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Chat Room Info";
};

class ChatRoomMessageDoc : public BaseCollectionObject {
public:

    std::string chat_room_id;
    std::string id;
    bsoncxx::oid message_sent_by; //"sB"; //oid; user that sent this timestamp NOTE: for joined chat room it is also the user that joined MUST ALWAYS EXIST
    MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::MESSAGE_BODY_NOT_SET; //"mT"; //int32; follows MessageSpecifics::MessageBodyCase from the oneof inside TypeOfChatMessage.proto enum MUST ALWAYS EXIST

    struct MessageSpecificsDoc {
    };

    struct ActiveMessageInfo {

        struct ReplyDocument {
            std::string sent_from_account_oid_string; //"rA"; //utf8; oid of the sender of the message being replied to
            std::string message_uuid; //"rF"; //utf8; uuid of the message being replied to
            ReplySpecifics::ReplyBodyCase reply_body_case; // = "rT"; //int32; type of message this is replying to; follows ReplySpecifics::ReplyBodyCase:: enum from TypeOfChatMessage.proto

            ReplyDocument() = delete;

            ReplyDocument(
                    std::string _sent_from_account_oid_string,
                    std::string _message_uuid,
                    ReplySpecifics::ReplyBodyCase _reply_body_case
            ) :
                    sent_from_account_oid_string(std::move(_sent_from_account_oid_string)),
                    message_uuid(std::move(_message_uuid)),
                    reply_body_case(_reply_body_case) {}
        };

        struct TextReplyDocument : public ReplyDocument {
            //TextChatReplyMessage = 1
            std::string chat_message_text; //"rE"; //utf8; message text of message this is replying to CHAT_TEXT_MESSAGE

            TextReplyDocument() = delete;

            TextReplyDocument(
                    const std::string& _sent_from_account_oid_string,
                    const std::string& _message_uuid,
                    ReplySpecifics::ReplyBodyCase _reply_body_case,
                    std::string _chat_message_text
            ) : ReplyDocument(
                    _sent_from_account_oid_string,
                    _message_uuid,
                    _reply_body_case
            ),
                chat_message_text(std::move(_chat_message_text)) {}
        };

        struct PictureReplyDocument : public ReplyDocument {
            //PictureReplyMessage = 2
            std::string picture_thumbnail_in_bytes; //"rN"; //utf8; the thumbnail of the picture or gif being replied to; used with MIME_TYPE_MESSAGE & PICTURE_MESSAGE
            int picture_thumbnail_size; //"rS"; //int32; the thumbnail size of the picture or gif being extracted; set when CHAT_ROOM_MESSAGE_IS_REPLY_THUMBNAIL is set

            PictureReplyDocument() = delete;

            PictureReplyDocument(
                    const std::string& _sent_from_account_oid_string,
                    const std::string& _message_uuid,
                    ReplySpecifics::ReplyBodyCase _reply_body_case,
                    std::string _picture_thumbnail_in_bytes,
                    int _picture_thumbnail_size
            ) : ReplyDocument(
                    _sent_from_account_oid_string,
                    _message_uuid,
                    _reply_body_case
            ),
                picture_thumbnail_in_bytes(std::move(_picture_thumbnail_in_bytes)),
                picture_thumbnail_size(_picture_thumbnail_size) {}
        };

        struct LocationReplyDocument : public ReplyDocument {
            //LocationReplyMessage = 3
            LocationReplyDocument() = delete;

            LocationReplyDocument(
                    const std::string& _sent_from_account_oid_string,
                    const std::string& _message_uuid,
                    ReplySpecifics::ReplyBodyCase _reply_body_case
            ) : ReplyDocument(
                    _sent_from_account_oid_string,
                    _message_uuid,
                    _reply_body_case
            ) {}
        };

        struct MimeTypeReplyDocument : public ReplyDocument {
            //MimeTypeReplyMessage = 4
            std::string mime_type_thumbnail_in_bytes; //"mN"; //utf8; the thumbnail of the picture or gif being replied to; used with MIME_TYPE_MESSAGE & PICTURE_MESSAGE
            int mime_type_thumbnail_size; //"mZ"; //int32; the thumbnail size of the picture or gif being extracted; set when CHAT_ROOM_MESSAGE_IS_REPLY_THUMBNAIL is set
            std::string reply_mime_type; //"mY"; //utf8; the mime type of the image; used when replying to MIME_TYPE_MESSAGE

            MimeTypeReplyDocument() = delete;

            MimeTypeReplyDocument(
                    const std::string& _sent_from_account_oid_string,
                    const std::string& _message_uuid,
                    ReplySpecifics::ReplyBodyCase _reply_body_case,
                    std::string _mime_type_thumbnail_in_bytes,
                    int _mime_type_thumbnail_size,
                    std::string _reply_mime_type
            ) : ReplyDocument(
                    _sent_from_account_oid_string,
                    _message_uuid,
                    _reply_body_case
            ),
                mime_type_thumbnail_in_bytes(std::move(_mime_type_thumbnail_in_bytes)),
                mime_type_thumbnail_size(_mime_type_thumbnail_size),
                reply_mime_type(std::move(_reply_mime_type)) {}
        };

        struct InvitedReplyDocument : public ReplyDocument {
            //InvitedReplyMessage = 5
            InvitedReplyDocument() = delete;

            InvitedReplyDocument(
                    const std::string& _sent_from_account_oid_string,
                    const std::string& _message_uuid,
                    ReplySpecifics::ReplyBodyCase _reply_body_case
            ) : ReplyDocument(
                    _sent_from_account_oid_string,
                    _message_uuid,
                    _reply_body_case
            ) {}
        };

        DeleteType chat_room_message_deleted_type_key; //"dT"; //int32; follows enum DeleteType inside TypeOfChatMessage.proto, used with MESSAGE_DELETED message type
        std::vector<std::string> chat_room_message_deleted_accounts; //array of utf8; if  CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY == DeleteType::DELETE_FOR_SINGLE_USER then this will store the oid it is deleted for
        std::shared_ptr<ReplyDocument> reply_document = nullptr; //"iR"; //document OR null; if the message is a reply, this will be set to a document with the relevant info, if not it will be set to null

        ActiveMessageInfo() = delete;

        ActiveMessageInfo(
                DeleteType _chat_room_message_deleted_type_key,
                std::vector<std::string> _chat_room_message_deleted_accounts_key,
                std::shared_ptr<ReplyDocument> _reply_document
        ) :
                chat_room_message_deleted_type_key(_chat_room_message_deleted_type_key),
                chat_room_message_deleted_accounts(std::move(_chat_room_message_deleted_accounts_key)),
                reply_document(std::move(_reply_document)) {}

        ActiveMessageInfo(const ActiveMessageInfo& other) {
            chat_room_message_deleted_type_key = other.chat_room_message_deleted_type_key;
            chat_room_message_deleted_accounts = other.chat_room_message_deleted_accounts;
            reply_document = other.reply_document;
        }

        ActiveMessageInfo(ActiveMessageInfo&& other) noexcept {
            chat_room_message_deleted_type_key = other.chat_room_message_deleted_type_key;
            chat_room_message_deleted_accounts = std::move(other.chat_room_message_deleted_accounts);
            reply_document = std::move(other.reply_document);
        }
    };

    struct ChatTextMessageSpecifics : MessageSpecificsDoc {
        ActiveMessageInfo active_message_info; //"aM"; //document; used inside 'active' messages for repeated fields (the nested document mirrors the proto messages)

        //CHAT_TEXT_MESSAGE = 0;
        std::string text_message; //"cM"; //utf8; will be set if was type chat, chat room name update, chat room password updated or name of user sending the invite message
        bool text_is_edited; //"iE"; //bool; true if message has been edited, false if not
        bsoncxx::types::b_date text_edited_time; //"eT"; //mongoDB Date; this will be set to the edited time if TEXT_IS_EDITED == true

        ChatTextMessageSpecifics() = delete;

        ChatTextMessageSpecifics(
                ActiveMessageInfo _active_message_info,
                std::string _text_message,
                bool _text_is_edited,
                bsoncxx::types::b_date _text_edited_time
        ) :
                active_message_info(std::move(_active_message_info)),
                text_message(std::move(_text_message)),
                text_is_edited(_text_is_edited),
                text_edited_time(_text_edited_time) {}
    };

    struct PictureMessageSpecifics : MessageSpecificsDoc {
        ActiveMessageInfo active_message_info; //"aM"; //document; used inside 'active' messages for repeated fields (the nested document mirrors the proto messages)

        //PICTURE_MESSAGE = 1;
        std::string picture_oid; //"pO"; //utf8; will be set if message type was picture (stored inside collection CHAT_MESSAGE_PICTURES_COLLECTION_NAME) or edited; IMPORTANT: this needs to remain a string instead of an OID for aggregation pipeline purposes with MESSAGE_EDITED type
        int picture_image_width; //"pW"; //int32; width of the stored image; used with gif and picture message types
        int picture_image_height; //"pH"; //int32; height of the stored image; used with gif and picture message types

        PictureMessageSpecifics() = delete;

        PictureMessageSpecifics(
                ActiveMessageInfo _active_message_info,
                std::string _picture_oid,
                int _picture_image_width,
                int _picture_image_height
        ) :
                active_message_info(std::move(_active_message_info)),
                picture_oid(std::move(_picture_oid)),
                picture_image_width(_picture_image_width),
                picture_image_height(_picture_image_height) {}
    };

    struct LocationMessageSpecifics : MessageSpecificsDoc {
        ActiveMessageInfo active_message_info; //"aM"; //document; used inside 'active' messages for repeated fields (the nested document mirrors the proto messages)

        //LOCATION_MESSAGE = 2;
        double location_longitude; //"lO"; //double; will be set if type was location
        double location_latitude; //"lA"; //double; will be set if type was location

        LocationMessageSpecifics() = delete;

        LocationMessageSpecifics(
                ActiveMessageInfo _active_message_info,
                double _location_longitude,
                double _location_latitude
        ) :
                active_message_info(std::move(_active_message_info)),
                location_longitude(_location_longitude),
                location_latitude(_location_latitude) {}
    };

    struct MimeTypeMessageSpecifics : MessageSpecificsDoc {
        ActiveMessageInfo active_message_info; //"aM"; //document; used inside 'active' messages for repeated fields (the nested document mirrors the proto messages)

        //MIME_TYPE_MESSAGE = 3;
        std::string mime_type_url; //"gU"; //utf8; url to be returned to the device
        std::string mime_type_type; //"iM"; //utf8; mime type of image, this will be one of the types listed inside the vector allowed_mime_types
        int mime_type_image_width; //"mW"; //int32; width of the stored image; used with gif and picture message types
        int mime_type_image_height; //"mH"; //int32; height of the stored image; used with gif and picture message types

        MimeTypeMessageSpecifics() = delete;

        MimeTypeMessageSpecifics(
                ActiveMessageInfo _active_message_info,
                std::string _mime_type_url,
                std::string _mime_type_type,
                int _mime_type_image_width,
                int _mime_type_image_height
        ) :
                active_message_info(std::move(_active_message_info)),
                mime_type_url(std::move(_mime_type_url)),
                mime_type_type(std::move(_mime_type_type)),
                mime_type_image_width(_mime_type_image_width),
                mime_type_image_height(_mime_type_image_height) {}
    };

    struct InviteMessageSpecifics : MessageSpecificsDoc {
        ActiveMessageInfo active_message_info; //"aM"; //document; used inside 'active' messages for repeated fields (the nested document mirrors the proto messages)

        //INVITED_TO_CHAT_ROOM = 4;
        std::string invited_user_account_oid; //"iO"; //utf8; url to be returned to the device
        std::string invited_user_name; //"nA"; //utf8; username used for new user joined
        std::string invited_chat_room_id; //"cI"; //utf8; used with invite to chat room
        std::string invited_chat_room_name; //"cN"; //utf8; used with invite to chat room
        std::string invited_chat_room_password; //"cP"; //utf8; used with invite to chat room

        InviteMessageSpecifics() = delete;

        InviteMessageSpecifics(
                ActiveMessageInfo _active_message_info,
                std::string _invited_user_account_oid,
                std::string _invited_user_name,
                std::string _invited_chat_room_id,
                std::string _invited_chat_room_name,
                std::string _invited_chat_room_password
        ) :
                active_message_info(std::move(_active_message_info)),
                invited_user_account_oid(std::move(_invited_user_account_oid)),
                invited_user_name(std::move(_invited_user_name)),
                invited_chat_room_id(std::move(_invited_chat_room_id)),
                invited_chat_room_name(std::move(_invited_chat_room_name)),
                invited_chat_room_password(std::move(_invited_chat_room_password)) {}
    };

    struct EditedMessageSpecifics : MessageSpecificsDoc {

        //MESSAGE_EDITED = 5;
        std::string edited_this_message_uuid; //"eM"; //utf8; uuid of THIS message (identical to the _id field) it is needed because _id is lost in the $group stage during an aggregation
        std::string edited_modified_message_uuid; //"eU"; //utf8; uuid of the message this message edited
        std::string edited_new_message_text; //"nM"; //utf8; new message text to update the CHAT_TEXT_MESSAGE to
        std::string edited_previous_message_text; //"pM"; //utf8; old message text CHAT_TEXT_MESSAGE held previously
        bsoncxx::types::b_date edited_modified_message_created_time; //"mC"; //mongodb Data; date that the modified message was created

        EditedMessageSpecifics() = delete;

        EditedMessageSpecifics(
                std::string _edited_this_message_uuid,
                std::string _edited_modified_message_uuid,
                std::string _edited_new_message_text,
                std::string _edited_previous_message_text,
                bsoncxx::types::b_date _edited_modified_message_created_time
        ) :
                edited_this_message_uuid(std::move(_edited_this_message_uuid)),
                edited_modified_message_uuid(std::move(_edited_modified_message_uuid)),
                edited_new_message_text(std::move(_edited_new_message_text)),
                edited_previous_message_text(std::move(_edited_previous_message_text)),
                edited_modified_message_created_time(_edited_modified_message_created_time) {}
    };

    struct DeletedMessageSpecifics : MessageSpecificsDoc {

        //MESSAGE_DELETED = 6;
        std::string deleted_this_message_uuid; //"dM"; //utf8; uuid of THIS message (identical to the _id field) it is needed because _id is lost in the $group stage during an aggregation
        std::string deleted_modified_message_uuid; //"dU"; //utf8; uuid of the message this message deleted
        DeleteType deleted_deleted_type; //"dT"; //int32; follows enum DeleteType inside TypeOfChatMessage.proto, used with MESSAGE_DELETED message type
        bsoncxx::types::b_date deleted_modified_message_created_time; //"dC"; //mongodb Data; date that the modified message was created

        DeletedMessageSpecifics() = delete;

        DeletedMessageSpecifics(
                std::string _deleted_this_message_uuid,
                std::string _deleted_modified_message_uuid,
                DeleteType _deleted_deleted_type,
                bsoncxx::types::b_date _deleted_modified_message_created_time
        ) :
                deleted_this_message_uuid(std::move(_deleted_this_message_uuid)),
                deleted_modified_message_uuid(std::move(_deleted_modified_message_uuid)),
                deleted_deleted_type(_deleted_deleted_type),
                deleted_modified_message_created_time(_deleted_modified_message_created_time) {}
    };

    struct UserKickedMessageSpecifics : MessageSpecificsDoc {

        //USER_KICKED_FROM_CHAT_ROOM = 7;
        std::string user_kicked_kicked_oid; //"kO"; //utf8; oid (in string form) of the user that was kicked

        UserKickedMessageSpecifics() = delete;

        explicit UserKickedMessageSpecifics(
                std::string _user_kicked_kicked_oid
        ) :
                user_kicked_kicked_oid(std::move(_user_kicked_kicked_oid)) {}
    };

    struct UserBannedMessageSpecifics : MessageSpecificsDoc {

        //USER_BANNED_FROM_CHAT_ROOM = 8;
        std::string user_banned_banned_oid; //"bO"; //utf8; oid (in string form) of the user that was banned

        UserBannedMessageSpecifics() = delete;

        explicit UserBannedMessageSpecifics(
                std::string _user_banned_banned_oid
        ) :
                user_banned_banned_oid(std::move(_user_banned_banned_oid)) {}
    };

    struct DifferentUserJoinedMessageSpecifics : MessageSpecificsDoc {

        //DIFFERENT_USER_JOINED_CHAT_ROOM = 9;
        AccountStateInChatRoom user_joined_account_state; //"aS"; //int32; uses enum AccountStateInChatRoom in ChatRoomCommands.proto; account state of user that joined chat room
        std::string user_joined_from_event; //"fE"; //string or does not exist; If this join was from a user swiping yes on an event, this field will be the event oid. If it is not from the user swiping yes on an event, the field will not exist.

        DifferentUserJoinedMessageSpecifics() = delete;

        DifferentUserJoinedMessageSpecifics(
                AccountStateInChatRoom _user_joined_account_state,
                std::string _user_joined_from_event
        ) :
                user_joined_account_state(_user_joined_account_state),
                user_joined_from_event(std::move(_user_joined_from_event)) {}
    };

    struct DifferentUserLeftMessageSpecifics : MessageSpecificsDoc {

        //DIFFERENT_USER_LEFT_CHAT_ROOM = 10;
        std::string user_new_account_admin_oid; //"aO"; //utf8; oid (in string form) of the user that was promoted to account admin ("" if none)

        DifferentUserLeftMessageSpecifics() = delete;

        explicit DifferentUserLeftMessageSpecifics(
                std::string _user_new_account_admin_oid
        ) :
                user_new_account_admin_oid(std::move(_user_new_account_admin_oid)) {}
    };

    struct UpdateObservedTimeMessageSpecifics : MessageSpecificsDoc {
    };

    struct ThisUserJoinedChatRoomStartMessageSpecifics : MessageSpecificsDoc {
    };
    struct ThisUserJoinedChatRoomMemberMessageSpecifics : MessageSpecificsDoc {
    };
    struct ThisUserJoinedChatRoomFinishedMessageSpecifics : MessageSpecificsDoc {
    };

    struct ThisLeftJoinedChatRoomMessageSpecifics : MessageSpecificsDoc {
    };

    struct UserActivityDetectedMessageSpecifics : MessageSpecificsDoc {
    };

    struct ChatRoomNameUpdatedMessageSpecifics : MessageSpecificsDoc {

        //CHAT_ROOM_NAME_UPDATED = 17;
        std::string name_new_name; //"nN"; //utf8; new chat room name

        ChatRoomNameUpdatedMessageSpecifics() = delete;

        explicit ChatRoomNameUpdatedMessageSpecifics(
                std::string _name_new_name
        ) :
                name_new_name(std::move(_name_new_name)) {}
    };

    struct ChatRoomPasswordUpdatedMessageSpecifics : MessageSpecificsDoc {

        //CHAT_ROOM_PASSWORD_UPDATED = 18;
        std::string password_new_password; //"nP"; //utf8; new chat room password

        ChatRoomPasswordUpdatedMessageSpecifics() = delete;

        explicit ChatRoomPasswordUpdatedMessageSpecifics(
                std::string _password_new_password
        ) :
                password_new_password(std::move(_password_new_password)) {}
    };

    struct NewAdminPromotedMessageSpecifics : MessageSpecificsDoc {

        //NEW_ADMIN_PROMOTED = 19;
        std::string new_admin_admin_account_oid; //"nC"; //utf8; new chat room admin account oid

        NewAdminPromotedMessageSpecifics() = delete;

        explicit NewAdminPromotedMessageSpecifics(
                std::string _new_admin_admin_account_oid
        ) :
                new_admin_admin_account_oid(std::move(_new_admin_admin_account_oid)) {}
    };

    struct NewPinnedLocationMessageSpecifics : MessageSpecificsDoc {

        //NEW_PINNED_LOCATION = 19;
        double pinned_location_longitude; //"nC"; //utf8; new chat room admin account oid
        double pinned_location_latitude; //"nC"; //utf8; new chat room admin account oid

        NewPinnedLocationMessageSpecifics() = delete;

        NewPinnedLocationMessageSpecifics(
                double _pinned_location_longitude,
                double _pinned_location_latitude
        ) :
                pinned_location_longitude(_pinned_location_longitude),
                pinned_location_latitude(_pinned_location_latitude) {}
    };

    struct ChatRoomCapMessageMessageSpecifics : MessageSpecificsDoc {
    };

    struct MatchCanceledMessageSpecifics : MessageSpecificsDoc {

        //MATCH_CANCELED = 20;
        std::string match_canceled_account_oid; //"cA"; //utf8; the account oid which is the other end of this match that this message should be sent to

        MatchCanceledMessageSpecifics() = delete;

        explicit MatchCanceledMessageSpecifics(
                std::string _match_canceled_account_oid
        ) :
                match_canceled_account_oid(std::move(_match_canceled_account_oid)) {}
    };

    struct NewUpdateTimeMessageSpecifics : MessageSpecificsDoc {
    };

    struct HistoryClearedMessageMessageSpecifics : MessageSpecificsDoc {
    };
    struct LoadingMessageMessageSpecifics : MessageSpecificsDoc {
    };

    std::shared_ptr<MessageSpecificsDoc> message_specifics_document = std::make_shared<MessageSpecificsDoc>(); //"mS"; //document; will contain different values based on message type (listed below) MUST ALWAYS EXIST, can be empty

    ChatRoomShared shared_properties;

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val,
                            const std::string& updated_chat_room_id);

    static void extractDeleteFromMessage(
            const std::string& requesting_user_account_oid,
            const ActiveMessageInfo& active_message_info,
            const std::function<void()>& set_is_deleted_to_true
    );

    static void extractReplyFromMessage(
            const std::shared_ptr<ActiveMessageInfo::ReplyDocument>& reply_document,
            AmountOfMessage amount_of_message,
            const std::function<void(bool /*is_reply*/)>& set_is_reply,
            const std::function<ReplyChatMessageInfo*()>& get_mutable_reply_info
    );

    //NOTE: This function is incomplete, it will currently only work with kTextMessage.
    [[nodiscard]] ChatMessageToClient convertToChatMessageToClient(
            const std::string& requesting_user_account_oid,
            AmountOfMessage amount_of_message,
            bool only_store_message,
            bool do_not_update_user_state
    ) const;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document,
                                const std::string& updated_chat_room_id);

    bool setIntoCollection() override;

    bool getFromCollection(
            const std::string& uuid,
            const std::string& _chat_room_id
    );

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    ChatRoomMessageDoc() = default;

    ChatRoomMessageDoc(
            const std::string& uuid,
            const std::string& _chat_room_id
    ) {
        getFromCollection(uuid, _chat_room_id);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const ChatRoomMessageDoc& v);

    bool operator==(const ChatRoomMessageDoc& other) const;

    bool operator!=(const ChatRoomMessageDoc& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Chat Room Message";
};
