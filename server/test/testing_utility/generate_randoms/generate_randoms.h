//
// Created by jeremiah on 6/1/22.
//

#pragma once

#include <reports_objects.h>
#include <string>
#include <utility_general_functions.h>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_rooms_objects.h>
#include "event_request_message_is_valid.h"
#include "create_event.h"

std::string generateRandomChatRoomId(int num_chars = 8);

ReportsLog generateRandomReportsLog(
        //the default timestamp is {Wed, 01 Jun 2022 0:0:0 GMT}
        const std::chrono::milliseconds& timestamp = std::chrono::milliseconds{rand() % 1654041600000},
        bool generate_random_chat_room_message = true
);

std::unique_ptr<OutstandingReports> generateRandomOutstandingReports(
        const bsoncxx::oid& generated_account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        bool generate_random_chat_room_message
);

OutstandingReports generateRandomOutstandingReports(
        const bsoncxx::oid& generated_account_oid,
        const std::chrono::milliseconds& currentTimestamp = getCurrentTimestamp()
);

//Generates a random UserPictureDoc class
//NOTE: Will NOT be inserted into the database.
//NOTE: picture_index needs to be set if this is to be used with a UserAccountDoc object.
UserPictureDoc generateRandomUserPicture(
        const std::chrono::milliseconds& currentTimestamp = getCurrentTimestamp(),
        const bsoncxx::oid& object_oid = bsoncxx::oid{}
);

//Generates a random UserPictureDoc class
//NOTE: Will NOT be inserted into the database.
ChatMessagePictureDoc generateRandomChatPicture(
        const std::string& chat_room_id = generateRandomChatRoomId(),
        const std::chrono::milliseconds& currentTimestamp = getCurrentTimestamp(),
        const bsoncxx::oid& object_oid = bsoncxx::oid{}
);

//Will generate and save the largest possible user account over the passed UserAccountDoc. This
// will save it to the collection as well.
void generateRandomLargestPossibleUserAccount(
        UserAccountDoc& user_account_doc,
        const std::chrono::milliseconds& category_time_frame_start_time = std::chrono::milliseconds{0}
);

EventRequestMessage generateRandomEventRequestMessage(
        const EventChatRoomAdminInfo& chat_room_admin_info,
        const std::chrono::milliseconds& current_timestamp
);

CreateEventReturnValues generateRandomEvent(
        const EventChatRoomAdminInfo& chat_room_admin_info,
        const std::string& created_by,
        const std::chrono::milliseconds& current_timestamp,
        EventRequestMessage event_info
);

CreateEventReturnValues generateRandomEventAndEventInfo(
        const EventChatRoomAdminInfo& chat_room_admin_info,
        const std::string& created_by,
        const std::chrono::milliseconds& current_timestamp
);

UserAccountType generateRandomValidUserAccountType();