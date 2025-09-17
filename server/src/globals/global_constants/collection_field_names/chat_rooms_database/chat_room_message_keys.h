//
// Created by jeremiah on 11/21/21.
//

#pragma once

#include <string>

//keys for (CHAT_ROOMS_DATABASE_NAME) (CHAT_ROOM_ID_)
//Fields present in message type documents
//These fields are organized similarly to the protobuf class MessageSpecifics.
namespace chat_room_message_keys {

    //MessageReturnString
    //situations
    //1) member sent a message and left (or kicked)
    //2) member sent a message and deleted their account
    //The problem is that right now I plan to remove the person from the list of chat room members
    //so if a new user joins the chat room they will have no access to older members thumbnails or names
    //I will probably want to keep a list of deleted members; This will also fulfil the condition of saving
    //previous info; will need user ids to be unique, so probably want a list of joined times and left times
    /** Chat messages use a generated UUID for their _id field inside the collection **/
    inline const std::string MESSAGE_SENT_BY = "sB"; //oid; user that sent this timestamp NOTE: for joined chat room it is also the user that joined MUST ALWAYS EXIST
    inline const std::string MESSAGE_TYPE = "mT"; //int32; follows MessageSpecifics::MessageBodyCase from the oneof inside TypeOfChatMessage.proto enum MUST ALWAYS EXIST
    inline const std::string MESSAGE_SPECIFICS_DOCUMENT = "mS"; //document; will contain different values based on message type (listed below) MUST ALWAYS EXIST, can be empty

    inline const std::string RANDOM_INT = "R"; //int32; this is used to check if a document was already present inside the database; it is an int32 to take up minimal space while still being random
    inline const std::string TIMESTAMP_TEMPORARY_EXPIRES_AT = "eX"; //date; This is a failsafe, if the document has not been deleted by this timestamp it will be removed by mongoDB, ideally it should rarely if ever happen.

    //namespace contains keys for MESSAGE_SPECIFICS_DOCUMENT document
    namespace message_specifics {

        /** These fields are mutually exclusive based on MESSAGE_TYPE as well as the specific parameters of the message. However
         * it is still best to keep unique field names. Otherwise when performing actions like projections there can be collisions
         * when projecting the same field name twice. Which while technically is more efficient (less fields for the database to project
         * and therefore less fields to search) it is harder to remember the overlapping field names. **/

        //Chat Message Active Message Info (used for 'active' type messages)
        inline const std::string ACTIVE_MESSAGE_INFO_DOCUMENT = "aM"; //document; used inside 'active' messages for repeated fields (the nested document mirrors the proto messages)

        //namespace contains keys for ACTIVE_MESSAGE_INFO_DOCUMENT document
        namespace active_info {

            inline const std::string CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY = "dT"; //int32; follows enum DeleteType inside TypeOfChatMessage.proto, also used with MESSAGE_DELETED message type
            inline const std::string CHAT_ROOM_MESSAGE_DELETED_ACCOUNTS_KEY = "dA"; //array of utf8; if  CHAT_ROOM_MESSAGE_DELETED_TYPE_KEY == DeleteType::DELETE_FOR_SINGLE_USER then this will store the oid it is deleted for
            inline const std::string REPLY_DOCUMENT = "iR"; //document OR null; if the message is a reply, this will be set to a document with the relevant info, if not it will be set to null

            //namespace contains keys for REPLY_DOCUMENT document
            namespace reply {

                inline const std::string SENT_FROM_ACCOUNT_OID_STRING = "rA"; //utf8; oid of the sender of the message being replied to
                inline const std::string MESSAGE_UUID = "rF"; //utf8; uuid of the message being replied to
                inline const std::string REPLY_BODY_CASE = "rT"; //int32; type of message this is replying to; follows ReplySpecifics::ReplyBodyCase:: enum from TypeOfChatMessage.proto
                //optional values

