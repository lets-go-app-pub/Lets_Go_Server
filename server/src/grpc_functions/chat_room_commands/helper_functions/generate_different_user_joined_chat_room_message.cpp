//
// Created by jeremiah on 7/8/21.
//

#include "chat_room_commands_helper_functions.h"

#include "utility_general_functions.h"

#include "chat_room_shared_keys.h"
#include "chat_room_message_keys.h"

std::string generateDifferentUserJoinedChatRoomMessage(
        bsoncxx::builder::stream::document& builder,
        const bsoncxx::oid& userAccountOID,
        AccountStateInChatRoom currentUserAccountState,
        const std::string& event_oid_str,
        const std::chrono::milliseconds& currentTimestamp
) {
    const std::string& message_uuid = generateUUID();

    builder
        << "_id" << message_uuid
        << chat_room_message_keys::MESSAGE_SENT_BY << userAccountOID
        << chat_room_message_keys::MESSAGE_TYPE << bsoncxx::types::b_int32{MessageSpecifics::MessageBodyCase::kDifferentUserJoinedMessage};

    if(currentTimestamp == std::chrono::milliseconds{-1}) {

#ifndef LG_TESTING
        builder
            << chat_room_shared_keys::TIMESTAMP_CREATED << "$$NOW";
#else
        if(testing_delay_for_messages == std::chrono::milliseconds{-1}) {
            builder
                    << chat_room_shared_keys::TIMESTAMP_CREATED << "$$NOW";
        } else {

            //Simulate gaps between the messages being stored.
            const std::chrono::milliseconds current_timestamp = generateTestingTimestamp();

            builder
                    << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{current_timestamp};
        }
#endif

    } else {
        builder
                << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{currentTimestamp};
    }

    builder
        << chat_room_message_keys::RANDOM_INT << bsoncxx::types::b_int32{generateRandomInt()};

    bsoncxx::builder::stream::document specifics_document;

    specifics_document
            << chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE << bsoncxx::types::b_int32{currentUserAccountState};

    //If this was joined from a user swiping yes on an event.
    if(event_oid_str != chat_room_values::EVENT_ID_DEFAULT) {
        specifics_document
                << chat_room_message_keys::message_specifics::USER_JOINED_FROM_EVENT << event_oid_str;
    }

    builder
        << chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT << specifics_document.view();

    return message_uuid;
}