//
// Created by jeremiah on 5/13/21.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <regex>
#include <store_mongoDB_error_and_exception.h>
#include <global_bsoncxx_docs.h>
#include <server_parameter_restrictions.h>
#include <build_debug_string_response.h>
#include <general_values.h>

#include "chat_message_stream.h"
#include "number_chat_streams_running.h"
#include "utility_testing_functions.h"
#include "send_messages_implementation.h"
#include "connection_pool_global_variable.h"
#include "chat_stream_container_object.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_stream_container.h"
#include "chat_room_header_keys.h"
#include "helper_functions/messages_to_client.h"
#include "chat_change_stream_helper_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

ChatStreamContainerObject::ChatStreamContainerObject(
        grpc_stream_chat::StreamChatService::AsyncService* service,
        grpc::ServerCompletionQueue* cq
        )
        : service_(service), cq_(cq) {

#ifdef CHAT_STREAM_LOG
    std::cout << "start ChatStreamContainerObject constructor \n";
#endif

    responder_ = std::make_unique<grpc::ServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>>(&ctx_);

    // As part of the initial CREATE state, we *request* that the system
    // start processing DeleteMeBiDi requests. In this request, "this" acts are
    // the tag uniquely identifying the request (so that different ChatStreamContainerObject
    // instances can serve different requests concurrently), in this case
    // the memory address of this ChatStreamContainerObject instance.
    service_->RequestStreamChatRPC(&ctx_, dynamic_cast<grpc::ServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>*>(responder_.get()), cq_, cq_, INITIALIZE_TAG.get());

    //This will give a callback when the client is done. It will not send the callback if the
    // call never starts (can shift click it to see). This means that if the tag is used (with the
    // atomic outstanding_notify_when_done_in_completion_queue to keep track of it), there
    // could be times when this object is not deleted because the call it was associated with
    // never started. If the atomic is NOT used, there could be times that NOTIFY_WHEN_DONE is
    // returned and attempts to reference a deleted object. The time_out_stream_alarm will make
    // sure that the object does not stay alive forever, even in unusual situations. Also in
    // testing NOTIFY_WHEN_DONE_TAG takes an unusually long time to return to the respective
    // instance. This will leave it running SUSPEND over and over taking up a queue slot in the
    // thread_pool while it waits to run cleanupObject().
    //outstanding_notify_when_done_in_completion_queue++;
    //ctx_.AsyncNotifyWhenDone(NOTIFY_WHEN_DONE_TAG.get());
}

void ChatStreamContainerObject::endStream(const grpc_stream_chat::StreamDownReasons& streamFinishedReason,
                                          const std::string& _optional_info) {

#ifdef CHAT_STREAM_LOG
    std::cout << "Running endStream(" << StreamDownReasons_Name(
            streamFinishedReason) << ") optional_info: " << _optional_info << "\n";
#endif
    IncrementDecrementAtomic lock(run_writer_atomic);

    if (!service_completed) {
        stream_finished_reason = streamFinishedReason;
        optional_info = _optional_info;

        bool current_value_bool = false;
        //do not allow the alarm to be injected multiple times
        //note that this is run inside the run_writer_command mutex already
        if (cancel_current_function_alarm_set.compare_exchange_strong(current_value_bool, true)) {
            number_outstanding_chat_stream_cancelled++;
            cancel_current_function_alarm.Set(cq_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME),
                                              CHAT_STREAM_CANCELLED_ALARM_TAG.get());
        }
    }
}

//reference_to_current_chat_stream_object MUST be copied because they are passed into a
// coroutine. If they are passed by reference, the reference will be lost. There is a
// volatile sink below to keep them from being optimized out.
ThreadPoolSuspend ChatStreamContainerObject::injectStreamResponseCoroutine(
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> reference_to_current_chat_stream_object
        ) {

    const std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();

    //NOTE: reference_to_current_chat_stream_object is a reference to this CURRENT ChatStreamContainerObject. It
    // is there to keep the reference count alive during this Write().
    if(!reference_to_current_chat_stream_object->ptr()->service_completed) {
        RUN_COROUTINE(writeVectorElement<false>);
    }

    if(rand() < 0) {
        //here to prevent compiler optimizing out the pass by value parameter
        volatile std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> just_a_placeholder = std::move(reference_to_current_chat_stream_object);
    }

    total_chat_stream_inject_run_time += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count();

    co_return;
}

void ChatStreamContainerObject::injectStreamResponse(
        const std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function,
        const std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>& reference_to_current_chat_stream_object
) {

#ifdef CHAT_STREAM_LOG
    std::cout << "Injecting a function call into the bi di stream\n";
#endif
    bool initialization_completed_before_push = false;

    //This must be called by the 'begin chat stream' thread to guarantee the messages are sent back in order.
    // Otherwise, the coroutines could run simultaneously and a later message could be sent back first.
    //NOTE: Do not check if service_complete BEFORE sending this message, if a reference to this object is still
    // outstanding, the message should be injected into it for cleanup purposes.
    size_t queue_size_after_emplace = writes_waiting_to_process.push_and_size_no_coroutine(
        std::make_pair(
            outside_function,
            PushedToQueueFromLocation::PUSHED_FROM_INJECTION
        ),
        [&] {
            //initialization_complete cannot be checked AFTER the push, otherwise an edge case can occur where.
            // 1) This thread pushes the message into writes_waiting_to_process while initialization_complete=false.
            // 2) The initialization thread stores its own messages in writes_waiting_to_process, then sets initialization_complete=true.
            // 3) This thread continues on and runs a Write() while the initialization thread is also running a Write().
            //initialization_complete cannot be checked BEFORE the push, otherwise an edge case can occur where.
            // 1) This thread checks initialization_complete and saves it as false, but does not push its message.
            // 2) The initialization thread completes and writes all of its remaining messages.
            // 3) This thread moves forward and cannot write even though it placed the value in the queue (this means that Write() cannot occur anymore).
            initialization_completed_before_push = initialization_complete;
        }
    );

    //Call Write() if this was the first message pushed and this object is not
    // initializing or completed.
    if (queue_size_after_emplace == 1
        && initialization_completed_before_push
        && !service_completed
        ) {
        //injectStreamResponse() can be called on the change chat stream thread. To avoid writeToClient() also being
        // called on this thread, it will be passed to the thread pool for execution. However, if the reference to this
        // object is destroyed, then there is a chance that the thread pool will attempt to run a function that no longer
        // exists. To fix this the reference is sent to the lambda.
        thread_pool.submit_coroutine(injectStreamResponseCoroutine(reference_to_current_chat_stream_object).handle);
    }
}

std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>
ChatStreamContainerObject::retrieveReferenceWrapperForCallData() {
    IncrementDecrementAtomic lock(run_writer_atomic);
    if (!service_completed) {
        incrementReferenceCount();
        return std::make_shared<ReferenceWrapper<ChatStreamContainerObject>>(
                this,
                [&]() {
                    decrementReferenceCount();
                }
        );
    } else {
        return nullptr;
    }
}

ThreadPoolSuspend ChatStreamContainerObject::streamIsFinished() {

    //Pause for all outstanding references to complete.
    //NOTE: These values can be waited for AFTER Finish() runs. They should not start
    // anymore after service_completed has been set to true inside beginFinish(). However,
    // they won't hurt anything getting to this point either.
    for(int i = 0;
        outstanding_read_in_completion_queue.load(std::memory_order_acquire) > 0
        || outstanding_write_in_completion_queue.load(std::memory_order_acquire) > 0
        || reference_count.load(std::memory_order_acquire) > 0;
        i++) {
        if(i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
            SUSPEND();
            i = 0;
        }
    }

    RUN_COROUTINE(cleanUpObject);

    co_return;
}

bool ChatStreamContainerObject::refreshTimeoutTimePoint() {
    IncrementDecrementAtomic lock(run_writer_atomic);

    if (!service_completed) {
        bool true_value = true;
        //don't cancel alarm unless it is currently running
        auto system_clock_now = std::chrono::system_clock::now();
        if (time_out_stream_alarm_set.compare_exchange_strong(true_value, false) && end_stream_refresh_time > system_clock_now) {
            generateStreamTimeOut(system_clock_now);
            time_out_stream_alarm.Cancel();
            return true;
        }
    }

    return false;
}

//called from initialize and when END_STREAM_ALARM_CALLED is called with ok bit false
void ChatStreamContainerObject::setEndStreamAlarmTimer() {
    IncrementDecrementAtomic lock(run_writer_atomic);

    bool temp_bool = false;
    if (!service_completed && time_out_stream_alarm_set.compare_exchange_strong(temp_bool, true)) {
        //make sure alarm can not be injected into the vector multiple times
        number_outstanding_time_out_stream_alarm++;
        time_out_stream_alarm.Set(cq_, end_stream_time, END_STREAM_ALARM_CALLED_TAG.get());
    }
}

//called from initialize and when refreshTimeoutTimePoint() is called
void ChatStreamContainerObject::generateStreamTimeOut(const std::chrono::system_clock::time_point& system_clock_now) {
    end_stream_time = system_clock_now + chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
    end_stream_refresh_time = system_clock_now + chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;
}