                //TextChatReplyMessage = 1
                inline const std::string CHAT_MESSAGE_TEXT = "rE"; //utf8; message text of message this is replying to CHAT_TEXT_MESSAGE
                //PictureReplyMessage = 2
                inline const std::string PICTURE_THUMBNAIL_IN_BYTES = "rN"; //utf8; the thumbnail of the picture or gif being replied to; used with MIME_TYPE_MESSAGE & PICTURE_MESSAGE
                inline const std::string PICTURE_THUMBNAIL_SIZE = "rS"; //int32; the thumbnail size of the picture or gif being extracted; set when CHAT_ROOM_MESSAGE_IS_REPLY_THUMBNAIL is set
                //LocationReplyMessage = 3
                //MimeTypeReplyMessage = 4
                inline const std::string MIME_TYPE_THUMBNAIL_IN_BYTES = "mN"; //utf8; the thumbnail of the picture or gif being replied to; used with MIME_TYPE_MESSAGE & PICTURE_MESSAGE
                inline const std::string MIME_TYPE_THUMBNAIL_SIZE = "mZ"; //int32; the thumbnail size of the picture or gif being extracted; set when CHAT_ROOM_MESSAGE_IS_REPLY_THUMBNAIL is set
                inline const std::string REPLY_MIME_TYPE = "mY"; //utf8; the mime type of the image; used when replying to MIME_TYPE_MESSAGE
                //InvitedReplyMessage = 5
                //none
            }

        }

        //The existence of the below fields are dependent on MESSAGE_TYPE

        //CHAT_TEXT_MESSAGE;
        inline const std::string TEXT_MESSAGE = "cM"; //utf8; will be set if was type chat, chat room name update, chat room password updated or name of user sending the invite message
        inline const std::string TEXT_IS_EDITED = "iE"; //bool; true if message has been edited, false if not (field always exists for text message types)
        inline const std::string TEXT_EDITED_TIME = "eT"; //mongoDB Date; this will be set to the edited time if TEXT_IS_EDITED == true (field always exists for text message types)
        //ACTIVE_MESSAGE_INFO_DOCUMENT

        //PICTURE_MESSAGE;
        inline const std::string PICTURE_OID = "pO"; //utf8; will be set if message type was picture (stored inside collection CHAT_MESSAGE_PICTURES_COLLECTION_NAME) or edited; IMPORTANT: this needs to remain a string instead of an OID for aggregation pipeline purposes with MESSAGE_EDITED type
        inline const std::string PICTURE_IMAGE_WIDTH = "pW"; //int32; width of the stored image; used with gif and picture message types
        inline const std::string PICTURE_IMAGE_HEIGHT = "pH"; //int32; height of the stored image; used with gif and picture message types
        //ACTIVE_MESSAGE_INFO_DOCUMENT

        //LOCATION_MESSAGE;
        inline const std::string LOCATION_LONGITUDE = "lO"; //double; will be set if type was location
        inline const std::string LOCATION_LATITUDE = "lA"; //double; will be set if type was location
        //ACTIVE_MESSAGE_INFO_DOCUMENT

        //MIME_TYPE_MESSAGE;
        inline const std::string MIME_TYPE_URL = "gU"; //utf8; url to be returned to the device
        inline const std::string MIME_TYPE_TYPE = "iM"; //utf8; mime type of image, this will be one of the types listed inside the vector accepted_mime_types
        inline const std::string MIME_TYPE_IMAGE_WIDTH = "mW"; //int32; width of the stored image; used with gif and picture message types
        inline const std::string MIME_TYPE_IMAGE_HEIGHT = "mH"; //int32; height of the stored image; used with gif and picture message types
        //ACTIVE_MESSAGE_INFO_DOCUMENT

        //INVITED_TO_CHAT_ROOM;
        inline const std::string INVITED_USER_ACCOUNT_OID = "iO"; //utf8; url to be returned to the device
        inline const std::string INVITED_USER_NAME = "nA"; //utf8; username used for new user joined
        inline const std::string INVITED_CHAT_ROOM_ID = "cI"; //utf8; used with invite to chat room
        inline const std::string INVITED_CHAT_ROOM_NAME = "cN"; //utf8; used with invite to chat room
        inline const std::string INVITED_CHAT_ROOM_PASSWORD = "cP"; //utf8; used with invite to chat room
        //ACTIVE_MESSAGE_INFO_DOCUMENT

