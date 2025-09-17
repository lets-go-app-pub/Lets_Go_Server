//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include <chrono>

namespace chat_stream_container {

    //For metadata format see [src/utility/async_server/_documentation.md] under 'Expected Input' header.
    namespace initial_metadata {
        //Initial metadata keys
        inline const std::string CURRENT_ACCOUNT_ID = "current_account_id";
        inline const std::string LOGGED_IN_TOKEN = "logged_in_token";
        inline const std::string LETS_GO_VERSION = "lets_go_version";
        inline const std::string INSTALLATION_ID = "installation_id";
        inline const std::string CHAT_ROOM_VALUES = "chat_room_values";
        inline const std::string CHAT_ROOM_VALUES_DELIMITER = "::";
    }

    namespace trailing_metadata {
        //Trailing metadata keys
        inline const std::string REASON_STREAM_SHUT_DOWN_KEY = "stream_down_reason";
        inline const std::string RETURN_STATUS_KEY = "return_status";
        inline const std::string OPTIONAL_INFO_OF_CANCELLING_STREAM = "optional_info_cancel"; //Will hold installation id of cancelling stream if REASON_STREAM_SHUT_DOWN_KEY==grpc_stream_chat::StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM;
    }

#ifdef LG_TESTING
    inline std::chrono::milliseconds TIME_CHAT_STREAM_STAYS_ACTIVE = std::chrono::milliseconds{30L*1000L}; //time before the chat stream will cancel itself (this will be refreshed when refreshChatStream is called)
    inline std::chrono::milliseconds TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{(long)((double)TIME_CHAT_STREAM_STAYS_ACTIVE.count() * .95)}; //this time will be set to guarantee a refresh never happens when the server is shutting down
#else
    //TODO: set to a more reasonable time (like 30 minutes, may want to take into account TIME_BETWEEN_TOKEN_VERIFICATION)
    inline const std::chrono::milliseconds TIME_CHAT_STREAM_STAYS_ACTIVE = std::chrono::milliseconds{30L*60L*1000L}; //time before the chat stream will cancel itself (this will be refreshed when refreshChatStream is called)
    inline const std::chrono::milliseconds TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{(long)((double)TIME_CHAT_STREAM_STAYS_ACTIVE.count() * .95)}; //this time will be set to guarantee a refresh never happens when the server is shutting down
#endif

    inline const std::string CHAT_CHANGE_STREAM_PASSED_STRING_TO_CONVERT = "from_chat_change_stream"; //passed to convertChatMessageDocumentToChatMessageToClient from beginChatChangeStream to notify that it is not a specific user doing the conversion
    inline const int MAX_NUMBER_MESSAGES_USER_CAN_REQUEST = 10; //maximum numbers messages the user can request at a time in bi-di stream and updateChatRoom, also number of full messages sent back with joinChatRoom()
}