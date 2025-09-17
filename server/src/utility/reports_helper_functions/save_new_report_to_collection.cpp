//
// Created by jeremiah on 9/19/21.
//

#include "report_helper_functions.h"

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <global_bsoncxx_docs.h>

#include "store_mongoDB_error_and_exception.h"

#include "database_names.h"
#include "collection_names.h"
#include "outstanding_reports_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool saveNewReportToCollection(
        mongocxx::client& mongo_cpp_client,
        const std::chrono::milliseconds& currentTimestamp,
        const bsoncxx::oid& reporting_user_account_oid,
        const bsoncxx::oid& reported_user_account_oid,
        ReportReason report_reason,
        ReportOriginType report_origin,
        const std::string& report_message,
        const std::string& chat_room_id,
        const std::string& message_uuid
) {

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];

    mongocxx::pipeline pipeline;

    bsoncxx::document::value report_log_element = document{}
        << outstanding_reports_keys::reports_log::ACCOUNT_OID << reporting_user_account_oid
        << outstanding_reports_keys::reports_log::REASON << report_reason
        << outstanding_reports_keys::reports_log::MESSAGE << report_message
        << outstanding_reports_keys::reports_log::REPORT_ORIGIN << report_origin
        << outstanding_reports_keys::reports_log::CHAT_ROOM_ID << chat_room_id
        << outstanding_reports_keys::reports_log::MESSAGE_UUID << message_uuid
        << outstanding_reports_keys::reports_log::TIMESTAMP_SUBMITTED << bsoncxx::types::b_date{currentTimestamp}
    << finalize;

    pipeline.add_fields(
            buildUpdateReportLogDocument(
                    reported_user_account_oid,
                    reporting_user_account_oid,
                    currentTimestamp,
                    report_log_element.view()
            )
    );

    mongocxx::stdx::optional<mongocxx::result::update> update_doc;
    try {
        mongocxx::options::update opts;

        opts.upsert(true);

        //find and update reported users' outstanding reports document
        update_doc = outstanding_reports_collection.update_one(
                document{}
                    << "_id" << reported_user_account_oid
                << finalize,
                pipeline,
                opts
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "update_pipeline", pipeline.view_array()
        );

        return false;
    }

    if (!update_doc) {
        const std::string error_string = "Failed to update outstanding reports collection.\n'!update_doc' returned.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "update_pipeline", pipeline.view_array()
        );

        return false;
    }
    //It is possible to NOT modify/upsert the document if reporting_user_account_oid already existed inside
    // REPORTS_LOG.
    else if (update_doc->matched_count() != 1 && update_doc->modified_count() != 1 && update_doc->result().upserted_count() != 1) {
        const std::string error_string = "Failed to update outstanding reports collection.\nModified count and upserted were not equal to 1.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "update_pipeline", pipeline.view_array(),
                "modified_count", std::to_string(update_doc->modified_count()),
                "upserted_count", std::to_string(update_doc->result().upserted_count())
        );

        return false;
    }

    return true;
}