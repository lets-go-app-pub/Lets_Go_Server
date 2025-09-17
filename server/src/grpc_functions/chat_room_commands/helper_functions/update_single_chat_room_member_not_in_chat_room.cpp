//
// Created by jeremiah on 5/18/21.
//

#include <deleted_thumbnail_info.h>
#include "chat_room_commands_helper_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "chat_room_header_keys.h"
#include "user_pictures_keys.h"
#include "extract_data_from_bsoncxx.h"
#include "utility_chat_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** Documentation related to this file can be found inside
 * [grpc_functions/chat_room_commands/request_information/_documentation.md] **/

//update a single chat room member, passed parameters are in memberFromClient, will be stored
// inside a response called from responseFunction
void updateSingleChatRoomMemberNotInChatRoom(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& chat_room_collection,
        const std::string& memberAccountOIDString,
        const bsoncxx::document::view& userFromChatRoomHeaderDocView,
        const OtherUserInfoForUpdates& memberFromClient,
        const std::chrono::milliseconds& currentTimestamp,
        bool alwaysSetResponseMessage,
        AccountStateInChatRoom accountStateFromChatRoom,
        const std::chrono::milliseconds& userLastActivityTime,
        UpdateOtherUserResponse* streamResponse,
        const std::function<void()>& responseFunction
) {

    bool updateAccountState = false;
    bool updateThumbnail = false;
    bool updateFirstName = false;

    std::chrono::milliseconds thumbnail_updated_timestamp;

    auto thumbnailTimestampElement = userFromChatRoomHeaderDocView[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP];
    if (thumbnailTimestampElement && thumbnailTimestampElement.type() == bsoncxx::type::k_date) { //if element exists and is type date
        thumbnail_updated_timestamp = thumbnailTimestampElement.get_date().value;
    } else { //if element does not exist or is not type date
        logElementError(__LINE__, __FILE__, thumbnailTimestampElement,
                        userFromChatRoomHeaderDocView, bsoncxx::type::k_date,
                        chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string());

        return;
    }

    auto thumbnail_size_element = userFromChatRoomHeaderDocView[chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE];
    if (thumbnail_size_element && thumbnail_size_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32

        const int thumbnail_size = thumbnail_size_element.get_int32().value;

        if (memberFromClient.thumbnail_timestamp() < thumbnail_updated_timestamp.count()
                && thumbnail_size != memberFromClient.thumbnail_size_in_bytes()) {

            if(!extractThumbnailFromHeaderAccountDocument(
                    mongo_cpp_client,
                    accounts_db,
                    chat_room_collection,
                    userFromChatRoomHeaderDocView,
                    currentTimestamp,
                    streamResponse->mutable_user_info(),
                    memberAccountOIDString,
                    thumbnail_size,
                    thumbnail_updated_timestamp
            )) {
                //error already stored
                return;
            }

            updateThumbnail = true;
        }
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, thumbnail_size_element,
                        userFromChatRoomHeaderDocView, bsoncxx::type::k_int32, chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string());

        return;
    }

    auto firstNameElement = userFromChatRoomHeaderDocView[chat_room_header_keys::accounts_in_chat_room::FIRST_NAME];
    if (firstNameElement && firstNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        if (firstNameElement.get_string().value.to_string() != memberFromClient.first_name()) {
            streamResponse->mutable_user_info()->set_account_name(firstNameElement.get_string().value.to_string());
            updateFirstName = true;
        }
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, firstNameElement,
                        userFromChatRoomHeaderDocView, bsoncxx::type::k_utf8, chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string());

        return;
    }

    if (accountStateFromChatRoom !=
        memberFromClient.account_state()) { //account state on server is not the same as account state on client
        updateAccountState = true;
    }

    if (updateAccountState || updateThumbnail || updateFirstName || alwaysSetResponseMessage) { //if streamResponse was updated at all OR this is for single chat room member function

        streamResponse->set_return_status(ReturnStatus::SUCCESS);
        streamResponse->set_timestamp_returned(currentTimestamp.count());
        streamResponse->set_account_state(accountStateFromChatRoom);
        streamResponse->set_account_last_activity_time(userLastActivityTime.count());
        streamResponse->mutable_user_info()->set_account_oid(memberAccountOIDString);
        //setting this is a bit redundant but convenient on the client
        streamResponse->mutable_user_info()->set_current_timestamp(currentTimestamp.count());
        streamResponse->mutable_user_info()->set_account_type(UserAccountType::USER_ACCOUNT_TYPE);

        responseFunction();
    }

}