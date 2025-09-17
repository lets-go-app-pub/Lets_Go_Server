//
// Created by jeremiah on 5/31/22.
//

#pragma once

#include <bsoncxx/builder/stream/document.hpp>
#include <utility>
#include <ReportMessages.grpc.pb.h>
#include <report_handled_move_reason.h>
#include <DisciplinaryActionType.grpc.pb.h>

#include "base_collection_object.h"
#include "HandleReports.pb.h"

struct ReportsLog {
    bsoncxx::oid account_oid; //"iId"; //OID; this will be the account that reported this user
    ReportReason reason{-1}; //"iRr"; //int32; this will be the reason passed back from the device; this is just the ReportReason enum from ReportMessages.proto
    std::string message; //"sRm"; //string; this will be the message passed back from the device about 'why' the user was reported; NOTE: '~' is empty message
    ReportOriginType report_origin{-1}; //"rEo"; //int32; where the message was sent from follows ReportOriginType enum from UserMatchOptions.proto
    std::string chat_room_id; //"cRi"; //string; this will be the message chat room id if this was sent from a chat room in any way (could have been from chat room fragment, chat room info fragment or member info fragment)
    std::string message_uuid; //"mUu"; //string; this will be the message uuid if REPORT_ORIGIN == ReportOriginType::REPORT_ORIGIN_CHAT_ROOM_MESSAGE
    bsoncxx::types::b_date timestamp_submitted{std::chrono::milliseconds{-1}}; //"dRt"; //Date; this will be the time the report was submitted

    ReportsLog() = delete;

    ReportsLog(
            bsoncxx::oid _account_oid,
            ReportReason _reason,
            std::string _message,
            ReportOriginType _report_origin,
            std::string _chat_room_id,
            std::string _message_uuid,
            bsoncxx::types::b_date _timestamp_submitted
    ) :
            account_oid(_account_oid),
            reason(_reason),
            message(std::move(_message)),
            report_origin(_report_origin),
            chat_room_id(std::move(_chat_room_id)),
            message_uuid(std::move(_message_uuid)),
            timestamp_submitted(_timestamp_submitted) {}

    ~ReportsLog() = default;

    ReportsLog(const ReportsLog& other) {
        *this = other;
    }

    ReportsLog(ReportsLog&& other) noexcept {
        account_oid = other.account_oid;
        reason = other.reason;
        message = std::move(other.message);
        report_origin = other.report_origin;
        chat_room_id = std::move(other.chat_room_id);
        message_uuid = std::move(other.message_uuid);
        timestamp_submitted = other.timestamp_submitted;
    }

    ReportsLog& operator=(const ReportsLog& other) = default;
};

class OutstandingReports : public BaseCollectionObject {
public:
    bsoncxx::types::b_date timestamp_limit_reached = DEFAULT_DATE; //"tLr"; //mongoDB Date; when the array hits the number for a proper report, this timestamp will be set otherwise set to -1 (collection indexed by this value)
    bsoncxx::types::b_date checked_out_end_time_reached = DEFAULT_DATE; //"cOe"; //mongoDB Date or missing; when this is withdrawn from the database, this timestamp will be set to some time in the future, after that time has passed, reports will be withdrawn again
    std::vector<ReportsLog> reports_log; //"aRs"; //array of documents; contains documents with report information about why this user was reported

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    [[nodiscard]] handle_reports::ReportedUserInfo convertToReportedUserInfo() const;

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection();

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    OutstandingReports() = default;

    virtual ~OutstandingReports() = default;

    explicit OutstandingReports(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    OutstandingReports(OutstandingReports&& other)  noexcept {
        current_object_oid = other.current_object_oid;
        timestamp_limit_reached = other.timestamp_limit_reached;
        checked_out_end_time_reached = other.timestamp_limit_reached;
        reports_log = std::move(other.reports_log);
    }

    OutstandingReports(const OutstandingReports& other) : BaseCollectionObject(other) {
        *this = other;
    }

    OutstandingReports& operator=(const OutstandingReports& other) {
        if(this == &other) {
            return *this;
        }
        current_object_oid = other.current_object_oid;
        timestamp_limit_reached = other.timestamp_limit_reached;
        checked_out_end_time_reached = other.timestamp_limit_reached;
        reports_log = other.reports_log;

        return *this;
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const OutstandingReports& v);

    bool operator==(const OutstandingReports& other) const;

    bool operator!=(const OutstandingReports& other) const {
        return !(*this == other);
    }

private:

    bool runGetFromCollection(bsoncxx::builder::stream::document& find_doc);

    const std::string OBJECT_CLASS_NAME = "Outstanding Reports";
};

class HandledReports : public BaseCollectionObject {
public:

