
#include <bsoncxx/builder/stream/document.hpp>

#include <utility_chat_functions.h>
#include <extract_thumbnail_from_verified_doc.h>
#include <server_values.h>
#include "update_single_other_user.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "user_account_keys.h"
#include "general_values.h"
#include "deleted_thumbnail_info.h"
#include "deleted_picture_info.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** Documentation related to this file can be found inside
 * [grpc_functions/chat_room_commands/request_information/_documentation.md] **/

enum UserRequiresUpdatedReturn {
    ERROR_USER_REQUIRES_UPDATE,
    USER_REQUIRES_UPDATED,
    USER_DOES_NOT_REQUIRES_UPDATE
};

UserRequiresUpdatedReturn checkIfBasicUserInfoRequiresUpdated(
        const bsoncxx::document::view& user_account_doc,
        const std::chrono::milliseconds& current_timestamp,
        const OtherUserInfoForUpdates& user_info_from_client
);

//Update a single chat room member, passed parameters are in memberFromClient, will be
// stored inside a response called from responseFunction.
//Works closely with insertOrUpdateOtherUser() in Android.
void updateSingleOtherUser(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        const bsoncxx::document::view& user_account_doc,
        mongocxx::collection& user_accounts_collection,
        const std::string& chat_room_collection_name,
        const std::string& user_oid_string,
        const OtherUserInfoForUpdates& user_info_from_client,
        const std::chrono::milliseconds& current_timestamp,
        bool always_set_response_message,
        AccountStateInChatRoom account_state_from_chat_room,
        const std::chrono::milliseconds& user_last_activity_time,
        HowToHandleMemberPictures how_to_handle_member_pictures,
        UpdateOtherUserResponse* response,
        const std::function<void()>& response_function
) {

    bsoncxx::oid user_account_oid = bsoncxx::oid{user_oid_string};

    bool update_account_state = false;
    bool update_user_info_besides_pictures;
    bool picture_was_updated = false;
    bool thumbnail_was_updated = false;

    //UpdateOtherUserResponse response;
    response->set_timestamp_returned(current_timestamp.count());
    response->set_account_last_activity_time(user_last_activity_time.count());

    const UserRequiresUpdatedReturn user_requires_update_return = checkIfBasicUserInfoRequiresUpdated(
            user_account_doc,
            current_timestamp,
            user_info_from_client
    );

    switch (user_requires_update_return) {
        case ERROR_USER_REQUIRES_UPDATE:
            //Error was already stored
            return;
        case USER_REQUIRES_UPDATED:
            update_user_info_besides_pictures = true;
            break;
        case USER_DOES_NOT_REQUIRES_UPDATE:
            update_user_info_besides_pictures = false;
            break;
    }

    bsoncxx::array::view picture_timestamps_array;

    //extract member picture locations
    auto pictures_element = user_account_doc[user_account_keys::PICTURES];
    if (pictures_element
        && pictures_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
        picture_timestamps_array = pictures_element.get_array().value;
    } else { //if element does not exist or is not type array
        logElementError(
            __LINE__, __FILE__,
            pictures_element, user_account_doc,
            bsoncxx::type::k_array, user_account_keys::PICTURES,
            database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection_name
        );
        return;
    }

    //this vector will be filled with all timestamps from the chat room, it will end up as size NUMBER_PICTURES_STORED_PER_ACCOUNT
    // any unused index will be set to -1
    std::vector<std::pair<std::string, std::chrono::milliseconds>> server_member_picture_timestamps;
    std::set<int> picture_indexes_used;
    const auto default_value = std::chrono::milliseconds{-1L};

    //std::copy(serverMemberPictureTimestamps)
    if (!extractPicturesToVectorFromChatRoom(
            mongo_cpp_client,
            user_accounts_collection,
            picture_timestamps_array,
            user_account_doc,
            user_account_oid,
            current_timestamp,
            server_member_picture_timestamps,
            picture_indexes_used)
            ) {
        //error has been stored already
        response->set_return_status(ReturnStatus::LG_ERROR);
        response_function();
        return;
    }

    while (server_member_picture_timestamps.size() < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {
        //fill up the vector to the max possible size
        server_member_picture_timestamps.emplace_back(
                "",
                default_value
        );
    }

    if (user_info_from_client.pictures_last_updated_timestamps_size() > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {

        std::string error_string = "Pictures array sent by the client was larger than is possible\n";
        error_string += "serverMemberPictureTimestamps:\n";
        for (size_t i = 0; i < server_member_picture_timestamps.size(); i++) {
            error_string += "[" + std::to_string(i) + "]: objectOID: " + server_member_picture_timestamps[i].first +
                            " timestamp: " + getDateTimeStringFromTimestamp(server_member_picture_timestamps[i].second) + "\n";
        }

        error_string += "serverMemberPictureTimestamps:\n";
        int index = 0;
        for (const auto& pic : user_info_from_client.pictures_last_updated_timestamps()) {
            error_string +=
                    "[" + std::to_string(index) + "]: index_number: " + std::to_string(pic.index_number()) +
                    " last_updated_timestamp: " + std::to_string(pic.index_number()) + "\n";
            index++;
        }

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection_name,
                "ObjectID_used", user_oid_string
        );

    }

    //I extract all timestamps & picture oid to serverMemberPictureTimestamps from matches' user account document.
    // If serverMemberPictureTimestamps does not have enough I fill it (it will always be NUMBER_PICTURES_STORED_PER_ACCOUNT size).
    // I extract all index used by the server to pictureIndexesUsed.
    // Iterates through client picture index.
    // If no index exists at location on server, add to indexToBeUpdated.
    // If client picture timestamp less than server timestamp, add to indexToBeUpdated.
    // Store any 'leftover' pictures the client did not 'know' about to indexToBeUpdated.
    // Finds the first element and returns that as the thumbnail.

    //NOTE: When requesting thumbnail, ONLY need to compare the first active element of the server.

    std::set<int> index_to_be_updated;

    auto store_index_to_be_updated = [&](int index) {

        //store this index to be updated
        auto inserted_result = index_to_be_updated.insert(index);

        if (!inserted_result.second) { //upsert failed because a duplicate exists

            std::string error_string = "A duplicate index was returned.\n";
            error_string += "serverMemberPictureTimestamps:\n";
            for (size_t i = 0; i < server_member_picture_timestamps.size(); i++) {
                error_string += "[" + std::to_string(i) + "]: objectOID: " + server_member_picture_timestamps[i].first +
                                " timestamp: " + getDateTimeStringFromTimestamp(server_member_picture_timestamps[i].second) +
                                "\n";
            }

            error_string += "indexToBeUpdated:\n";
            for (int upIndex : index_to_be_updated) {
                error_string += std::to_string(upIndex) + "\n";
            }

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection_name,
                    "ObjectID_used", user_oid_string,
                    "index", std::to_string(index)
            );

            //NOTE: OK to continue here
        }
    };

    for (const auto& client_picture_time_ms : user_info_from_client.pictures_last_updated_timestamps()) {

        const long index = client_picture_time_ms.index_number();

        if (-1 < index
            && index < (long) server_member_picture_timestamps.size()
                ) { //index passed from client is in range of array

            const std::chrono::milliseconds server_picture_timestamp = server_member_picture_timestamps[index].second;

            //erase all elements that the client references
            picture_indexes_used.erase((int) index);

            const std::chrono::milliseconds client_picture_timestamp{client_picture_time_ms.last_updated_timestamp()};

            if (
                client_picture_timestamp <
                server_picture_timestamp //if passed timestamp for this picture is less than timestamp stored on server
                || (server_picture_timestamp == default_value
                    && client_picture_timestamp != default_value //this is needed or deleted pictures on server will be sent back every time
                ) //if server picture is deleted and client is NOT deleted
            ) {
                store_index_to_be_updated((int) index);
            }
        }
        else { //index was out of range of array

            //this is not a duplicate of the above error, this error checks the passed value for picture index_number, not
            // the size of the array

            std::string error_string = "An index value was passed that is out of range of the array.\n";
            error_string += "serverMemberPictureTimestamps:\n";
            for (size_t i = 0; i < server_member_picture_timestamps.size(); i++) {
                error_string += "[" + std::to_string(i) + "]: objectOID: " + server_member_picture_timestamps[i].first +
                                " timestamp: " + getDateTimeStringFromTimestamp(server_member_picture_timestamps[i].second) +
                                "\n";
            }

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection_name,
                    "ObjectID_used", user_oid_string,
                    "index", std::to_string(index)
            );
        }
    }

    //Update any pictures existing on the server and not the client (the client will only
    // hold references for pictures that exits).
    for (const auto& picture : picture_indexes_used) {
        store_index_to_be_updated(picture);
    }

    int thumbnail_index = -1;

    //NOTE: this has already been checked to make sure at least 1 element exists
    for (size_t i = 0; i < server_member_picture_timestamps.size(); i++) {
        if (server_member_picture_timestamps[i].second != default_value) {
            thumbnail_index = (int) i;
            break;
        }
    }

    response->mutable_user_info()->set_account_oid(user_oid_string);

    switch (how_to_handle_member_pictures) {
        case HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO:
            break;
        case HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL: {

            //update thumbnail
            auto save_thumbnail_result = [&](
                    std::string& _thumbnail,
                    int /*thumbnail_size*/,
                    const std::string& /*thumbnail_reference_oid*/,
                    const int index,
                    const std::chrono::milliseconds& _thumbnail_timestamp
            ) {
                response->mutable_user_info()->set_account_thumbnail_size((int) _thumbnail.size());
                response->mutable_user_info()->set_account_thumbnail(std::move(_thumbnail));
                response->mutable_user_info()->set_account_thumbnail_index(index);
                response->mutable_user_info()->set_account_thumbnail_timestamp(_thumbnail_timestamp.count());
            };

            if (thumbnail_index == -1
                && user_info_from_client.thumbnail_index_number() != -1) { //all pictures deleted on server but not client
                DeletedThumbnailInfo::saveDeletedThumbnailInfo(response->mutable_user_info());
                thumbnail_was_updated = true;
            } else if (thumbnail_index !=
                       user_info_from_client.thumbnail_index_number()) { //if client and server thumbnail index are different

                if (extractThumbnailFromUserAccountDoc(
                        accounts_db,
                        user_account_doc,
                        user_account_oid,
                        nullptr,
                        save_thumbnail_result,
                        ExtractThumbnailAdditionalCommands())
                ) {
                    thumbnail_was_updated = true;
                } else {
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response_function();
                    return;
                }
            } else if (thumbnail_index != -1) { //if the thumbnail index from the client is the same as server AND all pictures are not deleted
                for (int index : index_to_be_updated) {
                    if(index == thumbnail_index) { //if picture exists on server and this value is thumbnail index
                        if (extractThumbnailFromUserAccountDoc(
                                accounts_db,
                                user_account_doc,
                                user_account_oid,
                                nullptr,
                                save_thumbnail_result,
                                ExtractThumbnailAdditionalCommands())
                        ) {
                            thumbnail_was_updated = true;
                        } else {
                            response->set_return_status(ReturnStatus::LG_ERROR);
                            response_function();
                            return;
                        }
                        break;
                    }
                }
            }

            break;
        }
        case HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO: {

            bool thumbnail_updated_inside_block = false;
            response->mutable_user_info()->set_pictures_checked_for_updates(true);

            for (int index : index_to_be_updated) {

                if (server_member_picture_timestamps[index].second == default_value) { //if server picture is deleted however client is NOT deleted

                    picture_was_updated = true;

                    auto pictureMessage = response->mutable_user_info()->add_picture();

                    //Thumbnail will be updated to deleted below if necessary.

                    DeletedPictureInfo::saveDeletedPictureInfo(pictureMessage, index);
                }
                else { //if picture exists on server

                    std::function<void(std::string&&, int, const std::chrono::milliseconds&)> set_picture_to_response =
                            [&response, &index](std::string&& picture_byte_string, int picture_size,
                                                const std::chrono::milliseconds& timestamp) {

                                auto picture_message = response->mutable_user_info()->add_picture();

                                picture_message->set_timestamp_picture_last_updated(timestamp.count());
                                picture_message->set_file_in_bytes(std::move(picture_byte_string));
                                picture_message->set_file_size(picture_size);
                                picture_message->set_index_number(index);
                            };

                    std::function<void(std::string&&, int, const std::chrono::milliseconds&)> set_thumbnail_to_response;

                    //this container is a set so duplicates can not exist
                    if (index == thumbnail_index) { //if thumbnail index requires updating
                        set_thumbnail_to_response = [&](
                                std::string&& passed_thumbnail,
                                int thumbnail_size,
                                const std::chrono::milliseconds& thumbnail_timestamp) {
                            response->mutable_user_info()->set_account_thumbnail_size(thumbnail_size);
                            response->mutable_user_info()->set_account_thumbnail(std::move(passed_thumbnail));
                            response->mutable_user_info()->set_account_thumbnail_index(thumbnail_index);
                            response->mutable_user_info()->set_account_thumbnail_timestamp(thumbnail_timestamp.count());
                        };
                    } else {
                        set_thumbnail_to_response = [](std::string&&, int, const std::chrono::milliseconds&) {};
                    }

                    if (!extractPictureToResponse(
                            mongo_cpp_client,
                            accounts_db,
                            user_accounts_collection,
                            current_timestamp,
                            user_account_doc,
                            bsoncxx::oid{server_member_picture_timestamps[index].first},
                            user_account_oid,
                            index,
                            set_picture_to_response,
                            !thumbnail_updated_inside_block,
                            set_thumbnail_to_response)
                    ) { //if function failed
                        //error is already stored
                        response->set_return_status(ReturnStatus::LG_ERROR);
                        response_function();
                        return;
                    } else { //picture was properly extracted
                        picture_was_updated = true;
                        thumbnail_updated_inside_block = index == thumbnail_index ? true : thumbnail_updated_inside_block;
                    }
                }
            }

            if (thumbnail_index == -1
                && user_info_from_client.thumbnail_index_number() != -1) { //thumbnail deleted on server but not client
                DeletedThumbnailInfo::saveDeletedThumbnailInfo(response->mutable_user_info());
                thumbnail_was_updated = true;
            } else if (
                    !thumbnail_updated_inside_block
                    && thumbnail_index != user_info_from_client.thumbnail_index_number()
                ) { //if thumbnail was not updated with above pictures AND client and server thumbnail index are different

                //update thumbnail
                auto save_thumbnail_result = [&](
                        std::string& _thumbnail,
                        int /*thumbnail_size*/,
                        const std::string& /*thumbnail_reference_oid*/,
                        const int index,
                        const std::chrono::milliseconds& _thumbnail_timestamp
                ) {
                    response->mutable_user_info()->set_account_thumbnail_size((int) _thumbnail.size());
                    response->mutable_user_info()->set_account_thumbnail(std::move(_thumbnail));
                    response->mutable_user_info()->set_account_thumbnail_index(index);
                    response->mutable_user_info()->set_account_thumbnail_timestamp(_thumbnail_timestamp.count());
                };

                if (extractThumbnailFromUserAccountDoc(
                        accounts_db,
                        user_account_doc,
                        user_account_oid,
                        nullptr,
                        save_thumbnail_result,
                        ExtractThumbnailAdditionalCommands())
                ) {
                    thumbnail_was_updated = true;
                } else {
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response_function();
                    return;
                }
            }
            //else //This case is handled above inside the for loop. If the index is changed and needs to be
            // updated, it will be updated with the picture at that index.

            break;
        }
    }

    //ACCOUNT_STATE_EVENT is not actually stored by the chat room, so it is irrelevant.
    if (account_state_from_chat_room != AccountStateInChatRoom::ACCOUNT_STATE_EVENT
        && account_state_from_chat_room != user_info_from_client.account_state()
    ) {
        update_account_state = true;
    }

    if (update_user_info_besides_pictures) { //if member info besides pictures and thumbnail requires updating
        if (!saveUserInfoToMemberSharedInfoMessage(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                user_account_doc,
                user_account_oid,
                response->mutable_user_info(),
                HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
                current_timestamp)
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            response_function();
            return;
        }
    }

    if (update_account_state || update_user_info_besides_pictures || picture_was_updated || thumbnail_was_updated ||
        always_set_response_message) { //if response was updated at all OR this is for single chat room member function

        response->mutable_user_info()->set_current_timestamp(current_timestamp.count());
        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_account_state(account_state_from_chat_room);
        response_function();
    }

}

