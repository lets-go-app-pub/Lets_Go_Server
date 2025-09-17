//
// Created by jeremiah on 5/7/21.
//
#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <thread>
#include <utility>
#include <queue>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/alarm.h>

#include <ChatMessageStream.grpc.pb.h>
#include <tbb/concurrent_unordered_map.h>
#include <tbb/concurrent_unordered_set.h>

#include "user_open_chat_streams.h"
#include "thread_pool_global_variable.h"
#include "utility_general_functions.h"
#include "mongo_oid_concurrent_container.h"
#include "reference_wrapper.h"
#include "call_data_command.h"
#include "thread_safe_writes_waiting_to_process_vector.h"
#include "pushed_from_queue_location_enum.h"

#include "utility_chat_functions.h"
#include "utility_testing_functions.h"
#include "store_mongoDB_error_and_exception.h"

#include "grpc_mock_stream/mock_stream.h"

/** Some of the documentation is in the comments for specific variables.
 * Other parts can be found inside [src/utility/async_server/_documentation.md]. **/
class ChatStreamContainerObject {
private:

    friend class GrpcServerImpl;
    friend class BeginChatChangeStreamTests;
public:

    inline static std::atomic_long total_number_message_left_in_queue_at_end = 0;

    //all of these times are expected to be in microseconds
    inline static std::atomic_long total_chat_stream_inject_run_time = 0;
    inline static std::atomic_long total_chat_stream_server_shutting_down_run_time = 0;

    inline static std::atomic_long total_chat_stream_run_function_end_of_vector_writes_run_time = 0;
    inline static std::atomic_long total_chat_stream_run_function_write_with_vector_elements_run_time = 0;
    inline static std::atomic_long total_chat_stream_run_function_read_run_time = 0;
    inline static std::atomic_long total_chat_stream_run_function_alarm_queue_write_run_time = 0;
    inline static std::atomic_long total_chat_stream_run_function_finish_run_time = 0;
    inline static std::atomic_long total_chat_stream_run_function_done_run_time = 0;

    inline static std::atomic_long total_num_chat_stream_run_function_end_of_vector_writes_run = 0;
    inline static std::atomic_long total_num_chat_stream_run_function_write_with_vector_elements_run = 0;
    inline static std::atomic_long total_num_chat_stream_run_function_read_run = 0;
    inline static std::atomic_long total_num_chat_stream_run_function_alarm_queue_write_run = 0;
    inline static std::atomic_long total_num_chat_stream_run_function_finish_run = 0;
    inline static std::atomic_long total_num_chat_stream_run_function_done_run = 0;

    inline static std::atomic_int num_new_objects_created = 0;
    inline static std::atomic_int num_objects_deleted = 0;
    inline static std::atomic_int num_times_finish_called = 0;

    // Take in the "service" instance (in this case representing an asynchronous
    // server) and the completion vector "cq" used for asynchronous communication
    // with the gRPC runtime.
    ChatStreamContainerObject(
            grpc_stream_chat::StreamChatService::AsyncService* service,
            grpc::ServerCompletionQueue* cq
            );

#ifdef LG_TESTING

