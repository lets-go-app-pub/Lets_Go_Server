//
// Created by jeremiah on 10/29/21.
//

#include "handle_errors.h"

#include <mongocxx/options/aggregate.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <StatusEnum.pb.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <utility_general_functions.h>
#include <admin_privileges_vector.h>
#include <store_mongoDB_error_and_exception.h>
#include <extract_data_from_bsoncxx.h>
#include <global_bsoncxx_docs.h>

#include "handle_function_operation_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "fresh_errors_keys.h"
#include "server_parameter_restrictions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void searchErrorsImplementation(
        const handle_errors::SearchErrorsRequest* request,
        handle_errors::SearchErrorsResponse* response
);

void searchErrors(
        const handle_errors::SearchErrorsRequest* request,
        handle_errors::SearchErrorsResponse* response
) {

    handleFunctionOperationException(
            [&] {
                searchErrorsImplementation(request, response);
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_success(false);
                response->set_error_msg(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request);

}

void appendMinMaxRangeFind(
        bsoncxx::builder::stream::document& find_doc,
        const std::string& key,
        int min_value,
        int max_value
) {
    find_doc
        << key << open_document
            << "$gte" << min_value
        << close_document;

    find_doc
        << key << open_document
            << "$lte" << max_value
        << close_document;
}

template<typename T>
void appendArrayTypeFind(
        bsoncxx::builder::stream::document& find_doc,
        const std::string& key,
        const std::set<T>& set
) {
    bsoncxx::builder::basic::array append_array;
    for (const auto& val : set) {
        append_array.append(val);
    }

    find_doc
        << key << open_document
            << "$in" << append_array
        << close_document;
}

void appendArrayTypeFind(
        bsoncxx::builder::stream::document& find_doc,
        const std::string& key,
        const std::set<int>& set
) {

    bsoncxx::builder::basic::array append_array;
    for (const auto& val : set) {
        append_array.append(val);
    }

    find_doc
        << key << open_document
            << "$in" << append_array
        << close_document;
}

void appendArrayTypeFindAll(
        bsoncxx::builder::stream::document& find_doc,
        const std::string& key
) {

    //NOTE: From mongodb documentation.
    // "BSON has a special timestamp type for internal MongoDB use
    // and is not associated with the regular Date type."
    // This means none of the keys used by the server will ever map
    // to type timestamp.
    find_doc
        << key << open_document
            << "$ne" << bsoncxx::types::b_timestamp{}
        << close_document;
}

void searchErrorsImplementation(
        const handle_errors::SearchErrorsRequest* request,
        handle_errors::SearchErrorsResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
        error_message = passed_error_message;
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id,
            store_error_message
    );

    auto return_error_to_client = [&](const std::string& error_message) {
        response->set_success(false);
        response->set_error_msg(error_message);
    };

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        return_error_to_client("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
        return;
    } else if (!admin_info_doc_value) {
        return_error_to_client("Could not find admin document.");
        return;
    }

    bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element
        && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__,
                        admin_privilege_element,
                        admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

        return_error_to_client("Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].request_error_messages()) {
        return_error_to_client("Admin level " + AdminLevelEnum_Name(admin_level) +
                               " does not have 'request_error_messages' access.");
        return;
    }

    document find_doc{};

    //NOTE: if-else statements are ordered so database can easily recognize an index is used
    if (request->all_error_origin_types()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::ERROR_ORIGIN
        );
    } else {
        if(request->types().size() > ErrorOriginType_MAX) {
            return_error_to_client("Too many error origin types.");
            return;
        }

        std::set<int> error_origin_types;

        for(int type : request->types()) {
            if(ErrorOriginType_IsValid(type)) {
                error_origin_types.insert(type);
            }
        }

        if(error_origin_types.empty()) {
            return_error_to_client("No valid error origin types.");
            return;
        }

        appendArrayTypeFind(
                find_doc,
                fresh_errors_keys::ERROR_ORIGIN,
                error_origin_types
        );
    }

    if (request->all_version_numbers()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::VERSION_NUMBER
        );
    } else {
        if(request->max_version_number() < request->min_version_number()) {
            return_error_to_client("Invalid version number range passed.");
            return;
        }

        appendMinMaxRangeFind(
                find_doc,
                fresh_errors_keys::VERSION_NUMBER,
                (int) request->min_version_number(),
                (int) request->max_version_number()
        );
    }

    if (request->all_file_names()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::FILE_NAME
        );
    } else {

        if(request->file_names().size() > 1000) {
            return_error_to_client("Too many error file names.");
            return;
        }

        std::set<std::string> file_names;

        for(const auto& file_name : request->file_names()) {
            if(file_name.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
                return_error_to_client("File names too large.");
                return;
            }
            file_names.insert(file_name);
        }

        appendArrayTypeFind(
                find_doc,
                fresh_errors_keys::FILE_NAME,
                file_names
        );
    }

    if (request->all_line_numbers()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::LINE_NUMBER
        );
    } else {
        if(request->line_numbers().size() > 1000) {
            return_error_to_client("Too many line numbers.");
            return;
        }

        std::set<int> line_numbers;

        for(unsigned int line_number : request->line_numbers()) {
            line_numbers.insert((int)line_number);
        }

        appendArrayTypeFind(
                find_doc,
                fresh_errors_keys::LINE_NUMBER,
                line_numbers
        );
    }

    if (request->all_error_urgency_levels()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::ERROR_URGENCY
        );
    } else {

        if(request->levels().size() > ErrorUrgencyLevel_MAX) {
            return_error_to_client("Too many error urgency levels.");
            return;
        }

        std::set<int> error_urgency_levels;

        for(int level : request->levels()) {
            if(ErrorUrgencyLevel_IsValid(level)) {
                error_urgency_levels.insert(level);
            }
        }

        if(error_urgency_levels.empty()) {
            return_error_to_client("No valid error urgency levels passed.");
            return;
        }

        appendArrayTypeFind(
                find_doc,
                fresh_errors_keys::ERROR_URGENCY,
                error_urgency_levels
        );
    }

    if (request->all_device_names()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::DEVICE_NAME
        );
    } else {

        if(request->device_names().size() > 1000) {
            return_error_to_client("Too many error device names.");
            return;
        }

        std::set<std::string> device_names;

        for(const std::string& device_name : request->device_names()) {
            if(device_name.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
                return_error_to_client("Device names too large.");
                return;
            }
            device_names.insert(device_name);
        }

        appendArrayTypeFind(
                find_doc,
                fresh_errors_keys::DEVICE_NAME,
                device_names
        );
    }

    if (request->all_timestamps()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::TIMESTAMP_STORED
        );
    } else {

        //timestamp is uint64 it can never be negative
        if (request->before_timestamp()) {
            find_doc
                    << fresh_errors_keys::TIMESTAMP_STORED << open_document
                    << "$lt" << bsoncxx::types::b_date{std::chrono::milliseconds{request->search_timestamp()}}
                    << close_document;
        } else {
            find_doc
                    << fresh_errors_keys::TIMESTAMP_STORED << open_document
                    << "$gt" << bsoncxx::types::b_date{std::chrono::milliseconds{request->search_timestamp()}}
                    << close_document;
        }
    }

    if (request->all_android_api()) {
        appendArrayTypeFindAll(
                find_doc,
                fresh_errors_keys::API_NUMBER
        );
    } else {

        if(request->max_api_number() < request->min_api_number()) {
            return_error_to_client("Invalid android API range passed.");
            return;
        }

        appendMinMaxRangeFind(
                find_doc,
                fresh_errors_keys::API_NUMBER,
                (int) request->min_api_number(),
                (int) request->max_api_number()
        );
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongoCppClient[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    static const std::string DOCUMENT_COUNT_KEY = "count";
    static const std::string NAME_OF_FIELD_KEY = "name";
    static const std::string NUMBER_OF_TIMES_REPEATED_KEY = "rep";

    mongocxx::pipeline pipeline;

    mongocxx::stdx::optional<mongocxx::cursor> searched_errors_aggregation_cursor;
    try {

        pipeline.match(find_doc.view());

        //NOTE: the $group stage does NOT use the indexes and so it can be
        // very slow. However, the $sort DOES use the indexes and running $sort
        // before running $group can greatly increase total run time when
        // the collection gets large.
        pipeline.sort(
                document{}
                        << fresh_errors_keys::ERROR_ORIGIN << 1
                        << fresh_errors_keys::VERSION_NUMBER << 1
                        << fresh_errors_keys::FILE_NAME << 1
                        << fresh_errors_keys::LINE_NUMBER << 1
                << finalize
        );

        //Generates a concatenated id of ERROR_ORIGIN_VERSION_NUMBER_FILE_NAME_LINE_NUMBER and so groups matching
        // errors together. Also projects out specific fields.
        pipeline.group(buildGroupSearchedErrorsDocument(DOCUMENT_COUNT_KEY));

        //Sort the arrays to allow the projection stage after this to count unique elements.
        pipeline.add_fields(
            document{}
                << fresh_errors_keys::API_NUMBER << open_document
                    << "$sortArray" << open_document
                        << "input" << "$" + fresh_errors_keys::API_NUMBER
                        << "sortBy" << 1
                    << close_document
                << close_document
                << fresh_errors_keys::DEVICE_NAME << open_document
                    << "$sortArray" << open_document
                        << "input" << "$" + fresh_errors_keys::DEVICE_NAME
                        << "sortBy" << 1
                    << close_document
                << close_document
            << finalize
        );

        pipeline.project(
                buildProjectSearchedErrorsDocument(
                        DOCUMENT_COUNT_KEY,
                        NAME_OF_FIELD_KEY,
                        NUMBER_OF_TIMES_REPEATED_KEY
                )
        );

        mongocxx::options::aggregate aggregation_options;

        //Pipeline stages have a limit of 100 megabytes of RAM. If a stage exceeds this limit, MongoDB will produce
        // an error. To allow for the handling of large datasets, use the allowDiskUse option to enable aggregation
        // pipeline stages to write data to temporary files.
        //The aggregation_options should not be used unless necessary, so it doesn't hurt to have it on. Otherwise,
        // could randomly get errors.
        aggregation_options.allow_disk_use(true);

        searched_errors_aggregation_cursor = fresh_errors_collection.aggregate(
                pipeline,
                aggregation_options
        );

    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "Document_passed", pipeline.view_array()
        );

        return_error_to_client("Error exception thrown when extracting error documents.");
        return;
    }

    if (!searched_errors_aggregation_cursor) {
        const std::string error_string = "MongoDB cursor was not set after running an 'aggregate' query.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ERRORS_DATABASE_NAME,
                "collection", collection_names::FRESH_ERRORS_COLLECTION_NAME,
                "Document_passed", pipeline.view_array()
        );

        return_error_to_client("Error cursor was not set after aggregating from error documents.");
        return;
    }

    for (const bsoncxx::document::view& doc_view : searched_errors_aggregation_cursor.value()) {

        try {

            auto message_statistics = response->add_error_message_statistics();

            message_statistics->set_number_times_error_occurred(
                    extractFromBsoncxx_k_int32(
                            doc_view,
                            DOCUMENT_COUNT_KEY)
            );

            message_statistics->set_error_origin(
                    ErrorOriginType(
                            extractFromBsoncxx_k_int32(
                                    doc_view,
                                    fresh_errors_keys::ERROR_ORIGIN)
                    )
            );

            message_statistics->set_error_urgency_level(
                    ErrorUrgencyLevel(
                            extractFromBsoncxx_k_int32(
                                    doc_view,
                                    fresh_errors_keys::ERROR_URGENCY)
                    )
            );

            message_statistics->set_version_number(
                    extractFromBsoncxx_k_int32(
                            doc_view,
                            fresh_errors_keys::VERSION_NUMBER)
            );

            message_statistics->set_file_name(
                    extractFromBsoncxx_k_utf8(
                            doc_view,
                            fresh_errors_keys::FILE_NAME)
            );

            message_statistics->set_line_number(
                    extractFromBsoncxx_k_int32(
                            doc_view,
                            fresh_errors_keys::LINE_NUMBER)
            );

            message_statistics->set_most_recent_timestamp(
                    extractFromBsoncxx_k_date(
                            doc_view,
                            fresh_errors_keys::TIMESTAMP_STORED).value.count()
            );

            bsoncxx::array::view api_values = extractFromBsoncxx_k_array(
                    doc_view,
                    fresh_errors_keys::API_NUMBER);

            for (const auto& ele : api_values) {

                const bsoncxx::document::view ele_view = extractFromBsoncxxArrayElement_k_document(ele);

                int api_name_value;
                int num_times_api_found;

                auto api_name_element = ele_view[NAME_OF_FIELD_KEY];
                if (api_name_element && api_name_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                    api_name_value = api_name_element.get_int32().value;
                } else if(api_name_element.type() == bsoncxx::type::k_null) {
                    //if field is null do not save it
                    continue;
                } else {
                    logElementError(__LINE__, __FILE__, api_name_element,
                                    ele_view, bsoncxx::type::k_int32, NAME_OF_FIELD_KEY,
                                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                    throw ErrorExtractingFromBsoncxx("Error requesting user info value " + NAME_OF_FIELD_KEY + ".");
                }

                num_times_api_found = extractFromBsoncxx_k_int32(
                        ele_view,
                        NUMBER_OF_TIMES_REPEATED_KEY);

                handle_errors::ApiNumberSearchResults* api_results = message_statistics->add_api_numbers();

                api_results->set_api_number(api_name_value);
                api_results->set_number_times_api_number_found(num_times_api_found);

            }

            bsoncxx::array::view device_names = extractFromBsoncxx_k_array(
                    doc_view,
                    fresh_errors_keys::DEVICE_NAME);

            for (const auto& ele : device_names) {

                const bsoncxx::document::view ele_view = extractFromBsoncxxArrayElement_k_document(ele);

                std::string device_name_value;
                int num_times_device_name_found;

                auto device_name_element = ele_view[NAME_OF_FIELD_KEY];
                if (device_name_element && device_name_element.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    device_name_value = device_name_element.get_string().value.to_string();
                } else if(device_name_element.type() == bsoncxx::type::k_null) {
                    //if field is null do not save it
                    continue;
                } else {
                    logElementError(__LINE__, __FILE__, device_name_element,
                                    doc_view, bsoncxx::type::k_utf8, NAME_OF_FIELD_KEY,
                                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                    throw ErrorExtractingFromBsoncxx("Error requesting user info value " + NAME_OF_FIELD_KEY + ".");
                }

                num_times_device_name_found = extractFromBsoncxx_k_int32(
                        ele_view,
                        NUMBER_OF_TIMES_REPEATED_KEY);

                handle_errors::DeviceNameSearchResults* device_results = message_statistics->add_device_names();

                device_results->set_device_name(device_name_value);
                device_results->set_number_times_device_name_found(num_times_device_name_found);
            }

        } catch (const ErrorExtractingFromBsoncxx& e) {

            //NOTE: error already stored
            //NOTE: ok to continue here
            continue;
        }

    }

    response->set_success(true);
}