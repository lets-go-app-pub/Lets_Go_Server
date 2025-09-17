//
// Created by jeremiah on 8/28/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <AdminLevelEnum.grpc.pb.h>
#include <global_bsoncxx_docs.h>

#include "store_mongoDB_error_and_exception.h"
#include "admin_privileges_vector.h"
#include "connection_pool_global_variable.h"
#include "handle_function_operation_exception.h"

#include "utility_general_functions.h"
#include "request_statistics.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "match_algorithm_results_keys.h"
#include "request_statistics_values.h"
#include "extract_data_from_bsoncxx.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void runMatchingActivityStaticsImplementation(
        const request_statistics::MatchingActivityStatisticsRequest* request,
        request_statistics::MatchingActivityStatisticsResponse* response
);

bool saveIndividualMatchStatsToAlgorithmResults(
        request_statistics::MatchingActivityStatisticsResponse* response,
        mongocxx::database& statistics_db,
        mongocxx::collection& match_results_collection,
        long current_timestamp_day
);

void runMatchingActivityStatistics(
        const request_statistics::MatchingActivityStatisticsRequest* request,
        request_statistics::MatchingActivityStatisticsResponse* response
) {
    handleFunctionOperationException(
            [&] {
                runMatchingActivityStaticsImplementation(request, response);
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request
    );
}

void runMatchingActivityStaticsImplementation(
        const request_statistics::MatchingActivityStatisticsRequest* request,
        request_statistics::MatchingActivityStatisticsResponse* response
) {

    {
        bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
        std::string error_message;
        std::string user_account_oid_str;
        std::string login_token_str;
        std::string installation_id;

        auto store_error_message = [&](
                bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                const std::string& passed_error_message
        ) {
            error_message = passed_error_message;
            admin_info_doc_value = std::move(returned_admin_info_doc);
        };

        const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS || !admin_info_doc_value) {
            response->set_success(false);
            response->set_error_msg("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        }

        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
        AdminLevelEnum admin_level;

        auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
        if (admin_privilege_element &&
            admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
        } else { //if element does not exist or is not type oid
            logElementError(
                    __LINE__, __FILE__,
                    admin_privilege_element, admin_info_doc_view,
                    bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
            );

            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return;
        }

        if (!admin_privileges[admin_level].view_matching_activity_statistics()) {
            response->set_success(false);
            response->set_error_msg("Admin level " + AdminLevelEnum_Name(admin_level) +
                                    " does not have 'view_matching_activity_statistics' access.");
            return;
        }
    }

    bsoncxx::builder::basic::array user_account_types_arr;

    if(request->login_info().lets_go_version() == 1
        && request->user_account_types().empty()) {
        //Allows for backwards compatibility with older version of Desktop Interface.
        user_account_types_arr.append(UserAccountType::USER_ACCOUNT_TYPE);
        user_account_types_arr.append(UserAccountType::USER_GENERATED_EVENT_TYPE);
        user_account_types_arr.append(UserAccountType::ADMIN_GENERATED_EVENT_TYPE);
    } else {
        for (const int account_type: request->user_account_types()) {
            if (UserAccountType_IsValid(account_type)
                && account_type != match_algorithm_results_keys::ACCOUNT_TYPE_KEY_HEADER_VALUE) {
                user_account_types_arr.append(account_type);
            }
        }

        if (user_account_types_arr.view().empty()) {
            response->set_success(false);
            response->set_error_msg("Must pass at least one valid UserAccountType.");
            return;
        }
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database statistics_db = mongo_cpp_client[database_names::INFO_FOR_STATISTICS_DATABASE_NAME];
    mongocxx::collection match_results_collection = statistics_db[collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME];

    if (request->number_days_to_search() < 1
        || request_statistics_values::MAXIMUM_NUMBER_DAYS_TO_SEARCH < request->number_days_to_search()
    ) {
        response->set_success(false);
        response->set_error_msg("Number of days to search must be a value between 1 and " +
                                std::to_string(request_statistics_values::MAXIMUM_NUMBER_DAYS_TO_SEARCH) + ".");
        return;
    }

    const long current_timestamp_day = current_timestamp.count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY;

    const long begin_day = current_timestamp_day - request->number_days_to_search();
    const long end_day = current_timestamp_day - 1;

    long number_document_headers_found;
    try {
        number_document_headers_found = match_results_collection.count_documents(
            document{}
                << match_algorithm_results_keys::GENERATED_FOR_DAY << open_document
                    << "$lte" << bsoncxx::types::b_int64{end_day}
                << close_document
                << match_algorithm_results_keys::GENERATED_FOR_DAY << open_document
                    << "$gte" << bsoncxx::types::b_int64{begin_day}
                << close_document
                << match_algorithm_results_keys::ACCOUNT_TYPE << match_algorithm_results_keys::ACCOUNT_TYPE_KEY_HEADER_VALUE
                << match_algorithm_results_keys::CATEGORIES_TYPE << match_algorithm_results_keys::TYPE_KEY_HEADER_VALUE
                << match_algorithm_results_keys::CATEGORIES_VALUE << match_algorithm_results_keys::VALUE_KEY_HEADER_VALUE
            << finalize
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );
        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    if (number_document_headers_found < request->number_days_to_search()) {
        if(!saveIndividualMatchStatsToAlgorithmResults(
                response,
                statistics_db,
                match_results_collection,
                current_timestamp_day)
        ) {
            //error is already stored
            return;
        }
    } else if (number_document_headers_found > request->number_days_to_search()) {
        const std::string error_string = "The number of documents exceeded the number of days to search.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME,
                "number_document_headers_found", std::to_string(number_document_headers_found),
                "request->number_days_to_search()", std::to_string(request->number_days_to_search())
        );

        //NOTE: can continue here
    }

    static const std::string TOTAL_TIMES_SWIPED_PIPELINE_FIELD = "times_swiped";

    mongocxx::stdx::optional<mongocxx::cursor> match_results_find_result;
    try {

        bsoncxx::builder::stream::document projection_document;

        projection_document
                << match_algorithm_results_keys::CATEGORIES_TYPE << 1
                << match_algorithm_results_keys::CATEGORIES_VALUE << 1;

        bool at_least_one_value_set = false;

        if (request->yes_yes()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_YES_YES << 1;
        }

        if (request->yes_no()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_YES_NO << 1;
        }

        if (request->yes_block_and_report()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT << 1;
        }

        if (request->yes_incomplete()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_YES_INCOMPLETE << 1;
        }

        if (request->no()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_NO << 1;
        }

        if (request->block_and_report()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_BLOCK_AND_REPORT << 1;
        }

        if (request->incomplete()) {
            at_least_one_value_set = true;
            projection_document
                    << match_algorithm_results_keys::NUM_INCOMPLETE << 1;
        }

        if (!at_least_one_value_set) {
            response->set_success(false);
            response->set_error_msg("Must send at least one statistics type to server.");
            return;
        }

        bsoncxx::builder::stream::document find_document;

        //find documents for the day excluding header documents
        find_document
            << match_algorithm_results_keys::GENERATED_FOR_DAY << open_document
                << "$lte" << bsoncxx::types::b_int64{end_day}
            << close_document
            << match_algorithm_results_keys::GENERATED_FOR_DAY << open_document
                << "$gte" << bsoncxx::types::b_int64{begin_day}
            << close_document
            << match_algorithm_results_keys::ACCOUNT_TYPE << open_document
                << "$in" << user_account_types_arr
            << close_document
            << match_algorithm_results_keys::CATEGORIES_VALUE << open_document
                << "$ne" << match_algorithm_results_keys::VALUE_KEY_HEADER_VALUE
            << close_document;

        if (!request->include_categories()) {
            find_document
                    << match_algorithm_results_keys::CATEGORIES_TYPE << AccountCategoryType::ACTIVITY_TYPE;
        } else {
            find_document
                    << match_algorithm_results_keys::CATEGORIES_TYPE << open_document
                        << "$ne" << match_algorithm_results_keys::TYPE_KEY_HEADER_VALUE
                    << close_document;
        }

        mongocxx::pipeline pipeline;

        pipeline.match(find_document.view());

        static const std::string TYPE_VALUE_PIPELINE_FIELD = "type_value";

        pipeline.add_fields(
            document{}
                << TYPE_VALUE_PIPELINE_FIELD << open_document
                    << "$concat" << open_array
                        << open_document
                            << "$toString" << "$" + match_algorithm_results_keys::CATEGORIES_TYPE
                        << close_document
                        << "_"
                        << open_document
                            << "$toString" << "$" + match_algorithm_results_keys::CATEGORIES_VALUE
                        << close_document
                    << close_array
                << close_document
            << finalize
        );

        //group all documents together and add results
        pipeline.group(
            document{}
                << "_id" << "$" + TYPE_VALUE_PIPELINE_FIELD
                << match_algorithm_results_keys::CATEGORIES_TYPE << open_document
                    << "$first" << "$" + match_algorithm_results_keys::CATEGORIES_TYPE
                << close_document
                << match_algorithm_results_keys::CATEGORIES_VALUE << open_document
                    << "$first" << "$" + match_algorithm_results_keys::CATEGORIES_VALUE
                << close_document
                << match_algorithm_results_keys::NUM_YES_YES << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_YES_YES
                << close_document
                << match_algorithm_results_keys::NUM_YES_NO << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_YES_NO
                << close_document
                << match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT
                << close_document
                << match_algorithm_results_keys::NUM_YES_INCOMPLETE << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_YES_INCOMPLETE
                << close_document
                << match_algorithm_results_keys::NUM_NO << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_NO
                << close_document
                << match_algorithm_results_keys::NUM_BLOCK_AND_REPORT << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_BLOCK_AND_REPORT
                << close_document
                << match_algorithm_results_keys::NUM_INCOMPLETE << open_document
                    << "$sum" << "$" + match_algorithm_results_keys::NUM_INCOMPLETE
                << close_document
            << finalize
        );

        pipeline.project(projection_document.view());

        auto extract_long_if_exists = [&](const std::string& key) -> bsoncxx::document::value {
            return document{}
                << "$cond" << open_document

                    << "if" << open_document
                        << "$eq" << open_array
                            << open_document
                                << "$type" << "$" + key
                            << close_document
                            << "long"
                        << close_array
                    << close_document
                    << "then" << "$" + key
                    << "else" << bsoncxx::types::b_int64{0}

                << close_document
            << finalize;
        };

        pipeline.project(
            document{}
                << match_algorithm_results_keys::CATEGORIES_TYPE << "$" + match_algorithm_results_keys::CATEGORIES_TYPE
                << match_algorithm_results_keys::CATEGORIES_VALUE << "$" + match_algorithm_results_keys::CATEGORIES_VALUE

                << TOTAL_TIMES_SWIPED_PIPELINE_FIELD << open_document
                    << "$add" << open_array

                        << extract_long_if_exists(match_algorithm_results_keys::NUM_YES_YES)
                        << extract_long_if_exists(match_algorithm_results_keys::NUM_YES_NO)
                        << extract_long_if_exists(match_algorithm_results_keys::NUM_YES_BLOCK_AND_REPORT)
                        << extract_long_if_exists(match_algorithm_results_keys::NUM_YES_INCOMPLETE)
                        << extract_long_if_exists(match_algorithm_results_keys::NUM_NO)
                        << extract_long_if_exists(match_algorithm_results_keys::NUM_BLOCK_AND_REPORT)
                        << extract_long_if_exists(match_algorithm_results_keys::NUM_INCOMPLETE)

                    << close_array
                << close_document
            << finalize);

        match_results_find_result = match_results_collection.aggregate(pipeline);
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    if (!match_results_find_result) {
        const std::string error_string = "Cursor for matching_activity_statistics came back un-set when no "
                                         "exception was thrown. This should never happen";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return;
    }

    for (auto& document : *match_results_find_result) {
        long total_times_swiped;
        AccountCategoryType category_type;
        int category_value;

        try {

            total_times_swiped = extractFromBsoncxx_k_int64(
                    document,
                    TOTAL_TIMES_SWIPED_PIPELINE_FIELD
            );

            category_type = AccountCategoryType(
                    extractFromBsoncxx_k_int32(
                            document,
                            match_algorithm_results_keys::CATEGORIES_TYPE
                    )
            );

            category_value = extractFromBsoncxx_k_int32(
                    document,
                    match_algorithm_results_keys::CATEGORIES_VALUE
            );

        } catch (const ErrorExtractingFromBsoncxx& e) {
            //Error already stored.
            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return;
        }

        request_statistics::NumberOfTimesActivityWasSwiped* activity_statistics = response->add_activity_statistics();
        activity_statistics->set_account_category_type(category_type);
        activity_statistics->set_activity_or_category_index(category_value);
        activity_statistics->set_number_times_swiped(total_times_swiped);

    }

    response->set_success(true);
}

bool saveIndividualMatchStatsToAlgorithmResults(
        request_statistics::MatchingActivityStatisticsResponse* response,
        mongocxx::database& statistics_db,
        mongocxx::collection& match_results_collection,
        const long current_timestamp_day
) {

    mongocxx::stdx::optional<mongocxx::cursor> find_final_timestamp_cursor;
    try {
        mongocxx::options::find opts;

        opts.projection(
                document{}
                        << match_algorithm_results_keys::GENERATED_FOR_DAY << 1
                << finalize
        );

        opts.sort(
                document{}
                        << match_algorithm_results_keys::GENERATED_FOR_DAY << -1
                << finalize
        );

        opts.limit(1);

        //will find the header with the maximum value for GENERATED_FOR_DAY
        find_final_timestamp_cursor = match_results_collection.find(
            document{}
                << match_algorithm_results_keys::ACCOUNT_TYPE << match_algorithm_results_keys::ACCOUNT_TYPE_KEY_HEADER_VALUE
                << match_algorithm_results_keys::CATEGORIES_TYPE << match_algorithm_results_keys::TYPE_KEY_HEADER_VALUE
                << match_algorithm_results_keys::CATEGORIES_VALUE << match_algorithm_results_keys::VALUE_KEY_HEADER_VALUE
            << finalize,
            opts
        );
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return false;
    }

    if (!find_final_timestamp_cursor) {
        const std::string error_string = "Cursor for matching_activity_statistics came back un-set"
                                         " when no exception was thrown. This should never happen";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server.");
        return false;
    }

    //NOTE: This must be set to a year before because the user can request say 365 days in the past on
    // the very first day available (the default value will only be used on the first attempt).
    static const long TIMESTAMP_FOR_AUGUST_FIRST_2020 = 1596240000000;
    long final_day_already_generated = TIMESTAMP_FOR_AUGUST_FIRST_2020 / request_statistics_values::MILLISECONDS_IN_ONE_DAY;

    if (find_final_timestamp_cursor->begin() != find_final_timestamp_cursor->end()) { //cursor is NOT empty

        //there is only 1 document inside this because limit == 1
        const bsoncxx::document::view final_timestamp_document = (*find_final_timestamp_cursor->begin());

        auto match_results_generated_element = final_timestamp_document[match_algorithm_results_keys::GENERATED_FOR_DAY];
        if (match_results_generated_element
            && match_results_generated_element.type() == bsoncxx::type::k_int64) { //if element exists and is type int64
            final_day_already_generated = match_results_generated_element.get_int64().value;
        } else { //if element does not exist or is not type int64
            logElementError(
                    __LINE__, __FILE__,
                    match_results_generated_element, final_timestamp_document,
                    bsoncxx::type::k_int64, match_algorithm_results_keys::GENERATED_FOR_DAY,
                    database_names::INFO_FOR_STATISTICS_DATABASE_NAME, collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
            );

            response->set_success(false);
            response->set_error_msg("Error stored on server.");
            return false;
        }
    }

    static const std::string SWIPE_PIPELINE_TYPE = "swipe_type";
    const mongocxx::pipeline pipeline = buildMatchingActivityStatisticsPipeline(
            SWIPE_PIPELINE_TYPE,
            current_timestamp_day,
            final_day_already_generated
    );

    mongocxx::collection individual_match_statistics_collection = statistics_db[collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME];

    mongocxx::stdx::optional<mongocxx::cursor> aggregation_results;
    try {

        mongocxx::options::aggregate aggregation_options;

        //Pipeline stages have a limit of 100 megabytes of RAM. If a stage exceeds this limit, MongoDB will produce an error. To allow for the
        // handling of large datasets, use the allowDiskUse option to enable aggregation pipeline stages to write data to temporary files.
        //The aggregation_options should not be used unless necessary, so it doesn't hurt to have it on. Otherwise, could randomly get errors.
        aggregation_options.allow_disk_use(true);

        //will find the maximum value for GENERATED_FOR_DAY
        aggregation_results = individual_match_statistics_collection.aggregate(pipeline, aggregation_options);
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server. Exception during aggregation.");
        return false;
    }

    if (!aggregation_results) { //aggregation failed
        const std::string error_string = "Cursor for matching_activity_statistics came back un-set when no "
                                         "exception was thrown. This should never happen";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server. Aggregation failed.");
        return false;
    }
    //NOTE: empty aggregation can come back on first time

    //upsert header documents
    //put these in vector first so if there is a problem insert_many will end without storing below documents
    std::vector<bsoncxx::document::value> cursor_documents;
    for (long i = final_day_already_generated + 1; i < current_timestamp_day; i++) {
        cursor_documents.emplace_back(
            document{}
                << match_algorithm_results_keys::GENERATED_FOR_DAY << i
                << match_algorithm_results_keys::ACCOUNT_TYPE << match_algorithm_results_keys::ACCOUNT_TYPE_KEY_HEADER_VALUE
                << match_algorithm_results_keys::CATEGORIES_TYPE << match_algorithm_results_keys::TYPE_KEY_HEADER_VALUE
                << match_algorithm_results_keys::CATEGORIES_VALUE << match_algorithm_results_keys::VALUE_KEY_HEADER_VALUE
            << finalize
        );
    }

    for (auto& cursor_document : *aggregation_results) {
        cursor_documents.emplace_back(cursor_document);
    }

    //This is possible if the admin is requesting a day that occurred before any matches were stored. For example
    // if three days after the app launches the admin requests a year in the past, cursor_documents can be empty.
    if(cursor_documents.empty()) {
        return true;
    }

    mongocxx::stdx::optional<mongocxx::result::insert_many> insert_matching_result_documents;
    try {
        //upsert all generated statistics documents
        insert_matching_result_documents = match_results_collection.insert_many(cursor_documents);
    }
    catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server. Exception during upsert match results.");
        return false;
    } catch (const mongocxx::operation_exception& e) {
        if (e.code().value() == 11000) { //duplicate key error

            //This could happen if another user was simultaneously updating the collection, the other user should
            // handle all updates, so this is completely fine
            const std::string error_string = "Duplicate entry found for insertMatchingResultDocuments. While"
                                             " this is technically possible with 2 users connecting at the "
                                             "exact same time, it is unlikely.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(e.what()), error_string,
                    "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                    "collection", collection_names::INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME
            );
        } else {
            if (e.raw_server_error()) { //raw_server_error exists
                throw mongocxx::operation_exception(
                        e.code(),
                        bsoncxx::document::value(e.raw_server_error().value()),
                        e.what()
                );
            } else { //raw_server_error does not exist
                throw mongocxx::operation_exception(
                        e.code(),
                        document{} << finalize,
                        e.what()
                );
            }
        }
    }

    if (!insert_matching_result_documents) { //upsert failed
        const std::string error_string = "Result for insertMatchingResultDocuments came back un-set when no exception"
                                         " was thrown. This should never happen";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::INFO_FOR_STATISTICS_DATABASE_NAME,
                "collection", collection_names::MATCH_ALGORITHM_RESULTS_COLLECTION_NAME
        );

        response->set_success(false);
        response->set_error_msg("Error stored on server. insertMatchingResultDocuments failed.");
        return false;
    }

    return true;
}

