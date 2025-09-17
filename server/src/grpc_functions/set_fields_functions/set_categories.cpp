//
// Created by jeremiah on 3/19/21.
//

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <global_bsoncxx_docs.h>
#include <store_mongoDB_error_and_exception.h>
#include <handle_function_operation_exception.h>
#include <set_fields_helper_functions/set_fields_helper_functions.h>
#include <sstream>
#include <store_info_to_user_statistics.h>
#include <sort_time_frames_and_remove_overlaps.h>
#include <save_activity_time_frames.h>
#include <save_category_time_frames.h>
#include <utility_testing_functions.h>

#include "set_fields_functions.h"

#include "utility_general_functions.h"
#include "time_frame_struct.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "activities_info_keys.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"
#include "server_parameter_restrictions.h"
#include "matching_algorithm.h"
#include "get_login_document.h"
#include "update_single_other_user.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setCategoriesImplementation(
        const setfields::SetCategoriesRequest* request,
        setfields::SetFieldResponse* response
);

void setCategories(
        const setfields::SetCategoriesRequest* request,
        setfields::SetFieldResponse* response
) {
    handleFunctionOperationException(
            [&] {
                setCategoriesImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            },
            __LINE__, __FILE__, request
    );
}

void setCategoriesImplementation(
        const setfields::SetCategoriesRequest* request,
        setfields::SetFieldResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    const auto set_empty_array_return = [&response]() {
        //This could also mean only values that the user was too young for were passed, invalid
        // entries were passed, or no entries were passed
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
    };

    const auto& grpc_categories = request->category();
    int num_activities = 0;

    //used to store non-repeating index values of grpc_categories that are being used
    std::vector<int> grpc_request_index_used_no_repeats;
    bsoncxx::builder::basic::array index_numbers_array_value; //used to search for activities

    {
        //used to catch duplicate activities
        std::set<int> used_activities;
        for (int i = 0; i < grpc_categories.size(); i++) {

            const int activity_index = grpc_categories[i].activity_index();
            const auto activity_iterator = used_activities.find(activity_index);

            if (activity_iterator == used_activities.end()
                && activity_index > 0
            ) { //if this activity is not a duplicate of a previous activity & activity index is valid (0 is the 'Unknown' activity)
                grpc_request_index_used_no_repeats.emplace_back(i);
                index_numbers_array_value.append((int)activity_index);
                used_activities.insert(activity_index);
                num_activities++;
            }

            //set the maximum number of activities possible
            if (grpc_request_index_used_no_repeats.size() ==
                server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT) {
                break;
            }
        }
    }

    //if no activities present, do nothing
    if (num_activities == 0) {
        set_empty_array_return();
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const bsoncxx::oid user_account_oid = bsoncxx::oid{user_account_oid_str};

    const bsoncxx::document::value merge_document = document{} << finalize;

    const bsoncxx::document::value projection_document = document{}
                << "_id" << 1
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << user_account_keys::AGE << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<true>(
            login_token_str,
            installation_id,
            merge_document.view(),
            current_timestamp
    );

    const bsoncxx::document::value calculate_age = buildDocumentToCalculateAge(current_timestamp);

    bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account;
    try {
        mongocxx::pipeline update_pipeline;
        update_pipeline.add_fields(
                document{}
                        << user_account_keys::AGE << calculate_age.view()
                << finalize
        );

        update_pipeline.replace_root(login_document.view());

        mongocxx::options::find_one_and_update find_one_and_update_opts;
        find_one_and_update_opts.projection(projection_document.view());

        find_one_and_update_opts.return_document(mongocxx::options::return_document::k_after);

        //find user account document
        find_and_update_user_account = user_accounts_collection.find_one_and_update(
                document{}
                        << "_id" << user_account_oid
                << finalize,
                update_pipeline,
                find_one_and_update_opts
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", user_account_oid,
                "Document_passed", merge_document.view()
        );
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_and_update_user_account,
            user_account_oid_str
    );

    if (return_status != SUCCESS) { //if login failed
        response->set_return_status(return_status);
        return;
    }

    int user_age;
    const bsoncxx::document::view user_account_doc_view = find_and_update_user_account->view();

    auto user_age_element = user_account_doc_view[user_account_keys::AGE];
    if (user_age_element
        && user_age_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        user_age = user_age_element.get_int32().value;
    } else { //if element does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                user_age_element, user_account_doc_view,
                bsoncxx::type::k_int32, user_account_keys::AGE,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
        );
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const bsoncxx::array::view index_numbers_array_view = index_numbers_array_value.view();
    static const std::string PROJECTED_ARRAY_NAME = "pAn";

    mongocxx::stdx::optional<mongocxx::cursor> find_activities_cursor;
    std::optional<std::string> find_activities_exception_string;
    try {
        mongocxx::pipeline find_activities;

        find_activities.match(
                document{}
                        << "_id" << activities_info_keys::ID
                << finalize
        );

        const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                PROJECTED_ARRAY_NAME,
                index_numbers_array_view,
                user_age
        );

        find_activities.project(project_activities_by_index.view());

        find_activities_cursor = activities_info_collection.aggregate(find_activities);
    } catch (const mongocxx::logic_error& e) {
        find_activities_exception_string = e.what();
    }

    if (!find_activities_cursor || find_activities_cursor->begin() == find_activities_cursor->end()) {
        const std::string error_string = "When finding activities, cursor came back unset or empty.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                find_activities_exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "indexNumbersArrayView", makePrettyJson(index_numbers_array_view)
        );
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    bsoncxx::array::view projected_categories_array_value;

    //should only have 1 element (included a matched stage searching by _id)
    for (const auto& projected_category_doc : *find_activities_cursor) {
        auto categories_array_element = projected_category_doc[PROJECTED_ARRAY_NAME];
        if (categories_array_element
            && categories_array_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
            projected_categories_array_value = categories_array_element.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(
                    __LINE__, __FILE__,
                    categories_array_element, projected_category_doc,
                    bsoncxx::type::k_array, PROJECTED_ARRAY_NAME,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
            );
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }
    }

    const long projected_array_value_size = std::distance(
            projected_categories_array_value.begin(),
            projected_categories_array_value.end()
    );

    if (projected_array_value_size != num_activities) {

        std::stringstream error_string_stream;
        error_string_stream
                << "The arrays were different sizes when they should be the same\n"
                << "gRPCCategories\n";

        for (const auto& temp_category : grpc_categories) {
            error_string_stream
                    << temp_category.DebugString() << '\n';
        }

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string_stream.str(),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "indexedElements", makePrettyJson(projected_categories_array_value),
                "indexNumbersArrayView", makePrettyJson(index_numbers_array_view)
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    //makes sure there are no duplicate activities
    //makes sure each start time is less than its respective stop time
    //NOTE: if the user chose 'anytime' then the timeframe array will be empty
    //if the start time is less than the min time range (currentTimestamp+TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED) it will be set to -1 (this is to check for overlaps)
    //if the stop time is less than the min time range (currentTimestamp+TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED) the time frame will not be stored
    //if the start time is greater than the max time range (endSeconds) the time frame will not be stored
    //if the stop time is greater than the max time range (endSeconds) it will be set to endSeconds
    //NOTE: this is a basic array because a stream array gives an error when using << to append an element to a stored array (access with [])
    bsoncxx::builder::basic::array account_activities_and_categories;
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    for (size_t i = 0; i < grpc_request_index_used_no_repeats.size(); i++) {

        int index_in_parameters = grpc_request_index_used_no_repeats[i];

        if (index_in_parameters >= grpc_categories.size()) {
            std::stringstream error_string_stream;
            error_string_stream
                    << "Inside set_categories an index that was set up to be inside of gRPCCategories was not.\n"
                    << "gRPCCategories\n";

            for (const auto& temp_category : grpc_categories) {
                error_string_stream
                        << temp_category.DebugString() << '\n';
            }

            error_string_stream
                    << "gRPCRequestIndexUsedNoRepeats\n";

            for (int index_val : grpc_request_index_used_no_repeats) {
                error_string_stream
                        << std::to_string(index_val) << ',';
            }

            error_string_stream
                    << '\n';

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string_stream.str(),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "indexedElements", makePrettyJson(projected_categories_array_value),
                    "indexNumbersArrayView", makePrettyJson(index_numbers_array_view),
                    "index_in_parameters", std::to_string(index_in_parameters)
            );

            continue;
        }

        int activity_index = grpc_categories[index_in_parameters].activity_index();

        if (i >= (size_t) projected_array_value_size) {

            std::stringstream error_string_stream;
            error_string_stream
                    << "Inside set_categories an index was too large to access the array when it should not be possible.\n"
                    << "gRPCCategories\n";

            for (const auto& temp_category : grpc_categories) {
                error_string_stream
                        << temp_category.DebugString() << '\n';
            }

            error_string_stream
                    << "gRPCRequestIndexUsedNoRepeats\n";

            for (int index_val : grpc_request_index_used_no_repeats) {
                error_string_stream
                        << std::to_string(index_val) << ',';
            }

            error_string_stream
                    << '\n';

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string_stream.str(),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "projectedArrayValue", makePrettyJson(projected_categories_array_value),
                    "indexNumbersArrayView", makePrettyJson(index_numbers_array_view),
                    "index_in_parameters", std::to_string(index_in_parameters)
            );

            continue;
        }

        //save the element otherwise the array must iterate through it
        const bsoncxx::array::element projected_element = projected_categories_array_value[i];

        if (projected_element.type() == bsoncxx::type::k_document) {

            const bsoncxx::document::view extracted_activity_info = projected_element.get_document().view();
            int category_index;

            auto category_index_element = extracted_activity_info[activities_info_keys::activities::CATEGORY_INDEX];
            if (category_index_element
                && category_index_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                    category_index = category_index_element.get_int32().value;
            } else { //if element does not exist or is not type int32
                logElementError(
                        __LINE__, __FILE__,
                        category_index_element, user_account_doc_view,
                        bsoncxx::type::k_int32, activities_info_keys::activities::CATEGORY_INDEX,
                        database_names::ACCOUNTS_DATABASE_NAME,
                        collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                );
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }

            save_activity_time_frames(
                    category_index_to_timeframes_map,
                    account_activities_and_categories,
                    grpc_categories[(int) i],
                    activity_index,
                    category_index,
                    current_timestamp
            );
        } else if (projected_element.type() != bsoncxx::type::k_null) {
            const std::string error_string = "A value should should be either document or null came back as neither.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "indexedElements", makePrettyJson(projected_categories_array_value),
                    "indexNumbersArrayView", makePrettyJson(index_numbers_array_view),
                    "element_type", convertBsonTypeToString(projected_element.type())
            );
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }
    }

    if (account_activities_and_categories.view().empty()) {
        set_empty_array_return();
        return;
    }

    save_category_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories
    );

    bsoncxx::builder::stream::document update_categories_document;

    appendDocumentToClearMatchingInfoUpdate<false>(update_categories_document);
    const bsoncxx::types::b_date mongodb_current_date{current_timestamp};

    update_categories_document
            << "$set" << open_document
                << user_account_keys::CATEGORIES << account_activities_and_categories
                << user_account_keys::CATEGORIES_TIMESTAMP << mongodb_current_date
            << close_document
            << "$max" << buildUpdateSingleOtherUserProjectionDoc<false>(mongodb_current_date);

    mongocxx::stdx::optional<mongocxx::result::update> update_categories_result;
    try {
        //update user account document
        update_categories_result = user_accounts_collection.update_one(
            document{}
                << "_id" << user_account_oid
            << finalize,
            update_categories_document.view()
        );
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", user_account_oid,
                "Document_passed", update_categories_document.view()
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    if (update_categories_result && update_categories_result->matched_count() == 1) {

        auto push_update_doc = document{}
                << user_account_statistics_keys::CATEGORIES << open_document
                        << user_account_statistics_keys::categories::ACTIVITIES_AND_CATEGORIES << account_activities_and_categories
                        << user_account_statistics_keys::categories::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
                << close_document
        << finalize;

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                bsoncxx::oid{user_account_oid_str},
                push_update_doc,
                current_timestamp
        );

        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_timestamp(current_timestamp.count());
    } else {
        const std::string error_string = "Error updating categories when running set_categories\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "ObjectID_used", user_account_oid,
                "Document_passed", update_categories_document.view()
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

}