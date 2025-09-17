//
// Created by jeremiah on 8/24/22.
//

#include <optional>
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include "utility_chat_functions.h"

#include "chat_room_header_keys.h"
#include "database_names.h"
#include "deleted_thumbnail_info.h"
#include "collection_names.h"
#include "user_pictures_keys.h"
#include "deleted_user_pictures_keys.h"
#include "store_mongoDB_error_and_exception.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool removePictureFromHeader(
        mongocxx::client& mongo_cpp_client,
        mongocxx::collection& chat_room_collection,
        const std::chrono::milliseconds& current_timestamp,
        const std::string& member_account_oid_string,
        const std::string& thumbnail_reference
        ) {

    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    std::optional<std::string> exception_string;
    mongocxx::stdx::optional<mongocxx::result::update> update_results;
    try {

        mongocxx::options::update update_opts;

        const std::string ELEM = "e";
        bsoncxx::builder::basic::array arrayBuilder{};
        arrayBuilder.append(
                document{}
                        << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << bsoncxx::oid{member_account_oid_string}
                        << finalize
        );

        update_opts.array_filters(arrayBuilder.view());

        const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";

        update_results = chat_room_collection.update_one(
                document{}
                        << "_id" << chat_room_header_keys::ID
                << finalize,
                document{}
                        << "$set" << open_document
                        << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << 0
                        << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                        << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::types::b_string{""}
                        << close_document
                << finalize,
                update_opts
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, std::string(e.what()),
                "collection_name", chat_room_collection.name().to_string(),
                "thumbnail_reference", thumbnail_reference,
                "chatRoomCollectionName", chat_room_collection.name().to_string()
        );
        return false;
    }

    if(!update_results || update_results->matched_count() == 0) {
        const std::string update_error_string = "Chat room header was not found (could be a bad chatRoomCollectionName).\n";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, update_error_string,
                "thumbnail_reference", thumbnail_reference,
                "chatRoomCollectionName", chat_room_collection.name().to_string()
        );

        //can continue here
    }

    return true;
}

