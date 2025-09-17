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
#include <extract_data_from_bsoncxx.h>
#include <utility_general_functions.h>
#include <individual_match_statistics_keys.h>

#include "info_for_statistics_objects.h"
#include "request_statistics_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void IndividualMatchStatisticsDoc::generateRandomValues(const bsoncxx::oid& matched_user_oid) {

    current_object_oid = bsoncxx::oid{};

    int status_array_length = rand() % 3;

    for(int i = 0; i < status_array_length; ++i) {
        sent_timestamp.emplace_back(
                bsoncxx::types::b_date{std::chrono::milliseconds{rand() % getCurrentTimestamp().count()}}
        );
    }

    //Status array can be one element shorted than sent timestamps because the matches that are
    // sent are not always returned.
    if(status_array_length < 2) {
        status_array_length -= rand() % 2;
    }

    for(int i = 0; i < status_array_length; ++i) {
        status_array.emplace_back(StatusArray(true));
    }

    day_timestamp = rand() % getCurrentTimestamp().count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY;

    user_extracted_list_element_document.generateRandomValues();

    matched_user_account_document.getFromCollection(matched_user_oid);
}

//converts this IndividualMatchStatisticsDoc object to a document and saves it to the passed builder
void IndividualMatchStatisticsDoc::convertToDocument(
        bsoncxx::builder::stream::document& document_result
        ) const {

    //NOTE: Let this go first, ExtractMatchAccountInfoAndWriteToClientTesting, algorithm_matched_list relies on it when
    // comparing the documents.
    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }

    bsoncxx::builder::basic::array status_array_arr;

    for (const StatusArray& status_array_ele : status_array) {
        status_array_arr.append(
                document{}
                        << individual_match_statistics_keys::status_array::STATUS << status_array_ele.status
                        << individual_match_statistics_keys::status_array::RESPONSE_TIMESTAMP << status_array_ele.response_timestamp
                        << finalize
        );
    }

    bsoncxx::builder::basic::array sent_timestamp_arr;

    for (const bsoncxx::types::b_date& timestamp : sent_timestamp) {
        sent_timestamp_arr.append(timestamp);
    }

    bsoncxx::builder::stream::document extracted_list_document_result;

    user_extracted_list_element_document.convertToDocument(extracted_list_document_result);

    bsoncxx::builder::stream::document matched_user_account_document_builder;

    matched_user_account_document.convertToDocument(
            matched_user_account_document_builder
            );

    document_result
            << individual_match_statistics_keys::STATUS_ARRAY << status_array_arr
            << individual_match_statistics_keys::SENT_TIMESTAMP << sent_timestamp_arr
            << individual_match_statistics_keys::DAY_TIMESTAMP << bsoncxx::types::b_int64{day_timestamp}
            << individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT << extracted_list_document_result
            << individual_match_statistics_keys::MATCHED_USER_ACCOUNT_DOCUMENT << matched_user_account_document_builder;

}

bool IndividualMatchStatisticsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection individual_match_statistics_collection = info_for_statistics_db[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            individual_match_statistics_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                    << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                    << finalize,
                    updateOptions
            );
        } else {

            auto idVar = individual_match_statistics_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in IndividualMatchStatisticsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in IndividualMatchStatisticsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool IndividualMatchStatisticsDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool IndividualMatchStatisticsDoc::getFromCollection(const bsoncxx::oid& find_oid) {
    return getFromCollection(
            document{}
                    << "_id" << find_oid
            << finalize
    );
}

bool IndividualMatchStatisticsDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection individual_match_statistics_collection = info_for_statistics_db[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = individual_match_statistics_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in IndividualMatchStatisticsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in IndividualMatchStatisticsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool IndividualMatchStatisticsDoc::saveInfoToDocument(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function IndividualMatchStatisticsDoc::saveInfoToDocument\n";
        return false;
    }
}

bool IndividualMatchStatisticsDoc::convertDocumentToClass(const bsoncxx::v_noabi::document::view& document_parameter) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                document_parameter,
                "_id"
        );

        bsoncxx::array::view status_array_arr = extractFromBsoncxx_k_array(
                document_parameter,
                individual_match_statistics_keys::STATUS_ARRAY
        );

        status_array.clear();
        for (const auto& ele : status_array_arr) {
            bsoncxx::document::view status_array_doc = ele.get_document().value;

            status_array.emplace_back(
                    StatusArray(
                            status_array_doc[individual_match_statistics_keys::status_array::STATUS].get_string().value.to_string(),
                            status_array_doc[individual_match_statistics_keys::status_array::RESPONSE_TIMESTAMP].get_date()
                    )
            );
        }

        bsoncxx::array::view sent_timestamp_arr = extractFromBsoncxx_k_array(
                document_parameter,
                individual_match_statistics_keys::SENT_TIMESTAMP
        );

        sent_timestamp.clear();
        for (const auto& ele : sent_timestamp_arr) {
            sent_timestamp.emplace_back(ele.get_date());
        }

        day_timestamp = extractFromBsoncxx_k_int64(
                document_parameter,
                individual_match_statistics_keys::DAY_TIMESTAMP
        );

        bsoncxx::document::view user_extracted_list_element_doc = extractFromBsoncxx_k_document(
                document_parameter,
                individual_match_statistics_keys::USER_EXTRACTED_LIST_ELEMENT_DOCUMENT
        );

        user_extracted_list_element_document = extractMatchingElementToClass(user_extracted_list_element_doc);

        bsoncxx::document::view matched_user_account_document_view = extractFromBsoncxx_k_document(
                document_parameter,
                individual_match_statistics_keys::MATCHED_USER_ACCOUNT_DOCUMENT
        );

        matched_user_account_document.convertDocumentToClass(
                matched_user_account_document_view
        );

    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in IndividualMatchStatisticsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool IndividualMatchStatisticsDoc::operator==(const IndividualMatchStatisticsDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (status_array.size() == other.status_array.size()) {
        for(size_t i = 0; i < status_array.size(); i++) {
            checkForEquality(
                    status_array[i].status,
                    other.status_array[i].status,
                    "STATUS_ARRAY.status",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    status_array[i].response_timestamp.value.count(),
                    other.status_array[i].response_timestamp.value.count(),
                    "STATUS_ARRAY.response_timestamp",
                    OBJECT_CLASS_NAME,
                    return_value
                    );
        }
    } else {
        checkForEquality(
                status_array.size(),
                other.status_array.size(),
                "STATUS_ARRAY.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    if (sent_timestamp.size() == other.sent_timestamp.size()) {
        for(size_t i = 0; i < sent_timestamp.size(); i++) {
            checkForEquality(
                    sent_timestamp[i].value.count(),
                    other.sent_timestamp[i].value.count(),
                    "SENT_TIMESTAMP",
                    OBJECT_CLASS_NAME,
                    return_value
                    );
        }
    } else {
        checkForEquality(
                sent_timestamp.size(),
                other.sent_timestamp.size(),
                "SENT_TIMESTAMP.size()",
                OBJECT_CLASS_NAME,
                return_value
                );
    }

    checkForEquality(
            day_timestamp,
            other.day_timestamp,
            "DAY_TIMESTAMP",
            OBJECT_CLASS_NAME,
            return_value
            );

    checkMatchingElementEquality(
            user_extracted_list_element_document,
            other.user_extracted_list_element_document,
            "USER_EXTRACTED_LIST_ELEMENT_DOCUMENT",
            OBJECT_CLASS_NAME,
            return_value
            );

    if(matched_user_account_document != other.matched_user_account_document) {
        return_value = false;
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const IndividualMatchStatisticsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
