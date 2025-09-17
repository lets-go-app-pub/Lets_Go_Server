//
// Created by jeremiah on 5/31/22.
//

#pragma once

#include <mongocxx/client.hpp>
#include <report_handled_move_reason.h>
#include <DisciplinaryActionType.pb.h>
#include <reports_objects.h>

void checkMoveOutstandingReportsToHandledResult(
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::oid& user_account_oid,
        const std::string& admin_name,
        ReportHandledMoveReason handled_move_reason,
        DisciplinaryActionTypeEnum disciplinary_action_taken,
        const OutstandingReports& current_outstanding_reports,
        const bool function_return_value,
        HandledReports& before_function_handled_reports //NOTE: This will change (don't keep it around)
        );