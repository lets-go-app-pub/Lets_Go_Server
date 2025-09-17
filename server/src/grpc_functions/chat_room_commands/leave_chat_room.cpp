//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <helper_objects/chat_room_commands_helper_objects.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <utility_chat_functions.h>
#include <global_bsoncxx_docs.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
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

void leaveChatRoomImplementation(const grpc_chat_commands::LeaveChatRoomRequest* request,
                                 grpc_chat_commands::LeaveChatRoomResponse* response);

//primary function for LeaveChatRoomRPC, called from gRPC server implementation
void leaveChatRoom(const grpc_chat_commands::LeaveChatRoomRequest* request,
                   grpc_chat_commands::LeaveChatRoomResponse* response) {

    handleFunctionOperationException(
            [&] {
                leaveChatRoomImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);

}

void leaveChatRoomImplementation(const grpc_chat_commands::LeaveChatRoomRequest* request,
                                 grpc_chat_commands::LeaveChatRoomResponse* response) {

    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationID;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            userAccountOIDStr,
            loginTokenStr,
            installationID
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(basicInfoReturnStatus);
        return;
    }

    const std::string& chatRoomId = request->chat_room_id();

    if (isInvalidChatRoomId(chatRoomId)) {
        response->set_timestamp_stored(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp_stored(-1L); //setting value for 'unset'
    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default
    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    const bsoncxx::oid userAccountOID{userAccountOIDStr};

    std::shared_ptr<std::vector<bsoncxx::document::value>> appendToAndStatementDoc = std::make_shared<std::vector<bsoncxx::document::value>>();

    //checks if user is a member of chat room
    bsoncxx::document::value userRequiredInChatRoomCondition = buildUserRequiredInChatRoomCondition(chatRoomId);
    appendToAndStatementDoc->emplace_back(userRequiredInChatRoomCondition.view());

    //remove the chat room from user the chat rooms list
    bsoncxx::document::value mergeDocument = document{}
            << user_account_keys::CHAT_ROOMS << open_document
                << "$filter" << open_document
                    << "input" << "$" + user_account_keys::CHAT_ROOMS
                    << "cond" << open_document
                        << "$ne" << open_array
                            << chatRoomId << "$$this." + user_account_keys::chat_rooms::CHAT_ROOM_ID
                        << close_array
                    << close_document
                << close_document
            << close_document
            << finalize;

    //project verified pictures key to extract thumbnail
    bsoncxx::document::value projectionDocument = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::PICTURES << 1
            << finalize;

    bsoncxx::document::value loginDocument = getLoginDocument<false>(
            loginTokenStr,
            installationID,
            mergeDocument,
            currentTimestamp,
            appendToAndStatementDoc
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccount;

    mongocxx::client_session::with_transaction_cb transactionCallback = [&](
            mongocxx::client_session* session) {

        if (!runInitialLoginOperation(
                findUserAccount,
                userAccountsCollection,
                userAccountOID,
                loginDocument,
                projectionDocument,
                session)
        ) {
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, userAccountOIDStr);

        if (returnStatus == ReturnStatus::UNKNOWN) {

            //this means user was not inside chat room
            response->set_timestamp_stored(currentTimestamp.count());
            response->set_return_status(ReturnStatus::SUCCESS);
        } else if (returnStatus != ReturnStatus::SUCCESS) {
            response->set_return_status(returnStatus);
        } else {

            const auto set_error_response = [&](
                    const ReturnStatus& returnStatus,
                    const std::chrono::milliseconds& timestamp_stored
            ) {
                response->set_return_status(returnStatus);
                response->set_timestamp_stored(timestamp_stored.count());
            };

            //return status is set inside function
            leaveChatRoom(
                     chatRoomDB,
                     accountsDB,
                     userAccountsCollection,
                     session,
                     findUserAccount->view(),
                     chatRoomId,
                     userAccountOID,
                     currentTimestamp,
                     set_error_response
            );
        }

    }; //end of mongoDB transaction

    mongocxx::client_session session = mongoCppClient.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling leaveChatRoomImplementation() transaction.\n" << e.what() << '\n';
#endif

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(),std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", userAccountOID,
                "chat_room_id", chatRoomId
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

}
