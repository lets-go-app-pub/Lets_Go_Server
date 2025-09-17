//
// Created by jeremiah on 11/21/21.
//

#pragma once

#include <string>

//keys for (CHAT_ROOMS_DATABASE_NAME) (CHAT_ROOM_ID_)
//fields present in message type documents
namespace chat_room_header_keys {

    //this collection represents all the chat for a chat room
    //there will be a 'header' document with the chat room details then 'message' documents for each message
    //these will be indexed by created time
    //these have 3 letter field names to be sure that no header values overlap with the high level message values (values
    // meaning things like "iDe" in header) this is because when searching for multiple documents, the other stuff can
    // be projected OUT of the header, however I don't want this to affect the messages
    //Header
    inline const std::string ID = "iDe"; //utf8; _id for the header (the chat room id is built into the collection name)
    inline const std::string CHAT_ROOM_NAME = "nAm"; //utf8; name of the chat room
    inline const std::string CHAT_ROOM_PASSWORD = "pWo"; //utf8; password of the chat room
    inline const std::string CHAT_ROOM_LAST_ACTIVE_TIME = "lAt"; //mongo Date; last time this chat room was active; NOTE: this is NOT updated on every message, only messages user will actively see
    inline const std::string MATCHING_OID_STRINGS = "mMm"; //null OR array of utf8; if this is a chat room for 2 people that swiped 'yes' on each other and no contact has been made yet will be an array of 2 strings, the OIDs of the user accounts; null otherwise

    //These three fields will exist if it is an event chat room. Or not exist if it is not an event chat room.
    inline const std::string EVENT_ID = "eId"; //oid or does not exist; If this is an event chat room, this will be set to the oid of the event inside user accounts collection. If it is not an event type chat room, the field will not exist.
    inline const std::string QR_CODE = "qRc"; //string or does not exist; If this is an event chat room, it will be the qr code (or QR_CODE_DEFAULT if there is no qr code). If it is not an event type chat room, the field will not exist.
    inline const std::string QR_CODE_MESSAGE = "qCm"; //string or does not exist; If this is an event chat room, it will be the timestamp (or QR_CODE_TIME_UPDATED_DEFAULT of there is no qr code). If it is not an event type chat room, the field will not exist.
    inline const std::string QR_CODE_TIME_UPDATED = "qRt"; //mongodb date or does not exist; If this is an event chat room, it will be the timestamp (or QR_CODE_TIME_UPDATED_DEFAULT of there is no qr code). If it is not an event type chat room, the field will not exist.

    inline const std::string PINNED_LOCATION = "pIl"; //document or does not exist; This field will not exist if there is no PINNED_LOCATION.

    inline const std::string MIN_AGE = "mAg"; //int32 or does not exist; If this is an event chat room, this will be set to the minimum age of the activity that was used to create it. If it does not exist, the value is assumed to be server_parameter_restrictions::LOWEST_ALLOWED_AGE.

    namespace pinned_location {
        inline const std::string LONGITUDE = "lo"; //double
        inline const std::string LATITUDE = "la"; //double
    }

    inline const std::string ACCOUNTS_IN_CHAT_ROOM = "aCc"; //array of documents; document elements are the variables until the empty line

    namespace accounts_in_chat_room {
        inline const std::string ACCOUNT_OID = "aO"; //OID: account OID
        inline const std::string STATE_IN_CHAT_ROOM = "aS"; //int32; uses enum AccountStateInChatRoom in ChatRoomCommands.proto
        inline const std::string FIRST_NAME = "fN"; //utf8; the account first name (used after the account is no longer inside the chat room)
        inline const std::string THUMBNAIL_REFERENCE = "tR"; //string (an oid of type string); the user current thumbnail reference (used after the account is no longer inside the chat room, can be set to DeletedThumbnailInfo::thumbnail_reference_oid if picture was inappropriate)
        inline const std::string THUMBNAIL_TIMESTAMP = "tA"; //mongodb Date; the last time the user thumbnail was set (this will be set when for example a user joins the chat room, however it will be updated EVEN IF THE THUMBNAIL_IN_BYTES DOES NOT CHANGE)
        //inline const std::string THUMBNAIL = "tN"; //utf8; the user current thumbnail (used after the account is no longer inside the chat room, can be set to DeletedThumbnailInfo::thumbnail if picture was inappropriate)
        inline const std::string THUMBNAIL_SIZE = "tS"; //int32; the user current thumbnail size (used for comparison purposes after account is no longer inside the chat room to see if the thumbnail requires an update, can be set to DeletedThumbnailInfo::thumbnail.size() if picture was inappropriate)
        inline const std::string LAST_ACTIVITY_TIME = "aT"; //mongo Date; the last time this user performed any action inside or viewed this chat room
        inline const std::string TIMES_JOINED_LEFT = "tJ"; //array of mongoDB Dates; will be ordered as [ joinedTime, leftTime, joinedTime ...]
    }

}