ThreadPoolSuspend ChatStreamContainerObject::cleanedSaveMessagesToMap() {

    /** This is called after all references to it have been removed, so no concurrency is relevant
     * to the class members.**/

    //std::shared_ptr<ChatMessageToClientWrapper> message = nullptr;
    std::shared_ptr<std::pair<
            std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
            PushedToQueueFromLocation>
    > write_from_queue;

    RUN_COROUTINE(writes_waiting_to_process.front_if_not_empty, write_from_queue);

    while (write_from_queue != nullptr) {

        if (write_from_queue->second == PushedToQueueFromLocation::PUSHED_FROM_INJECTION) {

            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> replies;

            try {
                //run task
                write_from_queue->first(replies);
            } catch (const std::bad_function_call& e) {
                const std::string errorString = "ChatStreamContainerObject::cleanedSaveMessagesToMap() had a bad_function_call exception thrown.";

                std::optional<std::string> exceptionString = e.what();
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, exceptionString,
                        errorString,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "PushedToQueueFromLocation",
                        convertPushedToQueueFromLocationToString(write_from_queue->second));
            }

            for (const auto& reply : replies) {
                total_number_message_left_in_queue_at_end++;
                if (reply->has_return_new_chat_message()) {

                    const auto& message_list = reply->return_new_chat_message().messages_list();
                    for (const auto& message : message_list) {

                        AddOrRemove add_or_remove_user = addOrRemoveUserBasedOnMessageType(message.message().message_specifics().message_body_case());

                        if (add_or_remove_user == AddOrRemove::MESSAGE_TARGET_ADD
                            && message.sent_by_account_id() == current_user_account_oid_str) {
                                chat_room_ids_user_is_part_of.insert(
                                        message.message().standard_message_info().chat_room_id_message_sent_from()
                                );
                        }

#ifdef CHAT_STREAM_LOG
                        std::cout << "Cleanup " << convertMessageBodyTypeToString(
                                message.message().message_specifics().message_body_case()) << '\n';
#endif
                    }
                }
            }
        }

        bool return_val = false;

        RUN_COROUTINE(writes_waiting_to_process.pop_front, return_val);

        RUN_COROUTINE(writes_waiting_to_process.front_if_not_empty, write_from_queue);
    }

    co_return;
}

ThreadPoolSuspend ChatStreamContainerObject::cleanUpObject() {

#ifdef CHAT_STREAM_LOG
    std::cout << "Running ~~~cleanUpObject~~~ for user " << current_user_account_oid_str << "\n";
#endif

    //This should NEVER happen, it means that another cleanUpObject instance exists and
    // (very likely) is currently in the process of de-allocating this object! This
    // is just here as a way to hopefully stop it from calling delete on the same
    // object twice.
    bool expected = false;
    if(!clean_up_running.compare_exchange_strong(expected, true)) {

        std::string errorString = "cleanUpObject() was called twice inside a ChatStreamContainerObject.\n";

        //NOTE: Do not send any info from this ChatStreamContainerObject into the function,
        // 'delete' could have already been called.
        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__, exceptionString, errorString);

        co_return;
    }

    bool successful = false;

    /** Make sure this is not accessed while run_writer_command is locked or deadlock could occur (see user_open_chat_streams.upsert inside
     * of initializeObject()).**/
    RUN_COROUTINE(
        user_open_chat_streams.eraseWithCondition,
        current_user_account_oid_str,
        successful,
        [&](const ChatStreamContainerObject* value) -> bool {

            if (value == this) { //delete the value if it is the same as this value
                return true;
            }

            return false;
        }
    );

    /** cleanedSaveMessagesToMap() and the erase loop below it can not be avoided even if this
     * was overwritten by another stream running. This is because there is a chance that say a
     * chat room was joined (match was made) in between the chat streams and it leaves the
     * potential for a memory leak.
     * For example
     * -Say that I am in ChatRoom A.
     * -leaveChatRoom() is called on Stream 1 and gets pushed to the queue.
     * -Stream 1 is replaced by Stream 2.
     * -On Stream 2 the user joins A.
     * -Stream 1 cleanup runs and leaves A.
     * This situation is prevented by passing in the object_pointer to eraseUserOIDFromChatRoomId() after clean.
     **/
    //guarantee that ChatStreamObject.chatRoomIdsUserIsPartOf is up-to-date so every chat room will be removed
    RUN_COROUTINE(cleanedSaveMessagesToMap);

    //Erase all chat room ids that are no longer needed if permanently deleting this chat room
    //NOTE: This must be done after user_open_chat_streams.eraseWithCondition is called. It is important
    // NOT to do it inside the call to user_open_chat_streams because deadlock can occur (see MongoDBOIDContainer
    // for details).

    for (const auto& chat_room_id : chat_room_ids_user_is_part_of) {
        RUN_COROUTINE(
                eraseUserOIDFromChatRoomId_coroutine,
                chat_room_id,
                current_user_account_oid_str,
                current_index_value
                );
    }

    decrementNumberChatStreamsRunning();

    num_objects_deleted++;

    //deallocate this object, do this as the last command
    delete this;

    co_return;
}

bool ChatStreamContainerObject::extractLoginInfoFromMetaData(
        std::multimap<grpc::string_ref, grpc::string_ref>& metaData,
        grpc_stream_chat::InitialLoginMessageRequest& initialLoginMessageRequest
) {

    auto currentAccountIDIterator = metaData.find(chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID);
    auto loggedInTokenIterator = metaData.find(chat_stream_container::initial_metadata::LOGGED_IN_TOKEN);
    auto letsGoVersionIterator = metaData.find(chat_stream_container::initial_metadata::LETS_GO_VERSION);
    auto installationIdIterator = metaData.find(chat_stream_container::initial_metadata::INSTALLATION_ID);
    auto chatRoomValuesIterator = metaData.find(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES);

    if (currentAccountIDIterator == metaData.end()
        || loggedInTokenIterator == metaData.end()
        || letsGoVersionIterator == metaData.end()
        || installationIdIterator == metaData.end()
            ) {
        std::stringstream errorString;
        errorString
                << "Invalid meta data received from client.\n"
                << "currentAccountIDIterator valid: " << std::to_string(currentAccountIDIterator == metaData.end()) << '\n'
                << "loggedInTokenIterator valid: " << std::to_string(loggedInTokenIterator == metaData.end()) << '\n'
                << "letsGoVersionIterator valid: " << std::to_string(letsGoVersionIterator == metaData.end()) << '\n'
                << "installationIdIterator valid: " << std::to_string(installationIdIterator == metaData.end()) << '\n'
                << "chatRoomValuesIterator valid: " << std::to_string(chatRoomValuesIterator == metaData.end()) << '\n';

        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, exceptionString,
                errorString.str());
        return false;
    }

    /** I read somewhere that it is unsafe to access the type grpc::string_ref directly,
        so using the below method; (by directly they meant something like 'currentAccountIDIterator->second.data()').
        It can have extra info on the end of data(), this happened once during testing. **/

    initialLoginMessageRequest.mutable_login_info()->set_current_account_id(
            std::string(currentAccountIDIterator->second.data(), currentAccountIDIterator->second.length()));

    initialLoginMessageRequest.mutable_login_info()->set_logged_in_token(
            std::string(loggedInTokenIterator->second.data(), loggedInTokenIterator->second.length()));

    const std::string letsGoVersionString = std::string(letsGoVersionIterator->second.data(),
                                                        letsGoVersionIterator->second.length());

    unsigned int letsGoVersionNumber = 0;

    for (char c : letsGoVersionString) {
        if (isdigit(c)) {
            letsGoVersionNumber *= 10;
            letsGoVersionNumber += c - '0';
        } else {
            std::string errorString = "Invalid letsGoVersionString received from client.";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, exceptionString,
                    errorString,
                    "letsGoVersionString", letsGoVersionString);
            return false;
        }
    }

    initialLoginMessageRequest.mutable_login_info()->set_lets_go_version(letsGoVersionNumber);

    initialLoginMessageRequest.mutable_login_info()->set_installation_id(
            std::string(installationIdIterator->second.data(), installationIdIterator->second.length()));

    if (chatRoomValuesIterator == metaData.end()
        || chatRoomValuesIterator->second.empty()) { //client had no chat rooms stored
        return true;
    }

    std::string chatRoomValuesString = std::string(chatRoomValuesIterator->second.data(),
                                                   chatRoomValuesIterator->second.length());

    std::regex rgx(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

    std::sregex_token_iterator iter(
        chatRoomValuesString.begin(),
        chatRoomValuesString.end(),
        rgx,
        -1
    );

    std::sregex_token_iterator end;

    while (iter != end) {
        grpc_stream_chat::ChatRoomValues* chatRoomValues = initialLoginMessageRequest.add_chat_room_values();

        //extract 'chat_room_id'
        chatRoomValues->set_chat_room_id(*iter);
        ++iter;

        //extract 'last_time_updated' field
        if (iter != end) { //if data was properly formatted
            long long chatRoomLastTimeUpdated = 0;
            std::string chatRoomLastTimeUpdatedString = *iter;

            if (chatRoomLastTimeUpdatedString.size() > 14 || chatRoomLastTimeUpdatedString.empty()) {
                std::string errorString = "Invalid chatRoomLastTimeUpdatedString received from client (too long).";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, exceptionString,
                        errorString,
                        "chatRoomLastTimeUpdatedString", chatRoomLastTimeUpdatedString);
                return false;
            }

            if (chatRoomLastTimeUpdatedString == "-1") { //string could be -1
                chatRoomLastTimeUpdated = -1L;
            } else {
                for (char c : chatRoomLastTimeUpdatedString) {
                    if (isdigit(c)) {
                        chatRoomLastTimeUpdated *= 10;
                        chatRoomLastTimeUpdated += c - '0';
                    } else {
                        std::string errorString = "Invalid chatRoomLastTimeUpdatedString received from client (char is not a digit).";

                        std::optional<std::string> exceptionString;
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__, exceptionString,
                                errorString,
                                "chatRoomLastTimeUpdatedString", chatRoomLastTimeUpdatedString);
                        return false;
                    }
                }
            }

            chatRoomValues->set_chat_room_last_time_updated(chatRoomLastTimeUpdated);
            ++iter;
        }
        else {
            std::string errorString = "Data was improperly formatted in chatRoomValuesString.";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, exceptionString,
                    errorString,
                    "chatRoomValuesString", chatRoomValuesString);
            return false;
        }

        //extract 'last_time_viewed' field
        if (iter != end) { //if data was properly formatted
            long long chatRoomLastTimeObserved = 0;
            std::string chatRoomLastTimeObservedString = *iter;

            if (chatRoomLastTimeObservedString.size() > 14 || chatRoomLastTimeObservedString.empty()) {
                std::string errorString = "Invalid chatRoomLastTimeUpdatedString received from client (too long).";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, exceptionString,
                        errorString,
                        "chatRoomLastTimeObservedString", chatRoomLastTimeObservedString);
                return false;
            }

            if (chatRoomLastTimeObservedString == "-1") { //string could be -1
                chatRoomLastTimeObserved = -1L;
            } else {
                for (char c : chatRoomLastTimeObservedString) {
                    if (isdigit(c)) {
                        chatRoomLastTimeObserved *= 10;
                        chatRoomLastTimeObserved += c - '0';
                    }  else {
                        std::string errorString = "Invalid chatRoomLastTimeUpdatedString received from client (char is not a digit).";

                        std::optional<std::string> exceptionString;
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__, exceptionString,
                                errorString,
                                "chatRoomLastTimeObservedString", chatRoomLastTimeObservedString);
                        return false;
                    }
                }
            }

            chatRoomValues->set_chat_room_last_time_viewed(chatRoomLastTimeObserved);
            ++iter;
        }
        else {
            std::string errorString = "Data was improperly formatted in chatRoomValuesString.";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, exceptionString,
                    errorString,
                    "chatRoomValuesString", chatRoomValuesString);
            return false;
        }

        int number_message_uuids_passed = 0;

        //extract 'number_message_uuids_passed' field
        if (iter != end) { //if data was properly formatted
            std::string number_message_uuids_passed_string = *iter;

            //if this value is larger than a 32-bit integer can be
            if (number_message_uuids_passed_string.size() > 10 || number_message_uuids_passed_string.empty()) {
                std::string errorString = "Invalid number_message_uuids_passed received from client (too long).";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, exceptionString,
                        errorString,
                        "number_message_uuids_passed_string", number_message_uuids_passed_string);
                return false;
            }

            for (char c : number_message_uuids_passed_string) {
                if (isdigit(c)) {
                    number_message_uuids_passed *= 10;
                    number_message_uuids_passed += c - '0';
                } else {
                    std::string errorString = "Invalid number_message_uuids_passed_string received from client (char is not a digit).";

                    std::optional<std::string> exceptionString;
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__, exceptionString,
                            errorString,
                            "number_message_uuids_passed_string", number_message_uuids_passed_string);
                    return false;
                }
            }
            ++iter;
        }
        else {
            std::string errorString = "Data was improperly formatted in chatRoomValuesString.";

            std::optional<std::string> exceptionString;
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, exceptionString,
                    errorString,
                    "chatRoomValuesString", chatRoomValuesString);
            return false;
        }

        for(int i = 0; i < number_message_uuids_passed; ++i) {
            //extract 'most_recent_message_uuid' field
            if (iter != end) { //if data was properly formatted
                std::string most_recent_message_uuid_string = *iter;

                if (most_recent_message_uuid_string.size() > 36) {
                    std::string errorString = "Invalid most_recent_message_uuid_string received from client (too long).";

                    std::optional<std::string> exceptionString;
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__, exceptionString,
                            errorString,
                            "most_recent_message_uuid_string", most_recent_message_uuid_string);
                    return false;
                }

                chatRoomValues->add_most_recent_message_uuids(most_recent_message_uuid_string);
                ++iter;
            }
            else {
                std::string errorString = "Data was improperly formatted in chatRoomValuesString.";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, exceptionString,
                        errorString,
                        "chatRoomValuesString", chatRoomValuesString);
                return false;
            }
        }


    }

    return true;
}

