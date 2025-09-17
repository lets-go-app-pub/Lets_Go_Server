//
// Created by jeremiah on 3/28/21.
//

#include "user_match_options_helper_functions.h"

#include "store_mongoDB_error_and_exception.h"
#include "extract_thumbnail_from_verified_doc.h"
#include "database_names.h"
#include "collection_names.h"
#include "individual_match_statistics_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void updateMatchStatisticsDocument(
        const mongocxx::client& mongo_cpp_client,
        const std::chrono::milliseconds& current_timestamp,
        const std::optional<bsoncxx::oid>& match_statistics_doc_oid,
        ResponseType response_type
) {

    if (match_statistics_doc_oid) {

        mongocxx::database statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
        mongocxx::collection individual_match_stats_collection = statistics_db[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];

        std::string individual_match_status = individual_match_statistics_keys::status_array::status_enum::UN_KNOWN;

        static const std::unordered_map<ResponseType, std::string> LOOKUP_TABLE{
                {ResponseType::USER_MATCH_OPTION_YES, individual_match_statistics_keys::status_array::status_enum::YES},
                {ResponseType::USER_MATCH_OPTION_NO, individual_match_statistics_keys::status_array::status_enum::NO},
                {ResponseType::USER_MATCH_OPTION_BLOCK, individual_match_statistics_keys::status_array::status_enum::BLOCK},
                {ResponseType::USER_MATCH_OPTION_REPORT, individual_match_statistics_keys::status_array::status_enum::REPORT}
        };

        auto match_status_ptr = LOOKUP_TABLE.find(response_type);

        if(match_status_ptr != LOOKUP_TABLE.end()) {
            individual_match_status = match_status_ptr->second;
        }
        //else {} // the error here is expected to be stored before this function is called

        std::optional<std::string> update_exception_string;
        mongocxx::stdx::optional<mongocxx::result::update> update_stats_doc;
        try {

            //NOTE: Not upserting here, the document would have practically no information and more checks would
            // need to be done when extracting info.
            //Update individual match statistics document to show which option the user selected.
            update_stats_doc = individual_match_stats_collection.update_one(
                document{}
                    << "_id" << *match_statistics_doc_oid
                << finalize,
                document{}
                    << "$push" << open_document
                        << individual_match_statistics_keys::STATUS_ARRAY << open_document
                            << individual_match_statistics_keys::status_array::STATUS << individual_match_status
                            << individual_match_statistics_keys::status_array::RESPONSE_TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                        << close_document
                    << close_document
                << finalize
            );
        }
        catch (const mongocxx::logic_error& e) {
            update_exception_string = e.what();
        }

        if (!update_stats_doc || update_stats_doc->modified_count() != 1) { //if update failed
            const std::string error_string = "Update to individual match statistics collection failed.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "OID_used", *match_statistics_doc_oid,
                    "individualMatchStatus", individual_match_status
            );

            //NOTE: This is just the statistics update, the user can still receive the result (ie ok to continue here).
        }
    }
}
