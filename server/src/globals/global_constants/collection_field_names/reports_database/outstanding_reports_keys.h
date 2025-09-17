//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (REPORTS_DATABASE_NAME) (OUTSTANDING_REPORTS_COLLECTION_NAME)
//NOTE: _id matches the user_accounts _id for each user
namespace outstanding_reports_keys {
    inline const std::string TIMESTAMP_LIMIT_REACHED = "tLr"; //mongoDB Date; when the array hits the number for a proper report, this timestamp will be set otherwise set to -1 (collection indexed by this value)
    inline const std::string CHECKED_OUT_END_TIME_REACHED = "cOe"; //mongoDB Date or missing; when this is withdrawn from the database, this timestamp will be set to some time in the future, after that time has passed, reports can be withdrawn again
    inline const std::string REPORTS_LOG = "aRs"; //array of documents; contains documents with report information about why this user was reported

    //namespace contains keys for REPORTS_LOG documents
    namespace reports_log {
        inline const std::string ACCOUNT_OID = "iId"; //OID; this will be the account that reported this user
        inline const std::string REASON = "iRr"; //int32; this will be the reason passed back from the device; this is just the ReportReason enum from ReportMessages.proto
        inline const std::string MESSAGE = "sRm"; //string; this will be the message passed back from the device about 'why' the user was reported; NOTE: '~' is empty message
        inline const std::string REPORT_ORIGIN = "rEo"; //int32; where the message was sent from follows ReportOriginType enum from UserMatchOptions.proto
        inline const std::string CHAT_ROOM_ID = "cRi"; //string; this will be the message chat room id if this was sent from a chat room in any way (could have been from chat room fragment, chat room info fragment or member info fragment)
        inline const std::string MESSAGE_UUID = "mUu"; //string; this will be the message uuid if REPORT_ORIGIN == ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MESSAGE
        inline const std::string TIMESTAMP_SUBMITTED = "dRt"; //Date; this will be the time the report was submitted
    }

}