    ChatStreamContainerObject() : service_(nullptr), cq_(nullptr) {
        std::cout << "TESTING CONSTRUCTOR CALLED!" << std::endl;
        responder_ = std::make_unique<grpc::testing::MockServerAsyncReaderWriter<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>>();
    }

    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_upsert);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_find);
    FRIEND_TEST(MongoOidConcurrentContainer, mongoDBOIDContainer_runCommandOnAllOid);

    FRIEND_TEST(ServerInitializationFunctions, beginChatChangeStream_matchMade);
    FRIEND_TEST(BeginChatChangeStreamTests, matchMadeForBothUsers);
    FRIEND_TEST(BeginChatChangeStreamTests, orderingEnforced);

    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_upsert);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_upsert_coroutine);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_erase);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_erase_coroutine);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_iterateAndSendMessageUniqueMessageToSender);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_iterateAndSendMessageExcludeSender);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_sendMessageToSpecificUser);
    FRIEND_TEST(ChatStreamSharedInfo, ChatStreamConcurrentSet_sendMessagesToTwoUsers);

    FRIEND_TEST(AsyncServer_SingleOperation, refreshChatStream);
    FRIEND_TEST(AsyncServer_SingleOperation, refreshChatStream_withDelay);

    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_userJoined);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_userLeft);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_userKicked);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_userBanned);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanedSaveMessagesToMap_multipleInQueue);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, cleanUpObject);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_emptyChatRooms);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_withChatRooms);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidCurrentAccountId);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidLoggedInToken);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidLetsGoVersion);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidInstallationId);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_noMessages);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_validMetaData_multipleMessages);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedChatRoomId);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedLastTimeUpdated);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedLastTimeViewed);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedNumberMessageUuids);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_skippedMostRecentMessageUuid);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_incorrectNumberMessageUuid);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, extractLoginInfoFromMetaData_invalidMetaData_mostRecentMessageUuidTooLong);
    FRIEND_TEST(ChatStreamContainerObjectBasicTests, setupRequestFullMessageInfoErrorMessageReturn);

    FRIEND_TEST(RequestFullMessageInfoTests, validInfoWithMultipleMessages);
    FRIEND_TEST(RequestFullMessageInfoTests, invalidChatRoomId);
    FRIEND_TEST(RequestFullMessageInfoTests, noMessagesSent);
    FRIEND_TEST(RequestFullMessageInfoTests, userNotInChatRoom);
    FRIEND_TEST(RequestFullMessageInfoTests, onlyInvalidMessageUuid);
    FRIEND_TEST(RequestFullMessageInfoTests, requestZeroMessages);
    FRIEND_TEST(RequestFullMessageInfoTests, requestMoreThanMaxNumberMessages);
    FRIEND_TEST(RequestFullMessageInfoTests, requestInvalidTypes);

    FRIEND_TEST(ExtractChatRoomsTests, invalidChatRoomId);
    FRIEND_TEST(ExtractChatRoomsTests, userNotInsideAnyChatRooms);
    FRIEND_TEST(ExtractChatRoomsTests, chatRoomExcludedFromRequest);
    FRIEND_TEST(ExtractChatRoomsTests, extraChatRoomAddedToRequest);
    FRIEND_TEST(ExtractChatRoomsTests, chatRoomProperlyUpdated);
    FRIEND_TEST(ExtractChatRoomsTests, updateObservedTimeOnServer);

    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, notALeaveOrJoinMessage);
    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, joinMessageInFront);
    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, leaveMessageInFront);
    FRIEND_TEST(IterateAndSendMessagesToTargetUsersTests, multipleMessageInFront);

    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, noMessagesToRequest);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, requestNormalMessage);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, requestkDifferentUserJoinedMessage);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, inviteForCurrentUserIsPassedThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, inviteForOtherUserDoesNotPassThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForAllIsPassedThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForCurrentUserIsPassedThrough);
    FRIEND_TEST(SendPreviouslyStoredMessagesWithDifferentUserJoinedTests, deleteForOtherUserDoesNotPassThrough);

    friend class RequestFullMessageInfoTests;
    friend class IterateAndSendMessagesToTargetUsersTests;
    friend class SendPreviouslyStoredMessagesWithDifferentUserJoinedTests;

#endif

    void endStream(const grpc_stream_chat::StreamDownReasons& streamFinishedReason,
                   const std::string& optional_info = "");

    ThreadPoolSuspend injectStreamResponseCoroutine(
        std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> reference_to_current_chat_stream_object
    );

    //This function will inject a lambda into the chat stream to be sent
    // this function can and WILL BE CALLED FROM OTHER THREADS
    void injectStreamResponse(
        const std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector)>& outside_function,
        const std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>>& reference_to_current_chat_stream_object
    );

    std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> retrieveReferenceWrapperForCallData();

    long getCurrentIndexValue() const {
        return current_index_value;
    }