void ChatStreamContainerObject::decrementNumberChatStreamsRunning() const {
    if (number_chat_streams_running_incremented) {
        number_chat_streams_running--;
    }
}

void ChatStreamContainerObject::incrementNumberChatStreamsRunning() {
    number_chat_streams_running_incremented = true;
    number_chat_streams_running++;
}

ThreadPoolSuspend ChatStreamContainerObject::initializeObject() {

    //Spawn a new ChatStreamContainerObject to serve new clients while this instance is
    // processed. This instance will deallocate itself.
    num_new_objects_created++;
    new ChatStreamContainerObject(service_, cq_);
    long number_values_remaining_to_be_written = -1L;

    bool exception_occurred = false;

    try {

        std::multimap<grpc::string_ref, grpc::string_ref> metaData = ctx_.client_metadata();

        grpc_stream_chat::InitialLoginMessageRequest initialLoginMessageRequest;
        if (!extractLoginInfoFromMetaData(metaData, initialLoginMessageRequest)
                ) { //meta data is invalid
            stream_finished_reason = grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED;
            stream_finished_return_status = ReturnStatus::INVALID_PARAMETER_PASSED;
            //This must be set so that cleanupObject() can continue.
            initialization_complete = true;
            RUN_COROUTINE(beginFinish);
            co_return;
        }

        incrementNumberChatStreamsRunning();

        std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();
        grpc_stream_chat::ChatToClientResponse parentResponseMessage;
        ReturnStatus checkLoginTokenReturnStatus = ReturnStatus::UNKNOWN;

        //NOTE: There is a call to the database in order to log in then another one later in extractChatRooms.
        // However, because in order for the chatRoomIdsUserIsPartOf set to be initialized AFTER the values are
        // extracted from the database it is necessary. Otherwise, this chat stream would cancel the previous
        // chat stream even if no one had sent a proper login token.
        current_user_account_oid_str = checkLoginToken(
                initialLoginMessageRequest,
                checkLoginTokenReturnStatus,
                currentTimestamp
        );

        if (checkLoginTokenReturnStatus != ReturnStatus::SUCCESS) { //if failed to log in

            //error stored inside checkLoginToken
            stream_finished_reason = grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED;
            stream_finished_return_status = checkLoginTokenReturnStatus;
            //This must be set so that cleanupObject() can continue.
            initialization_complete = true;
            RUN_COROUTINE(beginFinish);
            co_return;
        }

        bool upsert_successful = false;

        RUN_COROUTINE(
            user_open_chat_streams.upsert,
            current_user_account_oid_str,
            this,
            upsert_successful,
            [&initialLoginMessageRequest,
             this](ChatStreamContainerObject* previousRunningStream) {

                /** The other streams run_writer_command locking here WHILE the internal user_open_chat_streams mutex is locked
                 * so if the reverse order if ever true (where the internal user_open_chat_streams mutex is locked inside
                 * run_writer_command) then deadlock is possible. **/
                if(previousRunningStream != nullptr) {
                    previousRunningStream->endStream(
                            grpc_stream_chat::StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM,
                            initialLoginMessageRequest.login_info().installation_id()
                    );
                }

                //This is set when it is upserted for a few reasons.
                // 1) Unless the server is shutting down initializeObject() MUST be called by each object.
                // 2) Nothing is able to reference the current_index_value until this upsert is complete (need the variable set before references can be created).
                // 3) This will guarantee that the most recent ChatStreamContainerObject for this account_oid has the highest current_index_value
                //  at all times. This means greater than less than signs can be used to tell which object is 'valid'.
                current_index_value = chat_stream_container_index.fetch_add(1);
            }
        );

        if (!upsert_successful) {
            const std::string error_string = "Invalid current_user_account_oid_str was passed (it should be checked above).";
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "database", database_names::ACCOUNTS_DATABASE_NAME,
                    "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                    "current_user_account_oid_str", current_user_account_oid_str
            );

            stream_finished_reason = grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED;
            stream_finished_return_status = ReturnStatus::LG_ERROR;
            //This must be set so that cleanupObject() can continue.
            initialization_complete = true;
            RUN_COROUTINE(beginFinish);
            co_return;
        }

#ifdef LG_TESTING
        //This must be delayed AFTER it is inserted to user_open_chat_streams. Otherwise, injections are not allowed.
        if(time_for_initialize_to_delay.count() > 0) {
            std::this_thread::sleep_for(time_for_initialize_to_delay);
        }
#endif

        std::vector<
                std::pair<
                        std::function<void(
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                        PushedToQueueFromLocation>
        > initialization_writes_vector;

        //Let the client know that the initial download has begun.
        initialization_writes_vector.emplace_back(
            [](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply) {
                std::shared_ptr<grpc_stream_chat::ChatToClientResponse> initial_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
                initial_response->mutable_initial_connection_primer_response()->set_time_until_chat_stream_expires_in_millis(
                        chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE.count()
                );
                reply.emplace_back(initial_response);
            },
            PushedToQueueFromLocation::PUSHED_FROM_READ
        );

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> messages_reply_vector;

        bool successful = false;
        StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_INITIAL_MESSAGE> sendMessagesObject(messages_reply_vector);

        //NOTE: This is not relevant currently, however, it is worth noting that messages_reply_vector can
        // be cleared inside extractChatRooms() through sendMessagesObject if an error occurs.
        RUN_COROUTINE(
                extractChatRooms,
                initialLoginMessageRequest,
                sendMessagesObject,
                chat_room_ids_user_is_part_of,
                successful,
                current_index_value,
                current_user_account_oid_str,
                currentTimestamp
        );

        if (!successful) {
            stream_finished_reason = grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED;
            stream_finished_return_status = ReturnStatus::LG_ERROR;
            //This must be set so that cleanupObject() can continue.
            initialization_complete = true;
            RUN_COROUTINE(beginFinish);
            co_return;
        }

        sendMessagesObject.finalCleanup();

        //Save the initial messages and members if any are present.
        if(!messages_reply_vector.empty()) {
            initialization_writes_vector.emplace_back(
                    [messages_reply_vector = std::move(messages_reply_vector)](
                            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply) {
                        reply.insert(
                                reply.end(),
                                std::make_move_iterator(messages_reply_vector.begin()),
                                std::make_move_iterator(messages_reply_vector.end())
                        );
                    },
                    PushedToQueueFromLocation::PUSHED_FROM_READ
            );
        }

        //Let the client know that the initial download is complete.
        initialization_writes_vector.emplace_back(
                [](std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply) {
                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> initial_response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
                    initial_response->mutable_initial_connection_messages_complete_response();
                    reply.emplace_back(initial_response);
                },
                PushedToQueueFromLocation::PUSHED_FROM_READ
        );

        //writing them to the queue this way to guarantee initialization messages are always first (inject messages
        // could end up between them if added individually)
        {
            SCOPED_SPIN_LOCK(writes_waiting_to_process.getSpinLock());
            number_values_remaining_to_be_written = (long) initialization_writes_vector.size();

            writes_waiting_to_process.concat_vector_to_front_without_lock(
            initialization_writes_vector
            );
        }

    }
    catch (const mongocxx::operation_exception& e) {

        const std::string errorString = "ChatStreamContainerObject::initializeObject() threw an operation_exception.";

        std::optional<std::string> exceptionString = e.what();
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, exceptionString,
                errorString,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "exception_type", "mongocxx::operation_exception");

        stream_finished_reason = grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED;
        stream_finished_return_status = ReturnStatus::DATABASE_DOWN;
        exception_occurred = true;
    }
    catch (const std::exception& e) {

        const std::string errorString = "ChatStreamContainerObject::initializeObject() threw an std::exception.";

        std::optional<std::string> exceptionString = e.what();
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, exceptionString,
                errorString,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                "exception_type", "std::exception");

        stream_finished_reason = grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED;
        stream_finished_return_status = ReturnStatus::LG_ERROR;
        exception_occurred = true;
    }

    if(exception_occurred) {
        //This must be set so that cleanupObject() can continue.
        initialization_complete = true;
        RUN_COROUTINE(beginFinish);
        co_return;
    }

    auto system_clock_now = std::chrono::system_clock::now();
    generateStreamTimeOut(system_clock_now);

    //set up the stream to time out
    setEndStreamAlarmTimer();

    //this should happen after all initialization is done
    // especially after the message is added to writes_waiting_to_process
    //also don't want things re-ordered in case injectStreamResponse() is called
    initialization_complete = true;

    //run the 'Write()' for the initial login response(s) to be sent
    RUN_COROUTINE(writeVectorElement<false>, number_values_remaining_to_be_written);

    IncrementDecrementAtomic handle_atomic(run_writer_atomic);

    if (!service_completed) {
        outstanding_read_in_completion_queue++;
        responder_->Read(&request_, READ_TAG.get());
    }

    co_return;
}

