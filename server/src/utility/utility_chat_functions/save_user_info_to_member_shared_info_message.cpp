//
// Created by jeremiah on 3/19/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <server_values.h>

#include "utility_chat_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;


//saves the info from user_account_doc to the MemberSharedInfoMessage value
//NOTE: does not save current_object_oid (requires it as a parameter)
//Requires projections found in the function buildUpdateSingleOtherUserProjectionDoc().
bool saveUserInfoToMemberSharedInfoMessage(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& user_account_collection,
        const bsoncxx::document::view& user_account_doc,
        const bsoncxx::oid& memberOID,
        MemberSharedInfoMessage* userInfo,
        HowToHandleMemberPictures requestPictures,
        const std::chrono::milliseconds& currentTimestamp
) {

    userInfo->set_current_timestamp(currentTimestamp.count());

    try {

        userInfo->set_timestamp_other_user_info_updated(
            extractFromBsoncxx_k_date(
                user_account_doc,
                user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED
            ).value.count()
        );


        userInfo->set_account_name(
            extractFromBsoncxx_k_utf8(
                    user_account_doc,
                    user_account_keys::FIRST_NAME
            )
        );

        switch (requestPictures) {
            case HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO:
                break;
            case HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO: {
                mongocxx::collection userPictures = accountsDB[collection_names::USER_PICTURES_COLLECTION_NAME];

                //set this to true if all picture info was requested for this user
                userInfo->set_pictures_checked_for_updates(true);

                const bsoncxx::array::view picturesArray = extractFromBsoncxx_k_array(
                        user_account_doc,
                        user_account_keys::PICTURES
                );

                //save member pictures
                if (!savePicturesToMemberSharedInfoMessage(
                    mongoCppClient,
                    userPictures,
                    user_account_collection,
                    currentTimestamp,
                    picturesArray,
                    user_account_doc,
                    memberOID,
                    userInfo)
                ) {
                    return false;
                }
                break;
            }
            case HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL: {
                std::string thumbnail;
                int thumbnailIndex = -1;
                std::chrono::milliseconds thumbnail_timestamp;

                auto setThumbnailValue = [&](
                        std::string& _thumbnail,
                        const int /*thumbnail_size*/,
                        const std::string& /*thumbnail_reference_oid*/,
                        const int index,
                        const std::chrono::milliseconds& _thumbnail_timestamp
                        ) {
                    thumbnail = std::move(_thumbnail);
                    thumbnailIndex = index;
                    thumbnail_timestamp = _thumbnail_timestamp;
                };

                if (!extractThumbnailFromUserAccountDoc(
                    accountsDB,
                    user_account_doc,
                    memberOID,
                    nullptr,
                    setThumbnailValue)
                ) {
                    return false;
                }

                userInfo->set_account_thumbnail(thumbnail);
                userInfo->set_account_thumbnail_size((int)thumbnail.size());
                userInfo->set_account_thumbnail_index(thumbnailIndex);
                userInfo->set_account_thumbnail_timestamp(thumbnail_timestamp.count());
                break;
            }
        }

        userInfo->set_age(
                extractFromBsoncxx_k_int32(
                        user_account_doc,
                        user_account_keys::AGE
                )
        );

        userInfo->set_gender(
                extractFromBsoncxx_k_utf8(
                        user_account_doc,
                        user_account_keys::GENDER
                )
        );

        userInfo->set_city_name(
                extractFromBsoncxx_k_utf8(
                        user_account_doc,
                        user_account_keys::CITY
                )
        );

        userInfo->set_bio(
                extractFromBsoncxx_k_utf8(
                        user_account_doc,
                        user_account_keys::BIO
                )
        );

        userInfo->set_account_type(
                UserAccountType(
                        extractFromBsoncxx_k_int32(
                                user_account_doc,
                                user_account_keys::ACCOUNT_TYPE
                        )
                )
        );

        const std::chrono::milliseconds event_expiration_time =
                extractFromBsoncxx_k_date(
                        user_account_doc,
                        user_account_keys::EVENT_EXPIRATION_TIME
                ).value;

        const LetsGoEventStatus event_status = convertExpirationTimeToEventStatus(
                event_expiration_time,
                currentTimestamp
                );

        userInfo->set_letsgo_event_status(event_status);

        //event_values document (it may not exist)
        const auto event_values_document = user_account_doc[user_account_keys::EVENT_VALUES];
        if (event_values_document && event_values_document.type() == bsoncxx::type::k_document) {
            const bsoncxx::document::view event_values_doc = event_values_document.get_document().value;

            const std::string created_by = extractFromBsoncxx_k_utf8(
                    event_values_doc,
                    user_account_keys::event_values::CREATED_BY
            );

            if(isInvalidOIDString(created_by)) { //admin
                //no need to send back the admin name
                userInfo->set_created_by(general_values::APP_NAME);
            } else { //user oid
                userInfo->set_created_by(created_by);
            }

            userInfo->set_event_title(
                    extractFromBsoncxx_k_utf8(
                            event_values_doc,
                            user_account_keys::event_values::EVENT_TITLE
                    )
            );
        } else if(event_values_document)  { //if EVENT_VALUES is not type document
            logElementError(
                __LINE__, __FILE__,
                event_values_document, user_account_doc,
                bsoncxx::type::k_document, user_account_keys::EVENT_VALUES,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            return false;
        }

        const bsoncxx::array::view categories_array =
                extractFromBsoncxx_k_array(
                        user_account_doc,
                        user_account_keys::CATEGORIES
                );

        if (!saveActivitiesToMemberSharedInfoMessage(
                memberOID,
                categories_array,
                userInfo,
                user_account_doc)
        ) {
            return false;
        }

    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return false;
    }

    return true;
}
