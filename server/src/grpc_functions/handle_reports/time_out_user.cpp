//
// Created by jeremiah on 9/19/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <UserAccountStatusEnum.grpc.pb.h>
#include <DisciplinaryActionType.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <report_handled_move_reason.h>
#include <report_helper_functions.h>

#include "handle_reports.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "store_mongoDB_error_and_exception.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "report_values.h"
#include "general_values.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bsoncxx::document::value generateSuspendedOrBannedField(
        const int& banned_value,
        const int& suspended_value
        ) {

        return document{}
            << "$cond" << open_document
                << "if" << open_document
                    << "$gte" << open_array
                        << "$" + user_account_keys::NUMBER_OF_TIMES_TIMED_OUT
                        << report_values::NUMBER_TIMES_TIMED_OUT_BEFORE_BAN
                    << close_array
                << close_document
                << "then" << banned_value
                << "else" << suspended_value
            << close_document
        << finalize;
}

template <typename T>
bsoncxx::document::value generateDoNotUpdateTimedOutAccounts(
        const std::string& original_value,
        const T& suspended_value
        ) {

        return document{}
            << "$cond" << open_document
                << "if" << open_document
                    << "$or" << open_array
                        << open_document
                            << "$eq" << open_array
                                << "$" + user_account_keys::STATUS
                                << UserAccountStatus::STATUS_BANNED
                            << close_array
                        << close_document
                        << open_document
                            << "$eq" << open_array
                                << "$" + user_account_keys::STATUS
                                << UserAccountStatus::STATUS_SUSPENDED
                            << close_array
                        << close_document
                        << open_document
                            << "$eq" << open_array
                                << "$" + user_account_keys::STATUS
                                << UserAccountStatus::STATUS_REQUIRES_MORE_INFO
                            << close_array
                        << close_document
                    << close_array
                << close_document
                << "then" << original_value
                << "else" << suspended_value
            << close_document
        << finalize;
}

void timeOutUserImplementation(
        const handle_reports::TimeOutUserRequest* request,
        handle_reports::TimeOutUserResponse* response
);

