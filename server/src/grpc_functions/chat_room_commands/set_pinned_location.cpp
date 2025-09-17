//
// Created by jeremiah on 3/7/23.
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

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setPinnedLocationImplementation(const grpc_chat_commands::SetPinnedLocationRequest* request,
                                   grpc_chat_commands::SetPinnedLocationResponse* response);

void setPinnedLocation(const grpc_chat_commands::SetPinnedLocationRequest* request,
                       grpc_chat_commands::SetPinnedLocationResponse* response) {

    handleFunctionOperationException(
            [&] {
                setPinnedLocationImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);

}

void setPinnedLocationImplementation(const grpc_chat_commands::SetPinnedLocationRequest* request,
                                   grpc_chat_commands::SetPinnedLocationResponse* response) {

    std::string user_account_oid_sr;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_sr,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    const std::string& chat_room_id = request->chat_room_id();
    const std::string& message_uuid = request->message_uuid();

    const double new_longitude = request->new_pinned_longitude();
    const double new_latitude = request->new_pinned_latitude();

    if (isInvalidChatRoomId(chat_room_id)
        || (isInvalidLocation(new_longitude, new_latitude)
                && (new_longitude != chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE
                    || new_latitude != chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE)
           )
        || isInvalidUUID(message_uuid)
            ) {
        response->set_timestamp_message_stored(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp_message_stored(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const bsoncxx::oid user_account_oid{user_account_oid_sr};

    //NOTE: Not updating user_account_keys::chat_rooms::LAST_TIME_VIEWED here because the message timestamp will not be known
    // until after sendMessageToChatRoom() is run.
    const bsoncxx::document::value merge_document = document{} << finalize;

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

    //NOTE: Not using a transaction here because the only thing updated inside user account is the chat room observed time.

    if (!runInitialLoginOperation(
            find_user_account,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document
        )
    ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_user_account,
            user_account_oid_sr
    );

    if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
    }
    else {

        bool transaction_successful = false;

        //NOTE: this transaction is here to avoid inconsistencies between the chat room header and the user account info
        mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* session) {

            mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

            grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request;

            client_message_to_server_request.set_message_uuid(message_uuid);
            client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_new_pinned_location_message()->set_longitude(
                    new_longitude
            );
            client_message_to_server_request.mutable_message()->mutable_message_specifics()->mutable_new_pinned_location_message()->set_latitude(
                    new_latitude
            );

            const SendMessageToChatRoomReturn return_val = sendMessageToChatRoom(
                    &client_message_to_server_request,
                    chat_room_collection,
                    user_account_oid,
                    session
            );

            switch (return_val.successful) {
                case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                    std::string error_string = "Message UUID already existed when promote new admin attempted to send it.\n";

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "ObjectID_used", user_account_oid,
                            "new_longitude", std::to_string(new_longitude),
                            "new_latitude", std::to_string(new_latitude),
                            "message", client_message_to_server_request.DebugString()
                    );

                    session->abort_transaction();
                    //NOTE: timestamp was already set
                    response->set_return_status(ReturnStatus::SUCCESS);
                    response->set_timestamp_message_stored(return_val.time_stored_on_server.count());
                    return;
                }
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL: {
                    //NOTE: timestamp was already set
                    current_timestamp = return_val.time_stored_on_server;
                    response->set_return_status(ReturnStatus::SUCCESS);
                    response->set_timestamp_message_stored(return_val.time_stored_on_server.count());
                    break;
                }
                case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED: {
                    session->abort_transaction();
                    response->set_return_status(ReturnStatus::LG_ERROR);
                    response->set_timestamp_message_stored(-1);
                    return;
                }
            }

            bsoncxx::types::b_date mongodb_current_date{current_timestamp};

            std::optional<std::string> update_document_exception_string;
            bsoncxx::stdx::optional<mongocxx::result::update> updateChatRoomAccount;
            try {

                const std::string CURRENT_USER_ELEM = "e";
                const std::string CURRENT_USER_ELEM_ELEM_UPDATE =
                        chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + CURRENT_USER_ELEM + "].";

                mongocxx::options::update opts;

                bsoncxx::builder::basic::array arrayBuilder{};
                arrayBuilder.append(
                    document{}
                        << CURRENT_USER_ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID
                        << bsoncxx::types::b_oid{user_account_oid}
                    << finalize
                );

                opts.array_filters(arrayBuilder.view());

                //if this account is admin, and the other account is 'in chat room', promote other user to admin
                // and this user to 'in chat room'
                updateChatRoomAccount = chat_room_collection.update_one(
                    *session,
                    document{}
                        << "_id" << chat_room_header_keys::ID
                        << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                            << "$elemMatch" << open_document
                                << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                                << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                            << close_document
                        << close_document
                    << finalize,
                    document{}
                        << "$set" << open_document
                            << chat_room_header_keys::PINNED_LOCATION << open_document
                                << chat_room_header_keys::pinned_location::LONGITUDE << bsoncxx::types::b_double{new_longitude}
                                << chat_room_header_keys::pinned_location::LATITUDE << bsoncxx::types::b_double{new_latitude}
                            << close_document
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
            // 1) the chat room ID did not exist
            // 2) the user was not in the chat room
            // 3) the user was not admin
            //these could happen is something is a little out of sync
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
                        << "Set pinned location failed."
                        << "\nupdate_chat_rooms_account: " << update_chat_rooms_account
                        << "\nmatched_count: " << matched_count
                        << "\nmodified_count: " << modified_count
                        << '\n';

                storeMongoDBErrorAndException(__LINE__, __FILE__,
                                              update_document_exception_string, errorString.str(),
                                              "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                              "collection", chat_room_collection.name().to_string(),
                                              "ObjectID_used", user_account_oid,
                                              "chat_room_id", chat_room_id);

                session->abort_transaction();
                response->set_operation_failed(true);
                response->set_return_status(ReturnStatus::SUCCESS);
                return;
            }

            const bool update_user_viewed_time_return_value = updateUserLastViewedTime(
                    chat_room_id,
                    user_account_oid,
                    bsoncxx::types::b_date{current_timestamp},
                    user_accounts_collection,
                    session
            );

            if(!update_user_viewed_time_return_value) {
                session->abort_transaction();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }

            response->set_return_status(ReturnStatus::SUCCESS);
            response->set_timestamp_message_stored(return_val.time_stored_on_server.count());
        };

        mongocxx::client_session session = mongo_cpp_client.start_session();

        //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
        // more 'generic' errors. Can look here for more info
        // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
        try {
            session.with_transaction(transaction_callback);
        }
        catch (const mongocxx::logic_error& e) {
#ifndef _RELEASE
            std::cout << "Exception calling promoteNewAdminImplementation() transaction.\n" << e.what() << '\n';
#endif

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                    "user_OID", user_account_oid,
                    "chat_room_id", chat_room_id
            );
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