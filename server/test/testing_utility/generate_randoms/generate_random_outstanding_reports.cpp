//
// Created by jeremiah on 6/1/22.
//

#include <report_values.h>
#include "generate_randoms.h"

void generateRandomOutstandingReportsImplementation(
        OutstandingReports& outstanding_reports,
        const bsoncxx::oid& generated_account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        const bool generate_random_chat_room_message
) {
    std::chrono::milliseconds past_timestamp{currentTimestamp.count() - (10L * 1000L)};

    outstanding_reports.current_object_oid = generated_account_oid;
    outstanding_reports.timestamp_limit_reached = bsoncxx::types::b_date{past_timestamp};
    outstanding_reports.checked_out_end_time_reached = bsoncxx::types::b_date{
            std::chrono::milliseconds{currentTimestamp + report_values::CHECK_OUT_TIME}};

    for (size_t i = 0; i < report_values::NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED; i++) {
        past_timestamp = std::chrono::milliseconds{past_timestamp.count() - (10L * 1000L)};
        outstanding_reports.reports_log.emplace_back(generateRandomReportsLog(past_timestamp, generate_random_chat_room_message));
    }
}

std::unique_ptr<OutstandingReports> generateRandomOutstandingReports(
        const bsoncxx::oid& generated_account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        const bool generate_random_chat_room_message
) {

    std::unique_ptr<OutstandingReports> outstanding_reports = std::make_unique<OutstandingReports>();

    generateRandomOutstandingReportsImplementation(
            *outstanding_reports,
            generated_account_oid,
            currentTimestamp,
            generate_random_chat_room_message
    );

    return outstanding_reports;
}

OutstandingReports generateRandomOutstandingReports(
        const bsoncxx::oid& generated_account_oid,
        const std::chrono::milliseconds& currentTimestamp
) {
    OutstandingReports outstanding_reports;

    generateRandomOutstandingReportsImplementation(
            outstanding_reports,
            generated_account_oid,
            currentTimestamp,
            true
    );

    return outstanding_reports;
}