void timeOutUser(
        const handle_reports::TimeOutUserRequest* request,
        handle_reports::TimeOutUserResponse* response
) {
    handleFunctionOperationException(
            [&] {
                timeOutUserImplementation(request, response);
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request);
}

void timeOutUserImplementation(
        const handle_reports::TimeOutUserRequest* request,
        handle_reports::TimeOutUserResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;
    auto error_func = [&response](const std::string& error_str) {
        response->set_successful(false);
        response->set_error_message(error_str);
    };

    {
        std::string error_message;
        std::string login_token_str;
        std::string installation_id;

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
    }

    {
        AdminLevelEnum admin_level;
        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();

        auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
        if (admin_privilege_element
            && admin_privilege_element.type() == bsoncxx::type::k_int32
        ) { //if element exists and is type int32
            admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
        } else { //if element does not exist or is not type int32
            logElementError(__LINE__, __FILE__,
                            admin_privilege_element,
                            admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

            error_func("Error stored on server.");
            return;
        }

        if (!admin_privileges[admin_level].handle_reports()) {
            error_func("Admin level " + AdminLevelEnum_Name(admin_level) +
                       " does not have 'handle_reports' access.");
            return;
        }
    }

    if (isInvalidOIDString(request->user_oid())) {
        error_func("User id '" + request->user_oid() +
                   "' is invalid.");
        return;
    }

    if (request->inactive_message().size() < server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE < request->inactive_message().size()) {
        response->set_successful(false);
        response->set_error_message(
                "Inactive message passed was incorrect length.\nMust be between " +
                std::to_string(server_parameter_restrictions::MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE) + " and " +
                std::to_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE) + " characters.\nMessage contains " +
                std::to_string(request->inactive_message().size()) + " characters.");
        return;
    }

    bsoncxx::oid user_account_oid = bsoncxx::oid{request->user_oid()};
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    if(user_account_oid == event_admin_values::OID) {
        error_func("Cannot time out the event admin account. The account was generated by LetsGo in order to be the admin of event chat rooms.");
        return;
    }

    auto disciplinary_action_taken = DisciplinaryActionTypeEnum(-1);
    auto updated_user_account_status = UserAccountStatus(-1);
    std::chrono::milliseconds timestamp_last_updated = std::chrono::milliseconds(-1);
    std::chrono::milliseconds timestamp_suspension_expires = std::chrono::milliseconds(-1);

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

        bsoncxx::stdx::optional<bsoncxx::document::value> user_account_event_values;
        try {

            mongocxx::options::find opts;

            opts.projection(
                document{}
                    << user_account_keys::ACCOUNT_TYPE << 1
                    << user_account_keys::EVENT_VALUES << 1
                << finalize
            );

            //find user account in case it is an event
            user_account_event_values = user_accounts_collection.find_one(
                *callback_session,
                document{}
                    << "_id" << user_account_oid
                << finalize,
                opts
            );

        } catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", user_account_oid.to_string()
            );

            error_func("Error exception thrown when setting user info.");
            return;
        }

        if(!user_account_event_values) {
            error_func("Failed to find user info.\n'!user_account_event_values' returned.");
            return;
        }

        try {

            const bsoncxx::document::view user_account_event_values_doc = user_account_event_values->view();

            const auto account_type =
                    UserAccountType(
                            extractFromBsoncxx_k_int32(
                                    user_account_event_values_doc,
                                    user_account_keys::ACCOUNT_TYPE
                            )
                    );

            std::string event_account_oid_str;
            if(account_type == UserAccountType::ADMIN_GENERATED_EVENT_TYPE
                || account_type == UserAccountType::USER_GENERATED_EVENT_TYPE) {

                const bsoncxx::document::view event_values =
                        extractFromBsoncxx_k_document(
                                user_account_event_values_doc,
                                user_account_keys::EVENT_VALUES
                        );

                const std::string created_by_oid =
                        extractFromBsoncxx_k_utf8(
                                event_values,
                                user_account_keys::event_values::CREATED_BY
                        );

                if(isInvalidOIDString(created_by_oid)) {

                    const std::string error_string = "An admin event was attempted to be timed out.";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "request", request->DebugString(),
                            "created_by_oid", created_by_oid
                    );

                    moveOutstandingReportsToHandled(
                            mongo_cpp_client,
                            current_timestamp,
                            user_account_oid,
                            request->login_info().admin_name(),
                            ReportHandledMoveReason::REPORT_HANDLED_REASON_REPORTS_DISMISSED,
                            callback_session
                    );

                    error_func("Error calling timeout on an admin created event. Please cancel the event manually.");
                    return;
                } else { //Created by a user
                    event_account_oid_str = user_account_oid_str;
                    const bsoncxx::oid event_account_oid = user_account_oid;
                    user_account_oid = bsoncxx::oid{created_by_oid};

                    mongocxx::stdx::optional<mongocxx::result::update> update_event_account_results;
                    try {

                        const mongocxx::pipeline pipe = buildPipelineForCancelingEvent();

                        //cancel the event and turn off matching
                        update_event_account_results = user_accounts_collection.update_one(
                                *callback_session,
                            document{}
                                << "_id" << event_account_oid
                            << finalize,
                            pipe
                        );

                    } catch (const mongocxx::logic_error& e) {
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), std::string(e.what()),
                                "database", database_names::ACCOUNTS_DATABASE_NAME,
                                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                "ObjectID_used", user_account_oid.to_string()
                        );

                        error_func("Error exception thrown when setting user info.");
                        return;
                    }

                    //update_event_account_results
                    if (!update_event_account_results || update_event_account_results->matched_count() == 0) {
                        error_func("Error updating event to canceled.\nupdate_event_account_results: " + std::string(
                                (update_event_account_results.operator bool()) ? ("exits\nmatched_count: " +
                                                                                  std::to_string(
                                                                                          update_event_account_results->matched_count()) +
                                                                                  "\n") : "failed"));
                        return;
                    }
                }
            }

            bsoncxx::stdx::optional<bsoncxx::document::value> user_account_doc;
            try {

                mongocxx::pipeline pipeline;

                bsoncxx::builder::basic::array time_out_times;

                //There is a static_assert inside report_values.h (right below TIME_OUT_TIMES) that guarantees
                // TIME_OUT_TIMES is size NUMBER_TIMES_TIMED_OUT_BEFORE_BAN. This is important below when
                // generateSuspendedOrBannedField() is called.
                for(int i = 0; i < report_values::NUMBER_TIMES_TIMED_OUT_BEFORE_BAN; ++i) {
                    time_out_times.append(
                        document{}
                            << "case" << open_document
                                << "$eq" << open_array
                                    << "$" + user_account_keys::NUMBER_OF_TIMES_TIMED_OUT
                                    << i
                                << close_array
                            << close_document
                            << "then" << bsoncxx::types::b_date{current_timestamp + report_values::TIME_OUT_TIMES[i]}
                        << finalize
                    );
                }

                const bsoncxx::document::value generate_end_time = document{}
                    << "$switch" << open_document
                        << "branches" << std::move(time_out_times)
                        << "default" << bsoncxx::types::b_date{std::chrono::milliseconds{-1}} //banned, no expiration time
                    << close_document
                << finalize;

                const bsoncxx::document::value increment_timed_out_doc = document{}
                    << "$add" << open_array
                        << "$" + user_account_keys::NUMBER_OF_TIMES_TIMED_OUT
                        << 1
                    << close_array
                << finalize;

                const bsoncxx::document::value add_disciplinary_action_element = document{}
                    << "$let" << open_document
                        << "vars" << open_document
                            << "action_taken" << generateSuspendedOrBannedField(DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_BANNED,DisciplinaryActionTypeEnum::DISCIPLINE_ACCOUNT_TIME_OUT_SUSPENDED)
                            << "end_time" << generate_end_time.view()
                        << close_document
                        << "in" << open_document
                            << "$concatArrays" << open_array
                                << "$" + user_account_keys::DISCIPLINARY_RECORD
                                << open_array
                                    << open_document
                                        << user_account_keys::disciplinary_record::SUBMITTED_TIME << bsoncxx::types::b_date{current_timestamp}
                                        << user_account_keys::disciplinary_record::END_TIME << "$$end_time"
                                        << user_account_keys::disciplinary_record::ACTION_TYPE << "$$action_taken"
                                        << user_account_keys::disciplinary_record::REASON << request->inactive_message()
                                        << user_account_keys::disciplinary_record::ADMIN_NAME << request->login_info().admin_name()
                                    << close_document
                                << close_array
                            << close_array
                        << close_document
                    << close_document
                    << finalize;

                document add_fields_doc;
                add_fields_doc
                    << user_account_keys::STATUS << generateDoNotUpdateTimedOutAccounts(
                                            "$" + user_account_keys::STATUS,
                                            generateSuspendedOrBannedField(UserAccountStatus::STATUS_BANNED,UserAccountStatus::STATUS_SUSPENDED)
                                            )
                    << user_account_keys::INACTIVE_MESSAGE << generateDoNotUpdateTimedOutAccounts("$" + user_account_keys::INACTIVE_MESSAGE, request->inactive_message())
                    << user_account_keys::INACTIVE_END_TIME << generateDoNotUpdateTimedOutAccounts("$" + user_account_keys::INACTIVE_END_TIME , generate_end_time.view())
                    << user_account_keys::DISCIPLINARY_RECORD << generateDoNotUpdateTimedOutAccounts("$" + user_account_keys::DISCIPLINARY_RECORD, add_disciplinary_action_element.view())
                    << user_account_keys::NUMBER_OF_TIMES_TIMED_OUT << generateDoNotUpdateTimedOutAccounts("$" + user_account_keys::NUMBER_OF_TIMES_TIMED_OUT, increment_timed_out_doc.view());

                if(!event_account_oid_str.empty()) {
                    //If this was an event that was canceled, set it to canceled inside USER_CREATED_EVENTS.
                    add_fields_doc
                        << user_account_keys::USER_CREATED_EVENTS << open_document
                            << "$map" << open_document
                                << "input" << user_account_keys::USER_CREATED_EVENTS
                                << "in" << open_document
                                    << "$cond" << open_document

                                        << "if" << open_document
                                            << "$eq" << open_array
                                                << "$$this." + user_account_keys::user_created_events::EVENT_OID
                                                << bsoncxx::oid{event_account_oid_str}
                                            << close_array
                                        << close_document
                                        << "then" << open_document
                                            << "$mergeObjects" << open_array
                                                << "$$this"
                                                << open_document
                                                    << user_account_keys::user_created_events::EVENT_STATE << LetsGoEventStatus::CANCELED
                                                << close_document
                                            << close_array
                                        << close_document
                                        << "else" << "$$this"

                                    << close_document
                                << close_document
                            << close_document
                        << close_document;
                }

                pipeline.add_fields(add_fields_doc.view());

                mongocxx::options::find_one_and_update opts;

                //project only the final element of the array
                opts.projection(
                    document{}
                        << user_account_keys::STATUS << 1
                        << user_account_keys::DISCIPLINARY_RECORD << open_document
                                << "$slice" << -1
                        << close_document
                    << finalize
                );

                opts.return_document(mongocxx::options::return_document::k_after);

                //update user account document
                user_account_doc = user_accounts_collection.find_one_and_update(
                    *callback_session,
                    document{}
                        << "_id" << user_account_oid
                    << finalize,
                    pipeline,
                    opts
                );

            } catch (const mongocxx::logic_error& e) {
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "ObjectID_used", user_account_oid_str
                );

                error_func("Error exception thrown when setting user info.");
                return;
            }

            if (!user_account_doc) {
                error_func("Failed to find or update user info.\n'!user_account_doc' returned.");
                return;
            }

            const bsoncxx::document::view user_account_doc_view = user_account_doc->view();

            updated_user_account_status =
                UserAccountStatus(
                        extractFromBsoncxx_k_int32(
                                user_account_doc_view,
                                user_account_keys::STATUS
                        )
                );

            const bsoncxx::array::view disciplinary_record = extractFromBsoncxx_k_array(
                    user_account_doc_view,
                    user_account_keys::DISCIPLINARY_RECORD
            );

            //only 1 element should exist inside this array
            for(const auto& doc : disciplinary_record) {
                if (doc.type() == bsoncxx::type::k_document) { //if element exists and is type array
                    const bsoncxx::document::view doc_view = doc.get_document().value;

                    timestamp_last_updated = extractFromBsoncxx_k_date(
                            doc_view,
                            user_account_keys::disciplinary_record::SUBMITTED_TIME).value;

                    disciplinary_action_taken =
                            DisciplinaryActionTypeEnum(
                                    extractFromBsoncxx_k_int32(
                                            doc_view,
                                            user_account_keys::disciplinary_record::ACTION_TYPE)
                            );

                    timestamp_suspension_expires = extractFromBsoncxx_k_date(
                            doc_view,
                            user_account_keys::disciplinary_record::END_TIME).value;

                } else { //if element does not exist or is not type array
                    logElementError(
                            __LINE__, __FILE__,
                            bsoncxx::document::element{},user_account_doc_view,
                            bsoncxx::type::k_document, user_account_keys::DISCIPLINARY_RECORD,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                    throw ErrorExtractingFromBsoncxx("Error requesting user " + user_account_keys::DISCIPLINARY_RECORD + ".");
                }
            }

        } catch (const ErrorExtractingFromBsoncxx& e) {
            error_func(e.what());
            return;
        }

        if(timestamp_last_updated == current_timestamp) { //if disciplinary action was properly updated
            moveOutstandingReportsToHandled(
                    mongo_cpp_client,
                    current_timestamp,
                    user_account_oid,
                    request->login_info().admin_name(),
                    ReportHandledMoveReason::REPORT_HANDLED_REASON_DISCIPLINARY_ACTION_TAKEN,
                    callback_session,
                    disciplinary_action_taken
            );
        }

    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    try {

        //setting successful to true here, if the transaction fails it will be set to false
        response->set_successful(true);

        session.with_transaction(transaction_callback);

        response->set_updated_user_account_status(updated_user_account_status);
        response->set_timestamp_last_updated(timestamp_last_updated.count());
        response->set_timestamp_suspension_expires(timestamp_suspension_expires.count());
    } catch (const mongocxx::logic_error& e) {
        //Finished
        error_func("Exception when running transaction to time out user.\n" + std::string(e.what()));
        return;
    }

}