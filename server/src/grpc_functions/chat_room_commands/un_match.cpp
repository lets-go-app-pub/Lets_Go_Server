//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <global_bsoncxx_docs.h>
#include <utility_chat_functions.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>
#include <store_mongoDB_error_and_exception.h>

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void
unMatchImplementation(const grpc_chat_commands::UnMatchRequest* request, grpc_chat_commands::UnMatchResponse* response);


//primary function for UnMatchRPC, called from gRPC server implementation
void unMatch(const grpc_chat_commands::UnMatchRequest* request, grpc_chat_commands::UnMatchResponse* response) {
    handleFunctionOperationException(
            [&] {
                unMatchImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void unMatchImplementation(const grpc_chat_commands::UnMatchRequest* request,
                           grpc_chat_commands::UnMatchResponse* response) {

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

    const std::string& chat_room_id = request->chat_room_id();

    if (isInvalidChatRoomId(chat_room_id)) {
        response->set_timestamp(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    const std::string& matched_account_oid_str = request->matched_account_oid(); //check token

    if (isInvalidOIDString(matched_account_oid_str)) { //check if logged in token is 24 size
        response->set_timestamp(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if(matched_account_oid_str == user_account_oid_str) {
        response->set_timestamp(getCurrentTimestamp().count());
        response->set_return_status(ReturnStatus::SUCCESS);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chatRoom_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const bsoncxx::oid user_account_oid{user_account_oid_str};
    const bsoncxx::oid matched_account_oid{matched_account_oid_str};

    //remove element from blocked list
    const bsoncxx::document::value merge_document = document{} << finalize;

    //project verified pictures key to extract thumbnail
    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::PICTURES << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document,
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

        const auto set_return_status = [&](const ReturnStatus& returnStatus, const std::chrono::milliseconds& timestamp_stored [[maybe_unused]]) {
            //currentTimestamp can be updated inside unMatchHelper(), so set it here.
            response->set_return_status(returnStatus);
            response->set_timestamp(current_timestamp.count());
        };

        mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* callback_session) {

            //NOTE: this will set the return status and timestamp has already been set
            unMatchHelper(
                    accounts_db,
                    chatRoom_db,
                    callback_session,
                    user_accounts_collection,
                    find_user_account_doc->view(),
                    user_account_oid,
                    matched_account_oid,
                    chat_room_id,
                    current_timestamp,
                    set_return_status
            );
        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        try {
            session.with_transaction(transaction_callback);
        } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
            std::cout << "Exception calling unMatch() transaction.\n" << e.what() << '\n';
#endif
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(),std::string(e.what()),
                    "user_OID", user_account_oid,
                    "matched_OID", matched_account_oid,
                    "chatRoomId", chat_room_id
            );
            set_return_status(ReturnStatus::LG_ERROR, std::chrono::milliseconds{-1L});
            return;
        }

    }

}
