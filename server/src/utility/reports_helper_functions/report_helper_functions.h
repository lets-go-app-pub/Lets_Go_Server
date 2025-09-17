//
// Created by jeremiah on 9/19/21.
//

#pragma once

#include <mongocxx/client.hpp>
#include <UserMatchOptions.grpc.pb.h>
#include <DisciplinaryActionType.grpc.pb.h>

#include "report_handled_move_reason.h"

bool saveNewReportToCollection(mongocxx::client& mongo_cpp_client, const std::chrono::milliseconds& currentTimestamp,
                               const bsoncxx::oid& reporting_user_account_oid,
                               const bsoncxx::oid& reported_user_account_oid, ReportReason report_reason,
                               ReportOriginType report_origin, const std::string& report_message,
                               const std::string& chat_room_id, const std::string& message_uuid);

/**
 * Move reports for user_account_oid from Outstanding Reports collection
 * to Handled Reports collection.
 * **/
bool moveOutstandingReportsToHandled(
        mongocxx::client& mongo_cpp_client,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid& user_account_oid,
        const std::string& admin_name,
        ReportHandledMoveReason handled_move_reason,
        mongocxx::client_session* session = nullptr,
        DisciplinaryActionTypeEnum disciplinary_action_taken = DisciplinaryActionTypeEnum(-1)
        );