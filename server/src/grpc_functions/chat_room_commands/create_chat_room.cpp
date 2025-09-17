//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <create_chat_room_helper.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <store_mongoDB_error_and_exception.h>
#include <thread_pool_global_variable.h>

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "create_complete_chat_room_with_user.h"
#include "create_initial_chat_room_messages.h"
#include "server_parameter_restrictions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void createChatRoomImplementation(const grpc_chat_commands::CreateChatRoomRequest* request,
                                  grpc_chat_commands::CreateChatRoomResponse* response);

//primary function for CreateChatRoomRPC, called from gRPC server implementation
void createChatRoom(const grpc_chat_commands::CreateChatRoomRequest* request,
                    grpc_chat_commands::CreateChatRoomResponse* response) {

    handleFunctionOperationException(
            [&] {
                createChatRoomImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);

}

void createChatRoomImplementation(const grpc_chat_commands::CreateChatRoomRequest* request,
                                  grpc_chat_commands::CreateChatRoomResponse* response) {

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

    if (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < request->chat_room_name().size()) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default
    response->set_last_activity_time_timestamp(-1L); // -1 means not set
    response->mutable_chat_room_info()->set_last_activity_time_timestamp(-1L); // -1 means not set

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid(user_account_oid_str); //the OID of the verified account

    const GenerateNewChatRoomTimes chat_room_times(current_timestamp);

    std::string chat_room_id;

    //NOTE: This is done before login and can 'waste' chat room Ids if the user is not
    // logged in. However, it allows fewer documents inside the transaction and any waste should
    // be minimal. Also, it will not need to be called again if the transaction fails.
    if (!generateChatRoomId(
            chat_room_db,
            chat_room_id)
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const bsoncxx::document::value merge_document = document{}
            << user_account_keys::CHAT_ROOMS << open_document
                << "$concatArrays" << open_array
                    << "$" + user_account_keys::CHAT_ROOMS
                    << open_array
                        << open_document
                            << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                            << user_account_keys::chat_rooms::LAST_TIME_VIEWED << bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
                        << close_document
                    << close_array
                << close_array
            << close_document
        << finalize;

    const bsoncxx::document::value projection_document = document{}
                << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                << user_account_keys::FIRST_NAME << 1
                << user_account_keys::PICTURES << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            login_token_str,
            installation_id,
            merge_document,
            current_timestamp
    );

    std::shared_ptr<bsoncxx::builder::stream::document> different_user_joined_chat_room_message_doc = nullptr;
    std::string different_user_joined_message_uuid;

    bsoncxx::stdx::optional<bsoncxx::document::value> find_user_account;
    //run inside transaction to coordinate account and chat room are updated at the same time
    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* session) {

        if (!runInitialLoginOperation(
                find_user_account,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document,
                session)
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
        }
        else {

            const bsoncxx::document::view user_account_doc = find_user_account->view();

            std::string chat_room_name = request->chat_room_name();
            std::string chat_room_password;
            std::string cap_message_uuid;

            if(!createCompleteChatRoomWithUser(
                    accounts_db,
                    chat_room_db,
                    session,
                    chat_room_times,
                    user_account_oid,
                    user_account_doc,
                    chat_room_id,
                    std::optional<FullEventValues>(),
                    std::optional<CreateChatRoomLocationStruct>(),
                    chat_room_name,
                    chat_room_password,
                    cap_message_uuid)
                    ) {
                response->Clear();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }

            if(!createInitialChatRoomMessages(
                    cap_message_uuid,
                    user_account_oid,
                    user_account_oid_str,
                    chat_room_id,
                    chat_room_times,
                    response->mutable_chat_room_info()->mutable_chat_room_cap_message(),
                    response->mutable_chat_room_info()->mutable_current_user_joined_chat_message(),
                    different_user_joined_chat_room_message_doc,
                    different_user_joined_message_uuid)
                    ) {
                response->Clear();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }

            response->mutable_chat_room_cap_message()->CopyFrom(response->mutable_chat_room_info()->chat_room_cap_message());
            response->mutable_current_user_joined_chat_message()->CopyFrom(response->mutable_chat_room_info()->current_user_joined_chat_message());

            response->mutable_chat_room_info()->set_chat_room_id(chat_room_id);
            response->mutable_chat_room_info()->set_chat_room_password(chat_room_password);
            response->mutable_chat_room_info()->set_chat_room_name(chat_room_name);
            response->mutable_chat_room_info()->set_last_activity_time_timestamp(chat_room_times.chat_room_last_active_time.count());

            response->set_chat_room_password(chat_room_password);
            response->set_chat_room_id(chat_room_id);
            response->set_chat_room_name(chat_room_name);
            response->set_last_activity_time_timestamp(chat_room_times.chat_room_last_active_time.count());

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
        std::cout << "Exception calling createChatRoomImplementation() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_OID", user_account_oid,
                "chat_room_id", chat_room_id
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    if (response->return_status() == ReturnStatus::SUCCESS) {

        if(different_user_joined_chat_room_message_doc == nullptr) {
            const std::string error_string = "When success is returned different_user_joined_chat_room_message_doc should be set.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "user_OID", user_account_oid,
                    "chat_room_id", chat_room_id,
                    "response", response->DebugString()
            );

            response->Clear();
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

#ifndef _RELEASE
        std::cout << "Running sendDifferentUserJoinedChatRoom().\n";
#endif

        //NOTE: This is done to 'connect' the chat stream to the device, essentially when the chat change stream
        // receives a kDifferentUserJoinedChatRoom message it will add it to the list of chat rooms.
        thread_pool.submit(
                [
                        _chat_room_id = response->mutable_chat_room_info()->chat_room_id(),
                        _user_account_oid = user_account_oid,
                        different_user_joined_chat_room_message_doc = std::move(different_user_joined_chat_room_message_doc),
                        message_uuid = different_user_joined_message_uuid
                    ]{
                sendDifferentUserJoinedChatRoom(
                        _chat_room_id,
                        _user_account_oid,
                        *different_user_joined_chat_room_message_doc,
                        message_uuid
                );
            }
        );
    }

#ifndef _RELEASE
    std::cout << "Finishing CreateChatRoom With " << convertReturnStatusToString(response->return_status()) << '\n';
#endif

}
