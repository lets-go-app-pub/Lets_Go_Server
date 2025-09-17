//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (REPORTS_DATABASE_NAME) (HANDLED_REPORTS_COLLECTION_NAME)
//NOTE: _id matches the user_accounts _id for each user
namespace handled_reports_keys {
    inline const std::string HANDLED_REPORTS_INFO = "aRs"; //array of documents; contains documents with below fields

    //namespace contains keys for HANDLED_REPORTS_INFO documents
    namespace reports_array {
        inline const std::string ADMIN_NAME = "aAn"; //string; admin name that handled this 'chunk' of reports (if relevant, "~" if not)
        inline const std::string TIMESTAMP_HANDLED = "aTh"; //mongoDB Date; timestamp admin handled this 'chunk' of reports
        inline const std::string TIMESTAMP_LIMIT_REACHED = "tLr"; //mongoDB Date; timestamp this 'chunk' of reports passed the limit
        inline const std::string REPORT_HANDLED_MOVE_REASON = "hDc"; //int32; enum for command which made this move from OUTSTANDING collection to HANDLED collection, follows ReportHandledMoveReason in report_handled_move_reason.h
        inline const std::string DISCIPLINARY_ACTION_TAKEN = "dAt"; //int32 or missing; enum for disciplinary action taken, follows DisciplinaryActionTypeEnum, only set if REPORT_HANDLED_MOVE_REASON == ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN
        inline const std::string REPORTS_LOG = "aRs"; //array of documents; this is the extracted version of REPORTS_LOG
    }
}