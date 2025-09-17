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
#include <match_algorithm_statistics_keys.h>
#include <algorithm_pipeline_field_names.h>

#include "info_for_statistics_objects.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void MatchAlgorithmStatisticsDoc::RawAlgorithmResultsDoc::MatchStatisticsDoc::ActivityStatisticsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::basic::array between_times_arr;

    for (const long time: mongo_pipeline_between_times_array) {
        between_times_arr.append(bsoncxx::types::b_int64{time});
    }

    document_result
            << user_account_keys::categories::INDEX_VALUE << index_value
            << user_account_keys::categories::TYPE << type
            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR << mongo_pipeline_total_user_time
            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH << mongo_pipeline_total_time_for_match
            << algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR << mongo_pipeline_total_overlap_time
            << algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR << between_times_arr;
}

void MatchAlgorithmStatisticsDoc::RawAlgorithmResultsDoc::MatchStatisticsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {
    bsoncxx::builder::basic::array activity_statistics_arr;

    for (const RawAlgorithmResultsDoc::MatchStatisticsDoc::ActivityStatisticsDoc& activity_statistics_doc: activity_statistics) {
        bsoncxx::builder::stream::document activity_stats_builder;
        activity_statistics_doc.convertToDocument(activity_stats_builder);
        activity_statistics_arr.append(activity_stats_builder);
    }

    document_result
                << user_account_keys::LAST_TIME_FIND_MATCHES_RAN << last_time_find_matches_ran
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_MATCH_RAN_VAR << bsoncxx::types::b_int64{mongo_pipeline_time_match_ran}
                << algorithm_pipeline_field_names::MONGO_PIPELINE_EARLIEST_START_TIME_VAR << bsoncxx::types::b_int64{mongo_pipeline_earliest_start_time}
                << algorithm_pipeline_field_names::MONGO_PIPELINE_MAX_POSSIBLE_TIME_VAR << bsoncxx::types::b_int64{mongo_pipeline_max_possible_time}
                << algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR << mongo_pipeline_time_fall_off
                << algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY << mongo_pipeline_category_or_activity_points
                << algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY << mongo_pipeline_inactivity_points_to_subtract
                << user_account_keys::accounts_list::ACTIVITY_STATISTICS << activity_statistics_arr;
}

void MatchAlgorithmStatisticsDoc::RawAlgorithmResultsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::stream::document match_statistics_doc;
    mongo_pipeline_match_statistics.convertToDocument(match_statistics_doc);

    document_result
            << algorithm_pipeline_field_names::MONGO_PIPELINE_DISTANCE_KEY << mongo_pipeline_distance
            << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR << bsoncxx::types::b_int64{mongo_pipeline_match_expiration_time}
            << algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR << mongo_pipeline_final_points
            << algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_STATISTICS_VAR << match_statistics_doc;
}

//converts this MatchAlgorithmStatisticsDoc object to a document and saves it to the passed builder
void MatchAlgorithmStatisticsDoc::convertToDocument(bsoncxx::builder::stream::document& document_result) const {

    bsoncxx::builder::basic::array result_docs_array;

    for(const auto& result_doc : results_docs) {
        bsoncxx::builder::stream::document result_doc_builder;
        result_doc.convertToDocument(result_doc_builder);
        result_docs_array.append(result_doc_builder);
    }

    bsoncxx::builder::stream::document user_account_document_doc;
    user_account_document.convertToDocument(user_account_document_doc);

    document_result
            << match_algorithm_statistics_keys::TIMESTAMP_RUN << timestamp_run
            << match_algorithm_statistics_keys::END_TIME << end_time
            << match_algorithm_statistics_keys::ALGORITHM_RUN_TIME << bsoncxx::types::b_int64{algorithm_run_time}
            << match_algorithm_statistics_keys::QUERY_ACTIVITIES_RUN_TIME << bsoncxx::types::b_int64{query_activities_run_time}
            << match_algorithm_statistics_keys::USER_ACCOUNT_DOCUMENT << user_account_document_doc
            << match_algorithm_statistics_keys::RESULTS_DOCS << result_docs_array;

    if (current_object_oid.to_string() != "000000000000000000000000") {
        document_result
                << "_id" << current_object_oid;
    }
}