std::shared_ptr<grpc_stream_chat::ChatToClientResponse> ChatStreamContainerObject::setupRequestFullMessageInfoErrorMessageReturn(
        grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus requestStatus,
        const grpc_stream_chat::ChatToServerRequest &request_copy,
        const std::string &chat_room_id
        ) {
    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
    if (request_copy.has_request_full_message_info()) {
        //add all message UUIDs and amounts requested
        std::for_each(
                request_copy.request_full_message_info().message_uuid_list().begin(),
                request_copy.request_full_message_info().message_uuid_list().end(),
                [&message](
                        const grpc_stream_chat::MessageUUIDWithAmountOfMessage& element) {
                    message->mutable_request_full_message_info_response()->mutable_error_messages()->
                    add_message_uuid_list(element.message_uuid());
                });
    }

    message->mutable_request_full_message_info_response()->set_chat_room_id(
            chat_room_id);
    message->mutable_request_full_message_info_response()->set_request_status(
            requestStatus);

    return message;
}

#ifdef LG_TESTING
inline void runTestingDelay(const long delay) {
    if(delay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds{delay});
    }
}
#endif

struct PassedMessageUuidInfo {
    int index;
    AmountOfMessage highest_amount_of_message;

    PassedMessageUuidInfo() = delete;

    explicit PassedMessageUuidInfo(
            int _index,
            AmountOfMessage _highest_amount_of_message
            ) : index(_index), highest_amount_of_message(_highest_amount_of_message)
            {}
};

inline ReturnStatus setReturnValueForError(
        const std::unordered_map<std::string, PassedMessageUuidInfo>& map_of_uuid_to_message_info,
        const grpc_stream_chat::MessageUUIDWithAmountOfMessage& request_message_info,
        int sent_messages_index
        ) {

    ReturnStatus return_value = ReturnStatus::VALUE_NOT_SET;

    //The possibilities are.
    // 1) The message (the highest amount of message) was not found in database or wrong message type (sent_messages_index is the index in the map) return VALUE_NOT_SET.
    // 2) This is a duplicate message (sent_messages_index is a different index in the map) return UNKNOWN.
    // 3) Invalid uuid (not in map) return VALUE_NOT_SET.
    // 4) Invalid amount of message (not in map) return UNKNOWN.

    //NOTE: If the message_uuid OR AmountOfMessage is invalid it will not be in the map.
    auto ele_ptr = map_of_uuid_to_message_info.find(request_message_info.message_uuid());
    if(ele_ptr != map_of_uuid_to_message_info.end()) { //message was not found in database or was a duplicate
        if(ele_ptr->second.index == sent_messages_index) { //message not found in database
            return_value = ReturnStatus::VALUE_NOT_SET;
        } else { //message is a duplicate
            return_value = ReturnStatus::UNKNOWN;
        }
    }
    else if(
            !AmountOfMessage_IsValid(request_message_info.amount_of_messages_to_request())
            ) { //invalid amount of message
        return_value = ReturnStatus::UNKNOWN;
    }
    else { //invalid uuid
        return_value = ReturnStatus::VALUE_NOT_SET;
    }

    return return_value;
}

