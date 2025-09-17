//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <extract_thumbnail_from_verified_doc.h>
#include <how_to_handle_member_pictures.h>
#include <utility_chat_functions.h>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "chat_room_commands.h"

#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "store_and_send_messages.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "extract_data_from_bsoncxx.h"
#include "join_chat_room_with_user.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;
using grpc_chat_commands::ChatRoomStatus;

void joinChatRoomImplementation(const grpc_chat_commands::JoinChatRoomRequest* request,
                                grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* responseStream);

//primary function for JoinChatRoomRPC, called from gRPC server implementation
void joinChatRoom(const grpc_chat_commands::JoinChatRoomRequest* request,
                  grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* responseStream) {

    handleFunctionOperationException(
            [&] {
                joinChatRoomImplementation(request, responseStream);
            },
            [&] {
                grpc_chat_commands::JoinChatRoomResponse errorResponse;
                auto message = errorResponse.add_messages_list();
                message->set_primer(true);
                message->set_return_status(ReturnStatus::DATABASE_DOWN);
                responseStream->Write(errorResponse);
            },
            [&] {
                grpc_chat_commands::JoinChatRoomResponse errorResponse;
                auto message = errorResponse.add_messages_list();
                message->set_primer(true);
                message->set_return_status(ReturnStatus::LG_ERROR);
                responseStream->Write(errorResponse);
            }, __LINE__, __FILE__, request);

}

void joinChatRoomImplementation(const grpc_chat_commands::JoinChatRoomRequest* request,
                                grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* responseStream) {

    //NOTE: There are some things that could be done together inside this function. However, they deal with the header
    // which stores thumbnails and therefore could be fairly large. So not going to implement the update with the
    // aggregation to get rid of 2 database calls that will not usually be called anyway.

    std::string user_account_oid_str;
    std::string loginTokenStr;
    std::string installationID;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            loginTokenStr,
            installationID
    );

    bool send_chat_room_back_to_client = true;

    auto writeResponseToClient = [&responseStream, &send_chat_room_back_to_client](
            ReturnStatus returnStatus,
            ChatRoomStatus chatRoomStatus
    ) {
        if(returnStatus != ReturnStatus::SUCCESS || chatRoomStatus != ChatRoomStatus::SUCCESSFULLY_JOINED)
            send_chat_room_back_to_client = false;

        //NOTE: The current_timestamp should NOT be used to reflect the actual stored time of kDifferentUserJoined. This is
        // because it is not requested here, it is requested inside
        grpc_chat_commands::JoinChatRoomResponse message_response;
        auto message = message_response.add_messages_list();
        message->set_primer(true);
        message->set_return_status(returnStatus);
        //chat room status should only be read if SUCCESS is set to return status
        message_response.set_chat_room_status(chatRoomStatus);
        responseStream->Write(message_response);
    };

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        writeResponseToClient(basic_info_return_status, ChatRoomStatus::SUCCESSFULLY_JOINED);
        return;
    }

    const std::string& chat_room_id = request->chat_room_id();

    if (isInvalidChatRoomId(chat_room_id)) {
        writeResponseToClient(ReturnStatus::SUCCESS, ChatRoomStatus::INVALID_CHAT_ROOM_ID);
        return;
    }

    const std::string& chat_room_password = request->chat_room_password();

    if (chat_room_password.size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
        writeResponseToClient(ReturnStatus::SUCCESS, ChatRoomStatus::INVALID_CHAT_ROOM_PASSWORD);
        return;
    }

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    const bsoncxx::oid user_account_oid = bsoncxx::oid{user_account_oid_str};

    const bsoncxx::document::value merge_document = document{} << finalize;

    const bsoncxx::document::value projection_document = document{}
        << "_id" << 1
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << user_account_keys::PICTURES << 1
            << user_account_keys::FIRST_NAME << 1
            << user_account_keys::AGE << 1
        << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            loginTokenStr,
            installationID,
            merge_document,
            current_timestamp
    );

    std::string different_user_joined_message_uuid;
    bsoncxx::stdx::optional<bsoncxx::document::value> different_user_joined_chat_room_value;
    JoinChatRoomWithUserReturn join_chat_room_return_value{ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
    mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {
        bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_user_account_result;

        if (!runInitialLoginOperation(
                find_and_update_user_account_result,
                user_accounts_collection,
                user_account_oid,
                login_document,
                projection_document,
                session)
        ) {
            session->abort_transaction();
            writeResponseToClient(ReturnStatus::LG_ERROR, ChatRoomStatus::INVALID_CHAT_ROOM_ID);
            return;
        }

        const ReturnStatus login_return_status = checkForValidLoginToken(find_and_update_user_account_result, user_account_oid_str);

        if (login_return_status != SUCCESS) { //if login failed
            session->abort_transaction();
            writeResponseToClient(login_return_status, ChatRoomStatus::SUCCESSFULLY_JOINED);
            return;
        }

        if (!find_and_update_user_account_result) {
            std::string error_string = "runInitialLoginOperation() returned a document that was not set along with ReturnStatus::SUCCESS.\n";

            request->AppendToString(&error_string);

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    "user_account_oid", user_account_oid
            );
            session->abort_transaction();
            writeResponseToClient(ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED);
            return;
        }

        const bsoncxx::document::view user_account_doc_view = find_and_update_user_account_result->view();

        join_chat_room_return_value = joinChatRoomWithUser(
                different_user_joined_message_uuid,
                different_user_joined_chat_room_value,
                current_timestamp,
                accounts_db,
                user_accounts_collection,
                chat_room_collection,
                session,
                user_account_doc_view,
                user_account_oid,
                chat_room_id,
                chat_room_password,
                false,
                [&request](std::string& error_string){
                    error_string.append(
                            "Called from joinChatRoom().\n"
                            );
                    request->AppendToString(&error_string);
                }
        );

    };

    //nothing is 'read' after a 'write', so causal consistency should not be needed here
    mongocxx::client_session session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transactionCallback);
    } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
        std::cout << "Exception calling joinChatRoomImplementation() transaction.\n" << e.what() << '\n';
