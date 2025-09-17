//
// Created by jeremiah on 5/2/21.
//
#pragma once

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <store_and_send_messages.h>
#include <utility_chat_functions.h>

//can extract messages before or after the passed timeOfMessageToCompare depending on extractPreviousMessages
//NOTE: userAccountDocView is expected to have all info projected required to save inside a
// MemberSharedInfoMessage object (name, thumbnail, pictureOID array, age, gender, city, bio,
// categories).
//NOTE: message_uuids_to_exclude will have a value added to it
bool streamInitializationMessagesToClient(
        mongocxx::client& mongoCppClient,
        mongocxx::database& accountsDB,
        mongocxx::collection& chatRoomCollection,
        mongocxx::collection& userAccountsCollection,
        const bsoncxx::document::view& userAccountDocView,
        const std::string& chatRoomId,
        const std::string& currentUserAccountOIDStr,
        StoreAndSendMessagesVirtual* storeAndSendMessagesToClient,
        AmountOfMessage amountOfMessage,
        DifferentUserJoinedChatRoomAmount differentUserJoinedChatRoomAmount,
        bool do_not_update_user_state,
        bool onlyStoreMessageBoolValue,
        bool requestFinalFewMessagesInFull,
        const std::chrono::milliseconds& greater_than_or_equal_to_message_time,
        bsoncxx::builder::basic::array& message_uuids_to_exclude,
        const std::chrono::milliseconds& time_to_request_before_or_equal = std::chrono::milliseconds{LONG_MAX}
);