void ChatStreamContainerObject::requestFullMessageInfo(
        const grpc_stream_chat::ChatToServerRequest& request_copy,
        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector
) {

    /** See utility/async_server/_documentation.md under header 'Requesting Message Updates' for details, requirements and guarantees.**/

    //used for the aggregation pipeline to sort the documents by the index they were passed in
    static const std::string MESSAGE_TYPE_VALID_KEY = "type_valid";
    static const std::string SORTING_BY_INDEX_KEY = "sort_by_index";
    static const std::string ARRAY_ID_KEY = "id";
    static const std::string ARRAY_INDEX_KEY = "idx";

#ifdef LG_TESTING
    runTestingDelay(request_copy.testing_time_for_request_to_run_in_millis());
#endif

    /** DO NOT USE request_ here (it has most likely changed), use request_copy **/
    //check the request here first then run anything that needs to be run
    //after that need to save whatever needs saving to the reply_vector

    const std::string& chat_room_id = request_copy.request_full_message_info().chat_room_id();

    if (isInvalidChatRoomId(chat_room_id)) {
#ifndef _RELEASE
        std::cout << "invalid messages reached\n";
#endif
        reply_vector.clear();

        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                setupRequestFullMessageInfoErrorMessageReturn(
                        grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                        request_copy,
                        chat_room_id
                );
        reply_vector.emplace_back(std::move(message));
        return;
    }

    try {
#ifdef CHAT_STREAM_LOG
        std::cout << "Processing case kRequestMessageInfo message: \n";
#endif
        bsoncxx::oid currentUserAccountOID = bsoncxx::oid{
            current_user_account_oid_str
        };

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

        mongocxx::database accountsDB = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];
        mongocxx::collection userAccountCollection = accountsDB[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

        mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];
        mongocxx::collection chatRoomCollection = chatRoomDB[collection_names::CHAT_ROOM_ID_ + chat_room_id];

        //map is key: message_uuid, value: highest AmountOfMessage that occurred in request
        std::unordered_map<std::string, PassedMessageUuidInfo> map_of_uuid_to_message_info;
        bsoncxx::builder::basic::array sort_by_index_array;
        bsoncxx::builder::basic::array message_id_list;
        message_id_list.append(chat_room_header_keys::ID);

        StoreAndSendRequestedMessagesToClientBiDiStream storeMessagesToVector(reply_vector, chat_room_id);

        {
            int index = 0;
            for (const grpc_stream_chat::MessageUUIDWithAmountOfMessage& message_info : request_copy.request_full_message_info().message_uuid_list()) {
                //set this condition first so branch prediction will always skip it unless an error occurs.
                if(index == chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST) {
                    break;
                }

                if (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES < message_info.message_uuid().size()) {
                    reply_vector.clear();
                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                                    );
                    reply_vector.emplace_back(std::move(message));
                    return;
                } else if (!isInvalidUUID(message_info.message_uuid())
                    && AmountOfMessage_IsValid(message_info.amount_of_messages_to_request())) { //valid uuid & amount of message
                        auto result = map_of_uuid_to_message_info.insert(
                        std::pair(
                                message_info.message_uuid(),
                                PassedMessageUuidInfo(
                                    index,
                                    message_info.amount_of_messages_to_request()
                                )
                            )
                        );

                        //Want to only request info for first message that is the highest requested 'amount_of_messages_to_request'.
                        if(!result.second
                            && result.first->second.highest_amount_of_message < message_info.amount_of_messages_to_request()
                            ) { //uuid already existed AND amount of message requires an update
                            result.first->second.index = index;
                            result.first->second.highest_amount_of_message = message_info.amount_of_messages_to_request();
                        }
                }
                //else {} //invalid uuid
                //The message will be added (in order) below to storeMessagesToVector. If it does not exist inside the
                // map of messages extracted from the chat room collection (searched for using messages_list).
                index++;
            }
        }

        if (map_of_uuid_to_message_info.empty()) {

            reply_vector.clear();

            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message = std::make_shared<grpc_stream_chat::ChatToClientResponse>();

            message->mutable_request_full_message_info_response()->set_chat_room_id(
                    chat_room_id);
            message->mutable_request_full_message_info_response()->set_request_status(
                    grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST);
            if (request_copy.has_request_full_message_info()) {
                //add all message UUIDs and amounts requested
                std::for_each(
                    request_copy.request_full_message_info().message_uuid_list().begin(),
                    request_copy.request_full_message_info().message_uuid_list().end(),
                    [&message](
                            const grpc_stream_chat::MessageUUIDWithAmountOfMessage& element) {
                        auto full_message_list = message->mutable_request_full_message_info_response()->mutable_full_messages()->add_full_message_list();
                        //this return status will force the client to clear the message from its database
                        full_message_list->set_message_uuid(
                                element.message_uuid());
                        if(AmountOfMessage_IsValid(element.amount_of_messages_to_request())) { //invalid uuid
                            full_message_list->set_return_status(ReturnStatus::VALUE_NOT_SET);
                        } else { //invalid amount of message
                            full_message_list->set_return_status(ReturnStatus::UNKNOWN);
                        }
                    }
                );
            }

            //always send something back
            reply_vector.emplace_back(std::move(message));
            return;
        }

        for(const auto& [uuid, val] : map_of_uuid_to_message_info) {
            message_id_list.append(uuid);
            sort_by_index_array.append(
                    document{}
                        << ARRAY_ID_KEY << uuid
                        << ARRAY_INDEX_KEY << val.index
                    << finalize
                );
        }

        mongocxx::pipeline stages;

        //message type will be checked for inside replace_root stage
        stages.match(
            document{}
                << "_id" << open_document
                    << "$in" << message_id_list.view()
                << close_document
            << finalize
        );

        //this will extract all requested documents and minimize the info from the header document
        // to show the user is inside the chat room
        // new header document will only have 3 fields
        // _id: ID
        // TIMESTAMP_CREATED: $TIMESTAMP_CREATED
        // "found": (true/false) (NOTE: true if the user is found false if not)
        //All documents will have a field for 'SORTING_BY_INDEX_KEY', the header will
        // have value -1, each message will have its respective index inside the request
        // message list.
        stages.replace_root(
            buildCheckIfUserIsInHeaderAndKeepMessagesTheSame(
                currentUserAccountOID,
                MESSAGE_TYPE_VALID_KEY,
                SORTING_BY_INDEX_KEY,
                ARRAY_ID_KEY,
                ARRAY_INDEX_KEY,
                sort_by_index_array
            )
        );

        //sort so that header is first and other messages are ordered
        stages.sort(
            document{}
                << SORTING_BY_INDEX_KEY << 1
            << finalize
        );

        mongocxx::stdx::optional<mongocxx::cursor> mongocxx_messages_cursor;
        std::optional<std::string> mongocxx_messages_exception_string;
        try {
            mongocxx_messages_cursor = chatRoomCollection.aggregate(stages);
        } catch (const mongocxx::logic_error& e) {
            //operation exceptions surround the entire function
            mongocxx_messages_exception_string = e.what();
        }

        if (!mongocxx_messages_cursor) {
            //If an operation exception was thrown then this will not be reached
            std::stringstream errorStringStream;
            errorStringStream
                << "Error when attempting to request server info.\n"
                << "chat_room_id: " << chat_room_id
                << "chat_room_id: " << chat_room_id
                << "aggregation_doc: " << makePrettyJson(stages.view_array())
                << "message_uuid_list[";

            for (const auto& amount_and_message : request_copy.request_full_message_info().message_uuid_list()) {
                errorStringStream
                    << amount_and_message.ShortDebugString() << ", ";
            }

            //formatting
            if (!request_copy.request_full_message_info().message_uuid_list().empty()) {
                errorStringStream.seekp(-2, std::stringstream::cur);
            } else {
                errorStringStream
                    << ' ';
            }

            errorStringStream
                << "]\n";

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__, mongocxx_messages_exception_string,
                    errorStringStream.str(),
                    "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                    "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id);

            reply_vector.clear();
            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                    setupRequestFullMessageInfoErrorMessageReturn(
                            grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                            request_copy,
                            chat_room_id
                    );
            reply_vector.emplace_back(std::move(message));
            return;
        }

        ExtractUserInfoObjects extractUserInfoObjects(
            mongoCppClient,
            accountsDB,
            userAccountCollection
        );

        bool first_element = true;
        int sent_messages_index = 0;
        for (const auto& message_doc : *mongocxx_messages_cursor) {
            if (first_element) { //if header document (index value will be -1)

                //first message should be header because these are sorted by TIMESTAMP_CREATED
                std::string message_uuid;

                auto messageOIDElement = message_doc["_id"];
                if (messageOIDElement
                    && messageOIDElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    message_uuid = messageOIDElement.get_string().value.to_string();
                }
                else { //if element does not exist or is not type oid
                    logElementError(__LINE__, __FILE__,
                                    messageOIDElement,
                                    message_doc, bsoncxx::type::k_oid, "_id",
                                    database_names::CHAT_ROOMS_DATABASE_NAME,
                                    collection_names::CHAT_ROOM_ID_ + chat_room_id);

                    reply_vector.clear();

                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                            );

                    reply_vector.emplace_back(std::move(message));
                    return;
                }

                if (message_uuid == chat_room_header_keys::ID) {
                    auto userFoundElement = message_doc["found"];
                    if (userFoundElement
                        && userFoundElement.type() == bsoncxx::type::k_bool
                    ) { //if element exists and is type bool
                        if (userFoundElement.get_bool().value) { //if user was found inside header
                            first_element = false;
                            continue;
                        } else { //if user was not found inside header
                            std::string errorString = "User was not found inside chat room when requesting message.";

                            std::optional<std::string> exceptionString;
                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__, exceptionString,
                                    errorString,
                                    "database",
                                    database_names::CHAT_ROOMS_DATABASE_NAME,
                                    "collection",
                                    collection_names::CHAT_ROOM_ID_ + chat_room_id,
                                    "userAccountOid", currentUserAccountOID);

                            //This is possible if the user
                            // 1) Sends a request (it has a queue on server and client side to get delayed in)
                            // 2) User leaves chat room
                            // 3) This message is called
                            // NOTE: this could be fixed by say sending currentChatRoomInstanceId on the client
                            // back to the shared view model and having it deal with the return values after
                            // iterating through the messages inside the repository, but there will still be
                            // a small window where the user left the chat room but the chat room has not been
                            // removed on the client or something, order gets difficult to predict with
                            // the layers around this bi-di stream implementation on client and server side.
                            // So just going to send back the error and the client can deal with it.

                            reply_vector.clear();

                            std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                                    setupRequestFullMessageInfoErrorMessageReturn(
                                            grpc_stream_chat::RequestFullMessageInfoResponse::USER_NOT_A_MEMBER_OF_CHAT_ROOM,
                                            request_copy,
                                            chat_room_id
                                    );

                            reply_vector.emplace_back(std::move(message));
                            return;
                        }
                    }
                    else { //if element does not exist or is not type bool
                        logElementError(__LINE__, __FILE__,
                                        userFoundElement,
                                        message_doc, bsoncxx::type::k_bool, "found",
                                        database_names::CHAT_ROOMS_DATABASE_NAME,
                                        collection_names::CHAT_ROOM_ID_ +
                                        chat_room_id);

                        reply_vector.clear();

                        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                                setupRequestFullMessageInfoErrorMessageReturn(
                                        grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                        request_copy,
                                        chat_room_id
                                );

                        reply_vector.emplace_back(std::move(message));
                        return;
                    }
                }
                else { //first message was not the chat room header
                    std::string errorString = "Header did not exist (or was not first element) inside aggregation pipeline.";

                    std::optional<std::string> exceptionString;
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__, exceptionString,
                            errorString,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection",
                            collection_names::CHAT_ROOM_ID_ + chat_room_id,
                            "userAccountOid", currentUserAccountOID,
                            "message_doc", message_doc);

                    reply_vector.clear();

                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                            );

                    reply_vector.emplace_back(std::move(message));
                    return;
                }

            }
            else { //if message document

                std::string message_uuid;
                int index_for_element = -1;
                bool message_type_valid = false;

                auto messageUUIDElement = message_doc["_id"];
                if (messageUUIDElement
                    && messageUUIDElement.type() == bsoncxx::type::k_utf8
                ) { //if element exists and is type utf8
                    message_uuid = messageUUIDElement.get_string().value.to_string();
                }
                else { //if element does not exist or is not type oid
                    logElementError(__LINE__, __FILE__,
                                    messageUUIDElement,
                                    message_doc, bsoncxx::type::k_utf8, "_id",
                                    database_names::CHAT_ROOMS_DATABASE_NAME,
                                    collection_names::CHAT_ROOM_ID_ + chat_room_id);

                    reply_vector.clear();

                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                            );

                    reply_vector.emplace_back(std::move(message));
                    return;
                }

                auto indexForElement = message_doc[SORTING_BY_INDEX_KEY];
                if (indexForElement
                    && indexForElement.type() == bsoncxx::type::k_int32
                ) { //if element exists and is type int32
                    index_for_element = indexForElement.get_int32().value;
                }
                else { //if element does not exist or is not type int32
                    logElementError(__LINE__, __FILE__,
                                    indexForElement,
                                    message_doc, bsoncxx::type::k_int32, SORTING_BY_INDEX_KEY,
                                    database_names::CHAT_ROOMS_DATABASE_NAME,
                                    collection_names::CHAT_ROOM_ID_ + chat_room_id);

                    reply_vector.clear();

                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                                    );

                    reply_vector.emplace_back(std::move(message));
                    return;
                }

                auto messageTypeValidElement = message_doc[MESSAGE_TYPE_VALID_KEY];
                if (messageTypeValidElement
                    && messageTypeValidElement.type() == bsoncxx::type::k_bool
                ) { //if element exists and is type int32
                    message_type_valid = messageTypeValidElement.get_bool().value;
                    if(!message_type_valid) {
                        //ChatToClientResponse
                        ChatMessageToClient response_msg;

                        //clear because message may have been partially set
                        response_msg.Clear();
                        response_msg.set_message_uuid(
                                request_copy.request_full_message_info().message_uuid_list()[sent_messages_index].message_uuid());
                        response_msg.set_return_status(ReturnStatus::UNKNOWN);
                        storeMessagesToVector.sendMessage(std::move(response_msg));
                        sent_messages_index++;
                        continue;
                    }
                }
                else { //if element does not exist or is not type int32
                    logElementError(__LINE__, __FILE__,
                                    messageTypeValidElement,
                                    message_doc, bsoncxx::type::k_bool, MESSAGE_TYPE_VALID_KEY,
                                    database_names::CHAT_ROOMS_DATABASE_NAME,
                                    collection_names::CHAT_ROOM_ID_ + chat_room_id);

                    reply_vector.clear();

                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                                    );

                    reply_vector.emplace_back(std::move(message));
                    return;
                }

                //Stop here, this means an error occurred server side when generating the index. A message uuid will be skipped
                // regardless, so simple send that an error occurred back.
                if(index_for_element < 0 || request_copy.request_full_message_info().message_uuid_list_size() <= index_for_element) {
                    std::string error_string = "index_for_element (set above) was larger than the message_uuid_list_size.";

                    std::optional<std::string> dummy_exception_string;
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__, dummy_exception_string,
                            error_string,
                            "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                            "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                            "request_copy", buildDebugStringResponse(&request_copy),
                            "index_for_element", std::to_string(index_for_element)
                            );

                    reply_vector.clear();

                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                            setupRequestFullMessageInfoErrorMessageReturn(
                                    grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                                    request_copy,
                                    chat_room_id
                                    );

                    reply_vector.emplace_back(std::move(message));
                    return;
                }

                //It is possible some messages were not found, anything that was missed must be sent back
                // in order.
                while(sent_messages_index < index_for_element) {

                    //ChatToClientResponse
                    ChatMessageToClient response_msg;

                    //clear because message may have been partially set
                    response_msg.Clear();
                    response_msg.set_message_uuid(
                            request_copy.request_full_message_info().message_uuid_list()[sent_messages_index].message_uuid());

                    response_msg.set_return_status(
                        setReturnValueForError(
                            map_of_uuid_to_message_info,
                            request_copy.request_full_message_info().message_uuid_list()[sent_messages_index],
                            sent_messages_index
                        )
                    );

                    storeMessagesToVector.sendMessage(std::move(response_msg));
                    sent_messages_index++;
                }

                AmountOfMessage amount_of_message = request_copy.request_full_message_info().message_uuid_list()[index_for_element].amount_of_messages_to_request();

                ChatMessageToClient response_msg;

                //If message was found then map does not need to be searched.
                if (convertChatMessageDocumentToChatMessageToClient(
                        message_doc,
                        chat_room_id,
                        current_user_account_oid_str,
                        false,
                        &response_msg,
                        amount_of_message,
                        DifferentUserJoinedChatRoomAmount::SKELETON,
                        false,
                        &extractUserInfoObjects)
                        ) { //successfully extracted message
                    response_msg.set_return_status(ReturnStatus::SUCCESS);
                } else { //error occurred while extracting message (error has been stored

                    //clear because message may have been partially set
                    response_msg.Clear();
                    response_msg.set_message_uuid(message_uuid);
                    response_msg.set_return_status(ReturnStatus::VALUE_NOT_SET);
                }

                storeMessagesToVector.sendMessage(std::move(response_msg));

                sent_messages_index++;
            }
        }

        if(sent_messages_index < request_copy.request_full_message_info().message_uuid_list_size()) {

            bool error_stored = false;

            for(;sent_messages_index < request_copy.request_full_message_info().message_uuid_list_size(); ++sent_messages_index) {
                //ChatToClientResponse
                ChatMessageToClient response_msg;

                //clear because message may have been partially set
                response_msg.Clear();
                response_msg.set_message_uuid(
                        request_copy.request_full_message_info().message_uuid_list()[sent_messages_index].message_uuid());

                if(sent_messages_index < chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST) {
                    response_msg.set_return_status(
                        setReturnValueForError(
                            map_of_uuid_to_message_info,
                            request_copy.request_full_message_info().message_uuid_list()[sent_messages_index],
                            sent_messages_index
                        )
                    );
                } else { //too many messages were requested
                    //only store the error once
                    if(!error_stored) {
                        error_stored = true;

                        std::string error_string = "The passed message_uuid_list_size was larger than chat_stream_container::MAX_NUMBER_MESSAGES_USER_CAN_REQUEST.";

                        std::optional<std::string> dummy_exception_string;
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__, dummy_exception_string,
                                error_string,
                                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                                "request_copy", buildDebugStringResponse(&request_copy),
                                "sent_messages_index", std::to_string(sent_messages_index)
                                );
                    }

                    //Setting the uuid to UNKNOWN will not delete it. However, the client will know to remove it from the queue, and
                    // it did not get the info.
                    response_msg.set_return_status(ReturnStatus::UNKNOWN);
                }
                storeMessagesToVector.sendMessage(std::move(response_msg));
            }
        }

        //this must be cleaned before reply_vector is started
        storeMessagesToVector.finalCleanup();

        if(!reply_vector.empty()) { //if vector has at least 1 value stored inside it, set the find one to FINAL_MESSAGE_LIST
            reply_vector.back()->mutable_request_full_message_info_response()->set_request_status(
                    grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST
                    );
        } //else {} if the reply vector IS empty the error will be handled for below

    }
    catch (const mongocxx::operation_exception& e) {

        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, exceptionString,
                std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        reply_vector.clear();
        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> error_message =
                setupRequestFullMessageInfoErrorMessageReturn(
                        grpc_stream_chat::RequestFullMessageInfoResponse::DATABASE_DOWN,
                        request_copy,
                        chat_room_id
                );
        reply_vector.emplace_back(std::move(error_message));
    }
    catch (const std::exception& e) {

        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, exceptionString,
                std::string(e.what()),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        reply_vector.clear();
        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> error_message =
                setupRequestFullMessageInfoErrorMessageReturn(
                        grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                        request_copy,
                        chat_room_id
                );
        reply_vector.emplace_back(std::move(error_message));
    }

    if (reply_vector.empty()) { //if vector is empty an error occurred

        std::stringstream errorStringString;

        errorStringString
        << "Vector for updating messages should ALWAYS have at least 1 value stored inside of it.\n";

        for (const auto& uuid_and_amount : request_copy.request_full_message_info().message_uuid_list()) {
            errorStringString
            << uuid_and_amount.ShortDebugString() << '\n';
        }

        errorStringString
        << "]\n";

        std::optional<std::string> exceptionString;
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, exceptionString,
                errorStringString.str(),
                "database", database_names::CHAT_ROOMS_DATABASE_NAME,
                "collection", collection_names::CHAT_ROOM_ID_ + chat_room_id,
                "request_copy", buildDebugStringResponse(&request_copy));

        reply_vector.clear();
        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                setupRequestFullMessageInfoErrorMessageReturn(
                        grpc_stream_chat::RequestFullMessageInfoResponse::LG_SERVER_ERROR,
                        request_copy,
                        chat_room_id
                );
        reply_vector.emplace_back(std::move(message));
        return;
    }
}