#ifdef LG_TESTING
    inline static std::chrono::milliseconds time_for_initialize_to_delay = std::chrono::milliseconds{0};
#endif

private:
    ThreadPoolSuspend streamIsFinished();

    bool refreshTimeoutTimePoint();

    //called from initialize and when END_STREAM_ALARM_CALLED is called with ok bit false
    void setEndStreamAlarmTimer();

    //called from initialize and when refreshTimeoutTimePoint() is called
    void generateStreamTimeOut(const std::chrono::system_clock::time_point& system_clock_now);

    ThreadPoolSuspend cleanedSaveMessagesToMap();

    ThreadPoolSuspend cleanUpObject();

    static bool extractLoginInfoFromMetaData(std::multimap<grpc::string_ref, grpc::string_ref>& metaData,
                                             grpc_stream_chat::InitialLoginMessageRequest& initialLoginMessageRequest);

    ThreadPoolSuspend initializeObject();

    static std::shared_ptr<grpc_stream_chat::ChatToClientResponse> setupRequestFullMessageInfoErrorMessageReturn(
            grpc_stream_chat::RequestFullMessageInfoResponse_RequestStatus requestStatus,
            const grpc_stream_chat::ChatToServerRequest &request_copy,
            const std::string &chat_room_id
            );

    void requestFullMessageInfo(
            const grpc_stream_chat::ChatToServerRequest& request_copy,
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector
    );

    ThreadPoolSuspend runReadFunction();

    ThreadPoolSuspend runServerShuttingDown(CallDataCommandTypes commandType);

    //FRIENDLY REMINDER: Do not pass any rvalue by reference to a coroutine. For example
    // with void foo(const std::string& str) if foo("hello") is called and suspended, the
    // variable str will hold a reference to garbage because of the rvalue "hello".
    ThreadPoolSuspend runFunction(CallDataCommandTypes commandType, bool ok_byte);

    ThreadPoolSuspend beginFinish();

    void incrementReferenceCount();

    void decrementReferenceCount();

//    ThreadPoolSuspend runWriteVectorElement();

    ThreadPoolSuspend runWriteCallbackFunction();

    //This is expected to be called ONLY when it has full control over the number_vector_values_remaining_to_be_written
    // value, in other words
    // number_vector_values_remaining_to_be_written == 0
    // AND
    // writes_waiting_to_process.size() == 1 (because the calling function pushed a value in)
