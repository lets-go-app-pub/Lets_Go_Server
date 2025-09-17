//
// Created by jeremiah on 11/4/21.
//

#include "handle_errors_helper_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "handled_errors_keys.h"

#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <store_mongoDB_error_and_exception.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool mergeErrorsToHandled(
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::collection& fresh_errors_collection,
        const std::string& admin_name,
        const ErrorHandledMoveReason error_handled_reason,
        const std::string& reason_for_description,
        const bsoncxx::document::view& match_doc,
        const std::function<void(const std::string& /*error_message*/)>& return_error_to_client
) {

    mongocxx::pipeline pipeline;
    mongocxx::stdx::optional<mongocxx::cursor> copy_error_doc;
    std::optional<std::string> pipeline_exception_string;
    try {
        pipeline.match(match_doc);

        pipeline.add_fields(
                document{}
                        << handled_errors_keys::ADMIN_NAME << open_document
                            << "$literal" << admin_name
                        << close_document
                        << handled_errors_keys::TIMESTAMP_HANDLED << bsoncxx::types::b_date{current_timestamp}
                        << handled_errors_keys::ERROR_HANDLED_MOVE_REASON << error_handled_reason
                        << handled_errors_keys::ERRORS_DESCRIPTION << open_document
                            << "$literal" << reason_for_description
                        << close_document
                << finalize
        );

        pipeline.merge(
                document{}
                        << "into" << collection_names::HANDLED_ERRORS_COLLECTION_NAME
                        << "on" << "_id"
                        << "whenMatched" << "merge"
                        << "whenNotMatched" << "insert"
                << finalize
        );

        mongocxx::options::aggregate aggregation_options;

        //Pipeline stages have a limit of 100 megabytes of RAM. If a stage exceeds this limit, MongoDB will produce an
        // error. To allow for the handling of large datasets, use the allowDiskUse option to enable aggregation
        // pipeline stages to write data to temporary files.
        //The aggregation_options should not be used unless necessary, so it doesn't hurt to have it on. Otherwise,
        // could randomly get errors.
        aggregation_options.allow_disk_use(true);

        //This operation is not run inside a session, there are two reasons for that.
        // 1) A large amount of documents being inserted (via the merge stage) should not be done inside a transaction.
        // 2) This operation simply moves documents from one collection to another. Whether it fails to move them OR
        // succeeds in moving them it will still be irrelevant to other steps in the error system. This is because
        // if the operation as a whole fails, it can retry and there will simply be nothing to move. If the operation
        // as a whole succeeds this will succeed with it.
        copy_error_doc = fresh_errors_collection.aggregate(pipeline, aggregation_options);
    } catch (const mongocxx::logic_error& e) {
        pipeline_exception_string = e.what();
    }

    if (!copy_error_doc) {
        const std::string error_string = "MongoDB cursor was not set after aggregation query.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                pipeline_exception_string, error_string,
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "document_passed", pipeline.view_array()
        );

        return_error_to_client(std::string(error_string).append("\n").append(*pipeline_exception_string));
        return false;
    }

    //NOTE: Must leave this loop here or the $merge doesn't output to the collection at
    // all for some reason ($merge does not return any documents). The block could be left
    // empty however that leads to a warning. So putting an error inside it.
    for (const auto& d : *copy_error_doc) {
        const std::string error_string = "A $merge operation returned at least one document which should never happen.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "document_passed", pipeline.view_array(),
                "document_received", d
        );
    }

    return true;
}