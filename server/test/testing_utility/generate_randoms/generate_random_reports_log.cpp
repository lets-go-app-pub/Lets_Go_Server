//
// Created by jeremiah on 6/1/22.
//

#include <generate_random_account_info/generate_random_account_info.h>
#include "generate_randoms.h"

ReportsLog generateRandomReportsLog(
        const std::chrono::milliseconds& timestamp,
        const bool generate_random_chat_room_message
) {

    //value from 1-4
    auto report_reason = ReportReason((rand() % (ReportReason_MAX - 1)) + 1);

    //technically only REPORT_REASON_OTHER stores messages, however in actuality it should be set up
    // to be extendable
    std::string message = pickRandomBio();

    auto report_origin_type = ReportOriginType(rand() % ReportOriginType_MAX);

    std::string chat_room_id;
    std::string message_uuid;

        if (report_origin_type != ReportOriginType::REPORT_ORIGIN_SWIPING) {
            chat_room_id = generateRandomChatRoomId();
        }

    if(generate_random_chat_room_message) {
        if (report_origin_type == ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MESSAGE) {
            message_uuid = generateUUID();
        }
    }

    return {
            bsoncxx::oid{},
            report_reason,
            message,
            report_origin_type,
            chat_room_id,
            message_uuid,
            bsoncxx::types::b_date{timestamp}
            };
}