UserRequiresUpdatedReturn checkIfBasicUserInfoRequiresUpdated(
        const bsoncxx::document::view& user_account_doc,
        const std::chrono::milliseconds& current_timestamp,
        const OtherUserInfoForUpdates& user_info_from_client
        ) {

    try {
        const std::chrono::milliseconds latest_timestamp_info_updated{user_info_from_client.member_info_last_updated_timestamp()};

        const std::chrono::milliseconds last_time_displayed_info_updated = extractFromBsoncxx_k_date(
                user_account_doc,
                user_account_keys::LAST_TIME_DISPLAYED_INFO_UPDATED
        ).value;

        if (latest_timestamp_info_updated < last_time_displayed_info_updated) { //if element requires updated
            return UserRequiresUpdatedReturn::USER_REQUIRES_UPDATED;
        }

        const int user_age = extractFromBsoncxx_k_int32(
                user_account_doc,
                user_account_keys::AGE
        );

        if (user_age != general_values::EVENT_AGE_VALUE && user_info_from_client.age() != user_age) {
            return UserRequiresUpdatedReturn::USER_REQUIRES_UPDATED;
        }

        //EVENT_VALUES document (will only exist for events)
        const auto event_values_document = user_account_doc[user_account_keys::EVENT_VALUES];
        if (event_values_document && event_values_document.type() == bsoncxx::type::k_document) {
            const bsoncxx::document::view event_values_doc = event_values_document.get_document().value;

            const std::string event_title = extractFromBsoncxx_k_utf8(
                    event_values_doc,
                    user_account_keys::event_values::EVENT_TITLE
            );

            if(user_info_from_client.event_title() != event_title) {
                return UserRequiresUpdatedReturn::USER_REQUIRES_UPDATED;
            }
        } else if(event_values_document) { //if EVENT_VALUES is not type document
            logElementError(
                    __LINE__, __FILE__,
                    event_values_document, user_account_doc,
                    bsoncxx::type::k_document, user_account_keys::EVENT_VALUES,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            return UserRequiresUpdatedReturn::ERROR_USER_REQUIRES_UPDATE;
        }

        const std::chrono::milliseconds event_expiration_time = extractFromBsoncxx_k_date(
                user_account_doc,
                user_account_keys::EVENT_EXPIRATION_TIME
        ).value;

        const LetsGoEventStatus event_status = convertExpirationTimeToEventStatus(
                event_expiration_time,
                current_timestamp
        );

        if(user_info_from_client.event_status() != event_status) {
            return UserRequiresUpdatedReturn::USER_REQUIRES_UPDATED;
        }

    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        return UserRequiresUpdatedReturn::ERROR_USER_REQUIRES_UPDATE;
    }

    return UserRequiresUpdatedReturn::USER_DOES_NOT_REQUIRES_UPDATE;
}