//    ThreadPoolSuspend writeToClient(long number_values_remaining_to_be_written = -1L);

    void decrementNumberChatStreamsRunning() const;

    void incrementNumberChatStreamsRunning();

    enum WriteSuccessfulState {
        CONTAINER_WRITE_STATE_NONE,
        CONTAINER_WRITE_STATE_FAIL,
        CONTAINER_WRITE_STATE_SUCCESS
    };

    static inline std::string convertWriteSuccessfulStateToString(WriteSuccessfulState write_state) {
        std::string return_val = "Error invalid type";
        switch(write_state) {
            case CONTAINER_WRITE_STATE_NONE:
                return_val = "CONTAINER_WRITE_STATE_NONE";
                break;
            case CONTAINER_WRITE_STATE_FAIL:
                return_val = "CONTAINER_WRITE_STATE_FAIL";
                break;
            case CONTAINER_WRITE_STATE_SUCCESS:
                return_val = "CONTAINER_WRITE_STATE_SUCCESS";
                break;
        }
        return return_val;
    }

    ThreadPoolSuspend writeVectorElementBody(
            std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector,
            PushedToQueueFromLocation pushed_from_location,
            WriteSuccessfulState& write_state
    );

    //Set running_follow_up_write to true when the next element inside writes_waiting_to_process should run immediately. Set it to
    // false when it is the first time in a 'chain' that the 'Write()' is called.
    template <bool running_follow_up_write>
    ThreadPoolSuspend writeVectorElement(long number_values_remaining_to_be_written = -1) {

#ifdef CHAT_STREAM_LOG
        std::cout << "begin writeVectorElement() writes_waiting_to_process size: " << writes_waiting_to_process.size_for_testing() << '\n';
#endif

        std::shared_ptr<
                std::pair<
                        std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>&)>,
                        PushedToQueueFromLocation
                >
        > function_and_type;

        if(!running_follow_up_write) {
            RUN_COROUTINE(writes_waiting_to_process.front_if_not_empty, function_and_type);
        } else {
            //This will be the pop for the last 'Write()', not the current 'Write()'.
            bool empty_after_pop = true;

            RUN_COROUTINE(writes_waiting_to_process.pop_front_and_get_next, empty_after_pop, function_and_type);

            if (empty_after_pop) {
                const std::string errorString = "ChatStreamContainerObject::writeVectorElement() was called with no existing elements inside the writes_waiting_to_process queue.";

                std::optional<std::string> exceptionString;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        exceptionString,errorString,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "running_follow_up_write", "true"
                );

                co_return;
            }
        }

        if (function_and_type == nullptr || service_completed) {
            co_return;
        }

        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>> reply_vector;

        try {
            //run task
            function_and_type->first(reply_vector);
        } catch (const std::bad_function_call& e) {
            const std::string errorString = "ChatStreamContainerObject::writeVectorElement() had a bad_function_call exception thrown.";

            std::optional<std::string> exceptionString = e.what();
            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    exceptionString, errorString,
                    "PushedToQueueFromLocation",
                    convertPushedToQueueFromLocationToString(function_and_type->second),
                    "running_follow_up_write", (running_follow_up_write ? "true" : "false"));

            ERROR_INVALID_TYPE_FLAG = true;
        }

        WriteSuccessfulState write_state = WriteSuccessfulState::CONTAINER_WRITE_STATE_NONE;

        if (!reply_vector.empty()) {

            write_state = WriteSuccessfulState::CONTAINER_WRITE_STATE_FAIL;

            if(!running_follow_up_write) {
                number_vector_values_remaining_to_be_written =
                        number_values_remaining_to_be_written == -1 ? reply_vector.size() - 1
                                                                    : number_values_remaining_to_be_written - 1;
            } else {
                //NOTE: Unlike above inside (!running_follow_up_write) this is '+=' not just an '='. In order to
                // get smaller it must be able to subtract 1 when reply_vector.size() is 1. So it must have
                // a '-2' not a '-1'.
                number_vector_values_remaining_to_be_written += reply_vector.size() - 2;
            }

            RUN_COROUTINE(
                    writeVectorElementBody,
                    reply_vector,
                    function_and_type->second,
                    write_state
            );
        }

        if(write_state != WriteSuccessfulState::CONTAINER_WRITE_STATE_SUCCESS) {

            if (ERROR_INVALID_TYPE_FLAG) { //if error has been stored
                ERROR_INVALID_TYPE_FLAG = false;
            } else { //if error has not been stored
                std::optional<std::string> dummy_exception;
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__, dummy_exception,
                        std::string(
                                "ChatStreamContainerObject::writeVectorElement() had an empty_after_pop reply_vector returned."),
                        "PushedToQueueFromLocation",convertPushedToQueueFromLocationToString(function_and_type->second),
                        "number_vector_values_remaining_to_be_written", std::to_string(number_vector_values_remaining_to_be_written),
                        "running_follow_up_write", (running_follow_up_write ? "true" : "false"),
                        "write_state", convertWriteSuccessfulStateToString(write_state)
                );
            }

            //if not running follow up AND write_state was not sent to writeVectorElementBody()
            if(!running_follow_up_write && write_state == WriteSuccessfulState::CONTAINER_WRITE_STATE_NONE) {
                if(number_values_remaining_to_be_written <= 1) { //writes_waiting_to_process has 1 (or less) elements in it
                    number_vector_values_remaining_to_be_written = 0;
                } else { //more potential writes from writes_waiting_to_process
                    number_vector_values_remaining_to_be_written = number_values_remaining_to_be_written - 1;
                }
            } else {
                number_vector_values_remaining_to_be_written--;
            }

            if (number_vector_values_remaining_to_be_written == 0) {
                RUN_COROUTINE(runWriteCallbackFunction);
            } else {
                RUN_COROUTINE(writeVectorElement<true>);
            }
        }

