//
// Created by jeremiah on 5/5/21.
//

#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility_general_functions.h>
#include <store_mongoDB_error_and_exception.h>
#include <deleted_chat_message_pictures_keys.h>

#include "utility_chat_functions.h"

#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "chat_message_pictures_keys.h"
#include "chat_room_message_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool invalidOrCorruptFile(
        mongocxx::database& chat_room_db,
        mongocxx::collection& chat_message_pictures_collection,
        const bsoncxx::document::view& match_picture_doc_view,
        const std::string& chat_room_id,
        const std::string& picture_oid_string,
        const std::string& message_uuid
        ) {

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    std::optional<std::string> update_chat_room_message_exception_string;
    bsoncxx::stdx::optional<mongocxx::result::update> update_chat_room_message;
    try {

        update_chat_room_message = chat_room_collection.update_one(
                document{}
                << "_id" << message_uuid
                << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kPictureMessage
                << finalize,
                document{}
                    << "$set" << open_document
                        << std::string(chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT).append(".").append(chat_room_message_keys::message_specifics::PICTURE_OID) << ""
                        << std::string(chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT).append(".").append(chat_room_message_keys::message_specifics::PICTURE_IMAGE_WIDTH) << bsoncxx::types::b_int32{0}
                        << std::string(chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT).append(".").append(chat_room_message_keys::message_specifics::PICTURE_IMAGE_HEIGHT) << bsoncxx::types::b_int32{0}
                    << close_document
                << finalize);
    }
    catch (const mongocxx::logic_error& e) {
        update_chat_room_message_exception_string = e.what();
    }

    //not checking modified_count here, several threads could be attempting this update at the same time
    if(!update_chat_room_message) {

        std::string error_string = "Updating a chat room picture type message failed";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      update_chat_room_message_exception_string, error_string,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME,
                                      "picture_oid_used", picture_oid_string,
                                      "message_uuid", message_uuid);

        return false;
    }

    std::optional<std::string> merge_message_picture_exception_string;
    mongocxx::stdx::optional<mongocxx::cursor> merge_result;
    try {

        mongocxx::pipeline pipeline;

        pipeline.match(match_picture_doc_view);

        pipeline.add_fields(
                document{}
                << deleted_chat_message_pictures_keys::TIMESTAMP_REMOVED << bsoncxx::types::b_date{getCurrentTimestamp()}
                << finalize
                );

        pipeline.merge(document{}
            << "into" << open_document
                << "db" << database_names::DELETED_DATABASE_NAME
                << "coll" << collection_names::DELETED_CHAT_MESSAGE_PICTURES_COLLECTION_NAME
            << close_document
            << "on" << "_id"
        << finalize);

        merge_result = chat_message_pictures_collection.aggregate(pipeline);
    }
    catch (const mongocxx::logic_error& e) {
        merge_message_picture_exception_string = e.what();
    }

    if (!merge_result) {

        std::string error_string = "Moving a chat message picture document to the 'deleted' collection failed";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      merge_message_picture_exception_string, error_string,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME,
                                      "picture_oid_used", picture_oid_string,
                                      "message_uuid", message_uuid);

        return false;
    }

    //NOTE: must use the merge_result in the code or the aggregation could simply not happen
    for (const auto& doc : *merge_result) {
        std::string error_string = "$merge pipeline stage returned a result which should never happen.";
        std::optional<std::string> dummy_string;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummy_string, error_string,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME,
                                      "ObjectID picture_oid_used", picture_oid_string,
                                      "message_uuid", message_uuid,
                                      "doc", doc
                                      );

        //NOTE: ok to continue here
    }

    std::optional<std::string> delete_message_picture_exception_string;
    mongocxx::stdx::optional<mongocxx::result::delete_result> delete_result;
    try {
        delete_result = chat_message_pictures_collection.delete_one(match_picture_doc_view);
    }
    catch (const mongocxx::logic_error& e) {
        delete_message_picture_exception_string = e.what();
    }

    //not checking modified_count here, several threads could be attempting this update at the same time
    if (!delete_result) {
        std::string error_string = "Deleting a chat message picture document failed";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      delete_message_picture_exception_string, error_string,
                                      "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                      "collection", collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME,
                                      "picture_oid_used", picture_oid_string,
                                      "message_uuid", message_uuid);

        return false;
    }

    return true;
}

