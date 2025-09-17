
#include <utility_chat_functions.h>
#include "report_helper_functions.h"
#include "handle_function_operation_exception.h"
#include "user_match_options.h"
#include "helper_functions/user_match_options_helper_functions.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "report_values.h"
#include "helper_functions/build_update_doc_for_response_type.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "event_admin_values.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void receiveMatchYesImplementation(const UserMatchOptionsRequest* request, UserMatchOptionsResponse* response);

void receiveMatchYes(const UserMatchOptionsRequest* request,
                     UserMatchOptionsResponse* response) {

    handleFunctionOperationException(
            [&] {
                receiveMatchYesImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void receiveMatchYesImplementation(const UserMatchOptionsRequest* request, UserMatchOptionsResponse* response) {

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

    const std::string& match_account_oid_str = request->match_account_id();

    if (isInvalidOIDString(match_account_oid_str)) { //check if logged in token is 24 size
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default
    response->set_timestamp(-1L); // -1 means not set

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid{user_account_oid_str};
    //the OID of the matched account
    const bsoncxx::oid match_account_oid{match_account_oid_str};

    const bsoncxx::document::value merge_document = document{}
            << user_account_keys::NUMBER_TIMES_SWIPED_YES << open_document
                << "$add" << open_array
                    << "$" + user_account_keys::NUMBER_TIMES_SWIPED_YES << 1
                << close_array
            << close_document
            << finalize;

    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;

    if (!runInitialLoginOperation(
            find_user_account,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_user_account,
            user_account_oid_str
    );

    if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
    } else {

        std::optional<std::string> update_match_exception_string;
        bsoncxx::stdx::optional<bsoncxx::v_noabi::document::value> find_and_update_match_account;
        try {
            mongocxx::options::find_one_and_update opts;

            opts.projection(
                document{}
                    << user_account_keys::ACCOUNT_TYPE << 1
                    << user_account_keys::EVENT_VALUES << 1
                << finalize
            );

            //increment match number of times was swiped 'yes' on
            find_and_update_match_account = user_accounts_collection.find_one_and_update(
                document{}
                    << "_id" << match_account_oid
                << finalize,
                document{}
                    << "$inc" << open_document
                        << user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES << 1
                    << close_document
                << finalize,
                opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            update_match_exception_string = e.what();
        }

        UserAccountType account_type = UserAccountType::UNKNOWN_ACCOUNT_TYPE;
        const bsoncxx::document::view find_and_update_match_account_doc = find_and_update_match_account->view();

        if(find_and_update_match_account) {
            const auto account_type_element = find_and_update_match_account_doc[user_account_keys::ACCOUNT_TYPE];
            if (account_type_element
                && account_type_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                account_type = UserAccountType(account_type_element.get_int32().value);
            } else { //if element exists but is not type oid
                logElementError(
                    __LINE__, __FILE__,
                    account_type_element, find_and_update_match_account_doc,
                    bsoncxx::type::k_int32, user_account_keys::ACCOUNT_TYPE,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
        } else {
            const std::string error_string = "failed to increment '" + user_account_keys::NUMBER_TIMES_OTHERS_SWIPED_YES + "'.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_match_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "oid_used", match_account_oid
            );

            //NOTE: handleUserExtractedAccount() still needs to run.
        }

        std::optional<bsoncxx::oid> match_statistics_document;

        //NOTE: The above user account document increase should not be needed inside the transaction, the results are
        // never used, and it is just for statistics purposes.
        mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

            std::optional<bsoncxx::document::value> user_account_doc;
            bsoncxx::document::view user_account_doc_view;
            std::optional<bsoncxx::document::view> user_extracted_array_document;

            bool extracted_result = handleUserExtractedAccount(
                    current_timestamp,
                    user_accounts_collection,
                    callback_session,
                    user_account_oid,
                    match_account_oid,
                    ResponseType::USER_MATCH_OPTION_YES,
                    user_account_doc,
                    user_account_doc_view,
                    user_extracted_array_document,
                    match_statistics_document
            );

            if (!extracted_result) {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }
            else if (account_type == UserAccountType::ADMIN_GENERATED_EVENT_TYPE
                    || account_type == UserAccountType::USER_GENERATED_EVENT_TYPE
            ) { //if this is an 'event' type

                const auto event_values_element = find_and_update_match_account_doc[user_account_keys::EVENT_VALUES];
                if(!event_values_element
                    || event_values_element.type() != bsoncxx::type::k_document
                ) { //if element exists but is not type oid
                    logElementError(
                            __LINE__, __FILE__,
                            event_values_element, find_and_update_match_account_doc,
                            bsoncxx::type::k_document, user_account_keys::EVENT_VALUES,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );

                    //Ok to return success here, simply won't complete the swipe.
                    response->set_timestamp(current_timestamp.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                    return;
                }

                const bsoncxx::document::view event_values_doc = event_values_element.get_document().value;

                const auto event_chat_room_id_element = event_values_doc[user_account_keys::event_values::CHAT_ROOM_ID];
                if(!event_chat_room_id_element
                   || event_chat_room_id_element.type() != bsoncxx::type::k_utf8
                        ) { //if element exists but is not type oid
                    logElementError(
                            __LINE__, __FILE__,
                            event_values_element, find_and_update_match_account_doc,
                            bsoncxx::type::k_document, user_account_keys::EVENT_VALUES,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );

                    //Ok to return success here, simply won't complete the swipe.
                    response->set_timestamp(current_timestamp.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                    return;
                }

                const std::string chat_room_id = event_chat_room_id_element.get_string().value.to_string();

                if(isInvalidChatRoomId(chat_room_id)) {
                    const std::string error_string = "An invalid chat room id was stored inside event_values::CHAT_ROOM_ID.";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "chat_room_id", chat_room_id,
                            "user_account_oid", user_account_oid_str,
                            "event_account_oid", match_account_oid_str
                    );

                    //Ok to return success here, simply won't complete the swipe.
                    response->set_timestamp(current_timestamp.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                    return;
                }

                mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
                mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

#ifdef LG_TESTING
                int testing_result;
#endif // TESTING
                if(handleUserSwipedYesOnEvent(
                        mongo_cpp_client,
                        accounts_db,
                        user_accounts_collection,
                        chat_room_collection,
                        callback_session,
                        user_account_doc_view,
                        chat_room_id,
                        match_account_oid,
                        user_account_oid,
                        current_timestamp
#ifdef LG_TESTING
                        ,testing_result
#endif // TESTING
                ))
                { //if no error occurred
                    response->set_timestamp(current_timestamp.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                } else { //if an error occurred
                    response->set_return_status(ReturnStatus::LG_ERROR);
                }
            }
            else if (user_extracted_array_document) { //if a document was retrieved from 'extracted' array

                //NOTE: The value must be extracted from 'HAS_BEEN_EXTRACTED_ACCOUNTS_LIST'. Otherwise, not only are
                // the original stats for the match unknown, it will allow for matches to be artificially created by
                // sending in an account_oid for a match that never happened.

#ifdef LG_TESTING
                int testing_result;
#endif // TESTING

                if (moveExtractedElementToOtherAccount(
                        mongo_cpp_client,
                        accounts_db,
                        user_accounts_collection,
                        callback_session,
                        user_extracted_array_document,
                        user_account_doc_view,
                        match_account_oid,
                        user_account_oid,
                        current_timestamp
#ifdef LG_TESTING
                        , testing_result
#endif // TESTING
                )) { //if no error occurred
                    response->set_timestamp(current_timestamp.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                } else { //if an error occurred
                    response->set_return_status(ReturnStatus::LG_ERROR);
                }
            }
            else { //if no document was retrieved from the 'extracted' list

                //this means it was most likely automatically cleared out because it expired
                response->set_timestamp(current_timestamp.count());
                response->set_return_status(ReturnStatus::SUCCESS);
            }

        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
        // more 'generic' errors. Can look here for more info
        // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
        try {
            session.with_transaction(transaction_callback);
        } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
            std::cout << "Exception calling receiveMatchYesImplementation() transaction.\n" << e.what() << '\n';
#endif

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "user_OID", user_account_oid_str,
                    "match_account_oid", match_account_oid_str
            );

            response->set_return_status(ReturnStatus::LG_ERROR);
        }

        updateMatchStatisticsDocument(
                mongo_cpp_client,
                current_timestamp,
                match_statistics_document,
                ResponseType::USER_MATCH_OPTION_YES //NOTE: request->response_type() was not validated above.
        );
    }

}

void receiveMatchNoImplementation(const UserMatchOptionsRequest* request, UserMatchOptionsResponse* response);

void receiveMatchNo(const UserMatchOptionsRequest* request,
                    UserMatchOptionsResponse* response) {
    handleFunctionOperationException(
            [&] {
                receiveMatchNoImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void receiveMatchNoImplementation(const UserMatchOptionsRequest* request, UserMatchOptionsResponse* response) {

    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
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

    const std::string& match_account_oid_str = request->match_account_id(); //check for valid match oid

    if (isInvalidOIDString(match_account_oid_str)) { //check if logged in token is 24 size
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    std::string other_info = "~"; //otherInfo is only used with REPORT at the moment
    ReportReason report_reason = request->report_reason();
    if(!ResponseType_IsValid(request->response_type())) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    } else if (request->response_type() == ResponseType::USER_MATCH_OPTION_CONNECTION_ERR
        || request->response_type() == ResponseType::USER_MATCH_OPTION_YES) { //if response type was not set or is YES
        response->set_return_status(ReturnStatus::SUCCESS);
        return;
    } else if (request->response_type() == ResponseType::USER_MATCH_OPTION_REPORT) {
        //If the response is type REPORT and either the message is too long or the report reason is out of bounds.
        if(report_reason == ReportReason::REPORT_REASON_OTHER) {
            other_info = request->other_info();
        }
        if (!ReportReason_IsValid(report_reason)
            || (report_reason == ReportReason::REPORT_REASON_OTHER && other_info.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES)
            || report_reason == ReportReason::REPORT_REASON_UNKNOWN_DEFAULT
        ) {
            response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
            return;
        }
    }

    ReportOriginType report_origin;

    if(!ReportOriginType_IsValid(request->report_origin())) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    report_origin = request->report_origin();

    //chat room id must be empty OR a valid chat room Id
    if(!request->chat_room_id().empty() && isInvalidChatRoomId(request->chat_room_id())) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    //message uuid must be empty OR a valid UUID
    if(!request->message_uuid().empty() && isInvalidUUID(request->message_uuid())) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default
    response->set_timestamp(-1L); // -1 means not set

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid{user_account_oid_str};
    //the OID of the matched account
    const bsoncxx::oid match_account_oid{match_account_oid_str};

    bsoncxx::builder::stream::document merge_document_builder;
    mongocxx::pipeline update_current_user_pipeline;

    buildAddFieldsDocForBlockReportPipeline<USER_MATCH_OPTIONS_CHAT_ROOM_CURRENT_USER>(
            current_timestamp,
            merge_document_builder,
            request->response_type(),
            match_account_oid
    );

    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document_builder.view(),
            current_timestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;

    if (!runInitialLoginOperation(
            find_user_account,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    ReturnStatus return_status = checkForValidLoginToken(find_user_account, user_account_oid_str);

    if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
        return;
    }

    bsoncxx::document::view user_account_doc = *find_user_account;
    int number_times_spam_reports_sent;

    auto number_times_spam_reports_sent_element = user_account_doc[user_account_keys::NUMBER_TIMES_SPAM_REPORTS_SENT];
    if (number_times_spam_reports_sent_element
        && number_times_spam_reports_sent_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        number_times_spam_reports_sent = number_times_spam_reports_sent_element.get_int32().value;
    } else { //if element exists but is not type oid
        logElementError(__LINE__, __FILE__,
                        number_times_spam_reports_sent_element,
                        user_account_doc, bsoncxx::type::k_int32,
                        user_account_keys::accounts_list::SAVED_STATISTICS_OID,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    //failing saveNewReportToCollection() is recoverable, it simply won't store the report
    if (number_times_spam_reports_sent < report_values::MAX_NUMBER_OF_TIMES_CAN_SPAM_REPORTS
        &&
        (request->response_type() == ResponseType::USER_MATCH_OPTION_REPORT
         || request->response_type() == ResponseType::USER_MATCH_OPTION_BLOCK)
        && match_account_oid != event_admin_values::OID
    ) {
        //OK if this fails, can still continue (error stored inside function).
        saveNewReportToCollection(
                mongo_cpp_client,
                current_timestamp,
                user_account_oid,
                match_account_oid,
                request->report_reason(),
                report_origin,
                other_info,
                request->chat_room_id(),
                request->message_uuid()
        );
    }

    bsoncxx::builder::stream::document update_match_account_doc_builder;
    mongocxx::pipeline update_matched_user_pipeline;

    //This must run in order to store that the user inside OTHER_USERS_BLOCKED list.
    buildAddFieldsDocForBlockReportPipeline<USER_MATCH_OPTIONS_CHAT_ROOM_TARGET_USER>(
            current_timestamp,
            update_match_account_doc_builder,
            request->response_type(),
            user_account_oid
    );

    const bsoncxx::document::view update_match_account_doc_view = update_match_account_doc_builder.view();

    if(!update_match_account_doc_view.empty()) {

        std::optional<std::string> update_match_user_doc_exception_string;
        bsoncxx::stdx::optional<mongocxx::result::update> update_match_user_doc;
        try {
            update_matched_user_pipeline.add_fields(update_match_account_doc_view);

            //increment match number of times was swiped on
            update_match_user_doc = user_accounts_collection.update_one(
                document{}
                    << "_id" << match_account_oid
                << finalize,
                update_matched_user_pipeline
            );
        }
        catch (const mongocxx::logic_error& e) {
            update_match_user_doc_exception_string = e.what();
        }

        if (!update_match_user_doc || update_match_user_doc->modified_count() < 1) { //if update failed
            const std::string error_string = "Failed to increment values generated by buildAddFieldsDocForBlockReportPipeline().";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    update_match_user_doc_exception_string, error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "oid_used", match_account_oid
            );

            if(request->response_type() == ResponseType::USER_MATCH_OPTION_REPORT
                || request->response_type() == ResponseType::USER_MATCH_OPTION_BLOCK) {
                //Not recoverable, user must be stored inside OTHER_USERS_BLOCKED.
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
            //else {} //Recoverable, just for statistics purposes.
        }
    }

    std::optional<bsoncxx::oid> match_statistics_doc_oid;

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

        std::optional<bsoncxx::document::value> dummy_user_matching_account_value;
        bsoncxx::document::view dummy_user_matching_account_view;
        std::optional<bsoncxx::document::view> dummy_user_extracted_array_document;

        if (handleUserExtractedAccount(
                current_timestamp,
                user_accounts_collection,
                callback_session,
                user_account_oid,
                match_account_oid,
                request->response_type(),
                dummy_user_matching_account_value,
                dummy_user_matching_account_view,
                dummy_user_extracted_array_document,
                match_statistics_doc_oid)
        ) { //no errors were returned
            response->set_timestamp(current_timestamp.count());
            response->set_return_status(ReturnStatus::SUCCESS);
        } else { //errors were returned
            response->set_return_status(ReturnStatus::LG_ERROR);
        }
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling receiveMatchNoImplementation() transaction.\n" << e.what() << '\n';
#endif

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional <std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid_str,
                "match_account_OID", match_account_oid_str
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
    }

    updateMatchStatisticsDocument(
            mongo_cpp_client,
            current_timestamp,
            match_statistics_doc_oid,
            request->response_type()
    );

}





