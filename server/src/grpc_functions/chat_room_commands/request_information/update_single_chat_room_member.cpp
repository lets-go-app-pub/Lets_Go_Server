//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include "extract_thumbnail_from_verified_doc.h"
#include "helper_functions/chat_room_commands_helper_functions.h"
#include "utility_chat_functions.h"
#include "global_bsoncxx_docs.h"
#include "handle_function_operation_exception.h"
#include "connection_pool_global_variable.h"
#include "update_single_other_user.h"

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

/** Documentation for this file can be found inside [grpc_functions/chat_room_commands/request_information/_documentation.md] **/

void updateSingleChatRoomMemberImplementation(
        const grpc_chat_commands::UpdateSingleChatRoomMemberRequest* request,
        UpdateOtherUserResponse* response
);

//primary function for UpdateSingleChatRoomMemberRPC, called from gRPC server implementation
void updateSingleChatRoomMember(
        const grpc_chat_commands::UpdateSingleChatRoomMemberRequest* request,
        UpdateOtherUserResponse* response
) {
    handleFunctionOperationException(
            [&] {
                updateSingleChatRoomMemberImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            }, __LINE__, __FILE__, request);
}

void updateSingleChatRoomMemberImplementation(
            const grpc_chat_commands::UpdateSingleChatRoomMemberRequest* request,
            UpdateOtherUserResponse* response
) {

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

    const std::string& chat_room_id = request->chat_room_id();

    if (isInvalidChatRoomId(chat_room_id)) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        response->set_timestamp_returned(-2L);
        return;
    }

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    //NOTE: Make sure to use the error_checked_member instead of request->chat_room_member_info(). It
    // has been set up properly.
    OtherUserInfoForUpdates error_checked_member;

    if(!filterAndStoreSingleUserToBeUpdated(
            request->chat_room_member_info(),
            request->chat_room_member_info().account_state(),
            current_timestamp,
            error_checked_member)
    ) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        response->set_timestamp_returned(-2L);
        return;
    }

    const std::string& member_account_oid_str = error_checked_member.account_oid();

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    response->set_timestamp_returned(-1L);
    response->set_return_status(ReturnStatus::UNKNOWN); //setting unknown as default

    const bsoncxx::oid user_account_oid{user_account_oid_str};
    const bsoncxx::oid member_account_oid{member_account_oid_str};

    const bsoncxx::document::value merge_document = document{} << finalize;

    const bsoncxx::document::value projection_document = document{}
            << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
            << finalize;

    const bsoncxx::document::value login_document = getLoginDocument<false>(
            loginTokenStr,
            installationID,
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

    const ReturnStatus return_status = checkForValidLoginToken(find_user_account, user_account_oid_str);

    if (return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(return_status);
        return;
    }

    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];
    bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_chat_room_account;

    bool transaction_successful = false;

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* session) {

        const grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request =
                generateUserActivityDetectedMessage(
                        generateUUID(),
                        chat_room_id
                        );

        //set user activity detected when user updates this
        //no need to check for failure here, can continue even if message was not stored
        const SendMessageToChatRoomReturn send_msg_return_value = sendMessageToChatRoom(
                &client_message_to_server_request,
                chat_room_collection,
                user_account_oid,
                session
        );

        switch (send_msg_return_value.successful) {
            case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                const std::string error_string = "Message UUID already existed when update single chat room member attempted to send it.\n";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional <std::string>(), error_string,
                        "ObjectID_used", user_account_oid,
                        "message", client_message_to_server_request.DebugString()
                );

                //This should never happen, the uuid was randomly generated above.
                session->abort_transaction();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                //the LAST_ACTIVITY_TIME must be updated to the kUserActivityDetectedMessage time below
                current_timestamp = send_msg_return_value.time_stored_on_server;
                break;
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED:
                session->abort_transaction();
                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
        }

        mongocxx::options::find_one_and_update opts;

        //This will project only the specific element representing the requested user from the header and the event_oid
        // in case the passed user account is an event.
        opts.projection(
            document{}
                << "_id" << 0
                << chat_room_header_keys::EVENT_ID << 1
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                    << "$elemMatch" << open_document
                        << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << member_account_oid
                    << close_document
                << close_document
            << finalize
        );

        const std::string ELEM = "e";
        bsoncxx::builder::basic::array array_builder{};
        array_builder.append(
                document{}
                    << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                << finalize
        );

        opts.array_filters(array_builder.view());

        opts.return_document(mongocxx::options::return_document::k_after);

        try {

            const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";
            find_and_update_chat_room_account = chat_room_collection.find_one_and_update(
                    *session,
                document{}
                    << "_id" << chat_room_header_keys::ID
                    << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM << open_document
                        << "$elemMatch" << open_document
                            << chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
                            << "$or" << open_array
                                << open_document
                                    << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM}
                                << close_document
                                << open_document
                                    << chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM << bsoncxx::types::b_int32{AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN}
                                << close_document
                            << close_array
                        << close_document
                    << close_document
                << finalize,
                document{}
                    << "$max" << open_document
                        << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << bsoncxx::types::b_date{current_timestamp}
                    << close_document
                << finalize,
                opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "userAccountOID", user_account_oid,
                    "memberAccountOIDString", member_account_oid_str
            );

            session->abort_transaction();
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        //NOTE: LAST_VIEWED_TIME should not be updated here. That is because this function is called when a user is
        // viewing a different users' info card on the device. updateChatRoom() on the other hand is called when the
        // user is clicking a chat room initially and so it must be updated there because the user immediately views all
        // messages.

        //Leaving this block inside the transaction not because transactions can run multiple times. This means that the
        // result of the find_one_and_update for findAndUpdateChatRoomAccount could be hard to determine if it is not
        // directly after the database call.
        if(find_and_update_chat_room_account) {

            transaction_successful = true;
            response->set_timestamp_returned(current_timestamp.count());
        }
        else {
            //NOTE: there is a small window where this could happen
            // if AS the current user is requesting the messages from the database the current user is kicked;
            // then the message that this user left will just get added on to the end meaning that it could attempt
            // to request this info while not in the chat room
            // the user would have to be inside the chat room and have lost their connection for this to happen, however it is possible

            //this means the current user was not ACCOUNT_STATE_IS_ADMIN or STATE_IN_CHAT_ROOM
            const std::string error_string = "The current user is not a member of the passed chat room.\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "userAccountOID", user_account_oid,
                    "memberAccountOIDString", member_account_oid_str);

            session->abort_transaction();
            //sending back that AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM just in case.
            buildBasicUpdateOtherUserResponse(
                    response,
                    AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
                    user_account_oid_str,
                    current_timestamp,
                    -1L
            );
            return;
        }
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
        std::cout << "Exception calling updateSingleChatRoomMemberImplementation() transaction.\n" << e.what() << '\n';