bool extractChatPicture(const mongocxx::client& mongoCppClient, const std::string& pictureOIDString,
                        const std::string& chatRoomId, const std::string& userAccountOIDStr,
                        const std::string& message_uuid,
                        const std::function<void(const int /*pictureSize*/,
                                                 const int /*pictureHeight*/, const int /*pictureWidth*/,
                                                 std::string& /*pictureByteString*/)>& pictureSuccessfullyExtracted,
                        const std::function<void()>& pictureNotFoundOrCorrupt) {

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];
    mongocxx::collection chat_message_pictures_collection = chatRoomDB[collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME];

    //this can happen if the picture was corrupted, the OID will be set to null
    if (isInvalidOIDString(pictureOIDString)) {
        pictureNotFoundOrCorrupt();
        return true;
    }

    bsoncxx::document::value match_picture_doc = document{}
            << "_id" << bsoncxx::oid{pictureOIDString}
            << chat_message_pictures_keys::CHAT_ROOM_ID << bsoncxx::types::b_string{chatRoomId}
        << finalize;

    std::optional<std::string> findChatPictureExceptionString;
    bsoncxx::stdx::optional<bsoncxx::document::value> findChatMessagePicture;
    try {

        //find picture and make sure it is a part of the passed chat room
        findChatMessagePicture = chat_message_pictures_collection.find_one(match_picture_doc.view());
    }
    catch (const mongocxx::logic_error& e) {
        findChatPictureExceptionString = e.what();
    }

    if (findChatMessagePicture) { //if picture found

        bsoncxx::document::view pictureDoc = *findChatMessagePicture;

        std::string pictureByteString;
        int pictureHeight;
        int pictureWidth;
        int pictureSize;

        auto pictureSizeElement = pictureDoc[chat_message_pictures_keys::PICTURE_SIZE_IN_BYTES];
        if (pictureSizeElement &&
            pictureSizeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            pictureSize = pictureSizeElement.get_int32().value;
        }
        else { //if element does not exist or is not type int32

            bsoncxx::builder::stream::document pictureDocumentClone;

            pictureDocumentClone
                    << "message"
                    << "This document contains the picture in bytes and therefore cannot be displayed."
                    << "document oid" << pictureOIDString
                    << "user oid" << userAccountOIDStr
                    << "chatRoomId" << chatRoomId
                    << "failed element" << pictureSizeElement.get_value();

            bsoncxx::document::view pictureDocumentCloneView = pictureDocumentClone.view();

            logElementError(__LINE__, __FILE__,
                            pictureSizeElement,
                            pictureDocumentCloneView, bsoncxx::type::k_int32,
                            chat_message_pictures_keys::PICTURE_SIZE_IN_BYTES,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME);

            bool return_val = invalidOrCorruptFile(
                    chatRoomDB,
                    chat_message_pictures_collection,
                    match_picture_doc.view(),
                    chatRoomId,
                    pictureOIDString,
                    message_uuid
                    );

            if(!return_val)
                return false;

            pictureNotFoundOrCorrupt();

            return true;
        }

        auto pictureHeightElement = pictureDoc[chat_message_pictures_keys::HEIGHT];
        if (pictureHeightElement &&
            pictureHeightElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            pictureHeight = pictureHeightElement.get_int32().value;
        }
        else { //if element does not exist or is not type int32

            bsoncxx::builder::stream::document pictureDocumentClone;

            pictureDocumentClone
                    << "message"
                    << "This document contains the picture in bytes and therefore cannot be displayed."
                    << "document oid" << pictureOIDString
                    << "user oid" << userAccountOIDStr
                    << "chatRoomId" << chatRoomId
                    << "failed element" << pictureSizeElement.get_value();

            bsoncxx::document::view pictureDocumentCloneView = pictureDocumentClone.view();

            logElementError(__LINE__, __FILE__,
                            pictureSizeElement,
                            pictureDocumentCloneView, bsoncxx::type::k_int32, chat_message_pictures_keys::HEIGHT,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME);

            bool return_val = invalidOrCorruptFile(
                    chatRoomDB,
                    chat_message_pictures_collection,
                    match_picture_doc.view(),
                    chatRoomId,
                    pictureOIDString,
                    message_uuid
                    );

            if(!return_val)
                return false;

            pictureNotFoundOrCorrupt();

            return true;
        }

        auto pictureWidthElement = pictureDoc[chat_message_pictures_keys::WIDTH];
        if (pictureWidthElement &&
            pictureWidthElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            pictureWidth = pictureWidthElement.get_int32().value;
        }
        else { //if element does not exist or is not type int32

            bsoncxx::builder::stream::document pictureDocumentClone;

            pictureDocumentClone
                    << "message"
                    << "This document contains the picture in bytes and therefore cannot be displayed."
                    << "document oid" << pictureOIDString
                    << "user oid" << userAccountOIDStr
                    << "chatRoomId" << chatRoomId
                    << "failed element" << pictureSizeElement.get_value();

            bsoncxx::document::view pictureDocumentCloneView = pictureDocumentClone.view();

            logElementError(__LINE__, __FILE__,
                            pictureSizeElement,
                            pictureDocumentCloneView, bsoncxx::type::k_int32, chat_message_pictures_keys::HEIGHT,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME);

            bool return_val = invalidOrCorruptFile(
                    chatRoomDB,
                    chat_message_pictures_collection,
                    match_picture_doc.view(),
                    chatRoomId,
                    pictureOIDString,
                    message_uuid
                    );

            if(!return_val)
                return false;

            pictureNotFoundOrCorrupt();

            return true;
        }

        auto pictureByteStringElement = pictureDoc[chat_message_pictures_keys::PICTURE_IN_BYTES];
        if (pictureByteStringElement &&
            pictureByteStringElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8

            //NOTE: moving the temporary object prevents copy elision, so don't std::move here
            pictureByteString = pictureByteStringElement.get_string().value.to_string();
        }
        else { //if element does not exist or is not type utf8

            bsoncxx::builder::stream::document pictureDocumentClone;

            pictureDocumentClone
                    << "message"
                    << "This document contains the picture in bytes and therefore cannot be displayed."
                    << "document oid" << pictureOIDString
                    << "user oid" << userAccountOIDStr
                    << "chatRoomId" << chatRoomId;

            bsoncxx::document::view pictureDocumentCloneView = pictureDocumentClone.view();

            logElementError(__LINE__, __FILE__,
                            pictureSizeElement,
                            pictureDocumentCloneView, bsoncxx::type::k_utf8, chat_message_pictures_keys::PICTURE_IN_BYTES,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME);

            bool return_val = invalidOrCorruptFile(
                    chatRoomDB,
                    chat_message_pictures_collection,
                    match_picture_doc.view(),
                    chatRoomId,
                    pictureOIDString,
                    message_uuid
                    );

            if(!return_val)
                return false;

            pictureNotFoundOrCorrupt();

            return true;
        }

        if ((int)pictureByteString.size() == pictureSize) { //if file has not been corrupted
            pictureSuccessfullyExtracted(pictureSize, pictureHeight, pictureWidth, pictureByteString);
        }
        else { //if file has been corrupted
            std::stringstream error_string_stream;
            error_string_stream
            << "A picture message file stored inside of the database was found to be corrupted\n"
            << "pictureByteString: " << pictureByteString.size()
            << "\npictureSize: " << pictureSize << '\n';

            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          findChatPictureExceptionString, error_string_stream.str(),
                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                          "collection", collection_names::CHAT_MESSAGE_PICTURES_COLLECTION_NAME,
                                          "picture_oid_used", pictureOIDString,
                                          "message_uuid", message_uuid);

            bool return_val = invalidOrCorruptFile(
                    chatRoomDB,
                    chat_message_pictures_collection,
                    match_picture_doc.view(),
                    chatRoomId,
                    pictureOIDString,
                    message_uuid
                    );

            if(!return_val)
                return false;

            pictureNotFoundOrCorrupt();
        }

    } else { //if picture not found (or the picture was not a part of the passed chat room)

        std::string errStr = "Error occurred when finding chat message picture with passed chat room.";

        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      findChatPictureExceptionString, errStr,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                      "oid_used", userAccountOIDStr,
                                      "chat_room_id", chatRoomId,
                                      "picture_oid", pictureOIDString);

        pictureNotFoundOrCorrupt();
    }

    return true;
}