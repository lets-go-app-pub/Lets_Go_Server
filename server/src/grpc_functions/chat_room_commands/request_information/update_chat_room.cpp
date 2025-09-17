//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include "extract_thumbnail_from_verified_doc.h"
#include "helper_functions/chat_room_commands_helper_functions.h"
#include "how_to_handle_member_pictures.h"
#include "utility_chat_functions.h"
#include "global_bsoncxx_docs.h"
#include "handle_function_operation_exception.h"
#include "connection_pool_global_variable.h"
#include "helper_functions/messages_to_client.h"
#include <sstream>
#include "update_single_other_user.h"

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"
#include "database_names.h"
#include "collection_names.h"
#include "user_account_keys.h"
#include "server_parameter_restrictions.h"
#include "chat_room_values.h"
#include "general_values.h"
#include "chat_room_header_keys.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "extract_data_from_bsoncxx.h"
#include "event_admin_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** Documentation for this file can be found inside [grpc_functions/chat_room_commands/request_information/_documentation.md] **/

struct MemberHeaderInfo {

    MemberHeaderInfo() = delete;

    MemberHeaderInfo(
            std::string user_account_oid_str,
            const AccountStateInChatRoom accountStateInChatRoom,
            const std::chrono::milliseconds& userLastActivityTime,
            bsoncxx::document::view header_document
    ) : user_account_oid_str(std::move(user_account_oid_str)),
        account_state_in_chat_room(accountStateInChatRoom),
        user_last_activity_time(userLastActivityTime),
        header_document(header_document) {}

    std::string user_account_oid_str;
    AccountStateInChatRoom account_state_in_chat_room;
    std::chrono::milliseconds user_last_activity_time;
    bsoncxx::document::view header_document;

    bool operator<(const MemberHeaderInfo& rhs) const {
        return user_account_oid_str < rhs.user_account_oid_str;
    }

    bool operator>(const MemberHeaderInfo& rhs) const {
        return rhs < *this;
    }

    bool operator<=(const MemberHeaderInfo& rhs) const {
        return !(rhs < *this);
    }

    bool operator>=(const MemberHeaderInfo& rhs) const {
        return !(*this < rhs);
    }
};

struct MemberUpdateRequiringDatabaseAccess {

    enum UserRequirements {
        REQUIRES_UPDATE,
        REQUIRES_ADDED,
    };

    MemberUpdateRequiringDatabaseAccess() = delete;

    MemberUpdateRequiringDatabaseAccess(
            MemberHeaderInfo& memberHeaderInfo,
            const UserRequirements&& userRequirements,
            OtherUserInfoForUpdates* memberFromClient
    ) :
            member_header_info(memberHeaderInfo),
            user_requirements(userRequirements),
            member_from_client(memberFromClient) {}

    MemberUpdateRequiringDatabaseAccess(
            MemberHeaderInfo& memberHeaderInfo,
            const UserRequirements&& userRequirements
    ) :
            member_header_info(memberHeaderInfo),
            user_requirements(userRequirements) {}

    MemberHeaderInfo member_header_info;
    UserRequirements user_requirements;
    OtherUserInfoForUpdates* member_from_client = nullptr;

    bool operator<(const MemberUpdateRequiringDatabaseAccess& rhs) const {
        return member_header_info.user_account_oid_str < rhs.member_header_info.user_account_oid_str;
    }

    bool operator>(const MemberUpdateRequiringDatabaseAccess& rhs) const {
        return rhs < *this;
    }

    bool operator<=(const MemberUpdateRequiringDatabaseAccess& rhs) const {
        return !(rhs < *this);
    }

    bool operator>=(const MemberUpdateRequiringDatabaseAccess& rhs) const {
        return !(*this < rhs);
    }

};

void updateChatRoomImplementation(
        const grpc_chat_commands::UpdateChatRoomRequest* request,
        grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* responseStream
);

void runMemberFoundOnServerNotClient(
        grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* response_stream,
        const std::string& user_account_oid_str,
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& chat_room_collection,
        grpc_chat_commands::UpdateChatRoomResponse& response,
        bsoncxx::builder::basic::array& members_to_request,
        std::vector<MemberUpdateRequiringDatabaseAccess>& member_updates_requiring_database_access,
        std::vector<MemberHeaderInfo>& server_member_info,
        size_t server_index
);

//returns true if value was saved to member_response, false if otherwise
template <bool always_write_response>
bool userExistsInListHoweverNotDatabase(
        const std::chrono::milliseconds& currentTimestamp,
        const AccountStateInChatRoom account_state_from_client,
        const std::string& account_oid_from_client,
        const MemberHeaderInfo& server_user_info,
        const std::string& requesting_user_account_oid,
        UpdateOtherUserResponse* member_response
) {

    member_response->Clear();

    if (account_oid_from_client !=
        requesting_user_account_oid) { //if member is not this user

        if(!always_write_response
            && (account_state_from_client == AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
           || account_state_from_client == AccountStateInChatRoom::ACCOUNT_STATE_BANNED)
        ) {
            //No need to write response back if the other member is already not in chat room.
            return false;
        } else {
            buildBasicUpdateOtherUserResponse(
                    member_response,
                    AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM,
                    account_oid_from_client,
                    currentTimestamp,
                    -1L
            );
        }
    } else { //if member is this user
        buildBasicUpdateOtherUserResponse(
                member_response,
                server_user_info.account_state_in_chat_room,
                requesting_user_account_oid,
                currentTimestamp,
                server_user_info.user_last_activity_time.count()
        );
    }

    return true;
}

//primary function for UpdateAllChatRoomMembersRPC, called from gRPC server implementation
void updateChatRoom(
        const grpc_chat_commands::UpdateChatRoomRequest* request,
        grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* responseStream
) {

    handleFunctionOperationException(
            [&] {
                updateChatRoomImplementation(request, responseStream);
            },
            [&] {
                grpc_chat_commands::UpdateChatRoomResponse errorResponse;
                errorResponse.set_return_status(ReturnStatus::DATABASE_DOWN);
                responseStream->Write(errorResponse);
            },
            [&] {
                grpc_chat_commands::UpdateChatRoomResponse errorResponse;
                errorResponse.set_return_status(ReturnStatus::LG_ERROR);
                responseStream->Write(errorResponse);
            }, __LINE__, __FILE__, request);
}

