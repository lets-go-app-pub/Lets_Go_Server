//
// Created by jeremiah on 9/20/21.
//

#include "report_helper_functions.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <extract_data_from_bsoncxx.h>
#include <session_to_run_functions.h>

#include "store_mongoDB_error_and_exception.h"

#include "database_names.h"
#include "collection_names.h"
#include "outstanding_reports_keys.h"
#include "handled_reports_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool moveOutstandingReportsToHandled(
        mongocxx::client& mongo_cpp_client,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::oid& user_account_oid,
        const std::string& admin_name,
        ReportHandledMoveReason handled_move_reason,
        mongocxx::client_session* session,
        DisciplinaryActionTypeEnum disciplinary_action_taken
) {

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];
    mongocxx::collection handled_reports_collection = reports_db[collection_names::HANDLED_REPORTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> outstanding_doc;
    try {
        //find and deleted user outstanding reports document
        outstanding_doc = find_one_and_delete_optional_session(
                session,
                outstanding_reports_collection,
                document{}
                    << "_id" << user_account_oid
                << finalize
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::REPORTS_DATABASE_NAME,
                "collection", collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME,
                "user_account_oid", user_account_oid
        );

        return false;
    }

    //This should only happen when another function already handled the report.
    if (!outstanding_doc) {
        return true;
    }

    std::unique_ptr<bsoncxx::types::b_date> limit_reached_timestamp;
    bsoncxx::array::view reports_log;

    try {

        const bsoncxx::document::view outstanding_doc_view = outstanding_doc->view();

        limit_reached_timestamp = std::make_unique<bsoncxx::types::b_date>(
                extractFromBsoncxx_k_date(
                        outstanding_doc_view,
                        outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED)
        );

        reports_log = extractFromBsoncxx_k_array(
                outstanding_doc_view,
                outstanding_reports_keys::REPORTS_LOG);

    } catch (const ErrorExtractingFromBsoncxx& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::REPORTS_DATABASE_NAME,
                "collection", collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME,
                "user_account_oid", user_account_oid
        );

        return false;
    }

    bsoncxx::builder::stream::document handled_document;

    handled_document
            << handled_reports_keys::reports_array::ADMIN_NAME << open_document
                << "$literal" << admin_name
            << close_document
            << handled_reports_keys::reports_array::TIMESTAMP_HANDLED << bsoncxx::types::b_date{current_timestamp}
            << handled_reports_keys::reports_array::TIMESTAMP_LIMIT_REACHED << *limit_reached_timestamp
            << handled_reports_keys::reports_array::REPORT_HANDLED_MOVE_REASON << handled_move_reason;

    if (handled_move_reason == ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN) {
        handled_document
                << handled_reports_keys::reports_array::DISCIPLINARY_ACTION_TAKEN << disciplinary_action_taken;
    }

    handled_document
        << handled_reports_keys::reports_array::REPORTS_LOG << reports_log;

    mongocxx::pipeline pipeline;

    //if the element (the document) exists concat the array, else create the array
    pipeline.add_fields(document{}
            << "_id" << user_account_oid
            << handled_reports_keys::HANDLED_REPORTS_INFO << open_document
                << "$cond" << open_document

                    << "if" << open_document
                        << "$eq" << open_array
                            << open_document
                                << "$type" << "$" + handled_reports_keys::HANDLED_REPORTS_INFO
                            << close_document
                            << "array"
                        << close_array
                    << close_document
                    << "then" << open_document
                        << "$concatArrays" << open_array
                            << "$" + handled_reports_keys::HANDLED_REPORTS_INFO
                            << open_array
                                << handled_document.view()
                            << close_array
                        << close_array
                    << close_document
                    << "else" << open_array
                        << handled_document.view()
                    << close_array

                << close_document
            << close_document
        << finalize);

    mongocxx::stdx::optional<mongocxx::result::update> update_doc;
    try {

        mongocxx::options::update opts;

        opts.upsert(true);

        //update report to handled
        update_doc = update_one_optional_session(
                session,
                handled_reports_collection,
                document{}
                    << "_id" << user_account_oid
                << finalize,
                pipeline,
                opts
                );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::REPORTS_DATABASE_NAME,
                "collection", collection_names::HANDLED_REPORTS_COLLECTION_NAME,
                "pipeline", pipeline.view_array()
        );

        return false;
    }

    if(!update_doc) {
        const std::string error_string = "Error updating handled_report_collection. Returned !update_doc.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::REPORTS_DATABASE_NAME,
                "collection", collection_names::HANDLED_REPORTS_COLLECTION_NAME,
                "pipeline", pipeline.view_array()
        );

        return false;
    } else if(update_doc->modified_count() != 1 && update_doc->result().upserted_count() != 1) {
        const std::string error_string = "Error updating handled_report_collection. Did not modify or upsert any documents.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::REPORTS_DATABASE_NAME,
                "collection", collection_names::HANDLED_REPORTS_COLLECTION_NAME,
                "pipeline", pipeline.view_array(),
                "modified_count", std::to_string(update_doc->modified_count()),
                "upserted_count", std::to_string(update_doc->result().upserted_count())
        );

        return false;
    }

    return true;

}