        //MESSAGE_EDITED;
        inline const std::string EDITED_THIS_MESSAGE_UUID = "eM"; //utf8; uuid of THIS message (identical to the _id field) it is needed because _id is lost in the $group stage during an aggregation
        inline const std::string EDITED_MODIFIED_MESSAGE_UUID = "eU"; //utf8; uuid of the message this message edited
        inline const std::string EDITED_NEW_MESSAGE_TEXT = "nM"; //utf8; new message text to update the CHAT_TEXT_MESSAGE to
        inline const std::string EDITED_PREVIOUS_MESSAGE_TEXT = "pM"; //utf8; old message text CHAT_TEXT_MESSAGE held previously
        inline const std::string EDITED_MODIFIED_MESSAGE_CREATED_TIME = "mC"; //mongodb Data; date that the modified message was created

        //MESSAGE_DELETED;
        inline const std::string DELETED_THIS_MESSAGE_UUID = "dM"; //utf8; uuid of THIS message (identical to the _id field) it is needed because _id is lost in the $group stage during an aggregation
        inline const std::string DELETED_MODIFIED_MESSAGE_UUID = "dU"; //utf8; uuid of the message this message deleted
        inline const std::string DELETED_DELETED_TYPE = "dT"; //int32; follows enum DeleteType inside TypeOfChatMessage.proto, used with MESSAGE_DELETED message type
        inline const std::string DELETED_MODIFIED_MESSAGE_CREATED_TIME = "dC"; //mongodb Data; date that the modified message was created

        //USER_KICKED_FROM_CHAT_ROOM;
        inline const std::string USER_KICKED_KICKED_OID = "kO"; //utf8; oid (in string form) of the user that was kicked

        //USER_BANNED_FROM_CHAT_ROOM;
        inline const std::string USER_BANNED_BANNED_OID = "bO"; //utf8; oid (in string form) of the user that was banned

        //DIFFERENT_USER_JOINED_CHAT_ROOM;
        inline const std::string USER_JOINED_ACCOUNT_STATE = "aS"; //int32; uses enum AccountStateInChatRoom in ChatRoomCommands.proto; account state of user that joined chat room
        inline const std::string USER_JOINED_FROM_EVENT = "fE"; //string or does not exist; If this join was from a user swiping yes on an event, this field will be the event oid. If it is not from the user swiping yes on an event, the field will not exist.

        //DIFFERENT_USER_LEFT_CHAT_ROOM;
        inline const std::string USER_NEW_ACCOUNT_ADMIN_OID = "aO"; //utf8; oid (in string form) of the user that was promoted to account admin ("" if none)

        //UPDATE_CHAT_ROOM_OBSERVED_TIME;
        //THIS_USER_JOINED_CHAT_ROOM_START;
        //THIS_USER_JOINED_CHAT_ROOM_MEMBER;
        //THIS_USER_JOINED_CHAT_ROOM_FINISHED;
        //THIS_USER_LEFT_CHAT_ROOM;
        //Never Stored

        //USER_ACTIVITY_DETECTED;
        //None

        //CHAT_ROOM_NAME_UPDATED;
        inline const std::string NAME_NEW_NAME = "nN"; //utf8; new chat room name

        //CHAT_ROOM_PASSWORD_UPDATED;
        inline const std::string PASSWORD_NEW_PASSWORD = "nP"; //utf8; new chat room password

        //NEW_ADMIN_PROMOTED;
        inline const std::string NEW_ADMIN_ADMIN_ACCOUNT_OID = "nC"; //utf8; new chat room admin account oid

        //NEW_PINNED_LOCATION;
        inline const std::string PINNED_LOCATION_LONGITUDE = "pLo"; //double; longitude
        inline const std::string PINNED_LOCATION_LATITUDE = "pLa"; //double; latitude

        //MATCH_CANCELED;
        inline const std::string MATCH_CANCELED_ACCOUNT_OID = "cA"; //utf8; the account oid which is the other end of this match that this message should be sent to

        //CHAT_ROOM_CAP_MESSAGE = 21;
        //None

        //MATCH_CANCELED_MESSAGE = 500;
        //None

        //HISTORY_CLEARED = 1000;
        //LOADING_MESSAGE = 1001;
        //Client Specific
    }

}