#endif
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), std::string(e.what()),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                "user_OID", user_account_oid,
                "chat_room_id", chat_room_id
        );

        response->set_return_status(ReturnStatus::LG_ERROR);
        transaction_successful = false;
    }

    if(!transaction_successful) {
        return;
    }

    const bsoncxx::document::view chat_room_account_view = find_and_update_chat_room_account->view();

    enum AccountFoundInChatRoom {
        ACCOUNT_NOT_FOUND_IN_CHAT_ROOM,
        ACCOUNT_FOUND_IN_CHAT_ROOM_USER,
        ACCOUNT_FOUND_IN_CHAT_ROOM_EVENT
    };

    bsoncxx::document::view account_from_header;
    auto account_found = AccountFoundInChatRoom::ACCOUNT_NOT_FOUND_IN_CHAT_ROOM;
    bsoncxx::array::view accounts_in_chat_room;
    std::string event_oid;

    auto event_oid_element = chat_room_account_view[chat_room_header_keys::EVENT_ID];
    if (event_oid_element
        && event_oid_element.type() == bsoncxx::type::k_oid) { //if element exists and is type array
        event_oid = event_oid_element.get_oid().value.to_string();
    } else if(event_oid_element) { //if element does not exist or is not type array
        logElementError(
                __LINE__, __FILE__,
                event_oid_element, chat_room_account_view,
                bsoncxx::type::k_oid, chat_room_header_keys::EVENT_ID,
                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
        );

        //Can continue here.
    }

    if (member_account_oid_str == event_oid) {
        account_found = AccountFoundInChatRoom::ACCOUNT_FOUND_IN_CHAT_ROOM_EVENT;
    }

    if(account_found == AccountFoundInChatRoom::ACCOUNT_NOT_FOUND_IN_CHAT_ROOM) {
        auto accounts_in_chat_room_element = chat_room_account_view[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM];
        if (accounts_in_chat_room_element
            && accounts_in_chat_room_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
            accounts_in_chat_room = accounts_in_chat_room_element.get_array().value;
            //there should only be 1 element in this because of the projection
            for (const auto& account: accounts_in_chat_room) {
                if (account.type() == bsoncxx::type::k_document) {

                    std::string account_oid;

                    auto account_oid_element = account.get_document().view()[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID];
                    if (account_oid_element
                        && account_oid_element.type() == bsoncxx::type::k_oid) { //if element exists and is type array
                        account_oid = account_oid_element.get_oid().value.to_string();
                    } else { //if element does not exist or is not type array
                        logElementError(
                                __LINE__, __FILE__,
                                account_oid_element, chat_room_account_view,
                                bsoncxx::type::k_oid, chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                        );

                        continue;
                    }

                    if (member_account_oid_str == account_oid) {
                        account_found = AccountFoundInChatRoom::ACCOUNT_FOUND_IN_CHAT_ROOM_USER;
                        account_from_header = account.get_document().value;
                        break;
                    }
                } else {

                    const std::string error_string = "An element of ACCOUNTS_IN_CHAT_ROOM was not type document.\n";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", chat_room_collection.name().to_string(),
                            "ObjectID_used", user_account_oid,
                            "type", convertBsonTypeToString(account.type())
                    );

                    //NOTE: still continuable here
                }
            }
        }
            //The element can not exist at all if the requested user is not present inside the header accounts list.
        else if (accounts_in_chat_room_element) { //if element is not type array.
            logElementError(
                    __LINE__, __FILE__,
                    accounts_in_chat_room_element, chat_room_account_view,
                    bsoncxx::type::k_array, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
            );

            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }
    }

    if(account_found == AccountFoundInChatRoom::ACCOUNT_NOT_FOUND_IN_CHAT_ROOM) {
        //because members are never removed from a chat room they only change state, this should never be possible
        const std::string error_string = "The member existing on the client does not exist on the server.\n";

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "ObjectID_used", member_account_oid_str
        );

        buildBasicUpdateOtherUserResponse(
                response,
                AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
                member_account_oid_str,
                current_timestamp,
                -1L
        );

        return;
    }

    AccountStateInChatRoom account_state_from_chat_room;
    std::chrono::milliseconds user_last_activity_time;

    switch (account_found) {
        case ACCOUNT_NOT_FOUND_IN_CHAT_ROOM: {
            const std::string error_string = "account_found was set to an invalid valid.\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "ObjectID_used", member_account_oid_str
            );
            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }
        case ACCOUNT_FOUND_IN_CHAT_ROOM_USER: {

            auto account_state_element = account_from_header[chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM];
            if (account_state_element
                && account_state_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                account_state_from_chat_room = AccountStateInChatRoom(account_state_element.get_int32().value);
            } else { //if element does not exist or is not type int32
                logElementError(
                        __LINE__, __FILE__,
                        account_state_element, account_from_header,
                        bsoncxx::type::k_int32, chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string()
                );

                response->set_return_status(LG_ERROR);
                return;
            }

            auto user_last_activity_time_element = account_from_header[chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME];
            if (user_last_activity_time_element
                && user_last_activity_time_element.type() == bsoncxx::type::k_date) { //if element exists and is type date
                user_last_activity_time = user_last_activity_time_element.get_date().value;
            } else { //if element does not exist or is not type date
                logElementError(
                        __LINE__, __FILE__,
                        user_last_activity_time_element, account_from_header,
                        bsoncxx::type::k_date, chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME,
                        database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string()
                );

                response->set_return_status(LG_ERROR);
                return;
            }

            break;
        }
        case ACCOUNT_FOUND_IN_CHAT_ROOM_EVENT: {
            account_state_from_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_EVENT;
            user_last_activity_time = chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT;
            break;
        }
    }

    if (account_state_from_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
        || account_state_from_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
        || account_state_from_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_EVENT
    ) { //if user is inside chat room or the event

        bsoncxx::stdx::optional<bsoncxx::document::value> find_member_user_account;
        try {
            mongocxx::options::find find_user_acct_opts;

            //project only the specific element representing this user from the header
            find_user_acct_opts.projection(buildUpdateSingleOtherUserProjectionDoc());

            bsoncxx::builder::stream::document user_accounts_query;

            user_accounts_query
                    << "_id" << member_account_oid;

            if(account_state_from_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_EVENT) {
                user_accounts_query
                    << user_account_keys::ACCOUNT_TYPE << open_document
                        << "$in" << open_array
                            << UserAccountType::USER_GENERATED_EVENT_TYPE
                            << UserAccountType::ADMIN_GENERATED_EVENT_TYPE
                        << close_array
                    << close_document;
            } else if(member_account_oid != event_admin_values::OID) {
                user_accounts_query
                    << user_account_keys::CHAT_ROOMS << open_document
                        << "$elemMatch" << open_document
                            << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                        << close_document
                    << close_document;
            }

            //if account exists and is inside chat room
            find_member_user_account = user_accounts_collection.find_one(
                user_accounts_query.view(),
                find_user_acct_opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), e.what(),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used", member_account_oid_str
            );

            response->set_return_status(ReturnStatus::LG_ERROR);
            return;
        }

        if (find_member_user_account) { //if the member is found

            const bsoncxx::document::view current_user_document = find_member_user_account->view();
            auto how_to_handle_member_pictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO;

            //Set up HowToHandleMemberPictures for events based on EVENT_EXPIRATION_TIME.
            if(account_state_from_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_EVENT) {

                std::chrono::milliseconds event_expiration_time;

                auto event_expiration_time_element = current_user_document[user_account_keys::EVENT_EXPIRATION_TIME];
                if (!event_expiration_time_element
                    || event_expiration_time_element.type() != bsoncxx::type::k_date
                ) { //if element exists and is type date
                    logElementError(
                            __LINE__, __FILE__,
                            event_expiration_time_element, account_from_header,
                            bsoncxx::type::k_date, chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );

                    response->set_return_status(LG_ERROR);
                    return;
                }

                event_expiration_time = event_expiration_time_element.get_date().value;

                if(event_expiration_time == general_values::event_expiration_time_values::USER_ACCOUNT.value) {
                    const std::string error_string = "An event had an EVENT_EXPIRATION_TIME set to the USER_ACCOUNT value.\n";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection_name", chat_room_collection.name().to_string(),
                            "event_id", event_oid,
                            ""
                    );

                    //Don't want to continue here, don't know if it is an event or an account.
                    response->set_return_status(LG_ERROR);
                    return;
                }
                else if (event_expiration_time ==
                        general_values::event_expiration_time_values::EVENT_CANCELED.value
                        || current_timestamp >= event_expiration_time
                ) { //event is canceled or expired
                    how_to_handle_member_pictures = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;
                }
            }

            UpdateOtherUserResponse response_msg;

            updateSingleOtherUser(
                    mongo_cpp_client,
                    accounts_db,
                    current_user_document,
                    user_accounts_collection,
                    chat_room_collection.name().to_string(),
                    member_account_oid_str,
                    error_checked_member,
                    current_timestamp,
                    true,
                    account_state_from_chat_room,
                    user_last_activity_time,
                    how_to_handle_member_pictures,
                    &response_msg,
                    [&response, &response_msg]() {
                      response->Swap(&response_msg);
                    }
            );
        }
        else if(account_state_from_chat_room != AccountStateInChatRoom::ACCOUNT_STATE_EVENT) { //if the member is not found and not an event
            try {

                //count number of accounts (searched by _id so will be either 0 or 1)
                const bool member_user_account_found = user_accounts_collection.count_documents(
                        document{}
                                << "_id" << bsoncxx::oid{member_account_oid_str}
                             << finalize
                );

                const std::string error_string = member_user_account_found ?
                        //NOTE: This could technically happen if the user left the chat room after this RPC was sent.
                        "The user account of a member that was found on the device could not be extracted.\n" :
                        //NOTE: This could technically happen if the user was deleted after this RPC was sent.
                        "A member that was passed from the device to be updated was not found .\n";

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chat_room_collection.name().to_string(),
                        "ObjectID_used", member_account_oid_str,
                        "member_user_account_found", member_user_account_found ? "true" : "false"
                );

                buildBasicUpdateOtherUserResponse(
                        response,
                        AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
                        member_account_oid_str,
                        current_timestamp,
                        -1L
                );
            }
            catch (const mongocxx::logic_error& e) {
                //NOTE: this could technically happen if the user was deleted in after this RPC was sent
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), e.what(),
                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                        "collection", chat_room_collection.name().to_string(),
                        "ObjectID_used", member_account_oid_str
                );

                response->set_return_status(ReturnStatus::LG_ERROR);
                return;
            }
        }
    }
    else { //if user is not inside chat room
        UpdateOtherUserResponse response_msg;

        updateSingleChatRoomMemberNotInChatRoom(
                mongo_cpp_client,
                accounts_db,
                chat_room_collection,
                member_account_oid_str,
                account_from_header,
                error_checked_member,
                current_timestamp,
                true,
                account_state_from_chat_room,
                user_last_activity_time,
                &response_msg,
                [&response, &response_msg]() {
                    response->Swap(&response_msg);
                }
        );
    }

}
