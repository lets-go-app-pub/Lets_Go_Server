//
// Created by jeremiah on 3/20/21.
//

#include <sstream>

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <handle_function_operation_exception.h>
#include <connection_pool_global_variable.h>

#include "extract_thumbnail_from_verified_doc.h"
#include "helper_objects/chat_room_commands_helper_objects.h"
#include "helper_functions/chat_room_commands_helper_functions.h"
#include "utility_chat_functions.h"

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "chat_room_header_keys.h"
#include "extract_data_from_bsoncxx.h"
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

void promoteNewAdminImplementation(const grpc_chat_commands::PromoteNewAdminRequest* request,
                                   grpc_chat_commands::PromoteNewAdminResponse* response);

//primary function for PromoteNewAdminRPC, called from gRPC server implementation
void promoteNewAdmin(const grpc_chat_commands::PromoteNewAdminRequest* request,
                     grpc_chat_commands::PromoteNewAdminResponse* response) {

    handleFunctionOperationException(
            [&] {
                promoteNewAdminImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);

}

void promoteNewAdminImplementation(const grpc_chat_commands::PromoteNewAdminRequest* request,
                                   grpc_chat_commands::PromoteNewAdminResponse* response) {
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
    const std::string& accountIDToPromote = request->new_admin_account_oid(); //check oid
    const std::string& message_uuid = request->message_uuid();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    if (isInvalidChatRoomId(chatRoomId)
        || isInvalidOIDString(accountIDToPromote)
        || isInvalidUUID(message_uuid)
            ) {
        response->set_timestamp_message_stored(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    if (accountIDToPromote == userAccountOIDStr
       || accountIDToPromote == event_admin_values::OID.to_string()) {
        response->set_timestamp_message_stored(-1);
        response->set_user_account_states_matched(true);
        response->set_return_status(ReturnStatus::SUCCESS);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp_message_stored(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    const bsoncxx::oid userAccountOID{userAccountOIDStr};

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

    //NOTE: Not using a transaction here because the only thing updated inside user account is the chat room observed time.

    if (!runInitialLoginOperation(
            findUserAccount,
            userAccountsCollection,
            userAccountOID,
            loginDocument,
            projectionDocument
        )
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, userAccountOIDStr);

    if (returnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(returnStatus);
    }
    else {

        bool transaction_successful = false;

        //NOTE: this transaction is here to avoid inconsistencies between the chat room header and the user account info
        mongocxx::client_session::with_transaction_cb transactionCallback = [&](mongocxx::client_session* session) {

            mongocxx::collection chatRoomCollection = chatRoomDB[collection_names::CHAT_ROOM_ID_ + chatRoomId];

            bsoncxx::oid accountToPromoteOID(accountIDToPromote);
            grpc_chat_commands::ClientMessageToServerRequest clientMessageToServerRequest;

            clientMessageToServerRequest.set_message_uuid(message_uuid);
            clientMessageToServerRequest.mutable_message()->mutable_message_specifics()->mutable_new_admin_promoted_message()->set_promoted_account_oid(
                accountIDToPromote
            );

            const SendMessageToChatRoomReturn return_val = sendMessageToChatRoom(
                &clientMessageToServerRequest,
                chatRoomCollection,
                userAccountOID,
                session
            );

            switch (return_val.successful) {
                case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                    std::string errorString = "Message UUID already existed when promote new admin attempted to send it.\n";

                    std::optional <std::string> exceptionString;
                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  exceptionString, errorString,
                                                  "ObjectID_used", userAccountOID,
                                                  "accountToPromoteOID", accountToPromoteOID,
                                                  "message", clientMessageToServerRequest.DebugString());

                    session->abort_transaction();
                    //NOTE: timestamp was already set
                    response->set_return_status(ReturnStatus::SUCCESS);
                    response->set_timestamp_message_stored(return_val.time_stored_on_server.count());
                    return;
                }
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                    //NOTE: timestamp was already set
                    current_timestamp = return_val.time_stored_on_server;
                    response->set_return_status(ReturnStatus::SUCCESS);
                    response->set_timestamp_message_stored(return_val.time_stored_on_server.count());
                    break;
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED:
                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response->set_timestamp_message_stored(-1);
                    return;
            }

            bsoncxx::types::b_date mongodb_current_date{current_timestamp};

            std::optional<std::string> update_document_exception_string;
            bsoncxx::stdx::optional<mongocxx::result::update> updateChatRoomAccount;
            try {

                const std::string CURRENT_USER_ELEM = "elem";
                const std::string OTHER_USER_ELEM = "other";
                const std::string CURRENT_USER_ELEM_ELEM_UPDATE =
                        chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + CURRENT_USER_ELEM + "].";
                const std::string OTHER_USER_ELEM_ELEM_UPDATE =
                        chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + OTHER_USER_ELEM + "].";

                mongocxx::options::update opts;

                bsoncxx::builder::basic::array arrayBuilder{};
                arrayBuilder.append(
                        document{}
                                << CURRENT_USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                << bsoncxx::types::b_oid{userAccountOID}
                        << finalize
                );
                arrayBuilder.append(
                        document{}
                                << OTHER_USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                << bsoncxx::types::b_oid{accountToPromoteOID}
                        << finalize
                );

                opts.array_filters(arrayBuilder.view());

                //if this account is admin, and the other account is 'in chat room', promote other user to admin
                // and this user to 'in chat room'
                updateChatRoomAccount = chatRoomCollection.update_one(
                    *session,
                    document{}
                        << "_id" << chat_room_header_keys::ID
                        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                            << "$elemMatch" << open_document
                                << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << userAccountOID
                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                            << close_document
                        << close_document
                        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                            << "$elemMatch" << open_document
                                << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << accountToPromoteOID
                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM}
                            << close_document
                        << close_document
                    << finalize,
                    document{}
                        << "$set" << open_document
                            << CURRENT_USER_ELEM_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM}
                            << OTHER_USER_ELEM_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                        << close_document
                        << "$max" << open_document
                            << chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME << mongodb_current_date
                            << CURRENT_USER_ELEM_ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongodb_current_date
                        << close_document
                    << finalize,
                    opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                update_document_exception_string = std::string(e.what());
            }

            //when matched_count() == 0 means that either
            //1) the chat room ID did not exist
            //2) the user was not in the chat room
            //3) the user was not admin
            //4) the user to be removed was not in the chat room
            //these could happen is something is a little out of sync
            if (updateChatRoomAccount && updateChatRoomAccount->matched_count() == 0) {

                bsoncxx::stdx::optional<mongocxx::cursor> user_accounts_cursor;
                try {

                    mongocxx::pipeline pipe;

                    pipe.match(
                        document{}
                            << "_id" << chat_room_header_keys::ID
                        << finalize
                    );

                    //project out the sending user and the target user along with their account states
                    pipe.project(
                        document{}
                            << "_id" << 0
                            << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                                << "$reduce" << open_document
                                    << "input" << "$" + chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                    << "initialValue" << open_array << close_array
                                    << "in" << open_document
                                        << "$cond" << open_document

                                            //if this element is one of the user accounts
                                            << "if" << open_document

                                                << "$in" << open_array
                                                    << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                                    << open_array
                                                        << userAccountOID
                                                        << accountToPromoteOID
                                                    << close_array
                                                << close_array

                                            << close_document

                                            << "then" << open_document
                                                << "$concatArrays" << open_array
                                                    << "$$value"
                                                    << open_array
                                                        << open_document
                                                            << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << "$$this." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                                            << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << "$$this." + chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                                        << close_document
                                                    << close_array
                                                << close_array
                                            << close_document

                                            << "else" << "$$value"

                                        << close_document
                                    << close_document
                                << close_document
                            << close_document
                        << finalize
                    );

                    //extract user account info
                    user_accounts_cursor = chatRoomCollection.aggregate(pipe);
                }
                catch (const mongocxx::logic_error& e) {
                    update_document_exception_string = std::string(e.what());
                }

                if(!user_accounts_cursor) {
                    const std::string errorString = "Promote new admin failed to find for some reason.";

                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  update_document_exception_string, errorString,
                                                  "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                  "collection", chatRoomCollection.name().to_string(),
                                                  "ObjectID_used", userAccountOID,
                                                  "accountToPromoteOID", accountToPromoteOID);

                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response->set_timestamp_message_stored(-1);
                    return;
                }

                auto sending_user_account_state = AccountStateInChatRoom(-1);
                auto target_user_account_state = AccountStateInChatRoom(-1);

                try {

                    //There should only be one element inside the cursor, the chat room header.
                    for(const auto& doc : *user_accounts_cursor) {
                        bsoncxx::array::view account_in_chat_room = extractFromBsoncxx_k_array(
                                doc,
                                chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM
                                );

                        for(const auto& ele : account_in_chat_room) {

                            if(ele.type() == bsoncxx::type::k_document) {

                                bsoncxx::document::view account_doc = ele.get_document();

                                bsoncxx::oid account_oid = extractFromBsoncxx_k_oid(
                                        account_doc,
                                        chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                                );

                                if(account_oid == userAccountOID) {
                                    sending_user_account_state = AccountStateInChatRoom(
                                        extractFromBsoncxx_k_int32(
                                            account_doc,
                                            chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                        )
                                    );
                                } else if(account_oid == accountToPromoteOID) {
                                    target_user_account_state = AccountStateInChatRoom(
                                            extractFromBsoncxx_k_int32(
                                                    account_doc,
                                                    chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM
                                            )
                                    );
                                } else {
                                    std::string error_string = "Incorrect account oid found inside .\n";
                                    error_string += makePrettyJson(doc);

                                    storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        update_document_exception_string, error_string,
                                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                        "collection", chatRoomCollection.name().to_string(),
                                        "ObjectID_used", userAccountOID,
                                        "accountToPromoteOID", accountToPromoteOID,
                                        "extracted account_oid", account_oid
                                    );

                                    //NOTE: Can continue here.
                                }
                            } else {
                                std::string error_string = "Invalid type found in ACCOUNTS_IN_CHAT_ROOM not a document.\n";
                                error_string += makePrettyJson(doc);

                                storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    update_document_exception_string, error_string,
                                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                    "collection", chatRoomCollection.name().to_string(),
                                    "ObjectID_used", userAccountOID,
                                    "accountToPromoteOID", accountToPromoteOID
                                );

                                session->abort_transaction();
                                response->set_return_status(ReturnStatus::LG_ERROR);
                                response->set_timestamp_message_stored(-1);
                                return;
                            }
                        }
                    }

                }
                catch (const ErrorExtractingFromBsoncxx& e) {
                    //Error already stored inside.
                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response->set_timestamp_message_stored(-1);
                    return;
                }

                //This can mean one of two things.
                // 1) The chat room itself does not exist (the header was not found).
                // 2) One of the users has never been in the chat room before.
                if(sending_user_account_state == AccountStateInChatRoom(-1)
                    || target_user_account_state == AccountStateInChatRoom(-1)) {
                    std::string errorString = "Chat room header did not exist or a user has never been in chat room before.\n"
                                              "The users should always exist inside the chat room, even if they are ACCOUNT_STATE_NOT_IN_CHAT_ROOM.\n";

                    std::optional<std::string> dummy_exception_string;
                    storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                  dummy_exception_string, errorString,
                                                  "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                                  "collection", chatRoomCollection.name().to_string(),
                                                  "ObjectID_used", userAccountOID,
                                                  "accountToPromoteOID", accountToPromoteOID);

                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response->set_timestamp_message_stored(-1);
                    return;
                }

                //One of these cases is true if it reached this point.
                // Current user is not (possibly no longer) admin.
                // Other user is not ACCOUNT_STATE_IN_CHAT_ROOM.
                session->abort_transaction();
                response->set_return_status(ReturnStatus::SUCCESS);
                response->set_user_account_states_matched(false);
                response->set_timestamp_message_stored(-1);
                return;
            }
            else if(!updateChatRoomAccount) { //an error occurred
                bool update_chat_rooms_account = updateChatRoomAccount.operator bool();
                int matched_count = 0;
                int modified_count = 0;

                if (update_chat_rooms_account) {
                    matched_count = updateChatRoomAccount->matched_count();
                    modified_count = updateChatRoomAccount->modified_count();
                }

                std::stringstream errorString;

                errorString
                        << "Promote user to admin failed."
                        << "\nupdate_chat_rooms_account: " << update_chat_rooms_account
                        << "\nmatched_count: " << matched_count
                        << "\nmodified_count: " << modified_count
                        << '\n';

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        update_document_exception_string, errorString.str(),
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chatRoomCollection.name().to_string(),
                        "ObjectID_used", userAccountOID,
                        "accountToPromoteOID", accountToPromoteOID);

                session->abort_transaction();
                response->set_user_account_states_matched(false);
                response->set_return_status(ReturnStatus::LG_ERROR);
                response->set_timestamp_message_stored(-1);
                return;
            }
            else { //updated properly
                response->set_user_account_states_matched(true);
            }

            bool update_user_viewed_time_return_value = updateUserLastViewedTime(
                chatRoomId,
                userAccountOID,
                mongodb_current_date,
                userAccountsCollection,
                session
            );

            if(!update_user_viewed_time_return_value) {
                session->abort_transaction();
                response->set_user_account_states_matched(false);
                response->set_return_status(ReturnStatus::LG_ERROR);
                response->set_timestamp_message_stored(-1);
                return;
            }

            transaction_successful = true;
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
            std::cout << "Exception calling promoteNewAdminImplementation() transaction.\n" << e.what() << '\n';
#endif

            std::optional<std::string> dummyExceptionString;
            storeMongoDBErrorAndException(__LINE__, __FILE__, dummyExceptionString,
                                          std::string(e.what()),
                                          "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                          "collection", collection_names::CHAT_ROOM_ID_ + chatRoomId,
                                          "user_OID", userAccountOID,
                                          "chat_room_id", chatRoomId);

            transaction_successful = false;
        }

        //error was already stored
        if (!transaction_successful) {
            if (response->return_status() == ReturnStatus::UNKNOWN) {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }
            return;
        }
    }

}
