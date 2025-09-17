//
// Created by jeremiah on 11/4/21.
//

#pragma once

#include <optional>
#include <functional>

#include <store_mongoDB_error_and_exception.h>

#include <HandleErrors.grpc.pb.h>
#include <mongocxx/collection.hpp>

#include <mongocxx/exception/operation_exception.hpp>

bool errorParametersGood(
        const handle_errors::ErrorParameters& error_parameters,
        const std::function<void(const std::string& /*error_message*/)>& store_error_message);

//Moves documents from FRESH_ERRORS_COLLECTION to HANDLED_ERRORS_COLLECTION based on the match_doc passed.
//This function is idempotent.
bool mergeErrorsToHandled(
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::collection& fresh_errors_collection,
        const std::string& admin_name,
        ErrorHandledMoveReason error_handled_reason,
        const std::string& reason_for_description,
        const bsoncxx::document::view& match_doc,
        const std::function<void(const std::string& /*error_message*/)>& return_error_to_client
        );

//Deletes documents based on the passed filter.
//This function is idempotent.
template <bool delete_many>
bool deleteFromErrors(
        const std::function<void(const std::string& /*error_message*/)>& return_error_to_client,
        mongocxx::collection& fresh_errors_collection,
        const bsoncxx::document::view& delete_doc
) {

    mongocxx::stdx::optional<mongocxx::result::delete_result> delete_error_doc;
    std::optional<std::string> delete_error_exception_string;
    try {
        delete_error_doc = delete_many
                           ? fresh_errors_collection.delete_many(delete_doc)
                           : fresh_errors_collection.delete_one(delete_doc);
    } catch (const mongocxx::logic_error& e) {
        delete_error_exception_string = e.what();
    } catch (const mongocxx::operation_exception& e) {
        delete_error_exception_string = e.what();
    }

    if (!delete_error_doc) {
        const std::string error_string = "MongoDB result was not set after 'Delete' query.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                delete_error_exception_string, error_string,
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "document_passed", delete_doc
        );

        return_error_to_client(std::string(error_string).append("\n").append(*delete_error_exception_string));
        return false;
    }

    return true;
}

template <bool delete_many>
bool setMatchDocToHandled(
        const std::string& reason_for_description,
        const std::function<void(const std::string& /*error_message*/)>& return_error_to_client,
        const std::string& admin_name,
        mongocxx::collection& fresh_errors_collection,
        const std::chrono::milliseconds& current_timestamp,
        const bsoncxx::document::value& match_doc
) {

    //NOTE: The functions here both apply idempotent operations and so there is no need for a transaction.

    if(!mergeErrorsToHandled(
            current_timestamp,
            fresh_errors_collection,
            admin_name,
            ERROR_HANDLED_REASON_SINGLE_ITEM_DELETED,
            reason_for_description,
            match_doc.view(),
            return_error_to_client)
    ) {
        //error already stored
        return false;
    }

    if(!deleteFromErrors<delete_many>(
            return_error_to_client,
            fresh_errors_collection,
            match_doc)
    ) {
        //error already stored
        return false;
    }

    return true;
}