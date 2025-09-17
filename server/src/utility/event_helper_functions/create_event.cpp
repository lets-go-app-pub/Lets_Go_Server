//
// Created by jeremiah on 3/11/23.
//

#include "create_event.h"
#include "event_request_message_is_valid.h"

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <utility>

#include <admin_privileges_vector.h>
#include <utility_chat_functions.h>
#include <admin_functions_for_request_values.h>

#include "utility_general_functions.h"
#include "extract_data_from_bsoncxx.h"
#include "admin_account_keys.h"
#include "global_bsoncxx_docs.h"
#include "matching_algorithm.h"
#include "create_user_account.h"
#include "create_chat_room_helper.h"
#include "event_admin_values.h"
#include "create_complete_chat_room_with_user.h"
#include "run_initial_login_operation.h"
#include "get_login_document.h"
#include "chat_room_commands.h"
#include "thread_pool_global_variable.h"
#include "chat_room_commands_helper_functions.h"
#include "create_initial_chat_room_messages.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

CreateEventReturnValues createEvent(
        mongocxx::client& mongo_cpp_client,
        mongocxx::database& accounts_db,
        mongocxx::database& chat_room_db,
        mongocxx::collection& user_accounts_collection,
        mongocxx::collection& user_pictures_collection,
        const std::chrono::milliseconds& current_timestamp,
        const std::string& created_by,
        const EventChatRoomAdminInfo& chat_room_admin_info,
        EventRequestMessage* filtered_event_info,
        EventRequestMessageIsValidReturnValues& event_request_returns
        ) {

    CreateEventReturnValues return_values;
    EventValues event_values;

    //Cannot set chat_room_id yet because it has not yet been created.
    event_values.set_created_by(created_by);
    event_values.set_event_title(filtered_event_info->event_title());

    bsoncxx::builder::basic::array genders_range;

    if (event_request_returns.user_matches_with_everyone) {
        genders_range.append(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    } else {
        for (const auto& gender: event_request_returns.gender_range_vector) {
            genders_range.append(gender);
        }
    }

    bsoncxx::builder::basic::array timeframes_array;

    timeframes_array.append(
            document{}
                    << user_account_keys::categories::timeframes::TIME << event_request_returns.event_start_time.count()
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << 1
            << finalize
    );

    timeframes_array.append(
            document{}
                    << user_account_keys::categories::timeframes::TIME << event_request_returns.event_stop_time.count()
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << -1
            << finalize
    );

    bsoncxx::builder::basic::array categories_array;

    categories_array.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << filtered_event_info->activity().activity_index()
                    << user_account_keys::categories::TIMEFRAMES << timeframes_array
            << finalize
    );

    categories_array.append(
            document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << event_request_returns.category_index
                    << user_account_keys::categories::TIMEFRAMES << timeframes_array
            << finalize
    );

    const GenerateNewChatRoomTimes chat_room_times(current_timestamp);
    std::string chat_room_id;

    //NOTE: This can 'waste' chat room Ids if the user is not if the process fails. However, the document
    // used to store the chat room number can be a bottleneck, so don't want it to be part of the transaction. Also
    // don't want it to re-run if the transaction fails.
    if (!generateChatRoomId(
            chat_room_db,
            chat_room_id)
            ) {
        //NOTE: Error already stored.
        return_values.setError("Failed to generate chat room id.");
        return return_values;
    }

    bool transaction_successful = true;

    std::shared_ptr<bsoncxx::builder::stream::document> different_user_joined_chat_room_message_doc = nullptr;
    std::string different_user_joined_message_uuid;
    mongocxx::client_session::with_transaction_cb transaction_callback = [&](
            mongocxx::client_session* callback_session) {

        const bsoncxx::oid event_account_oid;
        return_values.event_account_oid = event_account_oid.to_string();

        //This will match the algorithm, because the earliest match time for the algorithm will be
        // current_time + TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED. This means that when it gets within
        // TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED, it will ignore the timeframe of the event which we don't
        // want. Instead, making the EVENT_EXPIRATION_TIME dependent on it will make it so the algorithm can't find it
        // after this point is reached.
        const bsoncxx::types::b_date event_expiration_time{
                event_request_returns.event_stop_time - matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED
        };

        bsoncxx::oid chat_room_admin_oid = event_admin_values::OID;
        bsoncxx::stdx::optional<bsoncxx::document::value> event_admin_doc;
        if(chat_room_admin_info.event_type == UserAccountType::USER_GENERATED_EVENT_TYPE) {

            const bsoncxx::document::value merge_document = document{}
                << user_account_keys::USER_CREATED_EVENTS << open_document
                    << "$concatArrays" << open_array
                        << "$" + user_account_keys::USER_CREATED_EVENTS
                        << open_array
                            << open_document
                                << user_account_keys::user_created_events::EVENT_OID << event_account_oid
                                << user_account_keys::user_created_events::EXPIRATION_TIME << event_expiration_time
                                << user_account_keys::user_created_events::EVENT_STATE << LetsGoEventStatus::ONGOING
                            << close_document
                        << close_array
                    << close_array
                << close_document
                << user_account_keys::CHAT_ROOMS << open_document
                    << "$concatArrays" << open_array
                        << "$" + user_account_keys::CHAT_ROOMS
                        << open_array
                            << open_document
                                << user_account_keys::chat_rooms::CHAT_ROOM_ID << bsoncxx::types::b_string{chat_room_id}
                                << user_account_keys::chat_rooms::LAST_TIME_VIEWED << bsoncxx::types::b_date{chat_room_times.chat_room_last_active_time}
                                << user_account_keys::chat_rooms::EVENT_OID << event_account_oid
                            << close_document
                        << close_array
                    << close_array
                << close_document
            << finalize;

            //project pictures key to extract thumbnail
            const bsoncxx::document::value projection_document = document{}
                        << user_account_keys::LOGGED_IN_RETURN_MESSAGE << 1
                        << user_account_keys::PICTURES << 1
                        << user_account_keys::FIRST_NAME << 1
                    << finalize;

            std::shared_ptr<std::vector<bsoncxx::document::value>> append_to_and_statement_doc = std::make_shared<std::vector<bsoncxx::document::value>>();

            //Make sure that the user
            append_to_and_statement_doc->emplace_back(
                document{}
                    << "$gte" << open_array
                        << "$" + user_account_keys::AGE << filtered_event_info->min_allowed_age()
                    << close_array
                << finalize
            );

            const bsoncxx::document::value login_document = getLoginDocument<false, true>(
                    chat_room_admin_info.login_token_str,
                    chat_room_admin_info.installation_id,
                    merge_document,
                    current_timestamp,
                    append_to_and_statement_doc
            );

            chat_room_admin_oid = bsoncxx::oid{chat_room_admin_info.user_account_oid_str};

            if (!runInitialLoginOperation(
                    event_admin_doc,
                    user_accounts_collection,
                    chat_room_admin_oid,
                    login_document,
                    projection_document,
                    callback_session)
            ) {
                //Error already stored here
                return_values.setError("runInitialLoginOperation() failed");
                callback_session->abort_transaction();
                transaction_successful = false;
                return;
            }

            const ReturnStatus return_status = checkForValidLoginToken(
                    event_admin_doc,
                    chat_room_admin_info.user_account_oid_str
            );

            if (return_status == ReturnStatus::UNKNOWN) {

                const std::string error_string = "An invalid age was passed in when attempting to create a user event.";
                return_values.setError(error_string);

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(),  error_string,
                        "user_account_oid", chat_room_admin_oid,
                        "min_allowed_age", std::to_string(filtered_event_info->min_allowed_age()),
                        "max_allowed_age", std::to_string(filtered_event_info->max_allowed_age())
                );

                return_values.return_status = ReturnStatus::INVALID_PARAMETER_PASSED;
                callback_session->abort_transaction();
                transaction_successful = false;
                return;
            } else if (return_status != ReturnStatus::SUCCESS) {
                return_values.return_status = return_status;
                callback_session->abort_transaction();
                //As successful as it could be, AND don't want the calling function to return LG_ERROR for a
                // failed login.
                transaction_successful = true;
                return;
            }
        }
        else {
            try {
                mongocxx::options::find opts;

                opts.projection(
                        document{}
                                << user_account_keys::FIRST_NAME << 1
                                << user_account_keys::PICTURES << 1
                                << finalize
                );

                event_admin_doc = user_accounts_collection.find_one(
                        document{}
                                << "_id" << event_admin_values::OID
                                << finalize,
                        opts
                );
            }
            catch (const mongocxx::logic_error& e) {
                return_values.setError(
                        "An exception occurred when accessing the event admin account.\nException: " +
                        std::string(e.what())
                );

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(),  std::string(e.what()),
                        "event_admin_values::OID", event_admin_values::OID
                );

                callback_session->abort_transaction();
                transaction_successful = false;
                return;
            }

            if (!event_admin_doc) {
                const std::string error_string = "The event admin account was not found.";
                return_values.setError(error_string);

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(),  error_string,
                        "event_admin_values::OID", event_admin_values::OID
                );

                callback_session->abort_transaction();
                transaction_successful = false;
                return;
            }
        }

        const AccountCreationParameterPack parameter_pack(
                event_account_oid,
                chat_room_admin_info.event_type,
                filtered_event_info->bio(),
                filtered_event_info->city(),
                std::move(event_request_returns.pictures_array),
                std::move(categories_array),
                event_values,
                event_expiration_time,
                filtered_event_info->min_allowed_age(),
                filtered_event_info->max_allowed_age(),
                std::move(genders_range)
        );

        //NOTE: this will set the return status and timestamp has already been set
        // (these are the only 2 fields inside the response at the moment)
        //It is important that either STATUS != STATUS_ACTIVE or MATCHING_ACTIVATED == false. This will prevent anything
        // from touching it during account creating.
        if (!createUserAccount(
                accounts_db,
                user_accounts_collection,
                callback_session,
                current_timestamp,
                AccountLoginType::LOGIN_TYPE_VALUE_NOT_SET,
                parameter_pack)
                ) {
            callback_session->abort_transaction();
            transaction_successful = false;
            return;
        }

        bsoncxx::stdx::optional<mongocxx::result::insert_many> insert_pictures_success;
        try {

            //NOTE: pictures_array, categories_array and genders_range have been moved.

            std::vector<bsoncxx::document::value> full_picture_documents;
            for (int i = 0; i < (int) event_request_returns.picture_oids.size(); ++i) {

                std::string* file_in_bytes = filtered_event_info->mutable_pictures(i)->release_file_in_bytes();
                std::string* thumbnail_in_bytes = filtered_event_info->mutable_pictures(i)->release_thumbnail_in_bytes();
                const int file_size_in_bytes = (int)file_in_bytes->size();
                const int thumbnail_size_in_bytes = (int)thumbnail_in_bytes->size();

                full_picture_documents.emplace_back(
                        createUserPictureDoc(
                                event_request_returns.picture_oids[i],
                                parameter_pack.user_account_oid,
                                current_timestamp,
                                i,
                                thumbnail_in_bytes,
                                thumbnail_size_in_bytes,
                                file_in_bytes,
                                file_size_in_bytes
                        )
                );

                delete file_in_bytes;
                delete thumbnail_in_bytes;
            }

            //insert new user picture
            insert_pictures_success = user_pictures_collection.insert_many(
                    *callback_session,
                    full_picture_documents
            );
        }
        catch (const mongocxx::logic_error& e) {
            return_values.setError(
                    "An exception occurred when storing pictures.\nException: " +
                    std::string(e.what()) +
                    "."
            );

            std::string picture_oids;

            for(const bsoncxx::oid& oid : event_request_returns.picture_oids) {
                picture_oids += oid.to_string() + ';';
            }

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), std::string(e.what()),
                    "picture_oids", picture_oids
            );

            callback_session->abort_transaction();
            transaction_successful = false;
            return;
        }

        if (!insert_pictures_success ||
            insert_pictures_success->result().inserted_count() != (int) event_request_returns.picture_oids.size()) { //insert failed
            const std::string error_string = "Failed to store all pictures for event.\ninserted_count: " +
                                             (insert_pictures_success ? std::string("insert_pictures_success->result().inserted_count()")
                                                                      : std::string("none inserted"));
            return_values.setError(error_string);

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string
            );

            callback_session->abort_transaction();
            transaction_successful = false;
            return;
        }

        //NOTE: The pictures array inside the request is invalid here. They were moved out then deleted.

        std::string chat_room_name = filtered_event_info->event_title() + " chat";
        std::string chat_room_password;
        std::string cap_message_uuid;

        std::optional<FullEventValues> chat_room_event_values;

        if (event_request_returns.qr_code_is_set) {
            chat_room_event_values.emplace(
                FullEventValues{
                    parameter_pack.user_account_oid,
                    filtered_event_info->release_qr_code_file_in_bytes(),
                    filtered_event_info->qr_code_message(),
                    chat_room_times.chat_room_last_active_time,
                    filtered_event_info->min_allowed_age()
                }
            );
        }
        else {
            chat_room_event_values.emplace(
                FullEventValues{
                    parameter_pack.user_account_oid,
                    filtered_event_info->min_allowed_age()
                }
            );
        }

        const std::optional<CreateChatRoomLocationStruct> chat_room_pinned_location{
                CreateChatRoomLocationStruct{
                        filtered_event_info->location_longitude(),
                        filtered_event_info->location_latitude()
                }
        };

        if (!createCompleteChatRoomWithUser(
                accounts_db,
                chat_room_db,
                callback_session,
                chat_room_times,
                chat_room_admin_oid,
                event_admin_doc->view(),
                chat_room_id,
                chat_room_event_values,
                chat_room_pinned_location,
                chat_room_name,
                chat_room_password,
                cap_message_uuid)
                ) {
            //NOTE: Error is already stored here.
            return_values.setError("Failed to create chat room.");
            callback_session->abort_transaction();
            transaction_successful = false;
            return;
        }

        mongocxx::stdx::optional<mongocxx::result::update> update_event_doc_to_matching;
        try {
            update_event_doc_to_matching = user_accounts_collection.update_one(
                *callback_session,
                document{}
                    << "_id" << parameter_pack.user_account_oid
                << finalize,
                document{}
                    << "$set" << open_document
                        << user_account_keys::STATUS << UserAccountStatus::STATUS_ACTIVE
                        << user_account_keys::MATCHING_ACTIVATED << bsoncxx::types::b_bool{true}
                        << user_account_keys::EVENT_VALUES + "." + user_account_keys::event_values::CHAT_ROOM_ID << chat_room_id
                        << user_account_keys::LOCATION + ".coordinates" << open_array
                            << bsoncxx::types::b_double{filtered_event_info->location_longitude()}
                            << bsoncxx::types::b_double{filtered_event_info->location_latitude()}
                        << close_array
                    << close_document
                << finalize
            );
        }
        catch (const mongocxx::logic_error& e) {
            return_values.setError(
                    "An exception occurred when setting event account to allow matches.\nException: " +
                    std::string(e.what())
            );

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(),  std::string(e.what()),
                    "user_account_oid", parameter_pack.user_account_oid
            );

            callback_session->abort_transaction();
            transaction_successful = false;
            return;
        }

        if (!update_event_doc_to_matching || update_event_doc_to_matching->modified_count() < 1) {

            const std::string error_string = "An error occurred when attempting to set the event to allow matches."
                                             "\nmodified_count: " + (update_event_doc_to_matching ? std::to_string(update_event_doc_to_matching->modified_count()): "-1") +
                                             "\nmatched_count: " + (update_event_doc_to_matching ? std::to_string(update_event_doc_to_matching->matched_count()): "-1");

            return_values.setError(error_string);

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(),  error_string,
                    "user_account_oid", parameter_pack.user_account_oid
            );

            callback_session->abort_transaction();
            transaction_successful = false;
            return;
        }

        if(chat_room_admin_info.event_type == UserAccountType::USER_GENERATED_EVENT_TYPE) {
            if(!createInitialChatRoomMessages(
                    cap_message_uuid,
                    bsoncxx::oid{chat_room_admin_info.user_account_oid_str},
                    chat_room_admin_info.user_account_oid_str,
                    chat_room_id,
                    chat_room_times,
                    return_values.chat_room_return_info.mutable_chat_room_cap_message(),
                    return_values.chat_room_return_info.mutable_current_user_joined_chat_message(),
                    different_user_joined_chat_room_message_doc,
                    different_user_joined_message_uuid)
                    ) {
                //NOTE: Error is already stored here.
                return_values.setError("createInitialChatRoomMessages() failed to extract messages.");
                callback_session->abort_transaction();
                transaction_successful = false;
                return;
            }
        }

        return_values.chat_room_return_info.set_chat_room_id(chat_room_id);
        return_values.chat_room_return_info.set_chat_room_password(chat_room_password);
        return_values.chat_room_return_info.set_chat_room_name(chat_room_name);
        return_values.chat_room_return_info.set_last_activity_time_timestamp(chat_room_times.chat_room_last_active_time.count());

        return_values.return_status = ReturnStatus::SUCCESS;
        return_values.error_message.clear();
    };

    mongocxx::client_session session = mongo_cpp_client.start_session();

    //client_session::with_transaction_cb uses the callback API for mongodb. This means that it will automatically handle the
    // more 'generic' errors. Can look here for more info
    // https://www.mongodb.com/docs/upcoming/core/transactions-in-applications/#callback-api-vs-core-api
    try {
        session.with_transaction(transaction_callback);
    } catch (const mongocxx::logic_error& e) {
        return_values.setError(
                "Exception occurred during the transaction. Inputting event canceled.\nException: " +
                std::string(e.what())
        );

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(),  std::string(e.what())
        );

        transaction_successful = false;
    }

    if (chat_room_admin_info.event_type == UserAccountType::USER_GENERATED_EVENT_TYPE
        && return_values.return_status == ReturnStatus::SUCCESS) {

        if(different_user_joined_chat_room_message_doc == nullptr) {
            const std::string error_string = "When success is returned different_user_joined_chat_room_message_doc should be set.";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "user_OID", chat_room_admin_info.user_account_oid_str,
                    "chat_room_return_info", return_values.chat_room_return_info.DebugString()
            );

            return_values.setError(error_string);
            transaction_successful = false;
            return return_values;
        }

#ifndef _RELEASE
        std::cout << "Running sendDifferentUserJoinedChatRoom().\n";
#endif

        //NOTE: This is done to 'connect' the chat stream to the device, essentially when the chat change stream
        // receives a kDifferentUserJoinedChatRoom message it will add it to the list of chat rooms.
        thread_pool.submit(
                [
                        _chat_room_id = return_values.chat_room_return_info.chat_room_id(),
                        _user_account_oid = bsoncxx::oid{chat_room_admin_info.user_account_oid_str},
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

    return return_values;
}