void updateChatRoomImplementation(
        const grpc_chat_commands::UpdateChatRoomRequest* request,
        grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* responseStream
) {

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
        grpc_chat_commands::UpdateChatRoomResponse error_response;
        error_response.set_return_status(basic_info_return_status);
        responseStream->Write(error_response);
        return;
    }

    const std::string& chat_room_id = request->chat_room_id();

    if (isInvalidChatRoomId(chat_room_id)
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < request->chat_room_name().size()
        || server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < request->chat_room_password().size()
    ) {
        grpc_chat_commands::UpdateChatRoomResponse error_response;
        error_response.set_return_status(INVALID_PARAMETER_PASSED);
        responseStream->Write(error_response);
        return;
    }

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::chrono::milliseconds passed_chat_room_last_activity_time = std::chrono::milliseconds{request->chat_room_last_activity_time()};

    if(passed_chat_room_last_activity_time > current_timestamp) {
        passed_chat_room_last_activity_time = current_timestamp;
    } else if(passed_chat_room_last_activity_time < general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP) {
        passed_chat_room_last_activity_time = std::chrono::milliseconds{-1};
    }

    std::vector<OtherUserInfoForUpdates> client_member_info = filterAndStoreListOfUsersToBeUpdated(
            request->chat_room_member_info(),
            current_timestamp
    );

    //NOTE: the event is allowed to go through filterAndStoreListOfUsersToBeUpdated() above in order to properly
    // validate the inputs.
    OtherUserInfoForUpdates event_info;
    event_info.set_account_oid(chat_room_values::EVENT_ID_DEFAULT);

    for(auto it = client_member_info.begin(); it != client_member_info.end(); it++) {
        if(it->account_oid() == request->event_oid()) {
            event_info = std::move(*it);
            client_member_info.erase(it);
            break;
        }
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

    const bsoncxx::oid user_account_oid{user_account_oid_str};

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

    if (!runInitialLoginOperation(
            find_user_account,
            user_accounts_collection,
            user_account_oid,
            login_document,
            projection_document)
    ) {
        grpc_chat_commands::UpdateChatRoomResponse error_response;
        error_response.set_return_status(LG_ERROR);
        responseStream->Write(error_response);
        return;
    }

    const ReturnStatus return_status = checkForValidLoginToken(
            find_user_account,
            user_account_oid_str
    );

    if (return_status != ReturnStatus::SUCCESS) {
        grpc_chat_commands::UpdateChatRoomResponse error_response;
        error_response.set_return_status(return_status);
        responseStream->Write(error_response);
        return;
    }

    bsoncxx::types::b_date mongo_db_date{std::chrono::milliseconds{-1}};
    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

    bool transaction_successful = false;
    bsoncxx::stdx::optional<bsoncxx::document::value> find_and_update_chat_room_account;
    const std::string user_activity_detected_uuid = generateUUID();

    mongocxx::client_session::with_transaction_cb transaction_callback = [&](mongocxx::client_session* const session) {

        grpc_chat_commands::ClientMessageToServerRequest client_message_to_server_request =
            generateUserActivityDetectedMessage(
                user_activity_detected_uuid,
                chat_room_id
            );

        //set user activity detected when user updates this
        const SendMessageToChatRoomReturn send_msg_return_val = sendMessageToChatRoom(
                &client_message_to_server_request,
                chat_room_collection,
                user_account_oid,
                session
        );

        switch (send_msg_return_val.successful) {
            case SendMessageToChatRoomReturn::SuccessfulReturn::MESSAGE_ALREADY_EXISTED: {
                const std::string error_string = "Message UUID already existed when update chat room attempted to send it.\n";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "ObjectID_used", user_account_oid,
                        "message", client_message_to_server_request.DebugString()
                );

                //This should never happen, the message uuid was generated randomly above
                session->abort_transaction();
                grpc_chat_commands::UpdateChatRoomResponse error_response;
                error_response.set_return_status(ReturnStatus::LG_ERROR);
                responseStream->Write(error_response);
                return;
            }
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_SUCCESSFUL:
                //the LAST_ACTIVITY_TIME must be updated to the kUserActivityDetectedMessage time below
                current_timestamp = send_msg_return_val.time_stored_on_server;
                mongo_db_date = bsoncxx::types::b_date{current_timestamp};
                break;
            case SendMessageToChatRoomReturn::SuccessfulReturn::SEND_MESSAGE_FAILED: {
                //error already stored
                session->abort_transaction();
                grpc_chat_commands::UpdateChatRoomResponse error_response;
                error_response.set_return_status(ReturnStatus::LG_ERROR);
                responseStream->Write(error_response);
                return;
            }
        }

        mongocxx::options::find_one_and_update extract_header_opts;

        //project OUT the specific element representing this user from the header
        extract_header_opts.projection(
            document{}
                << "_id" << 0
                << chat_room_header_keys::ID << 0
                << chat_room_header_keys::MATCHING_OID_STRINGS << 0
                << chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + '.' + chat_room_header_keys::accounts_in_chat_room::TIMES_JOINED_LEFT << 0
            << finalize
        );

        const std::string ELEM = "e";
        bsoncxx::builder::basic::array array_builder{};
        array_builder.append(
            document{}
                << ELEM + "." + chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID << user_account_oid
            << finalize
        );

        extract_header_opts.array_filters(array_builder.view());

        extract_header_opts.return_document(mongocxx::options::return_document::k_after);

        std::optional<std::string> exception_string;
        try {
            mongocxx::pipeline stages;

            const std::string ELEM_UPDATE = chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM + ".$[" + ELEM + "].";
            find_and_update_chat_room_account = chat_room_collection.find_one_and_update(
                *session,
                matchChatRoomUserInside(user_account_oid),
                document{}
                        << "$max" << open_document
                            << ELEM_UPDATE + chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME << mongo_db_date
                        << close_document
                    << finalize,
                extract_header_opts
            );
        }
        catch (const mongocxx::logic_error& e) {
            exception_string = std::string(e.what());
        }

        if(find_and_update_chat_room_account) {
            transaction_successful = true;
        }
        else {

            //NOTE: there is a small window where this could happen (see same spot in updateSingleChatRoomMemberForRPC)
            // however there is no reason to send anything back if it does

            const std::string error_string = "The current user is not a member of the passed chat room.\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    exception_string, error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "userAccountOID", user_account_oid);

            session->abort_transaction();
            grpc_chat_commands::UpdateChatRoomResponse response;
            response.mutable_user_activity_message()->set_user_exists_in_chat_room(false);
            response.mutable_user_activity_message()->set_timestamp_returned(current_timestamp.count());
            responseStream->Write(response);
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
        std::cout << "Exception calling updateChatRoomImplementation() transaction.\n" << e.what() << '\n';
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

    if (!transaction_successful) { //find and update failed (error stored above
        return;
    }

    std::optional<std::string> find_and_update_current_user_doc_exception_string;
    bsoncxx::stdx::optional<mongocxx::result::update> find_and_update_current_user_account_doc;
    try {
        mongocxx::options::update find_and_update_account_options;

        const std::string USER_ELEM = "e";
        bsoncxx::builder::basic::array find_and_update_account_array_builder{};
        find_and_update_account_array_builder.append(
            document{}
                << USER_ELEM + "." +  user_account_keys::chat_rooms::CHAT_ROOM_ID << chat_room_id
            << finalize
        );

        find_and_update_account_options.array_filters(find_and_update_account_array_builder.view());

        const std::string ELEM_UPDATE = user_account_keys::CHAT_ROOMS + ".$[" + USER_ELEM + "].";

        //if chat room exists inside user account update last time viewed
        find_and_update_current_user_account_doc = user_accounts_collection.update_one(
            document{}
                << "_id" << user_account_oid
                    << user_account_keys::CHAT_ROOMS + "." + user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
            << finalize,
            document{}
                << "$max" << open_document
                    << ELEM_UPDATE + user_account_keys::chat_rooms::LAST_TIME_VIEWED << mongo_db_date
                << close_document
            << finalize,
            find_and_update_account_options
        );

    }
    catch (const mongocxx::logic_error& e) {
        find_and_update_current_user_doc_exception_string = e.what();
    }

    if (!find_and_update_current_user_account_doc
        || find_and_update_current_user_account_doc->matched_count() == 0
            ) { //failed to find or update current user verified document

        std::string matched_account = "-1";

        if (find_and_update_current_user_account_doc) {
            matched_account = std::to_string(find_and_update_current_user_account_doc->matched_count());
        }

        const std::string error_string =
                "Failed to find or update verified account: " + user_account_oid.to_string() +
                "\nmatched_count: " + matched_account +
                "\n";
        storeMongoDBErrorAndException(
            __LINE__, __FILE__,
            find_and_update_current_user_doc_exception_string, error_string,
            "database", database_names::ACCOUNTS_DATABASE_NAME,
            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
            "ObjectID_used", user_account_oid,
            "chatRoomId", chat_room_id
        );

        //NOTE: ok to continue here
    }

    const bsoncxx::document::view chat_room_account_view = find_and_update_chat_room_account->view();
    std::string chat_room_name, chat_room_password, event_oid_str;
    std::chrono::milliseconds extracted_from_header_chat_room_last_active_time;
    double pinned_location_longitude, pinned_location_latitude;

    try {
        chat_room_name = extractFromBsoncxx_k_utf8(
                chat_room_account_view,
                chat_room_header_keys::CHAT_ROOM_NAME
        );

        chat_room_password = extractFromBsoncxx_k_utf8(
                chat_room_account_view,
                chat_room_header_keys::CHAT_ROOM_PASSWORD
        );

        extracted_from_header_chat_room_last_active_time = extractFromBsoncxx_k_date(
                chat_room_account_view,
                chat_room_header_keys::CHAT_ROOM_LAST_ACTIVE_TIME
        ).value;

        const auto event_id_element = chat_room_account_view[chat_room_header_keys::EVENT_ID];
        if (event_id_element && event_id_element.type() == bsoncxx::type::k_oid) {
            event_oid_str = event_id_element.get_oid().value.to_string();
        }
        else if(!event_id_element) { //if element does not exist
            event_oid_str = chat_room_values::EVENT_ID_DEFAULT;
        }
        else { //if element exists is not type oid
            logElementError(
                    __LINE__, __FILE__,
                    event_id_element, chat_room_account_view,
                    bsoncxx::type::k_oid, chat_room_header_keys::EVENT_ID,
                    database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string()
            );

            grpc_chat_commands::UpdateChatRoomResponse error_response;
            error_response.set_return_status(ReturnStatus::LG_ERROR);
            responseStream->Write(error_response);
            return;
        }

        const auto pinned_location_element = chat_room_account_view[chat_room_header_keys::PINNED_LOCATION];
        if (pinned_location_element && pinned_location_element.type() == bsoncxx::type::k_document) {
            const bsoncxx::document::view pinned_location_document = pinned_location_element.get_document().view();

            pinned_location_longitude = extractFromBsoncxx_k_double(
                    pinned_location_document,
                    chat_room_header_keys::pinned_location::LONGITUDE
            );

            pinned_location_latitude = extractFromBsoncxx_k_double(
                    pinned_location_document,
                    chat_room_header_keys::pinned_location::LATITUDE
            );
        }
        else if(!pinned_location_element) { //if element does not exist
            pinned_location_longitude = chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE;
            pinned_location_latitude = chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE;
        }
        else { //if element is not type document
            logElementError(
                    __LINE__, __FILE__,
                    pinned_location_element, chat_room_account_view,
                    bsoncxx::type::k_document, chat_room_header_keys::PINNED_LOCATION,
                    database_names::CHAT_ROOMS_DATABASE_NAME, chat_room_collection.name().to_string()
            );

            grpc_chat_commands::UpdateChatRoomResponse error_response;
            error_response.set_return_status(ReturnStatus::LG_ERROR);
            responseStream->Write(error_response);
            return;
        }

    } catch (const ErrorExtractingFromBsoncxx& e) {
        //Error already stored
        grpc_chat_commands::UpdateChatRoomResponse error_response;
        error_response.set_return_status(ReturnStatus::LG_ERROR);
        responseStream->Write(error_response);
        return;
    }

    //if name, password or last active time require an update, send back all 3
    if(request->chat_room_name() != chat_room_name
        || request->chat_room_password() != chat_room_password
        || request->pinned_location_longitude() != pinned_location_longitude
        || request->pinned_location_latitude() != pinned_location_latitude
        || request->event_oid() != event_oid_str
        || passed_chat_room_last_activity_time < extracted_from_header_chat_room_last_active_time
            ) {
        grpc_chat_commands::UpdateChatRoomResponse update_chat_room_response;
        update_chat_room_response.mutable_chat_room_info()->set_chat_room_name(std::move(chat_room_name));
        update_chat_room_response.mutable_chat_room_info()->set_chat_room_password(std::move(chat_room_password));
        update_chat_room_response.mutable_chat_room_info()->set_chat_room_last_activity_time(extracted_from_header_chat_room_last_active_time.count());
        update_chat_room_response.mutable_chat_room_info()->set_pinned_location_longitude(pinned_location_longitude);
        update_chat_room_response.mutable_chat_room_info()->set_pinned_location_latitude(pinned_location_latitude);
        update_chat_room_response.mutable_chat_room_info()->set_event_oid(event_oid_str);
        responseStream->Write(update_chat_room_response);
    }

    {
        //request messages
        const bsoncxx::document::value dummy_document = document{}
                << "Dummy" << "Dummy info from update_chat_room"
                << finalize;

        //NOTE: updateChatRoom() does not request back into the past like extractChatRooms() does when
        // streamInitializationMessagesToClient() is called. This is because it would require the device to do
        // a fairly expensive database call in order to extract recent message uuids. One is added so that
        // it will not request a direct duplicate of the previous message (it uses $gte). Missed messages (such
        // as potential duplicate timestamps) should be handled when the chat stream initializes.
        std::chrono::milliseconds chat_room_last_updated_time = std::chrono::milliseconds{
                request->chat_room_last_updated_time()} + std::chrono::milliseconds{1};

        if (chat_room_last_updated_time > current_timestamp) {
            chat_room_last_updated_time = current_timestamp;
        } else if (chat_room_last_updated_time < general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP) {
            chat_room_last_updated_time = std::chrono::milliseconds{-1};
        }

        StoreAndSendMessagesToUpdateChatRoom send_messages_object(responseStream);

        bsoncxx::builder::basic::array messages_to_exclude_builder;
        messages_to_exclude_builder.append(user_activity_detected_uuid);

        //It is possible for new messages to be sent back if a message has been stored by the chat stream since the
        // update time was passed to the client.
        streamInitializationMessagesToClient(
                mongo_cpp_client,
                accounts_db,
                chat_room_collection,
                user_accounts_collection,
                dummy_document.view(),
                chat_room_id,
                user_account_oid_str,
                &send_messages_object,
                AmountOfMessage::ONLY_SKELETON,
                DifferentUserJoinedChatRoomAmount::SKELETON,
                false,
                false,
                false,
                chat_room_last_updated_time, //can read note on variable for more info
                messages_to_exclude_builder
        );

        send_messages_object.finalCleanup();
    }

    bsoncxx::array::view accounts_in_chat_room;

    const auto accounts_in_chat_room_element = chat_room_account_view[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM];
    if (accounts_in_chat_room_element
        && accounts_in_chat_room_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
        accounts_in_chat_room = accounts_in_chat_room_element.get_array().value;
    } else { //if element does not exist or is not type array
        logElementError(
            __LINE__, __FILE__,
            accounts_in_chat_room_element, chat_room_account_view,
            bsoncxx::type::k_bool, chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
        );

        grpc_chat_commands::UpdateChatRoomResponse error_response;
        error_response.set_return_status(ReturnStatus::LG_ERROR);
        responseStream->Write(error_response);
        return;
    }

    //NOTE: This must be sent in case no updates were performed, also it makes it so client
    // doesn't have to update time values each time an update is sent back
    grpc_chat_commands::UpdateChatRoomResponse response;
    response.mutable_user_activity_message()->set_user_exists_in_chat_room(true);
    response.mutable_user_activity_message()->set_timestamp_returned(current_timestamp.count());
    responseStream->Write(response);

    std::vector<MemberHeaderInfo> server_member_info;

    for (const auto& account : accounts_in_chat_room) {
        if (account.type() == bsoncxx::type::k_document) {

            bsoncxx::document::view accountDoc = account.get_document().value;

            std::string member_oid;
            AccountStateInChatRoom account_state_from_chat_room;
            std::chrono::milliseconds user_last_activity_time;

            const auto account_oid_element = accountDoc[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID];
            if (account_oid_element &&
                account_oid_element.type() == bsoncxx::type::k_oid) { //if element exists and is type array
                member_oid = account_oid_element.get_oid().value.to_string();
            } else { //if element does not exist or is not type array
                logElementError(
                    __LINE__, __FILE__,
                    account_oid_element, accountDoc,
                    bsoncxx::type::k_oid, chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                );

                continue;
            }

            auto accountStateElement = accountDoc[chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM];
            if (accountStateElement &&
                accountStateElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
                account_state_from_chat_room = AccountStateInChatRoom(accountStateElement.get_int32().value);
            } else { //if element does not exist or is not type int32
                logElementError(
                    __LINE__, __FILE__,
                    accountStateElement, accountDoc,
                    bsoncxx::type::k_int32, chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                );

                continue;
            }

            auto userLastActivityTimeElement = accountDoc[chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME];
            if (userLastActivityTimeElement &&
                userLastActivityTimeElement.type() ==
                bsoncxx::type::k_date) { //if element exists and is type date
                user_last_activity_time = userLastActivityTimeElement.get_date().value;
            } else { //if element does not exist or is not type date
                logElementError(
                    __LINE__, __FILE__,
                    userLastActivityTimeElement, accountDoc,
                    bsoncxx::type::k_date, chat_room_header_keys::accounts_in_chat_room::LAST_ACTIVITY_TIME,
                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                );

                continue;
            }

            server_member_info.emplace_back(
                    member_oid,
                    account_state_from_chat_room,
                    user_last_activity_time,
                    accountDoc
            );

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

    std::sort(client_member_info.begin(), client_member_info.end(),
              [](const OtherUserInfoForUpdates& lhs,
                 const OtherUserInfoForUpdates& rhs) -> bool {
                  return lhs.account_oid() < rhs.account_oid();
              });

    std::sort(server_member_info.begin(), server_member_info.end());

    bsoncxx::builder::basic::array members_to_request;
    std::vector<MemberUpdateRequiringDatabaseAccess> member_updates_requiring_database_access;
    std::unique_ptr<MemberHeaderInfo> event_member_info = nullptr;

    //If this is an event chat room, want to request the event account.
    if(event_oid_str != chat_room_values::EVENT_ID_DEFAULT
        && !isInvalidOIDString(event_oid_str)) {
        members_to_request.append(bsoncxx::oid{event_oid_str});
        event_member_info = std::make_unique<MemberHeaderInfo>(
                        event_oid_str,
                        AccountStateInChatRoom::ACCOUNT_STATE_EVENT,
                        std::chrono::milliseconds{-1},
                        accounts_in_chat_room
                );
        member_updates_requiring_database_access.emplace_back(
                *event_member_info,
                MemberUpdateRequiringDatabaseAccess::UserRequirements::REQUIRES_UPDATE,
                &event_info
        );
    }

    size_t client_index = 0, server_index = 0;
    while (client_index < client_member_info.size() && server_index < server_member_info.size()) {
        if (client_member_info[client_index].account_oid() ==
            server_member_info[server_index].user_account_oid_str) { //member exists on both client and server

            if (server_member_info[server_index].user_account_oid_str !=
                user_account_oid_str) { //if member is not this user

                if (server_member_info[server_index].account_state_in_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                    || server_member_info[server_index].account_state_in_chat_room == AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN
                ) { //user exists in chat room

                    //will need to extract info from database and update user
                    members_to_request.append(
                            bsoncxx::oid{server_member_info[server_index].user_account_oid_str});
                    member_updates_requiring_database_access.emplace_back(
                            server_member_info[server_index],
                            MemberUpdateRequiringDatabaseAccess::UserRequirements::REQUIRES_UPDATE,
                            &client_member_info[client_index]
                    );
                }
                else { //user not in chat room

                    grpc_chat_commands::UpdateChatRoomResponse single_update_response;

                    updateSingleChatRoomMemberNotInChatRoom(
                            mongo_cpp_client,
                            accounts_db,
                            chat_room_collection,
                            server_member_info[server_index].user_account_oid_str,
                            server_member_info[server_index].header_document,
                            client_member_info[client_index],
                            current_timestamp, false,
                            server_member_info[server_index].account_state_in_chat_room,
                            server_member_info[server_index].user_last_activity_time,
                            single_update_response.mutable_member_response(),
                            [&responseStream, &single_update_response]() {
                                responseStream->Write(single_update_response);
                            }
                    );
                }
            }
            else { //if member is this user

                //only check account state, other info is handled on login
                if (server_member_info[server_index].account_state_in_chat_room !=
                    client_member_info[client_index].account_state()) {

                    buildBasicUpdateOtherUserResponse(
                            response.mutable_member_response(),
                            server_member_info[server_index].account_state_in_chat_room,
                            user_account_oid_str,
                            current_timestamp,
                            server_member_info[server_index].user_last_activity_time.count()
                    );

                    responseStream->Write(response);
                }

            }

            client_index++;
            server_index++;
        }
        else if (client_member_info[client_index].account_oid() <
                   server_member_info[server_index].user_account_oid_str) { //a member was found on the client that was not on the server

            //Because members are never removed from a chat room they only change state, this should never be possible.
            const std::string error_string = "An element existing on the client does not exist on the server.\n";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", chat_room_collection.name().to_string(),
                    "current_user_oid", user_account_oid,
                    "passed_oid", client_member_info[client_index].account_oid()
            );

            const bool successfully_saved = userExistsInListHoweverNotDatabase<false>(
                    current_timestamp,
                    client_member_info[client_index].account_state(),
                    client_member_info[client_index].account_oid(),
                    server_member_info[server_index],
                    user_account_oid_str,
                    response.mutable_member_response()
            );

            if(successfully_saved) {
                responseStream->Write(response);
            }

            //NOTE: continuing here will not hurt anything

            client_index++;
        }
        else { //a member was found on the server that was not on the client

            runMemberFoundOnServerNotClient(
                    responseStream,
                    user_account_oid_str,
                    current_timestamp,
                    mongo_cpp_client,
                    accounts_db,
                    chat_room_collection,
                    response,
                    members_to_request,
                    member_updates_requiring_database_access,
                    server_member_info,
                    server_index
            );

            server_index++;
        }
    }

    while (client_index <
           client_member_info.size()) { //a member was found on the client that was not on the server

        //because members are never removed from a chat room they only change state, this should never be possible
        const std::string error_string = "An element existing on the client does not exist on the server.\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "current_user_oid", user_account_oid,
                "passed_oid", client_member_info[client_index].account_oid()
        );

        const bool successfully_saved = userExistsInListHoweverNotDatabase<false>(
                current_timestamp,
                client_member_info[client_index].account_state(),
                client_member_info[client_index].account_oid(),
                server_member_info[server_index],
                user_account_oid_str,
                response.mutable_member_response()
        );

        if(successfully_saved) {
            responseStream->Write(response);
        }

        //NOTE: continuing here will not hurt anything

        client_index++;
    }

    while (server_index <
           server_member_info.size()) { //a member was found on the server that was not on the client
        //send back full user info
        runMemberFoundOnServerNotClient(
                responseStream,
                user_account_oid_str,
                current_timestamp,
                mongo_cpp_client,
                accounts_db,
                chat_room_collection,
                response,
                members_to_request,
                member_updates_requiring_database_access,
                server_member_info,
                server_index
        );

        server_index++;
    }

    bsoncxx::array::view members_to_request_view = members_to_request.view();

    auto how_to_handle_member_pictures = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;

    if (!members_to_request_view.empty()) { //if the chat room has members to request

        mongocxx::options::find find_members_options;

        //important for comparison in algorithm below
        find_members_options.sort(
            document{}
                << "_id" << 1
            << finalize
        );

        mongocxx::stdx::optional<mongocxx::cursor> find_members_cursor;
        try {

            //project only the specific element representing this user from the header
            find_members_options.projection(buildUpdateSingleOtherUserProjectionDoc());

            find_members_cursor = user_accounts_collection.find(
                document{}
                    << "_id" << open_document
                        << "$in" << members_to_request_view
                    << close_document
                    << "$or" << open_array
                        //User account is inside this chat room.
                        << open_document
                            << user_account_keys::CHAT_ROOMS << open_document
                                << "$elemMatch" << open_document
                                    << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                                << close_document
                            << close_document
                        << close_document
                        //User account is the event admin.
                        << open_document
                            << "_id" << event_admin_values::OID
                        << close_document
                        //User account is the event for this chat room.
                        << open_document
                            << user_account_keys::EVENT_VALUES + "." + user_account_keys::event_values::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                        << close_document
                    << close_array
                << finalize,
                find_members_options
            );
        }
        catch (const mongocxx::logic_error& e) {
            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), e.what(),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                "ObjectID_used", user_account_oid,
                "chatRoomId", chat_room_id
            );

            grpc_chat_commands::UpdateChatRoomResponse error_response;
            error_response.set_return_status(ReturnStatus::LG_ERROR);
            responseStream->Write(error_response);
            return;
        }

        if (!find_members_cursor) {
            const std::string error_string = "Cursor was not set after calling a mongoDB find operation.\n";
            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", chat_room_collection.name().to_string(),
                "ObjectID_used", user_account_oid,
                "users_oid_list", makePrettyJson(members_to_request_view)
            );

            grpc_chat_commands::UpdateChatRoomResponse error_response;
            error_response.set_return_status(ReturnStatus::LG_ERROR);
            responseStream->Write(error_response);
            return;
        }

        std::sort(member_updates_requiring_database_access.begin(),
                  member_updates_requiring_database_access.end());

        if (member_updates_requiring_database_access.size() >
            chat_room_values::MAXIMUM_NUMBER_USERS_IN_CHAT_ROOM_TO_REQUEST_ALL_INFO) {
            how_to_handle_member_pictures = HowToHandleMemberPictures::REQUEST_ONLY_THUMBNAIL;
        } else {
            how_to_handle_member_pictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO;
        }

        //the cursor and vector are both sorted here
        auto cursor_iterator = find_members_cursor->begin();
        size_t members_requiring_updates_size = 0;
        while (members_requiring_updates_size < member_updates_requiring_database_access.size() &&
               cursor_iterator != find_members_cursor->end()) {

            bsoncxx::document::view current_user_document = *cursor_iterator;
            std::string user_oid_from_database;

            auto userIdElement = current_user_document["_id"];
            if (userIdElement
                && userIdElement.type() == bsoncxx::type::k_oid) { //if element exists and is type array
                user_oid_from_database = userIdElement.get_oid().value.to_string();
            } else { //if element does not exist or is not type array
                logElementError(
                    __LINE__, __FILE__,
                    userIdElement, current_user_document,
                    bsoncxx::type::k_oid, "_id",
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );

                continue;
            }

            if (user_oid_from_database ==
                member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_account_oid_str
                    ) { //user oid matches database oid

                if(event_oid_str == user_oid_from_database) { //the event from the database

                    auto how_to_handle_event_pictures = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;

                    const std::chrono::milliseconds event_expiration_time =
                            extractFromBsoncxx_k_date(
                                    current_user_document,
                                    user_account_keys::EVENT_EXPIRATION_TIME
                            ).value;

                    if(event_expiration_time == general_values::event_expiration_time_values::USER_ACCOUNT.value) {
                        const std::string error_string = "An event had an EVENT_EXPIRATION_TIME set to the USER_ACCOUNT value.\n";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection_name", chat_room_collection.name().to_string(),
                                "event_user_account_doc", current_user_document
                        );

                        //Don't send back user account values if this point is reached.
                        continue;
                    }
                    else {

                        if (event_expiration_time ==
                            general_values::event_expiration_time_values::EVENT_CANCELED.value
                            || current_timestamp >= event_expiration_time
                        ) { //event is canceled or expired
                            how_to_handle_event_pictures = HowToHandleMemberPictures::REQUEST_NO_PICTURE_INFO;

                            //update client to expired
                            if(request->qr_code_last_timestamp() != chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT) {
                                grpc_chat_commands::UpdateChatRoomResponse qr_code_response;

                                qr_code_response.mutable_qr_code()->set_qr_code_image_bytes(chat_room_values::QR_CODE_DEFAULT);
                                qr_code_response.mutable_qr_code()->set_qr_code_message(chat_room_values::QR_CODE_MESSAGE_DEFAULT);
                                qr_code_response.mutable_qr_code()->set_qr_code_time_updated(chat_room_values::QR_CODE_TIME_UPDATED_DEFAULT);

                                responseStream->Write(qr_code_response);
                            }

                        } else { //event is ongoing
                            how_to_handle_event_pictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO;

                            const auto qr_code_element = chat_room_account_view[chat_room_header_keys::QR_CODE];
                            const auto qr_code_message_element = chat_room_account_view[chat_room_header_keys::QR_CODE_MESSAGE];
                            const auto qr_code_time_updated_element = chat_room_account_view[chat_room_header_keys::QR_CODE_TIME_UPDATED];
                            if (qr_code_element && qr_code_element.type() == bsoncxx::type::k_utf8
                                && qr_code_message_element && qr_code_message_element.type() == bsoncxx::type::k_utf8
                                && qr_code_time_updated_element && qr_code_time_updated_element.type() == bsoncxx::type::k_date) {

                                const long qr_code_timestamp = qr_code_time_updated_element.get_date().value.count();

                                //Qr code requires updated.
                                if(request->qr_code_last_timestamp() != qr_code_timestamp) {
                                    grpc_chat_commands::UpdateChatRoomResponse qr_code_response;

                                    qr_code_response.mutable_qr_code()->set_qr_code_image_bytes(qr_code_element.get_string().value.to_string());
                                    qr_code_response.mutable_qr_code()->set_qr_code_message(qr_code_message_element.get_string().value.to_string());
                                    qr_code_response.mutable_qr_code()->set_qr_code_time_updated(qr_code_timestamp);

                                    responseStream->Write(qr_code_response);
                                }
                            }
                            else if(qr_code_element || qr_code_message_element || qr_code_time_updated_element) { //if element(s) are wrong type
                                const std::string error_string = "An element was the wrong type inside the chat room header.\n";
                                storeMongoDBErrorAndException(
                                        __LINE__, __FILE__,
                                        std::optional<std::string>(), error_string,
                                        "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                        "collection", chat_room_collection.name().to_string(),
                                        "user_account_oid", user_account_oid,
                                        "event_oid_str", event_oid_str,
                                        "chat_room_header_doc", chat_room_account_view
                                );

                                //NOTE: Ok to continue here, will miss Qr Code.
                            }
                        }
                    }

                    if(event_oid_str == request->event_oid()) { //update event
                        grpc_chat_commands::UpdateChatRoomResponse responseMessage;

                        updateSingleOtherUser(
                                mongo_cpp_client,
                                accounts_db,
                                current_user_document,
                                user_accounts_collection,
                                chat_room_collection.name().to_string(),
                                event_oid_str,
                                event_info,
                                current_timestamp,
                                false,
                                AccountStateInChatRoom::ACCOUNT_STATE_EVENT,
                                chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT,
                                how_to_handle_event_pictures,
                                responseMessage.mutable_event_response(),
                                [&responseStream, &responseMessage]() {
                                    responseStream->Write(responseMessage);
                                }
                        );
                    }
                    else { //add event
                        const std::string error_string = "An event oid was added to a user when updating a chat room."
                                                         " This should never be possible because events are added when the"
                                                         " chat room is created.\n";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection", chat_room_collection.name().to_string(),
                                "ObjectID_used", user_account_oid,
                                "event_oid_str", event_oid_str
                        );

                        grpc_chat_commands::UpdateChatRoomResponse stream_response;
                        UpdateOtherUserResponse* stream_response_for_member = stream_response.mutable_event_response();

                        //Setup stream_response_for_member with the guaranteed values to the client.
                        buildBasicUpdateOtherUserResponse(
                                stream_response_for_member,
                                AccountStateInChatRoom::ACCOUNT_STATE_EVENT,
                                event_oid_str,
                                current_timestamp,
                                chat_room_values::EVENT_USER_LAST_ACTIVITY_TIME_DEFAULT.count()
                        );

                        if (!saveUserInfoToMemberSharedInfoMessage(
                                mongo_cpp_client,
                                accounts_db,
                                user_accounts_collection,
                                current_user_document,
                                bsoncxx::oid{event_oid_str},
                                stream_response_for_member->mutable_user_info(),
                                how_to_handle_event_pictures,
                                current_timestamp)
                                ) {
                            continue;
                        }

                        responseStream->Write(stream_response);
                    }
                } else { //a user from the database

                    if (member_updates_requiring_database_access[members_requiring_updates_size].user_requirements ==
                        MemberUpdateRequiringDatabaseAccess::UserRequirements::REQUIRES_UPDATE) {

                        grpc_chat_commands::UpdateChatRoomResponse responseMessage;

                        updateSingleOtherUser(
                                mongo_cpp_client,
                                accounts_db,
                                current_user_document,
                                user_accounts_collection,
                                chat_room_collection.name().to_string(),
                                user_oid_from_database,
                                *member_updates_requiring_database_access[members_requiring_updates_size].member_from_client,
                                current_timestamp,
                                false,
                                member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.account_state_in_chat_room,
                                member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_last_activity_time,
                                how_to_handle_member_pictures,
                                responseMessage.mutable_member_response(),
                                [&responseStream, &responseMessage]() {
                                    responseStream->Write(responseMessage);
                                }
                        );

                    } else { //REQUIRES_ADDED

                        grpc_chat_commands::UpdateChatRoomResponse streamResponse;
                        UpdateOtherUserResponse* streamResponseForMember = streamResponse.mutable_member_response();

                        //Setup streamResponseForMember with the guaranteed values to the client.
                        buildBasicUpdateOtherUserResponse(
                                streamResponseForMember,
                                member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.account_state_in_chat_room,
                                user_oid_from_database,
                                current_timestamp,
                                member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_last_activity_time.count()
                        );

                        if (!saveUserInfoToMemberSharedInfoMessage(
                                mongo_cpp_client,
                                accounts_db,
                                user_accounts_collection,
                                current_user_document,
                                bsoncxx::oid{user_oid_from_database},
                                streamResponseForMember->mutable_user_info(),
                                how_to_handle_member_pictures,
                                current_timestamp)
                                ) {
                            continue;
                        }

                        responseStream->Write(streamResponse);
                    }
                }

                members_requiring_updates_size++;
                cursor_iterator++;
            }
            else if (user_oid_from_database <
                       member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_account_oid_str
                    ) { //value exists in database that does not exist inside requested user list

                //not sure how this could ever happen (this case must be here to increment the iterator)
                const std::string error_string = "A database find() was called on an array of oid and returned a value that was in in the array.\n";

                storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "ObjectID_used",
                    member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_account_oid_str,
                    "user_oid_from_database", user_oid_from_database
                );

                cursor_iterator++;
            }
            else { //value exists in header that was not found in user accounts collection

                userExistsInListHoweverNotDatabase<true>(
                        current_timestamp,
                        member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.account_state_in_chat_room,
                        member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_account_oid_str,
                        member_updates_requiring_database_access[members_requiring_updates_size].member_header_info,
                        user_account_oid_str,
                        response.mutable_member_response()
                );

                responseStream->Write(response);

                members_requiring_updates_size++;
            }
        }

        while (members_requiring_updates_size < member_updates_requiring_database_access.size()
                ) {  //value exists in header that was not found in user accounts collection

            userExistsInListHoweverNotDatabase<true>(
                    current_timestamp,
                    member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.account_state_in_chat_room,
                    member_updates_requiring_database_access[members_requiring_updates_size].member_header_info.user_account_oid_str,
                    member_updates_requiring_database_access[members_requiring_updates_size].member_header_info,
                    user_account_oid_str,
                    response.mutable_member_response()
            );

            responseStream->Write(response);

            members_requiring_updates_size++;
        }

        while (cursor_iterator != find_members_cursor->end()
                ) { //value exists in database that does not exist inside requested user list

            bsoncxx::document::view current_user_document = *cursor_iterator;
            std::string user_oid_from_database;

            auto userIdElement = current_user_document["_id"];
            if (userIdElement &&
                userIdElement.type() == bsoncxx::type::k_oid) { //if element exists and is type array
                user_oid_from_database = userIdElement.get_oid().value.to_string();
            } else { //if element does not exist or is not type array
                logElementError(
                    __LINE__, __FILE__,
                    userIdElement, current_user_document,
                    bsoncxx::type::k_oid, "_id",
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );

                continue;
            }

            //not sure how this could ever happen (this case must be here to increment the iterator)
            const std::string error_string = "A database find() was called on an array of oid and returned a value that"
                                            " was in in the array.\n";

            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_oid_from_database", user_oid_from_database
            );
            cursor_iterator++;
        }
    }

    grpc_chat_commands::UpdateChatRoomResponse final_message;
    final_message.mutable_response_addendum()->set_current_timestamp(current_timestamp.count());
    final_message.mutable_response_addendum()->set_pictures_checked_for_updates(
            how_to_handle_member_pictures == HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO);
    responseStream->Write(final_message);

}

