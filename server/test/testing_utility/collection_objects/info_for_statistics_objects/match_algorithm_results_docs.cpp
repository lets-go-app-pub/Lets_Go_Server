//
// Created by jeremiah on 6/4/22.
//

#include <connection_pool_global_variable.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <mongocxx/exception/exception.hpp>
#include <gtest/gtest.h>
#include <user_pictures_keys.h>
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <match_algorithm_results_keys.h>

#include "info_for_statistics_objects.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//converts this MatchAlgorithmResultsDoc object to a document and saves it to the passed builder
void MatchAlgorithmResultsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    document_result
            << match_algorithm_results_keys::GENERATED_FOR_DAY << bsoncxx::types::b_int64{generated_for_day}
            << match_algorithm_results_keys::ACCOUNT_TYPE << account_type
            << match_algorithm_results_keys::CATEGORIES_TYPE << categories_type
            << match_algorithm_results_keys::CATEGORIES_VALUE << categories_value
            << match_algorithm_results_keys::NUM_YES_YES << bsoncxx::types::b_int64{num_yes_yes}
            << match_algorithm_results_keys::NUM_YES_NO << bsoncxx::types::b_int64{num_yes_no}
            << match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT << bsoncxx::types::b_int64{
            num_yes_block_and_report}
            << match_algorithm_results_keys::NUM_YES_INCOMPLETE << bsoncxx::types::b_int64{num_yes_incomplete}
            << match_algorithm_results_keys::NUM_NO << bsoncxx::types::b_int64{num_no}
            << match_algorithm_results_keys::NUM_BLOCK_AND_REPORT << bsoncxx::types::b_int64{num_block_and_report}
            << match_algorithm_results_keys::NUM_INCOMPLETE << bsoncxx::types::b_int64{num_incomplete};

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool MatchAlgorithmResultsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection match_algorithm_results_collection = info_for_statistics_db[collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME];

    bsoncxx::v_noabi::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            match_algorithm_results_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = match_algorithm_results_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in MatchAlgorithmResultsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmResultsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool MatchAlgorithmResultsDoc::getFromCollection(const bsoncxx::oid& findOID) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection match_algorithm_results_collection = info_for_statistics_db[collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = match_algorithm_results_collection.find_one(
            document{}
                << "_id" << findOID
            << finalize
        );
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in MatchAlgorithmResultsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmResultsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool MatchAlgorithmResultsDoc::saveInfoToDocument(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val
) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function MatchAlgorithmResultsDoc::saveInfoToDocument\n";
        return false;
    }
}

bool MatchAlgorithmResultsDoc::convertDocumentToClass(
        const bsoncxx::document::view& document_parameter
) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                document_parameter,
                "_id"
        );

        account_type = UserAccountType(
                extractFromBsoncxx_k_int32(
                        document_parameter,
                        match_algorithm_results_keys::ACCOUNT_TYPE
                )
        );

        generated_for_day = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::GENERATED_FOR_DAY
        );

        categories_type = AccountCategoryType(
                extractFromBsoncxx_k_int32(
                        document_parameter,
                        match_algorithm_results_keys::CATEGORIES_TYPE
                )
        );

        categories_value = extractFromBsoncxx_k_int32(
                document_parameter,
                match_algorithm_results_keys::CATEGORIES_VALUE
        );

        num_yes_yes = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_YES_YES
        );

        num_yes_no = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_YES_NO
        );

        num_yes_block_and_report = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT
        );

        num_yes_incomplete = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_YES_INCOMPLETE
        );

        num_no = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_NO
        );

        num_block_and_report = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_BLOCK_AND_REPORT
        );

        num_incomplete = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_results_keys::NUM_INCOMPLETE
        );

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmResultsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool MatchAlgorithmResultsDoc::operator==(const MatchAlgorithmResultsDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            account_type,
            other.account_type,
            "ACCOUNT_TYPE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            generated_for_day,
            other.generated_for_day,
            "GENERATED_FOR_DAY",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            categories_type,
            other.categories_type,
            "CATEGORIES_TYPE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            categories_value,
            other.categories_value,
            "CATEGORIES_VALUE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_yes_yes,
            other.num_yes_yes,
            "NUM_YES_YES",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_yes_no,
            other.num_yes_no,
            "NUM_YES_NO",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_yes_block_and_report,
            other.num_yes_block_and_report,
            "NUM_YES_BLOCK_AND_REPORT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_yes_incomplete,
            other.num_yes_incomplete,
            "NUM_YES_INCOMPLETE",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_no,
            other.num_no,
            "NUM_NO",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_block_and_report,
            other.num_block_and_report,
            "NUM_BLOCK_AND_REPORT",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            num_incomplete,
            other.num_incomplete,
            "STATUS_ARRAY",
            OBJECT_CLASS_NAME,
            return_value
    );

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const MatchAlgorithmResultsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
