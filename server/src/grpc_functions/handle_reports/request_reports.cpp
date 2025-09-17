//
// Created by jeremiah on 9/20/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <admin_functions_for_request_values.h>
#include <utility_chat_functions.h>

#include "handle_reports.h"

#include "handle_function_operation_exception.h"
#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "store_mongoDB_error_and_exception.h"
#include "extract_data_from_bsoncxx.h"
#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"
#include "outstanding_reports_keys.h"
#include "report_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void requestReportsImplementation(
        const handle_reports::RequestReportsRequest* request,
        grpc::ServerWriterInterface<handle_reports::RequestReportsResponse>* response_writer
);

//can throw ErrorExtractingFromBsoncxx
bsoncxx::oid saveReportDocToResponse(handle_reports::ReportedUserInfo* reported_user, bsoncxx::document::view doc_view);

void requestReports(
        const handle_reports::RequestReportsRequest* request,
        grpc::ServerWriterInterface<handle_reports::RequestReportsResponse>* response_writer
) {
    handleFunctionOperationException(
            [&] {
                requestReportsImplementation(request, response_writer);
            },
            [&] {
                handle_reports::RequestReportsResponse response;
                response.set_successful(false);
                response.set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
                response_writer->Write(response);
            },
            [&] {
                handle_reports::RequestReportsResponse response;
                response.set_successful(false);
                response.set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
                response_writer->Write(response);
            },
            __LINE__, __FILE__, request);
}