bool MatchAlgorithmStatisticsDoc::setIntoCollection() {

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection match_algorithm_statistics_collection = info_for_statistics_db[collection_names::MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME];

    bsoncxx::builder::stream::document insertDocument;
    convertToDocument(insertDocument);
    try {

        if (current_object_oid.to_string() != "000000000000000000000000") {

            mongocxx::options::update updateOptions;
            updateOptions.upsert(true);

            match_algorithm_statistics_collection.update_one(
                    document{}
                            << "_id" << current_object_oid
                            << finalize,
                    document{}
                            << "$set" << insertDocument.view()
                            << finalize,
                    updateOptions);
        } else {

            auto idVar = match_algorithm_statistics_collection.insert_one(insertDocument.view());

            current_object_oid = idVar->inserted_id().get_oid().value;
        }
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in MatchAlgorithmStatisticsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmStatisticsDoc::setIntoCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool MatchAlgorithmStatisticsDoc::getFromCollection() {
    return getFromCollection(document{} << finalize);
}

bool MatchAlgorithmStatisticsDoc::getFromCollection(const bsoncxx::oid& findOID) {
    return getFromCollection(
            document{}
                    << "_id" << findOID
            << finalize
    );
}

bool MatchAlgorithmStatisticsDoc::getFromCollection(const bsoncxx::document::view& find_doc) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database info_for_statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection match_algorithm_statistics_collection = info_for_statistics_db[collection_names::MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> findDocumentVal;
    try {
        findDocumentVal = match_algorithm_statistics_collection.find_one(find_doc);
    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "bsoncxx EXCEPTION THROWN in MatchAlgorithmStatisticsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const mongocxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmStatisticsDoc::getFromCollection\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return saveInfoToDocument(findDocumentVal);
}

bool MatchAlgorithmStatisticsDoc::saveInfoToDocument(
        const bsoncxx::stdx::optional<bsoncxx::document::value>& find_document_val) {
    if (find_document_val) {
        bsoncxx::document::view user_doc_view = *find_document_val;

        return convertDocumentToClass(user_doc_view);
    } else {
        //NOTE: There are times when this failing will be part of the check
        //std::cout << "ERROR, Document not found from function UserAccountDoc::saveInfoToDocument\n";
        return false;
    }
}

void MatchAlgorithmStatisticsDoc::generateRandomValues(
        const bsoncxx::oid& user_account_oid,
        const bool always_generate_results_doc
        ) {
    current_object_oid = bsoncxx::oid{};
    timestamp_run = bsoncxx::types::b_date{general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP + std::chrono::milliseconds{rand() % (2L * 365L * 24L * 60L * 60L * 1000L)}};
    bsoncxx::types::b_date earliest_possible_time{timestamp_run.value + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};
    end_time = bsoncxx::types::b_date{timestamp_run.value + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES};
    algorithm_run_time = 100 + rand() % 10000;
    query_activities_run_time = 100 + rand() % 10000;
    user_account_document.getFromCollection(user_account_oid);

    //2 is one for activities and one for categories
    int number_matches_from_algorithm = rand() % (matching_algorithm::TOTAL_NUMBER_DOCS_RETURNED - always_generate_results_doc) + always_generate_results_doc;
    for(int i = 0; i < number_matches_from_algorithm; ++i) {

        std::vector<RawAlgorithmResultsDoc::MatchStatisticsDoc::ActivityStatisticsDoc> activity_statistics;
        int activity_statistics_size = rand() % (2 * server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT);
        for(int j = 0; j < activity_statistics_size; ++j) {
            long total_user_time = rand() % 100000;
            long total_match_time = rand() % 100000;

            //this doesn't take into account between times
            long total_overlap_time = rand() % std::min(total_user_time, total_match_time);

            std::vector<long> between_times_array;

            int between_times_size = rand() % 5;
            for(int k = 0; k < between_times_size; ++k) {
                between_times_array.emplace_back(rand() % matching_algorithm::MAX_BETWEEN_TIME_WEIGHT_HELPER.count());
            }

            RawAlgorithmResultsDoc::MatchStatisticsDoc::ActivityStatisticsDoc(
                    rand() % 100 + 1,
                    AccountCategoryType(rand() % 2),
                    total_user_time,
                    total_match_time,
                    total_overlap_time,
                    between_times_array
            );
        }

        results_docs.emplace_back(
                    (double)(rand() % 100)/100 + (double)(rand() % 100),
                    timestamp_run.value.count() + rand() % (3 * 24 * 60 * 60 * 1000),
                    (double)(rand() % 10000)/10000 + (double)(rand() % 100000),
                    RawAlgorithmResultsDoc::MatchStatisticsDoc(
                            bsoncxx::types::b_date{timestamp_run.value + std::chrono::milliseconds{rand() % 100000 + 1}},
                            timestamp_run.value.count(),
                            earliest_possible_time.value.count(),
                            end_time.value.count(),
                            -1 * ((double)(rand() % 1000)/1000 + (double)(rand() % 10000)),
                            ((double)(rand() % 1000)/1000 + (double)(rand() % 100000)),
                            -1 * ((double)(rand() % 1000)/1000 + (double)(rand() % 10000)),
                            activity_statistics
                            )
                );
    }

}

bool MatchAlgorithmStatisticsDoc::convertDocumentToClass(
        const bsoncxx::document::view& document_parameter
) {
    try {
        current_object_oid = extractFromBsoncxx_k_oid(
                document_parameter,
                "_id"
        );

        timestamp_run = extractFromBsoncxx_k_date(
                document_parameter,
                match_algorithm_statistics_keys::TIMESTAMP_RUN
        );

        end_time = extractFromBsoncxx_k_date(
                document_parameter,
                match_algorithm_statistics_keys::END_TIME
        );

        algorithm_run_time = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_statistics_keys::ALGORITHM_RUN_TIME
        );

        query_activities_run_time = extractFromBsoncxx_k_int64(
                document_parameter,
                match_algorithm_statistics_keys::QUERY_ACTIVITIES_RUN_TIME
        );

        bsoncxx::document::view user_account_document_doc = extractFromBsoncxx_k_document(
                document_parameter,
                match_algorithm_statistics_keys::USER_ACCOUNT_DOCUMENT
        );

        user_account_document.convertDocumentToClass(user_account_document_doc);

        bsoncxx::array::view results_docs_arr = extractFromBsoncxx_k_array(
                document_parameter,
                match_algorithm_statistics_keys::RESULTS_DOCS
        );

        for(const auto& results_doc_ele : results_docs_arr) {

            bsoncxx::document::view results_docs_doc = extractFromBsoncxxArrayElement_k_document(results_doc_ele);

            bsoncxx::document::view match_statistics_doc = extractFromBsoncxx_k_document(
                    results_docs_doc,
                    algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_STATISTICS_VAR
            );

            bsoncxx::array::view activity_statistics_arr = extractFromBsoncxx_k_array(
                    match_statistics_doc,
                    user_account_keys::accounts_list::ACTIVITY_STATISTICS
            );

            std::vector<RawAlgorithmResultsDoc::MatchStatisticsDoc::ActivityStatisticsDoc> activity_statistics;

            for (const auto& ele: activity_statistics_arr) {
                bsoncxx::document::view activity_statistics_doc = ele.get_document().value;

                bsoncxx::array::view between_times_arr = extractFromBsoncxx_k_array(
                        activity_statistics_doc,
                        algorithm_pipeline_field_names::MONGO_PIPELINE_BETWEEN_TIMES_ARRAY_VAR
                );
                std::vector<long> mongo_pipeline_between_times_array;

                for (const auto& between_time: between_times_arr) {
                    mongo_pipeline_between_times_array.emplace_back(between_time.get_int64().value);
                }

                activity_statistics.emplace_back(
                        RawAlgorithmResultsDoc::MatchStatisticsDoc::ActivityStatisticsDoc(
                                extractFromBsoncxx_k_int32(
                                        activity_statistics_doc,
                                        user_account_keys::categories::INDEX_VALUE
                                ),
                                AccountCategoryType(
                                        extractFromBsoncxx_k_int32(
                                                activity_statistics_doc,
                                                user_account_keys::categories::TYPE
                                        )
                                ),
                                extractFromBsoncxx_k_int64(
                                        activity_statistics_doc,
                                        algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_USER_TIME_VAR
                                ),
                                extractFromBsoncxx_k_int64(
                                        activity_statistics_doc,
                                        algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_TIME_FOR_MATCH
                                ),
                                extractFromBsoncxx_k_int64(
                                        activity_statistics_doc,
                                        algorithm_pipeline_field_names::MONGO_PIPELINE_TOTAL_OVERLAP_TIME_VAR
                                ),
                                mongo_pipeline_between_times_array
                        )
                );
            }

            results_docs.emplace_back(
                RawAlgorithmResultsDoc(
                        extractFromBsoncxx_k_double(
                                results_docs_doc,
                                algorithm_pipeline_field_names::MONGO_PIPELINE_DISTANCE_KEY
                        ),
                        extractFromBsoncxx_k_int64(
                                results_docs_doc,
                                algorithm_pipeline_field_names::MONGO_PIPELINE_MATCH_EXPIRATION_TIME_VAR
                        ),
                        extractFromBsoncxx_k_double(
                                results_docs_doc,
                                algorithm_pipeline_field_names::MONGO_PIPELINE_FINAL_POINTS_VAR
                        ),
                    RawAlgorithmResultsDoc::MatchStatisticsDoc(
                            extractFromBsoncxx_k_date(
                                    match_statistics_doc,
                                    user_account_keys::LAST_TIME_FIND_MATCHES_RAN
                            ),
                            extractFromBsoncxx_k_int64(
                                    match_statistics_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_MATCH_RAN_VAR
                            ),
                            extractFromBsoncxx_k_int64(
                                    match_statistics_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_EARLIEST_START_TIME_VAR
                            ),
                            extractFromBsoncxx_k_int64(
                                    match_statistics_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_MAX_POSSIBLE_TIME_VAR
                            ),
                            extractFromBsoncxx_k_double(
                                    match_statistics_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_TIME_FALL_OFF_VAR
                            ),
                            extractFromBsoncxx_k_double(
                                    match_statistics_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_CATEGORY_OR_ACTIVITY_POINTS_KEY
                            ),
                            extractFromBsoncxx_k_double(
                                    match_statistics_doc,
                                    algorithm_pipeline_field_names::MONGO_PIPELINE_INACTIVITY_POINTS_TO_SUBTRACT_KEY
                            ),
                            activity_statistics
                    )
                )
            );

        }

    }
    catch (const bsoncxx::exception& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmStatisticsDoc::convertDocumentToClass\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        std::cout << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n"
                << "mongocxx EXCEPTION THROWN in MatchAlgorithmStatisticsDoc::convertDocumentToClass\n"
                << e.what() << '\n'
                << "EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE\n";
        return false;
    }

    return true;
}

bool MatchAlgorithmStatisticsDoc::operator==(const MatchAlgorithmStatisticsDoc& other) const {
    bool return_value = true;

    checkForEquality(
            current_object_oid.to_string(),
            other.current_object_oid.to_string(),
            "CURRENT_OBJECT_OID",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            timestamp_run.value.count(),
            other.timestamp_run.value.count(),
            "TIMESTAMP_RUN",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            end_time.value.count(),
            other.end_time.value.count(),
            "END_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            algorithm_run_time,
            other.algorithm_run_time,
            "ALGORITHM_RUN_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    checkForEquality(
            query_activities_run_time,
            other.query_activities_run_time,
            "QUERY_ACTIVITIES_RUN_TIME",
            OBJECT_CLASS_NAME,
            return_value
    );

    if (user_account_document != other.user_account_document) {
        return_value = false;
    }

    if(results_docs.size() == other.results_docs.size()) {

        for(size_t k = 0; k < results_docs.size(); ++k) {

            const RawAlgorithmResultsDoc& result_doc = results_docs[k];
            const RawAlgorithmResultsDoc& other_result_doc = other.results_docs[k];

            checkForEquality(
                    result_doc.mongo_pipeline_distance,
                    other_result_doc.mongo_pipeline_distance,
                    "RESULTS_DOCS.mongo_pipeline_distance",
                    OBJECT_CLASS_NAME,
                    return_value
            );

            checkForEquality(
                    result_doc.mongo_pipeline_match_expiration_time,
                    other_result_doc.mongo_pipeline_match_expiration_time,
                    "RESULTS_DOCS.mongo_pipeline_match_expiration_time",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_final_points,
                    other_result_doc.mongo_pipeline_final_points,
                    "RESULTS_DOCS.mongo_pipeline_final_points",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.last_time_find_matches_ran.value.count(),
                    other_result_doc.mongo_pipeline_match_statistics.last_time_find_matches_ran.value.count(),
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.last_time_find_matches_ran",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.mongo_pipeline_time_match_ran,
                    other_result_doc.mongo_pipeline_match_statistics.mongo_pipeline_time_match_ran,
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.mongo_pipeline_time_match_ran",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.mongo_pipeline_earliest_start_time,
                    other_result_doc.mongo_pipeline_match_statistics.mongo_pipeline_earliest_start_time,
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.mongo_pipeline_earliest_start_time",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.mongo_pipeline_max_possible_time,
                    other_result_doc.mongo_pipeline_match_statistics.mongo_pipeline_max_possible_time,
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.mongo_pipeline_max_possible_time",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.mongo_pipeline_time_fall_off,
                    other_result_doc.mongo_pipeline_match_statistics.mongo_pipeline_time_fall_off,
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.mongo_pipeline_time_fall_off",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.mongo_pipeline_category_or_activity_points,
                    other_result_doc.mongo_pipeline_match_statistics.mongo_pipeline_category_or_activity_points,
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.mongo_pipeline_category_or_activity_points",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            checkForEquality(
                    result_doc.mongo_pipeline_match_statistics.mongo_pipeline_inactivity_points_to_subtract,
                    other_result_doc.mongo_pipeline_match_statistics.mongo_pipeline_inactivity_points_to_subtract,
                    "RESULTS_DOCS.mongo_pipeline_match_statistics.mongo_pipeline_inactivity_points_to_subtract",
                    OBJECT_CLASS_NAME,
                    return_value
                    );

            if(result_doc.mongo_pipeline_match_statistics.activity_statistics.size() ==
                    other_result_doc.mongo_pipeline_match_statistics.activity_statistics.size()) {
                auto& stats = result_doc.mongo_pipeline_match_statistics.activity_statistics;
                auto& other_stats = other_result_doc.mongo_pipeline_match_statistics.activity_statistics;

                for(size_t i = 0; i < stats.size(); i++) {
                    checkForEquality(
                            stats[i].index_value,
                            other_stats[i].index_value,
                            "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.index_value",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            stats[i].type,
                            other_stats[i].type,
                            "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.type",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            stats[i].mongo_pipeline_total_user_time,
                            other_stats[i].mongo_pipeline_total_user_time,
                            "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.mongo_pipeline_total_user_time",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            stats[i].mongo_pipeline_total_time_for_match,
                            other_stats[i].mongo_pipeline_total_time_for_match,
                            "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.mongo_pipeline_total_time_for_match",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    checkForEquality(
                            stats[i].mongo_pipeline_total_overlap_time,
                            other_stats[i].mongo_pipeline_total_overlap_time,
                            "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.mongo_pipeline_total_overlap_time",
                            OBJECT_CLASS_NAME,
                            return_value
                            );

                    if(stats[i].mongo_pipeline_between_times_array.size() ==
                        other_stats[i].mongo_pipeline_between_times_array.size()) {
                        for(size_t j = 0; j < stats[i].mongo_pipeline_between_times_array.size(); j++) {
                            checkForEquality(
                                    stats[i].mongo_pipeline_between_times_array[j],
                                    other_stats[i].mongo_pipeline_between_times_array[j],
                                    "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.mongo_pipeline_between_times_array.mongo_pipeline_between_times_array",
                                    OBJECT_CLASS_NAME,
                                    return_value
                                    );
                        }
                    } else {
                        checkForEquality(
                                stats[i].mongo_pipeline_between_times_array.size(),
                                other_stats[i].mongo_pipeline_between_times_array.size(),
                                "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.mongo_pipeline_between_times_array.size()",
                                OBJECT_CLASS_NAME,
                                return_value
                                );
                    }
                }
            } else {
                checkForEquality(
                        result_doc.mongo_pipeline_match_statistics.activity_statistics.size(),
                        other_result_doc.mongo_pipeline_match_statistics.activity_statistics.size(),
                        "RESULTS_DOCS.mongo_pipeline_match_statistics.activity_statistics.size()",
                        OBJECT_CLASS_NAME,
                        return_value
                        );
            }
        }

    } else {
        checkForEquality(
                results_docs.size(),
                other.results_docs.size(),
                "RESULTS_DOCS.size()",
                OBJECT_CLASS_NAME,
                return_value
        );
    }

    return return_value;
}

std::ostream& operator<<(std::ostream& o, const MatchAlgorithmStatisticsDoc& v) {
    bsoncxx::builder::stream::document document_result;
    v.convertToDocument(document_result);
    o << makePrettyJson(document_result.view());
    return o;
}
