//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <helper_functions/user_match_options_helper_functions.h>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>
#include <utility_chat_functions.h>
#include "../../utility/reports_helper_functions/report_helper_functions.h"

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "report_values.h"
#include "helper_functions/build_update_doc_for_response_type.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void blockAndReportChatRoomImplementation(
        const grpc_chat_commands::BlockAndReportChatRoomRequest* request,
        UserMatchOptionsResponse* response
);

//primary function for BlockAndReportChatRoomRPC, called from gRPC server implementation
void blockAndReportChatRoom(
        const grpc_chat_commands::BlockAndReportChatRoomRequest* request,
        UserMatchOptionsResponse* response
) {
    handleFunctionOperationException(
            [&] {
                blockAndReportChatRoomImplementation(request, response);
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

void blockAndReportChatRoomImplementation(
        const grpc_chat_commands::BlockAndReportChatRoomRequest* request,
        UserMatchOptionsResponse* response
) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->match_options_request().login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    const std::string& match_account_oid_str = request->match_options_request().match_account_id(); //check for valid match oid

    if (isInvalidOIDString(match_account_oid_str)) { //check if logged in token is 24 size
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if(match_account_oid_str == user_account_oid_str) {
        response->set_timestamp(getCurrentTimestamp().count());
        response->set_return_status(ReturnStatus::SUCCESS);
        return;
    }

    const ReportReason report_reason = request->match_options_request().report_reason();

    //if either the message is too long or the report reason is out of bounds
    const std::string& other_info = request->match_options_request().other_info(); //otherInfo is only used with OTHER at the moment
    if (!ReportReason_IsValid(report_reason)
        || (report_reason == ReportReason::REPORT_REASON_OTHER
            && other_info.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES)
        || report_reason == ReportReason::REPORT_REASON_UNKNOWN_DEFAULT) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    //chat room id must be empty OR a valid chat room Id
    if(!request->match_options_request().chat_room_id().empty()
        && isInvalidChatRoomId(request->match_options_request().chat_room_id())
    ) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    //message uuid must be empty OR a valid UUID
    if(!request->match_options_request().message_uuid().empty()
        && isInvalidUUID(request->match_options_request().message_uuid())) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    const std::string& request_chat_room_id = request->match_options_request().chat_room_id();
    const std::string& request_message_uuid = request->match_options_request().message_uuid();

    const ReportOriginType report_origin = request->match_options_request().report_origin();

    if(!ReportOriginType_IsValid(request->match_options_request().report_origin())) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    switch (request->match_options_request().report_origin()) {
        case REPORT_ORIGIN_SWIPING:
        case REPORT_ORIGIN_CHAT_ROOM_INFO:
        case REPORT_ORIGIN_CHAT_ROOM_MEMBER:
        case REPORT_ORIGIN_CHAT_ROOM_MESSAGE:
        case REPORT_ORIGIN_CHAT_ROOM_MATCH_MADE:
            break;
        default:
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default
    response->set_timestamp(-1L); // -1 means not set

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid(user_account_oid_str); //the OID of the verified account
    const bsoncxx::oid match_account_oid(match_account_oid_str); //the OID of the matched account

    bsoncxx::builder::stream::document merge_document_builder;

    buildAddFieldsDocForBlockReportPipeline<BLOCK_AND_REPORT_CHAT_ROOM_CURRENT_USER>(
            current_timestamp,
            merge_document_builder,
            ResponseType::USER_MATCH_OPTION_REPORT,
            match_account_oid
    );

    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << 1
            << user_account_keys::PICTURES << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document_builder.view(),
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account_doc;

    if (!runInitialLoginOperation(
            find_user_account_doc,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_user_account_doc,
            user_account_oid_str
    );

    if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
    } else {

        const bsoncxx::document::view user_account_doc = *find_user_account_doc;
        int number_times_spam_reports_sent;

        auto number_times_spam_reports_sent_element = user_account_doc[user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT];
        if (number_times_spam_reports_sent_element &&
            number_times_spam_reports_sent_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            number_times_spam_reports_sent = number_times_spam_reports_sent_element.get_int32().value;
        } else { //if element exists but is not type oid
            logElementError(
                    __LINE__, __FILE__,
                    number_times_spam_reports_sent_element, user_account_doc,
                    bsoncxx::type::k_int32,user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
            );
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        //do not store the report if user has been marked as a 'spammer'
        //do not allow the event admin account to be reported
        if (number_times_spam_reports_sent < report_values::MAX_NUMBER_OF_TIMES_CAN_SPAM_REPORTS
            && match_account_oid != event_admin_values::OID) {
            //failing saveNewReportToCollection() is recoverable, it simply won't store the report
            //error stored internally
            saveNewReportToCollection(
                    mongo_cpp_client,
                    current_timestamp,
                    user_account_oid,
                    match_account_oid,
                    report_reason,
                    report_origin,
                    other_info,
                    request_chat_room_id,
                    request_message_uuid
            );
        }

        mongocxx::pipeline update_matched_user_pipeline;
        bsoncxx::builder::stream::document update_match_document;

        //This must run in order to store that the user inside OTHER_USERS_BLOCKED list.
        buildAddFieldsDocForBlockReportPipeline<BLOCK_AND_REPORT_CHAT_ROOM_TARGET_USER>(
                current_timestamp,
                update_match_document,
                ResponseType::USER_MATCH_OPTION_REPORT,
                match_account_oid
        );

        bsoncxx::document::view update_match_account_document = update_match_document.view();

        if(!update_match_account_document.empty()) {

            update_matched_user_pipeline.add_fields(
                    update_match_account_document
            );

            std::optional<std::string> update_match_user_doc_exception_string;
            bsoncxx::stdx::optional<mongocxx::result::update> updateMatchUserDoc;
            try {
                //increment match number of times was swiped on
                updateMatchUserDoc = user_accounts_collection.update_one(
                        document{}
                       << "_id" << match_account_oid
                   << finalize,
                        update_matched_user_pipeline
                );
            }
            catch (const mongocxx::logic_error& e) {
                update_match_user_doc_exception_string = e.what();
            }

            if (!updateMatchUserDoc || updateMatchUserDoc->modified_count() < 1) { //if update failed
                const std::string error_string = "Failed to increment values generated by buildAddFieldsDocForBlockReportPipeline().";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        update_match_user_doc_exception_string, error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "oid_used", match_account_oid
                );
                //Not recoverable, user must be stored inside OTHER_USERS_BLOCKED.
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
        }

        response->set_timestamp(current_timestamp.count());

        std::optional<bsoncxx::document::view> user_extracted_array_document;

        if (request->un_match()) { //if this function needs to 'un_match' as well

            const std::string& chat_room_id = request->chat_room_id();

            if(isInvalidChatRoomId(chat_room_id)) {
                response->set_timestamp(-1L);
                response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
                return;
            }

            const auto set_return_status = [&](const ReturnStatus& returnStatus, const std::chrono::milliseconds& timestamp_stored [[maybe_unused]]) {
                response->set_return_status(returnStatus);
            };

            mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

                //NOTE: this will set the return status and timestamp has already been set
                // (these are the only 2 fields inside the response at the moment)
                unMatchHelper(
                        accounts_db,
                        chat_room_db,
                        callback_session,
                        user_accounts_collection,
                        find_user_account_doc->view(),
                        user_account_oid,
                        match_account_oid,
                        chat_room_id,
                        current_timestamp,
                        set_return_status
                );
            };

            mongocxx::client_session session = mongo_cpp_client.start_session();

            //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
            // more 'generic' errors. Can look here for more info
            // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
            try {
                session.with_transaction(transaction_callback);
            } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
                std::cout << "Exception calling blockAndReportChatRoom() transaction.\n" << e.what() << '\n';
#endif
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), std::string(e.what()),
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "user_OID", user_account_oid,
                        "matched_OID", match_account_oid,
                        "chat_room_id", chat_room_id
                );

                set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
                return;
            }

        } else { //if this function needs does not need to 'un_match'
            response->set_return_status(ReturnStatus::SUCCESS);
        }

    }

}