bool extractThumbnailFromHeaderAccountDocument(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& chat_room_collection,
        const bsoncxx::document::view& user_from_chat_room_header_doc_view,
        const std::chrono::milliseconds& current_timestamp,
        MemberSharedInfoMessage* mutable_user_info,
        const std::string& member_account_oid_string,
        const int thumbnail_size_from_header,
        const std::chrono::milliseconds& thumbnail_timestamp_from_header
        ) {

    std::string thumbnail_reference;

    auto thumbnail_reference_element = user_from_chat_room_header_doc_view[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE];
    if (thumbnail_reference_element && thumbnail_reference_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type date
        thumbnail_reference = thumbnail_reference_element.get_string().value.to_string();
    } else { //if element does not exist or is not type date
        logElementError(__LINE__, __FILE__, thumbnail_reference_element,
                        user_from_chat_room_header_doc_view, bsoncxx::type::k_utf8,
                        chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string());
        return false;
    }

    if(thumbnail_size_from_header <= (int)DeletedThumbnailInfo::thumbnail.size()
        || isInvalidOIDString(thumbnail_reference)) { //thumbnail has been deleted
        DeletedThumbnailInfo::saveDeletedThumbnailInfo(mutable_user_info);
    }
    else {

        mongocxx::collection user_pictures_collection = accounts_db[collection_names::USER_PICTURES_COLLECTION_NAME];

        mongocxx::options::find opts;

        opts.projection(
            document{}
                << "_id" << 0
                << user_pictures_keys::THUMBNAIL_IN_BYTES << 1
                << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << 1
            << finalize
        );

        bsoncxx::stdx::optional<bsoncxx::document::value> user_picture;
        try {
            user_picture = user_pictures_collection.find_one(
                document{}
                    << "_id" << bsoncxx::oid{thumbnail_reference}
                    << finalize,
                opts
            );
        } catch (const mongocxx::logic_error& e) {
            std::optional<std::string> dummyExceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    dummyExceptionString,std::string(e.what()),
                    "collection_name", user_pictures_collection.name().to_string(),
                    "thumbnail_reference", thumbnail_reference,
                    "chatRoomCollectionName", chat_room_collection.name().to_string()
                    );
            return false;
        }

        if(!user_picture) { //check the deleted user pictures collection
            mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
            mongocxx::collection deleted_user_pictures_collection = deleted_db[collection_names::DELETED_USER_PICTURES_COLLECTION_NAME];

            try {
                user_picture = deleted_user_pictures_collection.find_one(
                    document{}
                        << "_id" << bsoncxx::oid{thumbnail_reference}
                        << deleted_user_pictures_keys::REFERENCES_REMOVED_AFTER_DELETE << open_document
                            << "$ne" << true
                        << close_document
                    << finalize,
                    opts
                );
            } catch (const mongocxx::logic_error& e) {
                std::optional<std::string> dummyExceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        dummyExceptionString,std::string(e.what()),
                        "collection_name", deleted_user_pictures_collection.name().to_string(),
                        "thumbnail_reference", thumbnail_reference,
                        "chatRoomCollectionName", chat_room_collection.name().to_string()
                );
                return false;
            }
        }

        if(!user_picture) { //user picture does not exist inside deleted collection either (was deleted by an admin)

            //This could happen if the picture was being deleted by an admin after the header doc was extracted. It
            // shouldn't happen much.
            //NOTE: In case a different thread is processing this, DON'T want to update it by calling
            // removePictureFromHeader().
            const std::string error_string = "The thumbnail reference stored inside the chat room header was not found.\n";

            std::optional<std::string> dummyExceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    dummyExceptionString,error_string,
                    "thumbnail_reference", thumbnail_reference,
                    "chatRoomCollectionName", chat_room_collection.name().to_string()
            );

            DeletedThumbnailInfo::saveDeletedThumbnailInfo(mutable_user_info);
        }
        else { //user picture exists

            bsoncxx::document::view user_picture_view = user_picture->view();

            std::string extracted_thumbnail_in_bytes;
            size_t extracted_thumbnail_size;

            try {
                extracted_thumbnail_in_bytes = extractFromBsoncxx_k_utf8(
                        user_picture_view,
                        user_pictures_keys::THUMBNAIL_IN_BYTES
                );

                extracted_thumbnail_size = (size_t)extractFromBsoncxx_k_int32(
                        user_picture_view,
                        user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES
                );
            } catch (const ErrorExtractingFromBsoncxx& e) {
                std::optional<std::string> dummyExceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        dummyExceptionString,std::string(e.what()),
                        "thumbnail_reference", thumbnail_reference,
                        "chatRoomCollectionName", chat_room_collection.name().to_string()
                );
                return false;
            }

            if(extracted_thumbnail_in_bytes.size() == extracted_thumbnail_size) {
                mutable_user_info->set_account_thumbnail_size((int)extracted_thumbnail_size);
                mutable_user_info->set_account_thumbnail_timestamp(thumbnail_timestamp_from_header.count());
                mutable_user_info->set_account_thumbnail(std::move(extracted_thumbnail_in_bytes));
                mutable_user_info->set_account_thumbnail_index(0);
            } else { //file corrupted

                const std::string error_string = "The thumbnail stored inside the database was corrupt.\n";

                std::optional<std::string> dummy_exception_string;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        dummy_exception_string, error_string,
                        "thumbnail_reference", thumbnail_reference,
                        "chatRoomCollectionName", chat_room_collection.name().to_string()
                );

                if(!removePictureFromHeader(
                    mongo_cpp_client,
                    chat_room_collection,
                    current_timestamp,
                    member_account_oid_string,
                    thumbnail_reference)
                ) {
                    return false;
                }

                DeletedThumbnailInfo::saveDeletedThumbnailInfo(mutable_user_info);
            }
        }
    }

    return true;
}