ThreadPoolSuspend ChatStreamContainerObject::runReadFunction() {

    if(service_completed) {
        co_return;
    }

    size_t queue_size_after_push;

    switch (request_.client_request_case()) {
        case grpc_stream_chat::ChatToServerRequest::kRequestFullMessageInfo: {

            bool temp_val = false;
            if(currently_handling_updates.compare_exchange_strong(temp_val, true)) {
                SCOPED_SPIN_LOCK(writes_waiting_to_process.getSpinLock());
                queue_size_after_push = writes_waiting_to_process.push_and_size_no_lock(
                        std::make_pair(
                                [request_copy(request_), this](
                                        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                                    this->requestFullMessageInfo(request_copy, reply_vector);
                                    },
                                    PushedToQueueFromLocation::PUSHED_FROM_READ
                                    )
                                    );
            }
            else {
                SCOPED_SPIN_LOCK(writes_waiting_to_process.getSpinLock());
                queue_size_after_push = writes_waiting_to_process.push_and_size_no_lock(
                    std::make_pair(
                        [request_copy(request_)](
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
#ifdef LG_TESTING
                            runTestingDelay(request_copy.testing_time_for_request_to_run_in_millis());
#endif

#ifdef CHAT_STREAM_LOG
                                std::cout << "Processing case kRequestMessageInfo CURRENTLY_PROCESSING_UPDATE_REQUEST is returned\n";
#endif

                                reply_vector.clear();

                                std::shared_ptr<grpc_stream_chat::ChatToClientResponse> message =
                                        setupRequestFullMessageInfoErrorMessageReturn(
                                                grpc_stream_chat::RequestFullMessageInfoResponse::CURRENTLY_PROCESSING_UPDATE_REQUEST,
                                                request_copy,
                                                request_copy.request_full_message_info().chat_room_id()
                                        );

                                reply_vector.emplace_back(std::move(message));

                            },
                        PushedToQueueFromLocation::PUSHED_FROM_READ
                    )
                );
            }
            break;
        }
        case grpc_stream_chat::ChatToServerRequest::kRefreshChatStream: {

            //Handling the refresh case outside the 'standard' lambda. This is because there are times when the user
            // attempts to refresh the chat stream and the channel will be backup up with requesting message updates.
            // This will allow the stream to still be refreshed although the client will still receive the response later.
            bool successfully_updated_time;

            if (!service_completed) {
                successfully_updated_time = refreshTimeoutTimePoint();
            } else {
                successfully_updated_time = false;
            }

            {
                SCOPED_SPIN_LOCK(writes_waiting_to_process.getSpinLock());
                queue_size_after_push = writes_waiting_to_process.push_and_size_no_lock(
                    std::make_pair(
                        [successfully_updated_time(successfully_updated_time), end_stream_time(end_stream_time), testing_time_for_request_to_run_in_millis(request_.testing_time_for_request_to_run_in_millis())](
                            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {

#ifdef LG_TESTING
                        runTestingDelay(testing_time_for_request_to_run_in_millis);
#endif

                        /** DO NOT USE request_ here, use request_copy **/
                        //check the request here first then run anything that needs to be run
                        //after that need to save whatever needs saving to the reply_vector
    #ifdef CHAT_STREAM_LOG
                        std::cout << "Processing case kRequestMessageInfo\n";
    #endif
                        long current_time = getCurrentTimestamp().count();
                        long end_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_stream_time.time_since_epoch()).count();

                        long time_until_chat_stream_expires_in_millis = end_time - current_time;

                        if(time_until_chat_stream_expires_in_millis < 1) {
                            time_until_chat_stream_expires_in_millis = 1;
                        }

                        std::shared_ptr<grpc_stream_chat::ChatToClientResponse> response = std::make_shared<grpc_stream_chat::ChatToClientResponse>();
                        response->mutable_refresh_chat_stream_response()->set_successfully_refreshed_time(
                                successfully_updated_time);
                        response->mutable_refresh_chat_stream_response()->set_time_until_chat_stream_expires_in_millis(
                                time_until_chat_stream_expires_in_millis);

                        reply_vector.emplace_back(std::move(response));
                        },
                     PushedToQueueFromLocation::PUSHED_FROM_READ
                    )
                );
            }

            break;
        }
        case grpc_stream_chat::ChatToServerRequest::CLIENT_REQUEST_NOT_SET: {

            {
                SCOPED_SPIN_LOCK(writes_waiting_to_process.getSpinLock());
                queue_size_after_push = writes_waiting_to_process.push_and_size_no_lock(
                        std::make_pair(
                        [request_copy(request_), this](
                                std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>&/*reply_vector*/) {

#ifdef LG_TESTING
                            runTestingDelay(request_copy.testing_time_for_request_to_run_in_millis());
#endif

                            /** DO NOT USE request_ here, use request_copy **/
                            //check the request here first then run anything that needs to be run
                            //after that need to save whatever needs saving to the reply_vector

                            //NOTE: This is a case where reply_vector will return empty(). It is handled by
                            // ERROR_INVALID_TYPE_FLAG.

                            ERROR_INVALID_TYPE_FLAG = true;

                            std::optional<std::string> dummy_exception_string;
                            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                                          dummy_exception_string,
                                                          std::string(
                                                                  "invalid client_request_case inside request passed to Read()"),
                                                                  "Request", buildDebugStringResponse(&request_copy)
                                                                  );

#ifdef CHAT_STREAM_LOG
                            std::cout << "Processing case CLIENT_REQUEST_NOT_SET; this shouldn't ever happen\n";
#endif

                            },
                            PushedToQueueFromLocation::PUSHED_FROM_READ
                            )
                        );
            }
            break;
        }
    }

    //this must be pushed onto the vector even though they may be called immediately because the 'Write()'
    // callback needs to pop them from the vector
    //making a copy of _request so that after this when it is sent to the Read() function the _request object
    // will not have changed

    //important thing here is that reply_is_from_client = false, however the request was already copied
    // and so clearing and re-using the old object should be more memory efficient
    request_.Clear();

    {
        IncrementDecrementAtomic lock(run_writer_atomic);

        if (!service_completed) {
            outstanding_read_in_completion_queue++;
            responder_->Read(&request_, READ_TAG.get());
        } else {
            co_return;
        }
    }

    if (queue_size_after_push == 1) { //if the vector was empty until this was pushed into it
        RUN_COROUTINE(writeVectorElement<false>);
    }

    co_return;
}