#endif
        send_chat_room_back_to_client = false;
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            std::optional<std::string>(), std::string(e.what()),
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
            "user_OID", user_account_oid,
            "chat_room_id", chat_room_id
        );

        join_chat_room_return_value = {ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED};
    }

    //This will set send_chat_room_back_to_client to false if an error occurred.
    //This is called here to avoid a Write() inside the transaction callback above.
    writeResponseToClient(join_chat_room_return_value.return_status, join_chat_room_return_value.chat_room_status);

    //if an error occurred, do not write success or the new chat room and messages back
    if (!send_chat_room_back_to_client) {
        return;
    }

    StoreAndSendMessagesToJoinChatRoom store_and_send_messages_to_join_chat_room(responseStream);

    const bsoncxx::document::value empty_doc = document{}
            << "NOTE:" << "Dummy document from join_chat_room.cpp"
            << finalize;

    //NOTE: Messages must be requested AFTER the kDifferentUserJoinedMessage has been sent. This way
    // the chat change stream will not leave any 'gaps' in the messages (although there could be some
    // duplicates).
    if (!sendNewChatRoomAndMessages(
            mongo_cpp_client,
            accounts_db,
            user_accounts_collection,
            chat_room_collection,
            empty_doc.view(),
            chat_room_id,
            current_timestamp, //set to user account as user_account_keys::chat_rooms::LAST_TIME_VIEWED above
            current_timestamp,
            user_account_oid.to_string(),
            &store_and_send_messages_to_join_chat_room,
            //NOTE: In order to keep the JoinChatRoom fairly short (it can block the Android channel)
            // not requesting user or message info.
            HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO,
            AmountOfMessage::ONLY_SKELETON, //this doesn't matter inside joinChatRoom because requestFinalFewMessagesInFull == true
            true,
            false,
            false,
            current_timestamp, //do not request after this timestamp kDifferentUserJoinedMember, or could potentially request a user kicked/banned as only_stored_message==true
            different_user_joined_message_uuid,
            [&](){

                //Because there is not necessarily a guarantee that the kDifferentUserJoined message will be found
                // during the read here (the 'write concern' and 'read concern' do not guarantee it). And the ChatStreamContainerObject
                // will turn any kDifferentUserJoined for this user into a newUpdateMessage, this lambda will guarantee that
                // the message is returned to the user.
                //Also want this message to be returned before the kThisUserJoinedChatRoomFinishedMessage message.
                ChatMessageToClient response_message;

                if(!convertChatMessageDocumentToChatMessageToClient(
                        different_user_joined_chat_room_value->view(),
                        chat_room_id,
                        user_account_oid_str,
                        true,
                        &response_message,
                        AmountOfMessage::ONLY_SKELETON,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        nullptr)
                        ) {
                    return false;
                }

                store_and_send_messages_to_join_chat_room.sendMessage(std::move(response_message));

                return true;
            }
        )
    ) {
        writeResponseToClient(ReturnStatus::LG_ERROR, ChatRoomStatus::SUCCESSFULLY_JOINED);
    }

    //send all messages back to client
    store_and_send_messages_to_join_chat_room.finalCleanup();

}