    struct ReportsArrayElement {
        std::string admin_name; //"aAn"; //string; admin name that handled this 'chunk' of reports (if relevant, "~" if not)
        bsoncxx::types::b_date timestamp_handled; //"aTh"; //mongoDB Date; timestamp admin handled this 'chunk' of reports
        bsoncxx::types::b_date timestamp_limit_reached; //"tLr"; //mongoDB Date; timestamp this 'chunk' of reports passed the limit
        ReportHandledMoveReason report_handled_move_reason; //"hDc"; //int32; enum for command which made this move from OUTSTANDING collection to HANDLED collection follows ReportHandledMoveReason in report_handled_move_reason.h
        DisciplinaryActionTypeEnum disciplinary_action_taken; //"dAt"; //int32 or missing; enum for disciplinary action taken, follows DisciplinaryActionTypeEnum, only set if REPORT_HANDLED_MOVE_REASON == ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN
        std::vector<ReportsLog> reports_log; //"aRs"; //array of documents; this is the extracted version of REPORTS_LOG

        ReportsArrayElement() = delete;

        ReportsArrayElement(
                std::string _admin_name,
                bsoncxx::types::b_date _timestamp_handled,
                bsoncxx::types::b_date _timestamp_limit_reached,
                ReportHandledMoveReason _report_handled_move_reason,
                DisciplinaryActionTypeEnum _disciplinary_action_taken,
                std::vector<ReportsLog> _reports_log
        ) :
                admin_name(std::move(_admin_name)),
                timestamp_handled(_timestamp_handled),
                timestamp_limit_reached(_timestamp_limit_reached),
                report_handled_move_reason(_report_handled_move_reason),
                disciplinary_action_taken(_disciplinary_action_taken) {
            std::copy(_reports_log.begin(), _reports_log.end(), std::back_inserter(reports_log));
        }

        ReportsArrayElement(
                const OutstandingReports& outstanding_reports,
                std::string _admin_name,
                bsoncxx::types::b_date _timestamp_handled,
                ReportHandledMoveReason _report_handled_move_reason,
                DisciplinaryActionTypeEnum _disciplinary_action_taken
        ) :
                admin_name(std::move(_admin_name)),
                timestamp_handled(_timestamp_handled),
                timestamp_limit_reached(outstanding_reports.timestamp_limit_reached),
                report_handled_move_reason(_report_handled_move_reason),
                disciplinary_action_taken(_disciplinary_action_taken) {
            std::copy(outstanding_reports.reports_log.begin(), outstanding_reports.reports_log.end(), std::back_inserter(reports_log));
        }
    };

    std::vector<ReportsArrayElement> handled_reports_info; //"aRs"; //array of documents; contains documents with below fields

    bool saveInfoToDocument(const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val);

    //converts this UserAccountDoc object to a document and saves it to the passed builder
    void convertToDocument(bsoncxx::builder::stream::document& document_result) const;

    bool convertDocumentToClass(const bsoncxx::document::view& user_account_document);

    bool setIntoCollection() override;

    bool getFromCollection(const bsoncxx::oid& findOID) override;

    HandledReports() = default;

    explicit HandledReports(const bsoncxx::oid& find_oid) {
        getFromCollection(find_oid);
    }

    //prints this document
    friend std::ostream& operator<<(std::ostream& output, const HandledReports& v);

    bool operator==(const HandledReports& other) const;

    bool operator!=(const HandledReports& other) const {
        return !(*this == other);
    }

private:
    const std::string OBJECT_CLASS_NAME = "Handled Reports";
};