ThreadPoolSuspend ChatStreamContainerObject::runServerShuttingDown(CallDataCommandTypes commandType) {

    const std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();

#ifdef CHAT_STREAM_LOG
    std::cout << "runServerShuttingDown() called\n";
#endif

    //It is possible that the shutdown was called when INITIALIZE has not yet completed. If this
    // happens then set initialization_complete to true (because no new streams need to be made).
    initialization_complete = true;
    switch (commandType) {
        case CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END:
        case CallDataCommandTypes::INITIALIZE:
        case CallDataCommandTypes::READ:
        case CallDataCommandTypes::END_OF_VECTOR_WRITES:
        case CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING:
        case CallDataCommandTypes::NOTIFY_WHEN_DONE:
        case CallDataCommandTypes::END_STREAM_ALARM_CALLED:
        case CallDataCommandTypes::CHAT_STREAM_CANCELLED_ALARM: {
            //this could finish before the shut-down has a chance to cancel it and so need to
            // set this here just in case, it may get set multiple times, however this overwrites
            // other saved StreamDownReasons
            stream_finished_reason = grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN;
            RUN_COROUTINE(beginFinish);
            break;
        }
        case CallDataCommandTypes::FINISH: {
            RUN_COROUTINE(streamIsFinished);
            break;
        }
    }

    total_chat_stream_server_shutting_down_run_time += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count();

    co_return;
}

ThreadPoolSuspend ChatStreamContainerObject::runFunction(CallDataCommandTypes commandType, bool ok_byte) {

    const std::chrono::system_clock::time_point start_time = std::chrono::system_clock::now();

#ifdef CHAT_STREAM_LOG
    std::cout << std::string("runFunction() commandType: " + convertCallDataCommandTypesToString(commandType) + " ok_byte: " + (ok_byte?"true":"false") + "\n");
#endif

    //The idea behind this class that there is at most 1 Read() coming in at a time and 1 Write() coming in at a time
    // the Read() will only ever run write if it added the only one to the vector. Also .Finish() is protected by a bool
    // and a bool and so can only be called once.

    //These CallDataCommandTypes are the type of command that these WERE CALLED FROM
    // for example a 'Write()' will send the CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING tag, which means it
    // will need to call the CallDataCommandTypes::WRITE_CALLBACK function
    if (ok_byte) {

        switch (commandType) {
            //First time this ChatStreamContainerObject has started.
            case CallDataCommandTypes::INITIALIZE: {
                RUN_COROUTINE(initializeObject);
                break;
            }
            //A Read() was called.
            case CallDataCommandTypes::READ: {
                //Read() calls itself because a Read() should always be running.
                RUN_COROUTINE(runReadFunction);
                total_chat_stream_run_function_read_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_read_run.fetch_add(1, std::memory_order_release);
                break;
            }
            //A 'Write()' was called and no more replies from the retrieved
            // vector need to be written.
            case CallDataCommandTypes::END_OF_VECTOR_WRITES:
                RUN_COROUTINE(runWriteCallbackFunction);

                total_chat_stream_run_function_end_of_vector_writes_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_end_of_vector_writes_run.fetch_add(1, std::memory_order_release);
                break;
            //A 'Write()' was called and more replies need to be written before sending
            // the message to the back of the vector.
            case CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING:
                RUN_COROUTINE(writeVectorElement<true>);
                total_chat_stream_run_function_write_with_vector_elements_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_write_with_vector_elements_run.fetch_add(1, std::memory_order_release);
                break;
            //A 'Write()' was called and this was put to the back of the Queue.
            case CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END:
                RUN_COROUTINE(writeVectorElement<false>);
                total_chat_stream_run_function_alarm_queue_write_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_alarm_queue_write_run.fetch_add(1, std::memory_order_release);
                break;
            //return for Finish()
            case CallDataCommandTypes::FINISH: {
                //should never be reached
                RUN_COROUTINE(streamIsFinished);
                total_chat_stream_run_function_finish_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_finish_run.fetch_add(1, std::memory_order_release);
                break;
            }
            //the alarm expired to time out stream
            case CallDataCommandTypes::END_STREAM_ALARM_CALLED:
                stream_finished_reason = grpc_stream_chat::StreamDownReasons::STREAM_TIMED_OUT;
                [[fallthrough]];
            //the Client finished
            case CallDataCommandTypes::NOTIFY_WHEN_DONE:
            //the stream was manually cancelled when server was shut down
            case CallDataCommandTypes::CHAT_STREAM_CANCELLED_ALARM:
                RUN_COROUTINE(beginFinish);
                total_chat_stream_run_function_done_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_done_run.fetch_add(1, std::memory_order_release);
                break;
        }

    }
    else { //ok_byte was false

        //these cases can be found in the comments above the ServerCompletionQueue->Next() function
        switch (commandType) {
            case CallDataCommandTypes::INITIALIZE: //can be sent back on server shutdown
                //END_OF_VECTOR_WRITES && WRITE_WITH_VECTOR_ELEMENTS_REMAINING means 'it is not going to the
                // wire because the call is already dead (i.e., canceled, deadline expired, other side dropped
                // the channel, etc.)'
            case CallDataCommandTypes::END_OF_VECTOR_WRITES:
            case CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING:
                //the stream was manually cancelled when server was shut down, the bit should never be false
                // because there is no Cancel() for it in the code anywhere, however on server shutdown it could be
            case CallDataCommandTypes::CHAT_STREAM_CANCELLED_ALARM:
            case CallDataCommandTypes::NOTIFY_WHEN_DONE: { //documentation says the 'good' bit can never be false for AsyncNotifyWhenDone (inside CompletionQueue::Next)
                RUN_COROUTINE(beginFinish);
                total_chat_stream_run_function_done_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_done_run.fetch_add(1, std::memory_order_release);
                break;
            }
            //READ means 'there is no valid message that got read', means no more writes are
            // coming (does not necessarily mean stream is dead, the server MIGHT still be able to send)
            //It could mean that the client called onCompleted() (the equivalent of WritesDone in C++)
            // which means the server can still write back to the client (at least it can write Finish()).
            //There does not seem to be a way to distinguish what exactly happened.
            case CallDataCommandTypes::READ: {
                RUN_COROUTINE(beginFinish);
                total_chat_stream_run_function_done_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_done_run.fetch_add(1, std::memory_order_release);
                break;
            }
            //ALARM_QUEUE_WRITE_AT_END means send_write_to_back_of_queue_alarm cancelled, there is no
            // send_write_to_back_of_queue_alarm.Cancel() in the code, so it must be internal
            case CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END: {
                RUN_COROUTINE(beginFinish);
                total_chat_stream_run_function_done_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_done_run.fetch_add(1, std::memory_order_release);
                break;
            }
            //FINISH means 'it is not going to the wire because the call is already
            // dead (i.e., canceled, deadline expired, other side dropped the channel, etc.)'
            case CallDataCommandTypes::FINISH: {
                RUN_COROUTINE(streamIsFinished);
                total_chat_stream_run_function_finish_run_time.fetch_add(
                    std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - start_time).count(),
                    std::memory_order_release
                );
                total_num_chat_stream_run_function_finish_run.fetch_add(1, std::memory_order_release);
                break;
            }
            //END_STREAM_ALARM_CALLED is called by an alarm, this means the alarm was canceled,
            // (probably using Cancel()) in order to reset it
            case CallDataCommandTypes::END_STREAM_ALARM_CALLED:
                //if time_out_stream_alarm was canceled then refresh it
                setEndStreamAlarmTimer();
                break;
        }

    }

    co_return;
}