#ifdef CHAT_STREAM_LOG
        std::cout << "end writeVectorElement() writes_waiting_to_process size: " << writes_waiting_to_process.size_for_testing() << '\n';
#endif

        co_return;
    }

    //the number of 'extra' values from the vector of writes_waiting_to_process to write before the writer
    // goes to the back of the vector
    //this is only accessed inside the 'Write()' commands which should inherently be thread safe (only 1 Write()
    // can run at a time in the grpc Async server)
    unsigned long number_vector_values_remaining_to_be_written = 0;

    //the functions that are waiting to be run
    ThreadSafeWritesWaitingToProcessVector<
        std::pair<
            std::function<void(std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply)>,
            PushedToQueueFromLocation
        >
    > writes_waiting_to_process;

    //this will store the chatRoomIds that the user is part of (or that this object is responsible for cleaning up)
    //this set is not made to be thread safe because
    // -it runs during Initialize, Write() and CleanUp
    // -initialize and clean up should not overlap
    // -initialize will need to complete before a 'Write()' can start
    // -only one Write() can be outstanding at a time
    // -cleanup will not run until initialize and all outstanding Write() have completed
    std::set<std::string> chat_room_ids_user_is_part_of;

    //set to a value greater than 0 (true) when the service calls beginFinish().
    std::atomic_bool service_completed = false;

    //Used with service_completed to protect the commands inside ReaderWriter
    // so that commands such as Read(), Write(), Set() etc... can not occur
    // during or after Finish() is being called.
    //Also used to make sure retrieveReferenceWrapperForCallData() cannot occur
    // after beginFinish() has been completed.
    std::atomic_int run_writer_atomic = 0;

    struct IncrementDecrementAtomic {

        explicit IncrementDecrementAtomic(std::atomic_int& _val) : val(_val) {
            val++;
        }

        ~IncrementDecrementAtomic() {
            decrement();
        }

        void decrement() {
            //only decrement once
            val -= num_to_decrement_by;
            num_to_decrement_by = 0;
        }

        IncrementDecrementAtomic() = delete;

        IncrementDecrementAtomic(IncrementDecrementAtomic& copy) = delete;

        IncrementDecrementAtomic(IncrementDecrementAtomic&& move) = delete;

        bool operator=(IncrementDecrementAtomic& rhs) = delete;

    private:
        std::atomic_int& val;
        int num_to_decrement_by = 1;
    };

    //the user account OID inside the map
    std::string current_user_account_oid_str;

    //set after the initialization has COMPLETED (before Read() and Write() are called)
    std::atomic_bool initialization_complete = false;

    //set to true when server is processing a 'request updates' message, then false when the
    // final message in that group is sent back
    std::atomic_bool currently_handling_updates = false;

    //the reference count to this ChatStreamContainerObject object
    std::atomic_int reference_count = 0;

    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    grpc_stream_chat::StreamChatService::AsyncService* service_;
    // The producer-consumer vector where for asynchronous server notifications.
    grpc::ServerCompletionQueue* cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    grpc::ServerContext ctx_;

    //Alarms
    //they cannot be destroyed until after the vector returns the message
    //the same alarm cannot be pushed to the CompletedQueue multiple times

    //used for sending a 'Write()' to the back of the CompletedQueue
    grpc::Alarm send_write_to_back_of_queue_alarm;

    //this will be set to cancel the server from running
    grpc::Alarm cancel_current_function_alarm;
    //set to true if time_out_stream_alarm is set, false if canceled
    std::atomic_bool cancel_current_function_alarm_set = false;

    //this will be set to make sure the stream can not be left hanging by a client improperly shutting down
    grpc::Alarm time_out_stream_alarm;
    //set to true if time_out_stream_alarm is set, false if canceled
    std::atomic_bool time_out_stream_alarm_set = false;

    //the end time_point for when the stream will time out
    std::chrono::system_clock::time_point end_stream_time;
    //the end time_point for when the stream will not allow the timer to be refreshed
    std::chrono::system_clock::time_point end_stream_refresh_time;

    //These are here because the request will be returned from the Read() function.
    //The reply however is simply more efficient to clear() than create a new one. Because
    // only one Write() function can be running at a time, it is thread safe.
    grpc_stream_chat::ChatToServerRequest request_;
    grpc_stream_chat::ChatToClientResponse reply_;

    // The means to get back to the client.
    std::unique_ptr<grpc::ServerAsyncReaderWriterInterface<grpc_stream_chat::ChatToClientResponse, grpc_stream_chat::ChatToServerRequest>> responder_ = nullptr;

    //TAGS
    //NOTE: These contain the pointer to this object as well as the enum. This means they cannot
    // be global or static. Also, there must be a guarantee that all outstanding references to them
    // are gone before this object calls delete on itself. This is done by keeping track of
    // outstanding alarms and outstanding operations and the idea that whatever goes into the
    // completion queue must be returned from the completion queue.
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> INITIALIZE_TAG =
            generateTag(CallDataCommandTypes::INITIALIZE);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> READ_TAG =
            generateTag(CallDataCommandTypes::READ);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> END_OF_VECTOR_WRITES_TAG =
            generateTag(CallDataCommandTypes::END_OF_VECTOR_WRITES);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> WRITE_WITH_VECTOR_ELEMENTS_REMAINING_TAG =
            generateTag(CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> ALARM_QUEUE_WRITE_AT_END_TAG =
            generateTag(CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> FINISH_TAG =
            generateTag(CallDataCommandTypes::FINISH);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> END_STREAM_ALARM_CALLED_TAG =
            generateTag(CallDataCommandTypes::END_STREAM_ALARM_CALLED);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> CHAT_STREAM_CANCELLED_ALARM_TAG =
            generateTag(CallDataCommandTypes::CHAT_STREAM_CANCELLED_ALARM);
    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> NOTIFY_WHEN_DONE_TAG =
            generateTag(CallDataCommandTypes::NOTIFY_WHEN_DONE);

    std::unique_ptr<CallDataCommand < ChatStreamContainerObject>> generateTag(
            const CallDataCommandTypes&& commandType
    ) {
        return std::make_unique<CallDataCommand < ChatStreamContainerObject>>
                (commandType, this);
    }

    //this is simply used to not double store a specific error
    bool ERROR_INVALID_TYPE_FLAG = false;

    grpc::Status return_status_code = grpc::Status::OK;

    //returned with the trailing metadata
    std::atomic<grpc_stream_chat::StreamDownReasons> stream_finished_reason = grpc_stream_chat::StreamDownReasons::UNKNOWN_STREAM_STOP_REASON;
    ReturnStatus stream_finished_return_status = ReturnStatus::VALUE_NOT_SET;
    std::string optional_info;

    bool number_chat_streams_running_incremented = false;

    std::atomic_bool clean_up_running = false;

    //outstanding alarms
    std::atomic_int number_outstanding_write_to_back_of_queue_alarm = 0;
    std::atomic_int number_outstanding_time_out_stream_alarm = 0;
    std::atomic_int number_outstanding_chat_stream_cancelled = 0;

    //outstanding operations
    //std::atomic_int outstanding_notify_when_done_in_completion_queue = 0;
    std::atomic_int outstanding_read_in_completion_queue = 0;
    std::atomic_int outstanding_write_in_completion_queue = 0;

    inline static std::atomic_long chat_stream_container_index = 0;
    long current_index_value = -1;

};