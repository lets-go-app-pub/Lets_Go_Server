//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

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

void unblockOtherUserImplementation(const grpc_chat_commands::UnblockOtherUserRequest* request,
                                    grpc_chat_commands::UnblockOtherUserResponse* response);

//primary function for UnblockOtherUserRPC, called from gRPC server implementation
void unblockOtherUser(const grpc_chat_commands::UnblockOtherUserRequest* request,
                      grpc_chat_commands::UnblockOtherUserResponse* response) {

    handleFunctionOperationException(
            [&] {
                unblockOtherUserImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void unblockOtherUserImplementation(const grpc_chat_commands::UnblockOtherUserRequest* request,
                                    grpc_chat_commands::UnblockOtherUserResponse* response) {

    std::string user_account_oid_str;
    std::string loginTokenStr;
    std::string installationID;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            loginTokenStr,
            installationID
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(basicInfoReturnStatus);
        return;
    }

    const std::string& userToUnblockAccountOIDString = request->user_to_unblock_account_id(); //check for valid match oid

    if (isInvalidOIDString(userToUnblockAccountOIDString)) { //check if logged in token is 24 size
        response->set_timestamp(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if(userToUnblockAccountOIDString == user_account_oid_str) {
        response->set_timestamp(getCurrentTimestamp().count());
        response->set_return_status(ReturnStatus::SUCCESS);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    //CHAT_ROOM_COMMANDS_UNBLOCK_COLLECTION_NAME

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default
    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    const bsoncxx::oid user_account_oid{user_account_oid_str};

    //remove element from blocked list
    bsoncxx::document::value mergeDocument = document{}
            << user_account_keys::OTHER_USERS_BLOCKED << open_document

                << "$filter" << open_document
                    << "input" << "$" + user_account_keys::OTHER_USERS_BLOCKED
                    << "cond" << open_document
                        << "$ne" << open_array
                            << userToUnblockAccountOIDString << "$$this." + user_account_keys::other_users_blocked::OID_STRING
                        << close_array
                    << close_document
                << close_document

            << close_document
        << finalize;

    //project verified pictures key to extract thumbnail
    bsoncxx::document::value projectionDocument = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << finalize;

    bsoncxx::document::value loginDocument = getLoginDocument<false>(
            loginTokenStr,
            installationID,
            mergeDocument,
            currentTimestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccount;

    if (!runInitialLoginOperation(
            findUserAccount,
            userAccountsCollection,
            user_account_oid,
            loginDocument,
            projectionDocument)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, user_account_oid_str);

    if (returnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(returnStatus);
    } else {
        response->set_timestamp(currentTimestamp.count());
        response->set_return_status(ReturnStatus::SUCCESS);
    }

}

