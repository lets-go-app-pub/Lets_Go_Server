//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/exception/logic_error.hpp>
#include <deleted_thumbnail_info.h>
#include <session_to_run_functions.h>

#include "extract_thumbnail_from_verified_doc.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "user_pictures_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//extracts thumbnail from first element of 'PICTURES' array and sets it to passed thumbnail string
// will also update the picture based on update_doc if present
//returns false if an error occurs true if no error occurs; stores all errors
//requires PICTURES to be projected to userAccountDocView
//pass nullptr to session if not needed
bool extractThumbnailFromUserAccountDoc(
        mongocxx::database& accountsDB,
        const bsoncxx::document::view& userAccountDocView,
        const bsoncxx::oid& userAccountOID,
        mongocxx::client_session* session,
        const std::function<void(
                std::string& /*thumbnail*/,
                const int /*thumbnail_size*/,
                const std::string& /*thumbnail_reference_oid*/,
                const int /*index*/,
                const std::chrono::milliseconds& /*timestamp*/
        )>& moveThumbnailToResult,
        const ExtractThumbnailAdditionalCommands& additionalCommands
) {

    bsoncxx::array::view picturesArray;

    auto picturesArrayElement = userAccountDocView[user_account_keys::PICTURES];
    if (picturesArrayElement &&
        picturesArrayElement.type() == bsoncxx::type::k_array) { //if element exists and is type utf8
        picturesArray = picturesArrayElement.get_array().value;
    } else { //if element does not exist or is not type utf8
        logElementError(
                __LINE__, __FILE__,
                picturesArrayElement, userAccountDocView,
                bsoncxx::type::k_array, user_account_keys::PICTURES,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        return false;
    }

    int indexValue = 0;
    bsoncxx::oid thumbnailOid;
    bool pictureFound = false;
    for (const auto& pictureDoc : picturesArray) {
        if (pictureDoc.type() == bsoncxx::type::k_document) {
            pictureFound = true;
            bsoncxx::document::view pictureInfo = pictureDoc.get_document().value;

            auto pictureOidElement = pictureInfo[user_account_keys::pictures::OID_REFERENCE];
            if (pictureOidElement
                && pictureOidElement.type() == bsoncxx::type::k_oid) { //if element exists and is type oid

                thumbnailOid = pictureOidElement.get_oid().value;

                mongocxx::collection userPictures = accountsDB[collection_names::USER_PICTURES_COLLECTION_NAME];

                std::optional <std::string> findPictureExceptionString;
                bsoncxx::stdx::optional <bsoncxx::document::value> findPictureDocumentValue;
                try {

                    bsoncxx::builder::stream::document projection_doc{};

                    projection_doc
                        << "_id" << 0
                        << user_pictures_keys::TIMESTAMP_STORED << 1
                        << user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES << 1;

                    if(!additionalCommands.getOnlyRequestThumbnailSize()) {
                        projection_doc
                            << user_pictures_keys::THUMBNAIL_IN_BYTES << 1;
                    }

                    switch (additionalCommands.getCommand()) {
                        case ExtractThumbnailAdditionalCommands::THUMB_ADDITIONAL_DO_NOTHING: {
                            mongocxx::options::find opts;

                            opts.projection(projection_doc.view());

                            findPictureDocumentValue = find_one_optional_session(
                                        session,
                                        userPictures,
                                        document{}
                                            << "_id" << thumbnailOid
                                        << finalize,
                                        opts
                                    );
                            break;
                        }
                        case ExtractThumbnailAdditionalCommands::THUMB_ADDITIONAL_ADD: {
                            mongocxx::options::find_one_and_update opts;

                            opts.projection(projection_doc.view());

                            findPictureDocumentValue = find_one_and_update_optional_session(
                                session,
                                userPictures,
                                document{}
                                    << "_id" << thumbnailOid
                                << finalize,
                                document{}
                                    << "$addToSet" << open_document
                                        << user_pictures_keys::THUMBNAIL_REFERENCES << additionalCommands.getChatRoomId()
                                    << close_document
                                << finalize,
                                opts
                            );

                            break;
                        }
                        case ExtractThumbnailAdditionalCommands::THUMB_ADDITIONAL_ADD_LIST: {
                            mongocxx::options::find_one_and_update opts;

                            opts.projection(projection_doc.view());

                            findPictureDocumentValue = find_one_and_update_optional_session(
                                        session,
                                        userPictures,
                                        document{}
                                            << "_id" << thumbnailOid
                                        << finalize,
                                        document{}
                                            << "$addToSet" << open_document
                                                << user_pictures_keys::THUMBNAIL_REFERENCES << open_document
                                                    << "$each" << additionalCommands.getChatRoomIdsList()
                                                << close_document
                                            << close_document
                                        << finalize,
                                        opts
                                    );
                            break;
                        }
                        case ExtractThumbnailAdditionalCommands::THUMB_ADDITIONAL_UPDATE: {

                            mongocxx::options::find_one_and_update opts;

                            opts.projection(projection_doc.view());

                            findPictureDocumentValue = find_one_and_update_optional_session(
                                        session,
                                        userPictures,
                                        document{}
                                            << "_id" << thumbnailOid
                                        << finalize,
                                        document{}
                                            << "$addToSet" << open_document
                                                << user_pictures_keys::THUMBNAIL_REFERENCES << additionalCommands.getChatRoomId()
                                            << close_document
                                        << finalize,
                                        opts
                                    );

                            //if a previous reference was used AND it is valid, run the function, if the function returns
                            // false, this function returns false as well
                            if(!additionalCommands.getThumbnailReferenceOid().empty()
                                && thumbnailOid.to_string() != additionalCommands.getThumbnailReferenceOid()
                                && !removeChatRoomIdFromUserPicturesReference(
                                        accountsDB, bsoncxx::oid{additionalCommands.getThumbnailReferenceOid()},
                                        additionalCommands.getChatRoomId(), session
                                    )
                            ) {
                                return false;
                            }

                            break;
                        }
                    }
                }
                catch (const mongocxx::logic_error& e) {
                    findPictureExceptionString = e.what();
                }

                if (findPictureDocumentValue) { //find picture succeeded

                    bsoncxx::document::view pictureDocumentView = findPictureDocumentValue->view();

                    auto errorLambda = [&](const std::string& errorString) {
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), errorString,
                                "picture_oid", thumbnailOid,
                                "ObjectID_used", userAccountOID,
                                "verified_account_doc", userAccountDocView,
                                "picture_element_doc", pictureInfo);
                    };

                    std::string thumbnail;
                    int thumbnail_size;
                    std::chrono::milliseconds thumbnail_timestamp;

                    if(additionalCommands.getOnlyRequestThumbnailSize()) {
                        auto thumbnail_size_element = pictureDocumentView[user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES];
                        if (thumbnail_size_element &&
                            thumbnail_size_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                            thumbnail_size = thumbnail_size_element.get_int32().value;
                        } else { //if element does not exist or is not type int32
                            logElementError(__LINE__, __FILE__, thumbnail_size_element,
                                            pictureDocumentView, bsoncxx::type::k_int32, user_pictures_keys::THUMBNAIL_SIZE_IN_BYTES,
                                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

                            return false;
                        }
                    } else {

                        //extract from pictureDocumentView and save to thumbnail
                        if (!extractThumbnailFromUserPictureDocument(
                                pictureDocumentView,
                                thumbnail,
                                errorLambda)
                                ) {
                            //error already stored
                            return false;
                        }

                        thumbnail_size = (int)thumbnail.size();
                    }

                    auto timestamp_element = pictureDocumentView[user_pictures_keys::TIMESTAMP_STORED];
                    if (timestamp_element
                        && timestamp_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                        thumbnail_timestamp = timestamp_element.get_date().value;
                    } else { //if element does not exist or is not type int32
                        logElementError(__LINE__, __FILE__, timestamp_element,
                                        pictureDocumentView, bsoncxx::type::k_date, user_pictures_keys::TIMESTAMP_STORED,
                                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_PICTURES_COLLECTION_NAME);

                        return false;
                    }

                    moveThumbnailToResult(
                        thumbnail,
                        thumbnail_size,
                        thumbnailOid.to_string(),
                        indexValue,
                        thumbnail_timestamp
                    );
                }
                else { //find picture failed
                    const std::string error_string = "Failed to find picture document when it was shown to exist inside user account.\n";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            findPictureExceptionString, error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_PICTURES_COLLECTION_NAME,
                            "ObjectID_used", userAccountOID,
                            "verified_account_doc", userAccountDocView,
                            "picture_element_doc", pictureInfo
                    );
                    return false;
                }

                break;
            }
            else { //if element does not exist or is not type oid
                logElementError(
                        __LINE__, __FILE__,
                        pictureOidElement, pictureInfo,
                        bsoncxx::type::k_oid, user_account_keys::pictures::OID_REFERENCE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                return false;
            }

        } else if (pictureDoc.type() != bsoncxx::type::k_null) {
            logElementError(
                    __LINE__, __FILE__,
                    picturesArrayElement, userAccountDocView,
                    bsoncxx::type::k_null, user_account_keys::pictures::OID_REFERENCE,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            return false;
        }
        indexValue++;
    }

    //if no pictures are found, return message to delete thumbnail on client
    if (!pictureFound) { //no picture (and therefore no thumbnail) was found

        std::string thumbnail_copy = DeletedThumbnailInfo::thumbnail;

        moveThumbnailToResult(
                thumbnail_copy,
                (int)DeletedThumbnailInfo::thumbnail.size(),
                DeletedThumbnailInfo::thumbnail_reference_oid,
                DeletedThumbnailInfo::index,
                DeletedThumbnailInfo::deleted_thumbnail_timestamp
                );
    }

    return true;

}