void runMemberFoundOnServerNotClient(
        grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* response_stream,
        const std::string& user_account_oid_str,
        const std::chrono::milliseconds& current_timestamp,
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::collection& chat_room_collection,
        grpc_chat_commands::UpdateChatRoomResponse& response,
        bsoncxx::builder::basic::array& members_to_request,
        std::vector<MemberUpdateRequiringDatabaseAccess>& member_updates_requiring_database_access,
        std::vector<MemberHeaderInfo>& server_member_info,
        size_t server_index
) {

    if (server_member_info[server_index].user_account_oid_str ==
        user_account_oid_str) { //if member is this user

        //NOTE: this means that
        // 1) user connected to chat room and ran chat room update
        // 2) chat room update FOUND the chat room ID inside the user account
        // 3) the user was not found inside the chat room (not even inside the ACCOUNT_STATE_NOT_IN_CHAT_ROOM state)
        // 4) this was called
        // OR that the client forgot to pass it as part of the chat_room_member_info() field

        const std::string error_string = "User was found inside server list however NOT inside client list.\n";

        std::stringstream server_member_info_string_stream;
        for(size_t i = 0; i < server_member_info.size(); i++) {
            server_member_info_string_stream
                << i << ": " << server_member_info[i].user_account_oid_str << '\n';
        }

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "user_account_oid", user_account_oid_str,
                "server_member_info", server_member_info_string_stream.str()
        );

        buildBasicUpdateOtherUserResponse(
                response.mutable_member_response(),
                server_member_info[server_index].account_state_in_chat_room,
                user_account_oid_str,
                current_timestamp,
                server_member_info[server_index].user_last_activity_time.count()
        );

        response_stream->Write(response);
        return;
    }

    if (server_member_info[server_index].account_state_in_chat_room ==
        ACCOUNT_STATE_IN_CHAT_ROOM
        || server_member_info[server_index].account_state_in_chat_room ==
           ACCOUNT_STATE_IS_ADMIN
            ) { //user exists in chat room

        //will need to extract info from database and add user
        members_to_request.append(bsoncxx::oid{server_member_info[server_index].user_account_oid_str});
        member_updates_requiring_database_access.emplace_back(
                server_member_info[server_index],
                MemberUpdateRequiringDatabaseAccess::REQUIRES_ADDED
        );

    } else { //user not in chat room

        grpc_chat_commands::UpdateChatRoomResponse single_update_response;

        //this will force all info to be sent back
        OtherUserInfoForUpdates dummy_client_info;
        dummy_client_info.set_account_state(AccountStateInChatRoom(-1));

        updateSingleChatRoomMemberNotInChatRoom(
                mongo_cpp_client,
                accounts_db,
                chat_room_collection,
                server_member_info[server_index].user_account_oid_str,
                server_member_info[server_index].header_document,
                dummy_client_info,
                current_timestamp, false,
                server_member_info[server_index].account_state_in_chat_room,
                server_member_info[server_index].user_last_activity_time,
                single_update_response.mutable_member_response(),
                [&response_stream, &single_update_response]() {
                    response_stream->Write(single_update_response);
                }
        );
    }
}


