//
// Created by jeremiah on 3/26/23.
//

#include "admin_event_commands.h"

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "matching_algorithm.h"
#include "create_complete_chat_room_with_user.h"
#include "user_account_keys.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void getEventsImplementation(
        const admin_event_commands::GetEventsRequest* request,
        admin_event_commands::GetEventsResponse* response
);

void getEvents(
        const admin_event_commands::GetEventsRequest* request,
        admin_event_commands::GetEventsResponse* response
) {
    handleFunctionOperationException(
            [&] {
                getEventsImplementation(request, response);
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            }, __LINE__, __FILE__, request);
}

void getEventsImplementation(
        const admin_event_commands::GetEventsRequest* request,
        admin_event_commands::GetEventsResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string error_message;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    auto error_func = [&response](const std::string& error_str) {
        response->set_successful(false);
        response->set_error_message(error_str);
    };

    auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                   const std::string& passed_error_message) {
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

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_successful(false);
        error_func("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
        return;
    } else if (!admin_info_doc_value) {
        error_func("Could not find admin document.");
        return;
    }

    bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element
        && admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                admin_privilege_element, admin_info_doc_view,
                bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );
        error_func("Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].request_events()) {
        error_func("Admin level " + AdminLevelEnum_Name(admin_level) +
                   " does not have 'request_events' access.");
        return;
    }

    if (request->events_created_by_case() !=
        admin_event_commands::GetEventsRequest::EventsCreatedByCase::kAccountTypesToSearch
        && request->events_created_by_case() !=
           admin_event_commands::GetEventsRequest::EventsCreatedByCase::kSingleUserCreatedBy) {
        error_func("Invalid 'events_created_by_case' passed to the server.");
        return;
    }

    if (request->events_created_by_case() ==
        admin_event_commands::GetEventsRequest::EventsCreatedByCase::kAccountTypesToSearch) {
        if (request->account_types_to_search().events_to_search_by().empty()) {
            error_func("Must pass in at least one valid UserAccountType when searching by account type.");
            return;
        }
        for (const auto& x: request->account_types_to_search().events_to_search_by()) {
            if (x < UserAccountType::ADMIN_GENERATED_EVENT_TYPE
                || !UserAccountType_IsValid(x)) {
                error_func("Invalid UserAccountType of " + UserAccountType_Name(x) + " passed to the server.");
                return;
            }
        }
    } else if (
            request->events_created_by_case() ==
            admin_event_commands::GetEventsRequest::EventsCreatedByCase::kAccountTypesToSearch
            && request->single_user_created_by().empty()
            ) {
        error_func("Must pass in a value to single_user_created_by.");
        return;
    }

    if (request->start_time_search() != admin_event_commands::GetEventsRequest_TimeSearch_GREATER_THAN
        && request->start_time_search() != admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN
        && request->start_time_search() != admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH
            ) {
        error_func("Must pass in a valid TimeSearch value to start_time_search.\nValue: " +
                   std::to_string(request->start_time_search()));
        return;
    }

    if (request->expiration_time_search() != admin_event_commands::GetEventsRequest_TimeSearch_GREATER_THAN
        && request->expiration_time_search() != admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN
        && request->expiration_time_search() != admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH
            ) {
        error_func("Must pass in a valid TimeSearch value to expiration_time_search.\nValue: " +
                   std::to_string(request->expiration_time_search()));
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds& current_timestamp = getCurrentTimestamp();

    std::optional<std::string> find_event_accounts_exception_string;
    bsoncxx::stdx::optional<mongocxx::cursor> find_event_accounts_cursor;
    try {

        mongocxx::options::find opts;

        opts.projection(
            document{}
                << "_id" << 1
                << user_account_keys::TIME_CREATED << 1
                << user_account_keys::EVENT_VALUES << 1
                << user_account_keys::EVENT_EXPIRATION_TIME << 1
                << user_account_keys::ACCOUNT_TYPE << 1
                << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES << 1
                << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO << 1
                << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK << 1
                << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT << 1
            << finalize
        );

        bsoncxx::builder::stream::document query_doc;

        if (
                request->events_created_by_case() ==
                admin_event_commands::GetEventsRequest::EventsCreatedByCase::kAccountTypesToSearch
                ) {
            bsoncxx::builder::basic::array account_types_arr;

            for (const auto& account_type: request->account_types_to_search().events_to_search_by()) {
                account_types_arr.append(account_type);
            }

            query_doc
                    << user_account_keys::ACCOUNT_TYPE << open_document
                        << "$in" << account_types_arr
                    << close_document;
        } else { //kSingleUserCreatedBy
            query_doc
                    << user_account_keys::ACCOUNT_TYPE << open_document
                        << "$gte" << UserAccountType::ADMIN_GENERATED_EVENT_TYPE
                    << close_document
                    << user_account_keys::EVENT_VALUES + "." + user_account_keys::event_values::CREATED_BY << request->single_user_created_by();
        }

        switch (request->start_time_search()) {
            case admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH: {
                //search all docs
                query_doc
                        << user_account_keys::TIME_CREATED << open_document
                            << "$gte" << bsoncxx::types::b_date{std::chrono::milliseconds{-1337}}
                        << close_document;
                break;
            }
            case admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN: {
                query_doc
                        << user_account_keys::TIME_CREATED << open_document
                            << "$lt" << bsoncxx::types::b_date{std::chrono::milliseconds{request->events_start_time_ms()}}
                        << close_document;
                break;
            }
            case admin_event_commands::GetEventsRequest_TimeSearch_GREATER_THAN: {
                query_doc
                        << user_account_keys::TIME_CREATED << open_document
                            << "$gt" << bsoncxx::types::b_date{std::chrono::milliseconds{request->events_start_time_ms()}}
                        << close_document;
                break;
            }
            case admin_event_commands::GetEventsRequest_TimeSearch_GetEventsRequest_TimeSearch_INT_MIN_SENTINEL_DO_NOT_USE_:
            case admin_event_commands::GetEventsRequest_TimeSearch_GetEventsRequest_TimeSearch_INT_MAX_SENTINEL_DO_NOT_USE_:
                error_func("Must pass in a valid TimeSearch value to start_time_search.\nValue: " +
                           std::to_string(request->start_time_search()));
                return;
        }

        switch (request->expiration_time_search()) {
            case admin_event_commands::GetEventsRequest_TimeSearch_NO_SEARCH: {
                //search all docs
                query_doc
                        << user_account_keys::EVENT_EXPIRATION_TIME << open_document
                            << "$gte" << bsoncxx::types::b_date{std::chrono::milliseconds{-1337}}
                        << close_document;
                break;
            }
            case admin_event_commands::GetEventsRequest_TimeSearch_LESS_THAN: {
                //Don't allow it to find canceled events
                query_doc
                        << user_account_keys::EVENT_EXPIRATION_TIME << open_document
                            << "$lt" << bsoncxx::types::b_date{std::chrono::milliseconds{request->events_expiration_time_ms()}}
                        << close_document
                        << user_account_keys::EVENT_EXPIRATION_TIME << open_document
                            << "$gt" << general_values::event_expiration_time_values::EVENT_CANCELED
                        << close_document;
                break;
            }
            case admin_event_commands::GetEventsRequest_TimeSearch_GREATER_THAN: {
                query_doc
                        << user_account_keys::EVENT_EXPIRATION_TIME << open_document
                            << "$gt" << bsoncxx::types::b_date{
                        std::chrono::milliseconds{request->events_expiration_time_ms()}}
                        << close_document;
                break;
            }
            case admin_event_commands::GetEventsRequest_TimeSearch_GetEventsRequest_TimeSearch_INT_MIN_SENTINEL_DO_NOT_USE_:
            case admin_event_commands::GetEventsRequest_TimeSearch_GetEventsRequest_TimeSearch_INT_MAX_SENTINEL_DO_NOT_USE_:
                error_func("Must pass in a valid TimeSearch value to expiration_time_search.\nValue: " +
                           std::to_string(request->expiration_time_search()));
                return;
        }

        find_event_accounts_cursor = user_accounts_collection.find(
                query_doc.view(),
                opts
        );
    }
    catch (const mongocxx::logic_error& e) {
        find_event_accounts_exception_string = e.what();
    }

    if (!find_event_accounts_cursor) {
        const std::string error_string = "An error occurred when querying for documentsNo curso";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                find_event_accounts_exception_string, error_string,
                "request", request->DebugString()
        );

        error_func("Must pass in a valid TimeSearch value to start_time_search.\nValue: " +
                   std::to_string(request->start_time_search()));
        return;
    }

    int num_events_found = 0;
    try {
        for (const auto& doc: *find_event_accounts_cursor) {
            admin_event_commands::SingleEvent* single_event = response->add_events();

            single_event->set_event_oid(
                    extractFromBsoncxx_k_oid(
                            doc,
                            "_id"
                    ).to_string()
            );

            single_event->set_time_created_ms(
                    extractFromBsoncxx_k_date(
                            doc,
                            user_account_keys::TIME_CREATED
                    ).value.count()
            );

            const std::chrono::milliseconds event_expiration_time = extractFromBsoncxx_k_date(
                    doc,
                    user_account_keys::EVENT_EXPIRATION_TIME
            ).value;

            single_event->set_expiration_time_ms(event_expiration_time.count());

            const bsoncxx::document::view event_values_doc = extractFromBsoncxx_k_document(
                    doc,
                    user_account_keys::EVENT_VALUES
            );

            single_event->set_created_by(
                    extractFromBsoncxx_k_utf8(
                            event_values_doc,
                            user_account_keys::event_values::CREATED_BY
                    )
            );

            single_event->set_chat_room_id(
                    extractFromBsoncxx_k_utf8(
                            event_values_doc,
                            user_account_keys::event_values::CHAT_ROOM_ID
                    )
            );

            single_event->set_event_title(
                    extractFromBsoncxx_k_utf8(
                            event_values_doc,
                            user_account_keys::event_values::EVENT_TITLE
                    )
            );

            single_event->set_account_type(
                    UserAccountType(
                            extractFromBsoncxx_k_int32(
                                    doc,
                                    user_account_keys::ACCOUNT_TYPE
                            )
                    )
            );

            const LetsGoEventStatus event_status = convertExpirationTimeToEventStatus(
                    event_expiration_time,
                    current_timestamp
            );

            single_event->set_event_status(event_status);

            single_event->set_number_swipes_yes(
                    extractFromBsoncxx_k_int32(
                            doc,
                            user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES
                    )
            );

            single_event->set_number_swipes_no(
                    extractFromBsoncxx_k_int32(
                            doc,
                            user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_NO
                    )
            );

            const int num_times_block = extractFromBsoncxx_k_int32(
                    doc,
                    user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_BLOCK
            );
            const int num_times_report = extractFromBsoncxx_k_int32(
                    doc,
                    user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_REPORT
            );
            single_event->set_number_swipes_block_and_report(
                    num_times_block + num_times_report
            );

            num_events_found++;
        }
    }
    catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored here
        error_func("Error extracting a value from the database.\nException: " + std::string(e.what()));
        return;
    }

    if(num_events_found == 0) {
        error_func("Not events found with the specified search criteria.");
        return;
    } else {
        response->set_successful(true);
        response->set_timestamp_ms(current_timestamp.count());
    }
}