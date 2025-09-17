//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <helper_functions/chat_room_commands_helper_functions.h>
#include <utility_chat_functions.h>
#include <global_bsoncxx_docs.h>
#include <sstream>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "chat_room_header_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void updateChatRoomInfoImplementation(const grpc_chat_commands::UpdateChatRoomInfoRequest* request,
                                      grpc_chat_commands::UpdateChatRoomInfoResponse* response);

//primary function for UpdateChatRoomInfoRPC, called from gRPC server implementation
void updateChatRoomInfo(const grpc_chat_commands::UpdateChatRoomInfoRequest* request,
                        grpc_chat_commands::UpdateChatRoomInfoResponse* response) {
    handleFunctionOperationException(
            [&] {
                updateChatRoomInfoImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void updateChatRoomInfoImplementation(const grpc_chat_commands::UpdateChatRoomInfoRequest* request,
                                      grpc_chat_commands::UpdateChatRoomInfoResponse* response) {

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
    const std::string& newChatRoomInfo = request->new_chat_room_info(); //check chat room info
    const std::string& message_uuid = request->message_uuid();

    if (isInvalidChatRoomId(chatRoomId)
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < newChatRoomInfo.size()
        || isInvalidUUID(message_uuid)
            || !grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_IsValid(
                    request->type_of_info_to_update())
            ) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        response->set_timestamp_message_stored(-2L);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp_message_stored(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const bsoncxx::oid user_account_oid{userAccountOIDStr};
    bsoncxx::types::b_date mongo_db_current_date{current_timestamp};

    //NOTE: Not updating user_account_keys::chat_rooms::LAST_TIME_VIEWED here because the message timestamp will not be known
    // until after sendMessageToChatRoom() is run.
    bsoncxx::document::value mergeDocument = document{} << finalize;

    //project verified pictures key to extract thumbnail
    bsoncxx::document::value projectionDocument = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << finalize;

    bsoncxx::document::value loginDocument = getLoginDocument<false>(
            loginTokenStr,
            installationID,
            mergeDocument,
            current_timestamp
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

    ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, userAccountOIDStr);

    if (returnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(returnStatus);
    }
    else {

        mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

            mongocxx::collection chat_room_collection = chatRoomDB[collection_names::CHAT_ROOM_ID_ + chatRoomId];

            grpc_chat_commands::ClientMessageToServerRequest clientMessageToServerRequest;
            clientMessageToServerRequest.set_message_uuid(message_uuid);
            std::string updateField;

            switch (request->type_of_info_to_update()) {
                case grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_NAME:
                    clientMessageToServerRequest.mutable_message()->mutable_message_specifics()->mutable_chat_room_name_updated_message()->set_new_chat_room_name(newChatRoomInfo);
                    updateField = chat_room_header_keys::CHAT_ROOM_NAME;
                    break;
                case grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UPDATE_CHAT_ROOM_PASSWORD:
                    clientMessageToServerRequest.mutable_message()->mutable_message_specifics()->mutable_chat_room_password_updated_message()->set_new_chat_room_password(newChatRoomInfo);
                    updateField = chat_room_header_keys::CHAT_ROOM_PASSWORD;
                    break;
                case grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_INT_MIN_SENTINEL_DO_NOT_USE_:
                case grpc_chat_commands::UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_UpdateChatRoomInfoRequest_ChatRoomTypeOfInfoToUpdate_INT_MAX_SENTINEL_DO_NOT_USE_:
                    //this was checked above this block should never be possible
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    return;
            }

            //set user activity detected when user updates this
            const SendMessageToChatRoomReturn return_value = sendMessageToChatRoom(
                    &clientMessageToServerRequest,
                    chat_room_collection,
                    user_account_oid,
                    session
            );

            switch (return_value.successful) {
                case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                    std::string errorString = "Message UUID already existed when update chat room info attempted to send it.\n";

                    std::optional <std::string> dummy_exception_string;
                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  dummy_exception_string, errorString,
                                                  "ObjectID_used", user_account_oid,
                                                  "chatRoomId", chatRoomId,
                                                  "message", clientMessageToServerRequest.DebugString());

                    //This means it was already stored, can return success.
                    session->abort_transaction();
                    response->set_timestamp_message_stored(return_value.time_stored_on_server.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                    return;
                }
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                    current_timestamp = return_value.time_stored_on_server;
                    mongo_db_current_date = bsoncxx::types::b_date{current_timestamp};
                    break;
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED:
                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    break;
            }

            mongocxx::options::update opts;

            const std::string ELEM = "e";
            bsoncxx::builder::basic::array arrayBuilder{};
            arrayBuilder.append(
                document{}
                    << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                    << user_account_oid
                << finalize
            );

            opts.array_filters(arrayBuilder.view());

            const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";

            std::optional<std::string> updateChatRoomExceptionString;
            bsoncxx::stdx::optional<mongocxx::result::update> updateChatRoomAccount;
            try {
                //if this account is admin and the field exists, update the desired field
                updateChatRoomAccount = chat_room_collection.update_one(
                    *session,
                    document{}
                        << "_id" << chat_room_header_keys::ID
                        << updateField << open_document
                            << "$exists" << true
                        << close_document
                        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                            << "$elemMatch" << open_document
                                << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                            << close_document
                        << close_document
                    << finalize,
                    document{}
                        << "$set" << open_document
                            << bsoncxx::types::b_string{updateField} << bsoncxx::types::b_string{newChatRoomInfo}
                        << close_document
                        << "$max" << open_document
                            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongo_db_current_date
                            << ELEM_UPDATE +chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongo_db_current_date
                        << close_document
                    << finalize,
                    opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                updateChatRoomExceptionString = std::string(e.what());
            }

            if (!updateChatRoomAccount || updateChatRoomAccount->matched_count() == 0) {

                //when matched_count() == 0 means that either
                //1) the chat room ID did not exist
                //2) the user was not in the chat room
                //3) the user was not admin
                //these could happen is something is a little out of sync, so the
                // response the client will have is to log the user out on all of them
                // therefore just sending back an error message so the client will log out

                bool update_chat_rooms_account = updateChatRoomAccount.operator bool();
                int matched_count = 0;
                int modified_count = 0;

                if (update_chat_rooms_account) {
                    matched_count = updateChatRoomAccount->matched_count();
                    modified_count = updateChatRoomAccount->modified_count();
                }

                std::stringstream errorString;

                errorString
                        << "Update chat room info failed."
                        << "\nupdate_chat_rooms_account: " << update_chat_rooms_account
                        << "\nmatched_count: " << matched_count
                        << "\nmodified_count: " << modified_count
                        << '\n';

                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              updateChatRoomExceptionString, errorString.str(),
                                              "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                              "collection", chat_room_collection.name().to_string(),
                                              "ObjectID_used", user_account_oid,
                                              "chatRoomId", chatRoomId);

                session->abort_transaction();
                response->set_operation_failed(true);
                response->set_return_status(ReturnStatus::SUCCESS);
                return;
            }

            bool update_user_viewed_time_return_value = updateUserLastViewedTime(
                    chatRoomId,
                    user_account_oid,
                    mongo_db_current_date,
                    userAccountsCollection,
                    session
            );

            if(!update_user_viewed_time_return_value) {
                session->abort_transaction();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }

            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp_message_stored(return_value.time_stored_on_server.count());
        };

        mongocxx::client_session session = mongoCppClient.start_session();

        //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
        // more 'generic' errors. Can look here for more info
        // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
        try {
            session.with_transaction(transactionCallback);
        }
        catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
            std::cout << "Exception calling updateChatRoomInfoImplementation() transaction.\n" << e.what() << '\n';
#endif

            std::optional<std::string> dummyExceptionString;
            storeMongoDBErrorAndException(__LINE__, __FILE__, dummyExceptionString,
                                          std::string(e.what()),
                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                          "user_OID", user_account_oid,
                                          "chat_room_id", chatRoomId);

        }

    }

}
