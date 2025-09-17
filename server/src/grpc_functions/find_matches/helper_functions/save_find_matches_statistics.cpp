//
// Created by jeremiah on 3/27/21.
//

#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/concatenate.hpp>
#include <store_info_to_user_statistics.h>

#include "find_matches_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_statistics_keys.h"
#include "match_algorithm_statistics_keys.h"
#include "individual_match_statistics_keys.h"
#include "request_statistics_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void saveFindMatchesStatistics(
        UserAccountValues& user_account_values,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& stats_db,
        mongocxx::database& accounts_db,
        const std::vector<bsoncxx::document::value>& statistics_documents_to_insert,
        const bsoncxx::array::view& update_statistics_documents_query
) {

    bsoncxx::array::view algorithm_match_results = user_account_values.raw_algorithm_match_results;
    if (!algorithm_match_results.empty()) { //if the algorithm ran
        mongocxx::collection match_algorithm_collection = stats_db[collection_names::MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME];

        const bsoncxx::document::value stats_document = document{}
                    << match_algorithm_statistics_keys::TIMESTAMP_RUN << bsoncxx::types::b_date{user_account_values.current_timestamp}
                    << match_algorithm_statistics_keys::END_TIME << bsoncxx::types::b_date{user_account_values.end_of_time_frame_timestamp}
                    << match_algorithm_statistics_keys::ALGORITHM_RUN_TIME << bsoncxx::types::b_int64{user_account_values.algorithm_run_time_ns.count()}
                    << match_algorithm_statistics_keys::QUERY_ACTIVITIES_RUN_TIME << bsoncxx::types::b_int64{user_account_values.check_for_activities_run_time_ns.count()}
                    << match_algorithm_statistics_keys::USER_ACCOUNT_DOCUMENT << user_account_values.user_account_doc_view
                    << match_algorithm_statistics_keys::RESULTS_DOCS << algorithm_match_results
                << finalize;

        std::optional<std::string> insert_exception_string;
        mongocxx::stdx::optional<mongocxx::result::insert_one> insert_match_record;
        try {
            //This saves the match algorithm parameters and return values
            insert_match_record = match_algorithm_collection.insert_one(stats_document.view());
        }
        catch (const mongocxx::logic_error& e) {
            insert_exception_string = e.what();
        }

        if (!insert_match_record || insert_match_record->result().inserted_count() == 0) { //if insert failed
            const std::string error_string = "Inserting match record failed.";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    insert_exception_string, error_string,
                    "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                    "collection", collection_names::MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME,
                    "match_account_doc", user_account_values.user_account_doc_view,
                    "stats_doc", stats_document.view()
            );

            //NOTE: not ending here because this is just for statistics, the function can complete
        }
    }

    mongocxx::collection individual_match_statistics_collection = stats_db[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];

    //store matches statistics
    if (!statistics_documents_to_insert.empty()) {

        std::optional<std::string> insert_statistics_exception_string;
        mongocxx::stdx::optional<mongocxx::result::insert_many> insert_statistics_doc;
        try {
            //upsert document into individual statistics collection
            insert_statistics_doc = individual_match_statistics_collection.insert_many(
                    statistics_documents_to_insert
                    );
        }
        catch (const mongocxx::logic_error& e) {
            insert_statistics_exception_string = e.what();
        }

        if (!insert_statistics_doc ||
            insert_statistics_doc->inserted_count() < (int32_t) statistics_documents_to_insert.size()) { //upsert failed

            int insertedCount = 0;

            if (insert_statistics_doc) {
                insertedCount = (*insert_statistics_doc).inserted_count();
            }

            const std::string error_string = "Inserting statistics documents failed.";

            //not inserting all the documents here because they are a bit large
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    insert_statistics_exception_string, error_string,
                    "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                    "collection", collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME,
                    "single_inserted_doc", statistics_documents_to_insert[0].view(),
                    "num_docs_inserted", std::to_string(insertedCount)
            );

            //NOTE: not ending here because this is just for statistics, the function can complete
        }
    }

    //update previous matches statistics
    if (!update_statistics_documents_query.empty()) {

        std::optional<std::string> insert_statistics_exception_string;
        mongocxx::stdx::optional<mongocxx::result::update> update_statistics_doc;
        try {

            //upsert document into individual statistics collection
            update_statistics_doc = individual_match_statistics_collection.update_many(
                document{}
                    << "_id" << open_document
                        << "$in" << update_statistics_documents_query
                    << close_document
                << finalize,
                document{}
                    << "$push" << open_document
                        << individual_match_statistics_keys::SENT_TIMESTAMP << bsoncxx::types::b_date{user_account_values.current_timestamp}
                    << close_document
                    << "$set" << open_document
                        << individual_match_statistics_keys::DAY_TIMESTAMP << bsoncxx::types::b_int64{user_account_values.current_timestamp.count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY}
                    << close_document
                << finalize
            );
        }
        catch (const mongocxx::logic_error& e) {
            insert_statistics_exception_string = e.what();
        }

        if (!update_statistics_doc
            || update_statistics_doc->matched_count() < (int32_t) statistics_documents_to_insert.size()
            || update_statistics_doc->modified_count() < (int32_t) statistics_documents_to_insert.size()
                ) { //update failed

            int matched_count = 0;
            int modified_count = 0;

            if (update_statistics_doc) {
                matched_count = (*update_statistics_doc).matched_count();
                modified_count = (*update_statistics_doc).modified_count();
            }

            const std::string error_string = "Updating statistics documents failed.";

            //not inserting all the documents here because they are a bit large
            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          insert_statistics_exception_string, error_string,
                                          "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                                          "collection", collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME,
                                          "single_inserted_doc", update_statistics_documents_query,
                                          "matched_count", std::to_string(matched_count),
                                          "modified_count", std::to_string(modified_count)
            );

            //NOTE: not ending here because this is just for statistics, the function can complete
        }
    }

    if (!user_account_values.user_account_doc_view.empty()) { //if the initial location was saved successfully, this document should be set
        auto push_update_doc = document{}
                << user_account_statistics_keys::LOCATIONS << open_document
                    << user_account_statistics_keys::locations::LOCATION << open_array
                        << user_account_values.longitude
                        << user_account_values.latitude
                    << close_array
                    << user_account_statistics_keys::locations::TIMESTAMP << bsoncxx::types::b_date{user_account_values.current_timestamp}
                << close_document
                << finalize;

        std::cout << user_account_values.user_account_oid.to_string() << '\n';

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                user_account_values.user_account_oid,
                push_update_doc,
                user_account_values.current_timestamp
        );
    }
}