void requestReportsImplementation(
        const handle_reports::RequestReportsRequest* request,
        grpc::ServerWriterInterface<handle_reports::RequestReportsResponse>* response_writer
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;

    auto error_func = [&response_writer](const std::string& error_str) {
        handle_reports::RequestReportsResponse response;
        response.set_successful(false);
        response.set_error_message(error_str);
        response_writer->Write(response);
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

        ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            error_func("ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        } else if (!admin_info_doc_value) {
            error_func("Could not find admin document.");
            return;
        }

    }

    AdminLevelEnum admin_level;

    {

        const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();

        auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
        if (admin_privilege_element &&
            admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
        } else { //if element does not exist or is not type int32
            logElementError(__LINE__, __FILE__,
                            admin_privilege_element,
                            admin_info_doc_view, bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME);

            error_func("Error stored on server.");
            return;
        }

    }

    if (!admin_privileges[admin_level].handle_reports()) {
        error_func("Admin level " + AdminLevelEnum_Name(admin_level) +
                   " does not have 'handle_reports' access.");
        return;
    }

    if (request->number_user_to_request() < 1) {
        error_func(
                "Must request at least 1 report, requested " + std::to_string(request->number_user_to_request()) + ".");
        return;
    }

    int number_to_request = 0;
    if (request->number_user_to_request() > report_values::MAX_NUMBER_OF_REPORTED_USERS_ADMIN_CAN_REQUEST_NOTIFIED) {
        number_to_request = report_values::MAX_NUMBER_OF_REPORTED_USERS_ADMIN_CAN_REQUEST_NOTIFIED;
    } else {
        number_to_request = request->number_user_to_request();
    }

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database reports_db = mongo_cpp_client[database_names::REPORTS_DATABASE_NAME];
    mongocxx::collection outstanding_reports_collection = reports_db[collection_names::OUTSTANDING_REPORTS_COLLECTION_NAME];

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::database deleted_db = mongo_cpp_client[database_names::DELETED_DATABASE_NAME];
    mongocxx::collection deleted_user_accounts_collection = deleted_db[collection_names::DELETED_DELETED_ACCOUNTS_COLLECTION_NAME];

    bsoncxx::document::value find_outstanding_reports_doc = document{}
            << "$expr" << open_document
                << "$and" << open_array

                    //limit reached has been set
                    << open_document
                        << "$gt" << open_array
                            << "$" + outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED
                            << bsoncxx::types::b_date{std::chrono::milliseconds{-1}}
                        << close_array
                    << close_document

                    << open_document
                        //report has NOT been checked out
                        << "$or" << open_array
                            << open_document
                                << "$eq" << open_array
                                    << open_document
                                        << "$type" << "$" + outstanding_reports_keys::CHECKED_OUT_END_TIME_REACHED
                                    << close_document
                                    << "missing"
                                << close_array
                            << close_document
                            << open_document
                                << "$lt" << open_array
                                    << "$" + outstanding_reports_keys::CHECKED_OUT_END_TIME_REACHED
                                    << bsoncxx::types::b_date{current_timestamp}
                                << close_array
                            << close_document
                        << close_array
                    << close_document

                << close_array
            << close_document
            << finalize;

    bsoncxx::document::value update_outstanding_reports_doc = document{}
            << "$set" << open_document
                << outstanding_reports_keys::CHECKED_OUT_END_TIME_REACHED << bsoncxx::types::b_date{current_timestamp + report_values::CHECK_OUT_TIME}
            << close_document
            << finalize;

    std::vector<std::unique_ptr<handle_reports::ReportedUserInfo>> reports;

    if (number_to_request == 1) {

        bsoncxx::stdx::optional<bsoncxx::document::value> outstanding_report;
        try {
            mongocxx::options::find_one_and_update opts;

            //this does work although in the shell findOne() will not work with sort()
            opts.sort(
                    document{}
                            << outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED << 1
                    << finalize
            );

            outstanding_report = outstanding_reports_collection.find_one_and_update(
                    find_outstanding_reports_doc.view(),
                    update_outstanding_reports_doc.view(),
                    opts);
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

        reports.emplace_back(
                std::make_unique<handle_reports::ReportedUserInfo>()
                );

        if (outstanding_report) {
            bsoncxx::document::view doc_view = outstanding_report->view();
            try {
                saveReportDocToResponse(reports.back().get(), doc_view);
            } catch (const ErrorExtractingFromBsoncxx& e) {
                error_func(e.what());
                return;
            }
        } else {
            //if outstanding_report is not set, this means no more reports to check out atm
            handle_reports::RequestReportsResponse response;
            response.set_successful(true);
            response.mutable_returned_info()->mutable_no_reports();
            response_writer->Write(response);
            return;
        }

    }
    else { //more than 1 document requested

        //This transaction is called here in order to avoid the 'gap' between when the reports are found and when they
        // are updated. There is no way currently in mongodb to find and update multiple documents (that I can find).
        // Perhaps something using $merge could work, however the gap between find and update there is probably not
        // a transaction either.
        mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

            mongocxx::stdx::optional<mongocxx::cursor> outstanding_reports;
            try {
                mongocxx::options::find opts;

                opts.sort(
                        document{}
                                << outstanding_reports_keys::TIMESTAMP_LIMIT_REACHED << 1
                        << finalize
                );

                opts.limit(number_to_request);

                outstanding_reports = outstanding_reports_collection.find(
                        *callback_session,
                        find_outstanding_reports_doc.view(),
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

            if (!outstanding_reports) {
                error_func("Cursor was not set after finding report documents.");
                return;
            }

            bsoncxx::builder::basic::array extracted_ids;

            try {

                for (const auto& doc : *outstanding_reports) {
                    reports.emplace_back(
                            std::make_unique<handle_reports::ReportedUserInfo>()
                            );
                    bsoncxx::document::view doc_view = doc;
                    extracted_ids.append(saveReportDocToResponse(reports.back().get(), doc_view));
                }

            } catch (const ErrorExtractingFromBsoncxx& e) {
                error_func(e.what());
                return;
            }

            const bsoncxx::array::view extracted_ids_view = extracted_ids.view();

            if (extracted_ids_view.empty()) { //no reports to return
                handle_reports::RequestReportsResponse response;
                response.set_successful(true);
                response.mutable_returned_info()->mutable_no_reports();
                response_writer->Write(response);
                return;
            }

            mongocxx::stdx::optional<mongocxx::result::update> update_reports;
            try {

                update_reports = outstanding_reports_collection.update_many(
                        *callback_session,
                        document{}
                                << "_id" << open_document
                                        << "$in" << extracted_ids_view
                                << close_document
                        << finalize,
                        update_outstanding_reports_doc.view()
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

            if (!update_reports) {
                error_func("Error when updating extracted reports.\n !update_reports received.");
                return;
            } else if (update_reports->modified_count() != (int32_t)reports.size()) {
                error_func("Error when updating extracted reports.\n modified_count(): " +
                           std::to_string(update_reports->modified_count()) +
                           "\nexpected: " + std::to_string(reports.size()));
                return;
            }

        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        try {
            session.with_transaction(transaction_callback);
        } catch (const mongocxx::logic_error& e) {
            //Finished
            error_func("Exception when running transaction to time out user.\n" + std::string(e.what()));
            return;
        }
    }

    if (reports.empty()) {
        handle_reports::RequestReportsResponse response;
        response.set_successful(true);
        response.mutable_returned_info()->mutable_no_reports();
        response_writer->Write(response);
        return;
    }

    //extract users and message UUIDs for reports
    for (auto & report : reports) {

        ///report is swapped into the response here, so don't use it below (use mutable_reported_user_info())
        //the client expects the RequestReportsResponse first
        handle_reports::RequestReportsResponse report_info_response;
        report_info_response.set_successful(true);
        report_info_response.mutable_returned_info()->mutable_reported_user_info()->Swap(report.release());
        response_writer->Write(report_info_response);

        const std::string& reported_user_account_oid_str = report_info_response.returned_info().reported_user_info().user_account_oid();

        {

            const bsoncxx::document::value query_doc = document{}
                << "_id" << bsoncxx::oid{reported_user_account_oid_str}
            << finalize;

            //extract reported user info
            handle_reports::RequestReportsResponse user_info_response;

            if (extractUserInfo(
                    mongo_cpp_client,
                    accounts_db,
                    deleted_db,
                    user_accounts_collection,
                    deleted_user_accounts_collection,
                    true,
                    query_doc.view(),
                    user_info_response.mutable_returned_info()->mutable_user_account_info(),
                    error_func)
            ) { //success
                user_info_response.set_successful(true);
                response_writer->Write(user_info_response);
            } else { //failed
                //error is already sent using error_func and stored on server
                return;
            }
        }

        //extract any referenced messages
        for(const auto& single_report: report_info_response.mutable_returned_info()->mutable_reported_user_info()->reports_log()) {

            //not all reports have a message stored, if they do then chat room id and message uuid will be valid
            if(isInvalidChatRoomId(single_report.chat_room_id()) || isInvalidUUID(single_report.message_uuid())) {
                continue;
            }

            handle_reports::RequestReportsResponse message_info_response;
            mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + single_report.chat_room_id()];

            if(extractSingleMessageAtTimeStamp(
                    mongo_cpp_client,
                    accounts_db,
                    user_accounts_collection,
                    chat_room_collection,
                    single_report.chat_room_id(),
                    single_report.message_uuid(),
                    std::chrono::milliseconds{single_report.timestamp_submitted()},
                    message_info_response.mutable_returned_info()->mutable_chat_message(),
                    error_func)
            ) { //successful
                message_info_response.set_successful(true);
                response_writer->Write(message_info_response);
            } else { //failed
                //error is already sent using error_func and stored on server
                return;
            }
        }

        //signal all info for report has been sent
        handle_reports::RequestReportsResponse report_completed_response;
        report_completed_response.set_successful(true);
        report_completed_response.mutable_returned_info()->mutable_completed()->set_user_account_oid(reported_user_account_oid_str);
        response_writer->Write(report_completed_response);
    }

    //if not enough reports were returned,
    if((int)reports.size() < number_to_request) {
        handle_reports::RequestReportsResponse response;
        response.set_successful(true);
        response.mutable_returned_info()->mutable_no_reports();
        response_writer->Write(response);
        return;
    }

}

bsoncxx::oid saveReportDocToResponse(
        handle_reports::ReportedUserInfo* reported_user,
        bsoncxx::document::view doc_view
) {

    bsoncxx::oid reported_user_oid = extractFromBsoncxx_k_oid(
            doc_view,
            "_id"
    );

    reported_user->set_user_account_oid(reported_user_oid.to_string());

    bsoncxx::array::view reports_log = extractFromBsoncxx_k_array(
            doc_view,
            outstanding_reports_keys::REPORTS_LOG
    );

    for (const auto& report : reports_log) {
        if (report.type() == bsoncxx::type::k_document) {
            bsoncxx::document::view report_view = report.get_document().value;
            auto single_report = reported_user->add_reports_log();

            single_report->set_account_oid_of_report_sender(
                    extractFromBsoncxx_k_oid(
                            report_view,
                            outstanding_reports_keys::reports_log::ACCOUNT_OID
                    ).to_string()
            );

            single_report->set_report_reason(
                    ReportReason(
                            extractFromBsoncxx_k_int32(
                                    report_view,
                                    outstanding_reports_keys::reports_log::REASON)

                    )
            );

            single_report->set_message(
                    extractFromBsoncxx_k_utf8(
                            report_view,
                            outstanding_reports_keys::reports_log::MESSAGE
                    )
            );

            single_report->set_origin_type(
                    ReportOriginType(
                            extractFromBsoncxx_k_int32(
                                    report_view,
                                    outstanding_reports_keys::reports_log::REPORT_ORIGIN
                            )
                    )
            );

            single_report->set_chat_room_id(
                    extractFromBsoncxx_k_utf8(
                            report_view,
                            outstanding_reports_keys::reports_log::CHAT_ROOM_ID
                    )
            );

            single_report->set_message_uuid(
                    extractFromBsoncxx_k_utf8(
                            report_view,
                            outstanding_reports_keys::reports_log::MESSAGE_UUID
                    )
            );

            single_report->set_timestamp_submitted(
                    extractFromBsoncxx_k_date(
                            report_view,
                            outstanding_reports_keys::reports_log::TIMESTAMP_SUBMITTED
                    ).value.count()
            );

        } else { //if element does not exist or is not type document
            logElementError(
                    __LINE__, __FILE__,
                    bsoncxx::document::element{}, doc_view,
                    bsoncxx::type::k_document, outstanding_reports_keys::REPORTS_LOG,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );

            throw ErrorExtractingFromBsoncxx("Error requesting " + outstanding_reports_keys::REPORTS_LOG + ".");
        }
    }

    return reported_user_oid;
}
