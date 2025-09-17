//
// Created by jeremiah on 6/1/22.
//

#include <report_helper_functions.h>
#include <gtest/gtest.h>
#include "report_helper_functions_test.h"

void checkMoveOutstandingReportsToHandledResult(
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::oid& user_account_oid,
        const std::string& admin_name,
        ReportHandledMoveReason handled_move_reason,
        DisciplinaryActionTypeEnum disciplinary_action_taken,
        const OutstandingReports& current_outstanding_reports,
        const bool function_return_value,
        HandledReports& before_function_handled_reports
        ) {

    //no outstanding reports existed to move
    if(current_outstanding_reports.current_object_oid.to_string() != "000000000000000000000000") {

        before_function_handled_reports.current_object_oid = user_account_oid;

        auto stored_disciplinary_action_taken = DisciplinaryActionTypeEnum(-1);

        if (handled_move_reason == ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN) {
            stored_disciplinary_action_taken = disciplinary_action_taken;
        }

        before_function_handled_reports.handled_reports_info.emplace_back(
                HandledReports::ReportsArrayElement(
                        admin_name,
                        bsoncxx::types::b_date{currentTimestamp},
                        current_outstanding_reports.timestamp_limit_reached,
                        handled_move_reason,
                        stored_disciplinary_action_taken,
                        current_outstanding_reports.reports_log
                        )
                        );
    }

    HandledReports extracted_handled_reports(user_account_oid);

    EXPECT_EQ(before_function_handled_reports, extracted_handled_reports);

    EXPECT_EQ(function_return_value, true);
}
