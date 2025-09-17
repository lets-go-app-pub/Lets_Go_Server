//
// Created by jeremiah on 11/8/21.
//

#include "move_user_account_statistics_document.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_statistics_documents_completed_keys.h"
#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <store_mongoDB_error_and_exception.h>
#include <build_user_statistics_document.h>
#include <session_to_run_functions.h>

//This function is meant to be run inside a try catch block for mongodb logic errors.
void insertDocumentToStatistics(
        const bsoncxx::oid& user_account_oid,
        mongocxx::stdx::optional<mongocxx::cursor>& merge_result,
        const std::optional<std::string>& exception_string,
        mongocxx::collection& user_account_statistics_collection);

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool moveUserAccountStatisticsDocument(
        mongocxx::client& mongo_cpp_client,
        const bsoncxx::oid& user_account_oid,
        mongocxx::collection& user_accounts_statistics_collection,
        const std::chrono::milliseconds& currentTimestamp,
        bool create_new_statistics_document,
        mongocxx::client_session* session
) {

    auto match_user_account = document{}
            << "_id" << user_account_oid
            << finalize;

    mongocxx::stdx::optional<mongocxx::cursor> merge_result;
    std::optional<std::string> exception_string;
    try {

        mongocxx::pipeline pipeline;

        pipeline.match(match_user_account.view());

        pipeline.add_fields(document{}
                << user_account_statistics_documents_completed_keys::PREVIOUS_ID << "$_id"
                << user_account_statistics_documents_completed_keys::TIMESTAMP_MOVED << bsoncxx::types::b_date{currentTimestamp}
            << finalize);

        //cannot have the "_id" inside the field otherwise multiple of this user's user_account_oid
        // could exist at a time, however need all the other fields, so ONLY project out the _id
        pipeline.project(document{}
                                    << "_id" << 0
                                 << finalize);

        if (session == nullptr) {

            //NOTE: Cannot use pipeline stage merge inside a transaction.
            pipeline.merge(document{}
                << "into" << open_document
                    << "db" << database_names::INFO_FOR_STATISTICS_DATABASE_NAME
                    << "coll" << collection_names::USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME
                << close_document
            << finalize);

            merge_result = user_accounts_statistics_collection.aggregate(pipeline);
        } else {

            mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
            mongocxx::collection user_account_statistics_collection = info_for_statistics_db[collection_names::USER_ACCOUNT_STATISTICS_DOCUMENTS_COMPLETED_COLLECTION_NAME];

            merge_result = aggregate_optional_session(
                    session,
                    user_accounts_statistics_collection,
                    pipeline
            );

            //error will be handled below if !merge_result
            if (merge_result) {
                insertDocumentToStatistics(
                        user_account_oid,
                        merge_result,
                        exception_string,
                        user_account_statistics_collection
                );
            }
        }
    } catch (const mongocxx::logic_error& e) {
        exception_string = e.what();
    }

    if (!merge_result) {
        std::string error_string = "Moving a user statistics document to the 'completed' collection failed";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID_used", user_account_oid);

        return false;
    }

    //NOTE: must use the merge_result in the code or the aggregation could simply not happen
    for (const auto& doc : *merge_result) {
        std::string error_string = "$merge pipeline stage returned a result which should never happen.";
        std::optional<std::string> dummy_string;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummy_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID Used", user_account_oid,
                                      "Returned Document", doc
        );

        //NOTE: ok to continue here
    }

    mongocxx::stdx::optional<mongocxx::result::delete_result> delete_result;
    try {
        delete_result =  delete_one_optional_session(
                session,
                user_accounts_statistics_collection,
                match_user_account.view()
        );
    } catch (const mongocxx::logic_error& e) {
        exception_string = e.what();
    }

    if (!delete_result || delete_result->deleted_count() < 1) {

        std::string error_string = "Deleting a user statistics document failed";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID_used", user_account_oid,
                                      "delete_result", delete_result ? "true" : "false",
                                      "deleted_count",
                                      std::to_string(delete_result ? -1 : delete_result->deleted_count())
        );

        return false;
    }

    if (!create_new_statistics_document) {
        return true;
    }

    bsoncxx::stdx::optional<mongocxx::v_noabi::result::insert_one> insert_statistics_doc_success;
    auto user_account_statistics_doc = buildUserStatisticsDocument(user_account_oid);
    try {
        //upsert new user statistics document
        insert_statistics_doc_success = insert_one_optional_session(
                session,
                user_accounts_statistics_collection,
                user_account_statistics_doc.view()
        );
    }
    catch (const mongocxx::logic_error& e) {
        exception_string = std::string(e.what());
    }

    if (!insert_statistics_doc_success || insert_statistics_doc_success->result().inserted_count() < 1) {

        std::string error_string = "Inserting the new blank user statistics document failed";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID_used", user_account_oid,
                                      "insert_doc_successful", insert_statistics_doc_success ? "true" : "false",
                                      "inserted_count", std::to_string(
                        insert_statistics_doc_success ? -1 : insert_statistics_doc_success->result().inserted_count())
        );

        return false;
    }

    return true;
}

void insertDocumentToStatistics(
        const bsoncxx::oid& user_account_oid,
        mongocxx::stdx::optional<mongocxx::cursor>& merge_result,
        const std::optional<std::string>& exception_string,
        mongocxx::collection& user_account_statistics_collection) {

    if(merge_result->begin() == merge_result->end()) {
        std::string error_string = "When extracting a user statistics document the document was not found";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID_used", user_account_oid);

        //nothing to add, end the function
        return;
    }

    bsoncxx::document::view user_doc_view;
    int num_results = 0;
    for(const auto& val : *merge_result) {
        user_doc_view = val;
        num_results++;
    }

    if(num_results != 1) {
        std::string error_string = "Too many matches were found for user_account_statistics.";
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID_used", user_account_oid,
                                      "num_results", std::to_string(num_results));

        //can continue here, will only upsert one
    }

    auto result = user_account_statistics_collection.insert_one(user_doc_view);

    if(result->result().inserted_count() != 1) {

        std::string error_string = "Moving a user statistics document to the 'completed' collection failed\n inserted_oid:" +
                result->inserted_id().get_oid().value.to_string();
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      exception_string, error_string,
                                      "database", database_names::ACCOUNTS_DATABASE_NAME,
                                      "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                                      "ObjectID_used", user_account_oid,
                                      "inserted_count", std::to_string(result->result().inserted_count()),
                                      "num_results", std::to_string(num_results));

        //can continue here, damage is already done
    }

}
