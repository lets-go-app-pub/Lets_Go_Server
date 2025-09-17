//
// Created by jeremiah on 3/8/23.
//

#include "user_event_commands.h"

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <utility_chat_functions.h>

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "matching_algorithm.h"
#include "user_pictures_keys.h"
#include "create_chat_room_helper.h"
#include "get_login_document.h"
#include "run_initial_login_operation.h"
#include "global_bsoncxx_docs.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void cancelEventImplementation(const user_event_commands::CancelEventRequest* request,
                                 user_event_commands::CancelEventResponse* response);

void cancelEvent(const user_event_commands::CancelEventRequest* request,
                     user_event_commands::CancelEventResponse* response) {
    handleFunctionOperationException(
            [&] {
                cancelEventImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void cancelEventImplementation(
        const user_event_commands::CancelEventRequest* request,
        user_event_commands::CancelEventResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                         const std::string& passed_error_message) {
        response->set_error_string(passed_error_message);
        admin_info_doc_value = std::move(returned_admin_info_doc);
    };

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ADMIN_AND_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id,
            store_error_message
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    } else if (request->login_info().admin_info_used()) {
        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
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
            response->set_return_status(LG_ERROR);
            return;
        }

        if (!admin_privileges[admin_level].cancel_events()) {
            response->set_return_status(LG_ERROR);
            response->set_error_string("Admin level " + AdminLevelEnum_Name(admin_level) +
                       " does not have 'cancel_events' access.");
            return;
        }
    }

    if(isInvalidOIDString(request->event_oid())) {
        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("An invalid event oid of " + request->event_oid() + " was passed.");
        } else { //user
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        }
        return;
    }

    const bsoncxx::oid event_oid{request->event_oid()};

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds& current_timestamp = getCurrentTimestamp();

    if(!request->login_info().admin_info_used()) { //user

        const bsoncxx::document::value merge_document = document{} << finalize;

        const bsoncxx::document::value projection_document = document{}
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << finalize;

        const bsoncxx::document::value login_document = getLoginDocument<false>(
                login_token_str,
                installation_id,
                merge_document,
                current_timestamp
        );

        bsoncxx::stdx::optional<bsoncxx::document::value> user_account_doc;
        if (!runInitialLoginOperation(
                user_account_doc,
                user_accounts_collection,
                bsoncxx::oid{user_account_oid_str},
                login_document,
                projection_document)
                ) {
            //Error already stored here
            return;
        }

        const ReturnStatus return_status = checkForValidLoginToken(
                user_account_doc,
                user_account_oid_str
        );

        if (return_status != ReturnStatus::SUCCESS) {
            response->set_return_status(return_status);
            return;
        }
    }

    document query_event_doc;
    query_event_doc
        << "_id" << event_oid
        << user_account_keys::EVENT_EXPIRATION_TIME << open_document
            << "$gt" << bsoncxx::types::b_date{current_timestamp}
        << close_document;

    if(!request->login_info().admin_info_used()) {
        query_event_doc
            << user_account_keys::EVENT_VALUES + "." + user_account_keys::event_values::CREATED_BY << user_account_oid_str;
    }

    bsoncxx::stdx::optional<bsoncxx::document::value> find_event_account;
    std::optional<std::string> exception_string;
    try {
        mongocxx::options::find_one_and_update opts;

        opts.projection(
            document{}
                << user_account_keys::EVENT_VALUES + "." + user_account_keys::event_values::CREATED_BY << 1
            << finalize
        );

        const mongocxx::pipeline pipe = buildPipelineForCancelingEvent();

        find_event_account = user_accounts_collection.find_one_and_update(
            query_event_doc.view(),
            pipe,
            opts
        );
    }
    catch (const mongocxx::logic_error& e) {
        exception_string = e.what();
    }

    if(!find_event_account) {
        const std::string error_string = "Failed to find event account.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                "request", request->DebugString(),
                "query_event_doc", query_event_doc.view()
        );

        if (request->login_info().admin_info_used()) { //admin
            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string(error_string);
        } else { //user
            //In this case either the event was not created, it has already expired or the event oid was not found. In
            // any of these cases the client has done everything they can.
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp(current_timestamp.count());
        }
        return;
    }

    std::string created_by_oid_str = user_account_oid_str;
    if(request->login_info().admin_info_used()) { //admin

        const bsoncxx::document::view find_event_account_doc = find_event_account->view();
        bsoncxx::document::view event_values_doc;

        auto event_values_doc_element = find_event_account_doc[user_account_keys::EVENT_VALUES];
        if(event_values_doc_element
           && event_values_doc_element.type() == bsoncxx::type::k_document) {
            event_values_doc = event_values_doc_element.get_document().value;
        } else {
            logElementError(
                    __LINE__, __FILE__,
                    event_values_doc_element,find_event_account_doc,
                    bsoncxx::type::k_document, user_account_keys::EVENT_VALUES,
                    database_names::ACCOUNTS_DATABASE_NAME,collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );

            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("EVENT_VALUES not found inside event document.");
            return;
        }

        auto created_by_element = event_values_doc[user_account_keys::event_values::CREATED_BY];
        if(created_by_element
           && created_by_element.type() == bsoncxx::type::k_utf8) {
            created_by_oid_str = created_by_element.get_string().value.to_string();
        } else {
            logElementError(
                    __LINE__, __FILE__,
                    created_by_element,event_values_doc,
                    bsoncxx::type::k_document, user_account_keys::event_values::CREATED_BY,
                    database_names::ACCOUNTS_DATABASE_NAME,collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );

            response->set_return_status(ReturnStatus::LG_ERROR);
            response->set_error_string("event_values::CREATED_BY not found inside event document.");
            return;
        }

        if(isInvalidOIDString(created_by_oid_str)) {
            //Then this event was NOT created by a user. Event has been canceled, can complete.
            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp(current_timestamp.count());
            return;
        }

    }

    mongocxx::stdx::optional<mongocxx::result::update> update_user_account;
    try {

        static const std::string ELEM = "e";
        static const std::string ELEM_UPDATE = user_account_keys::USER_CREATED_EVENTS + ".$[" + ELEM + "].";

        bsoncxx::builder::basic::array array_builder{};
        array_builder.append(
            document{}
                << ELEM + "." + user_account_keys::user_created_events::EVENT_OID << event_oid
            << finalize
        );

        mongocxx::options::update opts;

        opts.array_filters(array_builder.view());

        update_user_account = user_accounts_collection.update_one(
            document{}
                << "_id" << bsoncxx::oid{created_by_oid_str}
            << finalize,
            document{}
                << "$set" << open_document
                    << ELEM_UPDATE + user_account_keys::user_created_events::EVENT_STATE << LetsGoEventStatus::CANCELED
                << close_document
            << finalize,
            opts
        );

    } catch (const mongocxx::logic_error& e) {
        exception_string = e.what();
    }

    if(!update_user_account || update_user_account->modified_count() < 1) {
        const std::string error_string = "Failed to find user account that created event. This could mean that the user deleted their account.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                exception_string, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNT_STATISTICS_COLLECTION_NAME,
                "request", request->DebugString(),
                "query_event_doc", query_event_doc.view()
        );

        if (request->login_info().admin_info_used()) { //admin
            response->set_error_string(error_string);
        }

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    response->set_return_status(ReturnStatus::SUCCESS);
    response->set_timestamp(current_timestamp.count());
}