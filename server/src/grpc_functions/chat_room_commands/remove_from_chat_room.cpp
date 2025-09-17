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
#include <global_bsoncxx_docs.h>
#include <utility_chat_functions.h>
#include <sstream>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "chat_room_header_keys.h"
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


void removeFromChatRoomImplementation(const grpc_chat_commands::RemoveFromChatRoomRequest* request,
                                      grpc_chat_commands::RemoveFromChatRoomResponse* response);

//primary function for RemoveFromChatRoomRPC, called from gRPC server implementation
void removeFromChatRoom(const grpc_chat_commands::RemoveFromChatRoomRequest* request,
                        grpc_chat_commands::RemoveFromChatRoomResponse* response) {
    handleFunctionOperationException(
            [&] {
                removeFromChatRoomImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void removeFromChatRoomImplementation(const grpc_chat_commands::RemoveFromChatRoomRequest* request,
                                      grpc_chat_commands::RemoveFromChatRoomResponse* response) {

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
    const std::string& accountOIDToRemoveStr = request->account_id_to_remove(); //check token
    const std::string& message_uuid = request->message_uuid();

    std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    if (request->account_id_to_remove() == event_admin_values::OID.to_string()) {
        response->set_timestamp_stored(currentTimestamp.count());
        response->set_return_status(ReturnStatus::SUCCESS);
        response->set_operation_failed(true);
        return;
    }

    if (isInvalidChatRoomId(chatRoomId)
        || isInvalidOIDString(accountOIDToRemoveStr)
        || isInvalidUUID(message_uuid)
            ) {
        response->set_timestamp_stored(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if(!grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_IsValid(request->kick_or_ban())) {
        response->set_timestamp_stored(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if (accountOIDToRemoveStr == userAccountOIDStr) {
        response->set_timestamp_stored(getCurrentTimestamp().count());
        response->set_return_status(ReturnStatus::SUCCESS);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp_stored(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    const bsoncxx::oid user_account_oid{userAccountOIDStr};
    const bsoncxx::oid accountOIDToRemove{accountOIDToRemoveStr};

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
            currentTimestamp
    );

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccount;

    if (!runInitialLoginOperation(
            findUserAccount,
            user_accounts_collection,
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

        std::optional<std::string> find_pictures_in_user_account_exception_string;
        bsoncxx::stdx::optional<bsoncxx::document::value> user_account_to_remove_pictures_doc_value;
        try {

            mongocxx::options::find opts;

            opts.projection(document{}
                                    << "_id" << 0
                                    << user_account_keys::PICTURES << 1
                                    << finalize);

            user_account_to_remove_pictures_doc_value = user_accounts_collection.find_one(
                    document{}
                        << "_id" << accountOIDToRemove
                    << finalize,
                    opts);
        }
        catch (const mongocxx::logic_error& e) {
            find_pictures_in_user_account_exception_string = std::string(e.what());
        }

        bsoncxx::document::view userToRemoveAccountDocView;
        int thumbnail_size = 0;
        std::string updated_thumbnail_reference_oid;

        if (user_account_to_remove_pictures_doc_value) { //if verified account found
            userToRemoveAccountDocView = user_account_to_remove_pictures_doc_value->view();
        }
        else { //if verified account not found
            std::string errorString = "Failed to find user to remove verified account.\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    find_pictures_in_user_account_exception_string, errorString,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", user_account_oid);

            //the only way this could happen is (an exception or) if the account was deleted between the send function
            // the delete function will automatically remove it however there is a 'gap' there
            // where it could still attempt the remove, if returning success there is no difference
            // between having achieved the kick to the client
            response->set_timestamp_stored(currentTimestamp.count());
            response->set_return_status(ReturnStatus::SUCCESS);
            return;
        }

        auto setThumbnailValue = [&](
                std::string& /*thumbnail*/,
                int _thumbnail_size,
                const std::string& _thumbnail_reference_oid,
                const int /*index*/,
                const std::chrono::milliseconds& /*thumbnail_timestamp*/
                ) {
            thumbnail_size = _thumbnail_size;
            updated_thumbnail_reference_oid = _thumbnail_reference_oid;
        };

        ExtractThumbnailAdditionalCommands extractThumbnailAdditionalCommands;
        extractThumbnailAdditionalCommands.setupForAddToSet(chatRoomId);

        //extract thumbnail for user to remove; requires PICTURES projection
        if (!extractThumbnailFromUserAccountDoc(
                accountsDB,
                userToRemoveAccountDocView,
                accountOIDToRemove,
                nullptr,
                setThumbnailValue,
                extractThumbnailAdditionalCommands)
            ) { //failed to extract thumbnail

            //errors are already stored
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        bool errorOccurredDuringTransaction = false;

        //run the removing user accounts from header and user account inside transaction to keep them in sync
        mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

            mongocxx::collection chat_room_collection = chatRoomDB[collection_names::CHAT_ROOM_ID_ + chatRoomId];

            grpc_chat_commands::ClientMessageToServerRequest generated_request;
            generated_request.set_message_uuid(message_uuid);

            if (request->kick_or_ban() == grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_BAN) {
                generated_request.mutable_message()->mutable_message_specifics()->mutable_user_banned_message()->set_banned_account_oid(
                    accountOIDToRemove.to_string()
                );
            }
            else {
                generated_request.mutable_message()->mutable_message_specifics()->mutable_user_kicked_message()->set_kicked_account_oid(
                    accountOIDToRemove.to_string()
                );
            }

            const SendMessageToChatRoomReturn sendMsgReturnVal = sendMessageToChatRoom(
                &generated_request,
                chat_room_collection,
                user_account_oid,
                session
            );

            switch (sendMsgReturnVal.successful) {
                case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                    std::string errorString = "Message UUID already existed when remove from chat room attempted to send it.\n";

                    std::optional <std::string> exceptionString;
                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  exceptionString, errorString,
                                                  "user_OID", userAccountOIDStr,
                                                  "chatRoomId", chatRoomId,
                                                  "account_OID_to_remove", accountOIDToRemoveStr,
                                                  "message", generated_request.DebugString());

                    session->abort_transaction();
                    response->set_timestamp_stored(sendMsgReturnVal.time_stored_on_server.count());
                    response->set_operation_failed(false);
                    response->set_return_status(ReturnStatus::SUCCESS);
                    return;
                }
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                    currentTimestamp = sendMsgReturnVal.time_stored_on_server;
                    response->set_timestamp_stored(sendMsgReturnVal.time_stored_on_server.count());
                    response->set_return_status(ReturnStatus::SUCCESS);
                    response->set_operation_failed(false);
                    break;
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED:
                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response->set_operation_failed(true);
                    return;
            }

            bsoncxx::builder::stream::document updateDoc{};

            const std::string REMOVE_ELEM = "rem";
            const std::string REMOVE_ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + REMOVE_ELEM + "].";

            const std::string USER_ELEM = "usr";
            const std::string USER_ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + USER_ELEM + "].";

            auto mongo_db_date_object = bsoncxx::types::b_date{currentTimestamp};

            updateDoc
                    << "$max" << open_document
                        << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongo_db_date_object
                        << USER_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongo_db_date_object
                    << close_document
                    << "$set" << open_document
                        << REMOVE_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_SIZE << thumbnail_size
                        << REMOVE_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_TIMESTAMP << mongo_db_date_object
                        << REMOVE_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << bsoncxx::types::b_string{updated_thumbnail_reference_oid}
                    << close_document;

            //if account was banned, add the banned list
            if (request->kick_or_ban() == grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_BAN) {
                updateDoc
                        << "$set" << open_document
                            << REMOVE_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                            << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_BANNED}
                        << close_document;
            } else {
                updateDoc
                        << "$set" << open_document
                            << REMOVE_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                            << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM}
                        << close_document;
            }

            updateDoc
                << "$push" << open_document
                    << REMOVE_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << mongo_db_date_object
                << close_document;

            mongocxx::options::find_one_and_update opts;

            bsoncxx::builder::basic::array arrayBuilder{};
            arrayBuilder.append(
                document{}
                    << REMOVE_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << accountOIDToRemove
                << finalize
            );
            arrayBuilder.append(
                document{}
                    << USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                << finalize
            );

            opts.array_filters(arrayBuilder.view());

            //project the admin
            //NOTE: would prefer to project out the single element inside the array AND only the header account OID, however
            // starting in MongoDB 4.4 I need an aggregation pipeline to do that (mentioned inside the $slice documentation).
            // so I need to either take a single element which matches the parameters (which would extract the user thumbnail)
            // OR take all array elements however only the account OID and account state
            opts.projection(
                document{}
                    << "_id" << 0
                    << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << 1
                    << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + "." + chat_room_header_keys::accounts_in_chat_room::THUMBNAIL_REFERENCE << 1
                << finalize
            );

            bsoncxx::stdx::optional<bsoncxx::document::value> findAndUpdateChatRoomAccount;
            std::optional<std::string> find_and_update_chat_room_exception_string;
            try {

                //if this account is admin, remove the account to be removed
                findAndUpdateChatRoomAccount = chat_room_collection.find_one_and_update(
                        *session,
                        document{}
                            << "_id" << chat_room_header_keys::ID
                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                            << "$elemMatch" << open_document
                                << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                            << close_document
                            << close_document
                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                                << "$elemMatch" << open_document
                                    << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << accountOIDToRemove
                                    << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM}
                                << close_document
                            << close_document
                        << finalize,
                        updateDoc.view(),
                        opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                find_and_update_chat_room_exception_string = e.what();
            }

            if (!findAndUpdateChatRoomAccount) {
                std::string errorString = "Find and update chat room to remove verified OID failed OR account that sent this was not admin.\n";

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        find_and_update_chat_room_exception_string, errorString,
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chat_room_collection.name().to_string(),
                        "user_oid", user_account_oid,
                        "message_oid", request->message_uuid()
                        );

                //This has done all it can (it might be possible to reach this
                // if a user does a 'kick' then a 'promote to admin' OR the
                // other user leaves before this is called).
                session->abort_transaction();
                response->set_timestamp_stored(sendMsgReturnVal.time_stored_on_server.count());
                response->set_return_status(ReturnStatus::SUCCESS);
                response->set_operation_failed(true);
                return;
            }

            bsoncxx::document::view chat_room_header_doc_view = findAndUpdateChatRoomAccount->view();

            if(!extractUserAndRemoveChatRoomIdFromUserPicturesReference(
                    accountsDB,
                    chat_room_header_doc_view,
                    user_account_oid,
                    updated_thumbnail_reference_oid,
                    chatRoomId,
                    session)
            ) {
                session->abort_transaction();
                errorOccurredDuringTransaction = true;
                response->set_return_status(ReturnStatus::LG_ERROR);
                response->set_operation_failed(true);
                return;
            }

            bsoncxx::stdx::optional<mongocxx::v_noabi::result::update> update_user_account;
            std::optional<std::string> update_user_account_exception_string;
            try {

                mongocxx::pipeline pipeline;

                //Remove chat room element from user that is being removed.
                //Update last viewed time on user that is removing.
                pipeline.add_fields(
                    document{}
                        << user_account_keys::CHAT_ROOMS << open_document
                            << "$cond" << open_document
                                //check which account oid this doc is (only 2 are requested)
                                << "if" << open_document
                                    << "$eq" << open_array
                                        << "$_id"
                                        << accountOIDToRemove
                                    << close_array
                                << close_document

                                //this is the account oid to remove
                                << "then" << open_document
                                    << "$filter" << open_document
                                        << "input" << "$" + user_account_keys::CHAT_ROOMS
                                        << "cond" << open_document
                                            << "$ne" << open_array
                                                << "$$this." + user_account_keys::chat_rooms::CHAT_ROOM_ID
                                                << chatRoomId
                                            << close_array
                                        << close_document
                                    << close_document
                                << close_document

                                //this is the current user account oid to update
                                << "else" << open_document
                                    << "$map" << open_document
                                        << "input" << "$" + user_account_keys::CHAT_ROOMS
                                        << "in" << open_document
                                            << "$cond" << open_document

                                                //if: using this chat room Id
                                                << "if" << open_document
                                                    << "$eq" << open_array
                                                        << chatRoomId << "$$this." + user_account_keys::chat_rooms::CHAT_ROOM_ID
                                                    << close_array
                                                << close_document

                                                //then: update last viewed time
                                                << "then" << open_document
                                                    << "$mergeObjects" << open_array
                                                        << "$$this"
                                                        << open_document
                                                            << user_account_keys::chat_rooms::LAST_TIME_VIEWED << open_document
                                                                << "$max" << open_array
                                                                    << "$$this." + user_account_keys::chat_rooms::LAST_TIME_VIEWED << mongo_db_date_object
                                                                << close_array
                                                            << close_document
                                                        << close_document
                                                    << close_array
                                                << close_document

                                                //else: use the default element
                                                << "else" << "$$this"

                                            << close_document
                                        << close_document
                                    << close_document
                                << close_document
                            << close_document
                        << close_document
                    << finalize
                );

                update_user_account = user_accounts_collection.update_many(
                        *session,
                        document{}
                            << "_id" << open_document
                                << "$in" << open_array
                                    << accountOIDToRemove << user_account_oid
                                << close_array
                            << close_document
                        << finalize,
                        pipeline);
            }
            catch (const mongocxx::logic_error& e) {
                update_user_account_exception_string = e.what();
            }

            if (!update_user_account ||
                update_user_account->matched_count() == 0) { //if the removed user account oid does not exist it is OK (value of 1)

                bool update_chat_rooms_account = findAndUpdateChatRoomAccount.operator bool();

                std::stringstream errorString;

                errorString
                        << "Remove user from chat room failed."
                        << "\nupdate_chat_rooms_account: " << update_chat_rooms_account
                        << '\n';

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        update_user_account_exception_string, errorString.str(),
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chat_room_collection.name().to_string(),
                        "current_object_oid", user_account_oid,
                        "ObjectID_used", accountOIDToRemove);

                session->abort_transaction();
                errorOccurredDuringTransaction = true;
                response->set_return_status(ReturnStatus::LG_ERROR);
                response->set_operation_failed(true);
                return;
            }

        };

        mongocxx::client_session session = mongoCppClient.start_session();

        //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
        // more 'generic' errors. Can look here for more info
        // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
        try {
            session.with_transaction(transactionCallback);
        } catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
            std::cout << "Exception calling removeFromChatRoomImplementation() transaction.\n" << e.what() << '\n';
#endif

            std::optional <std::string> dummyExceptionString;
            storeMongoDBErrorAndException(__LINE__, __FILE__, dummyExceptionString,
                                          std::string(e.what()),
                                          "database", database_names::ACCOUNTS_DATABASE_NAME,
                                          "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                                          "user_OID", userAccountOIDStr,
                                          "chatRoomId", chatRoomId,
                                          "account_OID_to_remove", accountOIDToRemoveStr);

            errorOccurredDuringTransaction = true;
            response->set_return_status(ReturnStatus::LG_ERROR);
        }

        if(errorOccurredDuringTransaction) {
            if(response->return_status() == ReturnStatus::UNKNOWN) {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }
            return;
        }

    }

}