ThreadPoolSuspend ChatStreamContainerObject::beginFinish() {

    //only run this block once
    bool false_bool = false;
    if (service_completed.compare_exchange_strong(false_bool, true, std::memory_order_relaxed)) {
    //if (!service_completed.fetch_add(1, std::memory_order_relaxed)) {

    #ifdef CHAT_STREAM_LOG
        std::cout << "Running Finish(); ctx_.IsCancelled(): " << ctx_.IsCancelled() << '\n';
    #endif

        //Do not allow Read() or Write() to run during/after Finish().
        for(int i = 0; run_writer_atomic.load(std::memory_order_acquire) > 0; ++i) {
            if(i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
                SUSPEND();
                i = 0;
            }
        }

        //although this is technically false (the alarms are about to be canceled) it
        // will make sure no alarms can be set AFTER the server closes
        cancel_current_function_alarm_set = true;
        time_out_stream_alarm_set = true;

        //Finish() does not seem to cancel alarms, so if an alarm is set in the future (the
        // time_out_stream_alarm alarm is a good example) then it can occur AFTER the Finish()
        // command does
        //Cancel can be called even if the alarm is not currently set
        cancel_current_function_alarm.Cancel();
        time_out_stream_alarm.Cancel();
        send_write_to_back_of_queue_alarm.Cancel();

        //make sure not to inject Finish() until all alarms have been completed, otherwise there is a
        // chance that the alarm can be called AFTER finish.
        for(int i=0;
            number_outstanding_write_to_back_of_queue_alarm > 0
            || number_outstanding_time_out_stream_alarm > 0
            || number_outstanding_chat_stream_cancelled > 0
            || !initialization_complete; //must wait for this here, it doesn't seem to be returned after Finish() is called
            i++) {

            if(i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
                SUSPEND();
                i = 0;
            }
        }

        if(number_outstanding_write_to_back_of_queue_alarm != 0) {
            std::cout << "number_outstanding_write_to_back_of_queue_alarm: " << number_outstanding_write_to_back_of_queue_alarm << '\n';
        }

        ctx_.AddTrailingMetadata(chat_stream_container::trailing_metadata::REASON_STREAM_SHUT_DOWN_KEY,
                                 std::to_string(stream_finished_reason));

        if (stream_finished_reason == grpc_stream_chat::StreamDownReasons::RETURN_STATUS_ATTACHED) {
            ctx_.AddTrailingMetadata(chat_stream_container::trailing_metadata::RETURN_STATUS_KEY,
                                     std::to_string(stream_finished_return_status));
        }

        if (!optional_info.empty()) {
            ctx_.AddTrailingMetadata(chat_stream_container::trailing_metadata::OPTIONAL_INFO_OF_CANCELLING_STREAM, optional_info);
        }

        responder_->Finish(return_status_code, FINISH_TAG.get());
        num_times_finish_called++;

    }

    co_return;
}

void ChatStreamContainerObject::incrementReferenceCount() {
    reference_count.fetch_add(1, std::memory_order_relaxed);
#ifdef CHAT_STREAM_LOG
    std::cout << std::string("incremented reference count: ").append(std::to_string(new_reference_count + 1)).append(
            "\n");
#endif
}

void ChatStreamContainerObject::decrementReferenceCount() {
    reference_count.fetch_sub(1, std::memory_order_relaxed);

#ifdef CHAT_STREAM_LOG
    const int new_reference_count = reference_count.fetch_sub(1, std::memory_order_relaxed) - 1;
    std::cout << std::string("decremented reference count: ").append(std::to_string(new_reference_count)).append(
            "\n");
#endif

}

ThreadPoolSuspend ChatStreamContainerObject::runWriteCallbackFunction() {

#ifdef CHAT_STREAM_LOG
    std::cout << "begin runWriteCallbackFunction() writes_waiting_to_process size: " << writes_waiting_to_process.size_for_testing() << '\n';
#endif

    const std::chrono::milliseconds outer_check_start = getCurrentTimestamp();

    //doing the pop here so that the mutex primary lock only needs to be locked in 2 places instead of 3, and can get rid of a boolean
    //run a new Write(), however add it to the BACK of the ServerCompletionQueue
    //running both pop() and empty() for the vector at the same time here will not allow other threads to access it inside
    // the 'gap' and make it false
    bool empty = true;
    RUN_COROUTINE(writes_waiting_to_process.pop_front_and_empty,empty);

    if (!empty) {
        IncrementDecrementAtomic lock(run_writer_atomic);

        if (!service_completed) {
            number_outstanding_write_to_back_of_queue_alarm++;
            send_write_to_back_of_queue_alarm.Set(cq_, gpr_now(gpr_clock_type::GPR_CLOCK_REALTIME),
                                                  ALARM_QUEUE_WRITE_AT_END_TAG.get());
        }
    }

#ifdef CHAT_STREAM_LOG
    std::cout << "end runWriteCallbackFunction() writes_waiting_to_process size: " << writes_waiting_to_process.size_for_testing() << '\n';
#endif

    co_return;
}

ThreadPoolSuspend ChatStreamContainerObject::writeVectorElementBody(
        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector,
        PushedToQueueFromLocation pushed_from_location,
        WriteSuccessfulState& write_state
        ) {

    //protobuf documentation was fairly clear that it is cheaper to reuse object rather
    // than create new ones
    reply_.Clear();

    //move first element to be written now, send the rest of the elements
    // into the vector to be written later
    //to guarantee order of messages, run this entire process in a mutex
    {
        SCOPED_SPIN_LOCK(writes_waiting_to_process.getSpinLock());
        //NOTE: Do NOT make this std::move, it will move it from inside the shared_ptr to reply_, this means
        // that only 1 user will be able to receive the message.
        //NOTE: reply_vector was checked before this call to not be empty
        reply_ = *reply_vector[0];

        std::vector<std::pair<
                std::function<void(
                        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
                PushedToQueueFromLocation>
        > temp_vector;

        //Doing it this way because I assume that creating a single vector then passing it to the concat_vector_to_front
        // is faster than making an entire new vector each time. Also, I can take advantage of emplace_back to skip
        // initialization of the std::pair. However, this will need profiling if performance becomes an issue.
        for (size_t i = 1; i < reply_vector.size(); i++) {
            temp_vector.emplace_back(
                    [saved_reply = reply_vector[i]](
                            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector) {
                        //this lambda should only ever return a single element inside the vector (vector of size 1)
                        reply_vector.emplace_back(saved_reply);
                    },
                    pushed_from_location
            );
        }

        if (!temp_vector.empty()) {
            //NOTE: This value has already been properly set inside the writeVectorElement(). No need
            // to increment it here.
            //number_vector_values_remaining_to_be_written += temp_vector.size();

            //add them after index 0 so that when runWriteVectorElement() is called after this and an element is popped
            // the new starting element will be the first to be written, this will also allow ordering 'Write()' queue
            // and in this way, so they are ordered
            writes_waiting_to_process.concat_vector_to_index_one_without_lock(temp_vector);
        }
    }

    //If messages_list is empty, do not send reply. Error will be stored in calling function.
    if(
        (reply_.has_return_new_chat_message() && reply_.return_new_chat_message().messages_list().empty())
        ||
        (reply_.has_initial_connection_messages_response() && reply_.initial_connection_messages_response().messages_list().empty())
    ) {
        write_state = WriteSuccessfulState::CONTAINER_WRITE_STATE_FAIL;
        co_return;
    }

#ifdef CHAT_STREAM_LOG
    std::cout << "service_completed: " << service_completed << '\n';
#endif

    if (!service_completed) {

        //this is inside the block so that service_completed is checked before chat_room_ids_user_is_part_of is
        // accessed
        if (pushed_from_location == PushedToQueueFromLocation::PUSHED_FROM_INJECTION) {

            auto message_list = reply_.mutable_return_new_chat_message()->mutable_messages_list();

            for(auto& message : *message_list) {

                if (message.sent_by_account_id() == current_user_account_oid_str
                    && addOrRemoveUserBasedOnMessageType(message.message().message_specifics().message_body_case()) == AddOrRemove::MESSAGE_TARGET_ADD
                    ) {
                    chat_room_ids_user_is_part_of.insert(
                            message.message().standard_message_info().chat_room_id_message_sent_from()
                    );

                    //A kDifferentUserJoined message pushed from injection sent by the current
                    // user is a special case. Need it to add the chat room above, however no need
                    // to send it back.
                    if(!message.message().standard_message_info().internal_force_send_message_to_current_user()) {
                        const std::string chat_room_id = message.message().standard_message_info().chat_room_id_message_sent_from();
                        long timestamp_updated = message.timestamp_stored();
                        const std::string message_uuid = message.message_uuid();

                        message.Clear();

                        //Set this with a kNewUpdateTimeMessage message type instead of kDifferentUserJoinedMessage.
                        buildNewUpdateTimeMessageResponse(
                                &message,
                                chat_room_id,
                                timestamp_updated,
                                message_uuid
                        );
                    }
                }
            }
        }

#ifdef CHAT_STREAM_LOG
        std::cout << "runWriteVectorElement() ServerResponseCase: " << convertServerResponseCaseToString(
                        reply_.server_response_case()) << '\n';
#endif

        //If this is the final message from requesting an update, set the bool saying handling an update to false.
        if(reply_.server_response_case() == grpc_stream_chat::ChatToClientResponse::ServerResponseCase::kRequestFullMessageInfoResponse
           && reply_.request_full_message_info_response().request_status() != grpc_stream_chat::RequestFullMessageInfoResponse::INTERMEDIATE_MESSAGE_LIST
                ) {
            currently_handling_updates = false;
        }

        //If messages_list is empty, do not send reply. Error will be stored in calling function.
        if(
            (reply_.has_return_new_chat_message() && reply_.return_new_chat_message().messages_list().empty())
            || (reply_.has_initial_connection_messages_response() && reply_.initial_connection_messages_response().messages_list().empty())
        ) {
            write_state = WriteSuccessfulState::CONTAINER_WRITE_STATE_FAIL;
            co_return;
        }

        IncrementDecrementAtomic lock(run_writer_atomic);
        if (!service_completed) {
            outstanding_write_in_completion_queue++;
            responder_->Write(reply_,
                              number_vector_values_remaining_to_be_written == 0 ? END_OF_VECTOR_WRITES_TAG.get()
                                                                                : WRITE_WITH_VECTOR_ELEMENTS_REMAINING_TAG.get());
        }

    }

    write_state = WriteSuccessfulState::CONTAINER_WRITE_STATE_SUCCESS;
    co_return;
}
