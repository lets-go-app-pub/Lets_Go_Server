//
// Created by jeremiah on 3/8/23.
//

#include "create_complete_chat_room_with_user.h"

#include <mongocxx/uri.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <create_chat_room_helper.h>
#include <handle_function_operation_exception.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <store_mongoDB_error_and_exception.h>
#include <thread_pool_global_variable.h>

#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "run_initial_login_operation.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//Creates a chat room with the passed user as admin.
//The final two parameters will be set.
//NOTE: This will not initialize the chat room with the chat stream to the device. That is done at the end of
// createChatRoom().
//User doc requires FIRST_NAME and PICTURES to be projected out.
bool createCompleteChatRoomWithUser(
        mongocxx::database& accounts_db,
        mongocxx::database& chat_room_db,
        mongocxx::client_session* session,
        const GenerateNewChatRoomTimes& chat_room_times,
        const bsoncxx::oid& admin_user_account_oid,
        const bsoncxx::document::view& admin_user_account_doc,
        const std::string& chat_room_id,
        const std::optional<FullEventValues>& event_values,
        const std::optional<CreateChatRoomLocationStruct>& location_values,
        std::string& chat_room_name,
        std::string& chat_room_password,
        std::string& cap_message_uuid
) {

    if(session == nullptr) {
        const std::string error_string = "createChatRoomWithUser() was called when session was set to nullptr.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "chat_room_id", chat_room_id,
                "admin_user_account_oid", admin_user_account_oid
        );
        return false;
    }

    std::string userName;
    int thumbnail_size = 0;
    std::string thumbnail_reference_oid;

    auto userNameElement = admin_user_account_doc[user_account_keys::FIRST_NAME];
    if (userNameElement &&
        userNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        userName = userNameElement.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(
                __LINE__, __FILE__,
                userNameElement, admin_user_account_doc,
                bsoncxx::type::k_utf8, user_account_keys::FIRST_NAME,
                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_INFO
        );
        return false;
    }

    auto setThumbnailValues = [&](
            std::string& /*thumbnail*/,
            int _thumbnail_size,
            const std::string& _thumbnail_reference_oid,
            const int /*index*/,
            const std::chrono::milliseconds& /*thumbnail_timestamp*/
    ) {
        thumbnail_size = _thumbnail_size;
        thumbnail_reference_oid = _thumbnail_reference_oid;
    };

    ExtractThumbnailAdditionalCommands extractThumbnailAdditionalCommands;
    extractThumbnailAdditionalCommands.setupForAddToSet(chat_room_id);
    extractThumbnailAdditionalCommands.setOnlyRequestThumbnailSize();

    //extract thumbnail size requires PICTURES projection
    if (!extractThumbnailFromUserAccountDoc(
            accounts_db,
            admin_user_account_doc,
            admin_user_account_oid,
            session,
            setThumbnailValues,
            extractThumbnailAdditionalCommands)
    ) { //failed to extract thumbnail
        //errors are already stored
        return false;
    }

    //NOTE: at this point chatRoomGeneratedId and chatRoomGeneratedPassword are set
    if (chat_room_name.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES
        || chat_room_name.empty()) { //check if name is valid
        chat_room_name = generateEmptyChatRoomName(userName);
    }

    bsoncxx::builder::basic::array headerAccountsInChatRoomArray;

    headerAccountsInChatRoomArray.append(
            createChatRoomHeaderUserDoc(
                    admin_user_account_oid,
                    AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN,
                    userName,
                    thumbnail_size,
                    thumbnail_reference_oid,
                    bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time},
                    chat_room_times.chat_room_last_active_time
            )
    );

    auto setReturnStatus = [&chat_room_password](
            const std::string& chatRoomId [[maybe_unused]],
            const std::string& _chatRoomPassword
    ) {
        chat_room_password = _chatRoomPassword;
    };

#ifndef _RELEASE
    std::cout << "Starting createChatRoomFromId().\n";
#endif

    if (!createChatRoomFromId(
            headerAccountsInChatRoomArray,
            chat_room_name,
            cap_message_uuid,
            admin_user_account_oid,
            chat_room_times,
            session,
            setReturnStatus,
            bsoncxx::array::view{},
            chat_room_db,
            chat_room_id,
            location_values,
            event_values)
            ) {
        return false;
    }

#ifndef _RELEASE
    std::cout << "Create chat room finished, setting response.\n";
#endif

    return true;
}