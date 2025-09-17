//
// Created by jeremiah on 10/26/21.
//

#include "async_server.h"

#include <grpcpp/grpcpp.h>
#include <shared_mutex>

#include <email_sending_messages.h>
#include <request_admin_info.h>
#include <handle_feedback.h>
#include <request_user_account_info.h>
#include <handle_reports.h>
#include <admin_chat_room_commands.h>
#include <login_function.h>
#include <sMS_verification.h>
#include <login_support_functions.h>
#include <send_error_to_server.h>
#include <request_fields_functions.h>
#include <set_fields_functions.h>
#include <find_matches.h>
#include <user_match_options.h>
#include <set_admin_fields.h>
#include <chat_room_commands.h>
#include <send_pictures_for_testing.h>
#include <server_values.h>

#include "chat_stream_container_object.h"
#include "user_open_chat_streams.h"
#include "thread_pool_global_variable.h"
#include "server_accepting_connections_bool.h"
#include "request_statistics.h"
#include "manage_server_commands.h"
#include "server_initialization_functions.h"
#include "retrieve_server_load.h"
#include "handle_errors.h"
#include "general_values.h"
#include "grpc_values.h"
#include "admin_event_commands.h"
#include "user_event_commands.h"

void GrpcServerImpl::beginShutdownOnSeparateThread() {
    bool false_bool = false;
    if (shut_down_started.compare_exchange_strong(false_bool, true)) {
        shut_down_thread = std::make_unique<std::thread>(
                std::thread(&GrpcServerImpl::blockWhileWaitingForShutdown, this,
#ifdef LG_TESTING
                            std::chrono::milliseconds{100}
#else
                        general_values::TIME_BETWEEN_SERVER_SHUT_DOWN_STATES
#endif
                            )
                );
    }
}

void GrpcServerImpl::blockWhileWaitingForShutdown(const std::chrono::milliseconds& passed_shutdown_time) {

    //NOTE: The synchronous server seems to take care of its own shut-downs (at least on the unary
    // calls) most of this is for the asynchronous service.

    server_status = ServerStatus::RETURN_SERVER_DOWN;
    std::cout << "server_status: RETURN_SERVER_DOWN" << std::endl;

    server_accepting_connections = false;

    cancelChatChangeStream();

    //Sleep while all messages are sent that were in queue by the change stream.
    std::this_thread::sleep_for(
            chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME
            + chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING
            + chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME
            + std::chrono::milliseconds{100}
    );

    user_open_chat_streams.runCommandOnAllOid([](ChatStreamContainerObject* call_data) {
        call_data->endStream(grpc_stream_chat::StreamDownReasons::SERVER_SHUTTING_DOWN);
    });

    //Sleep for an arbitrary amount of time and attempt to allow all threads to complete.
    std::this_thread::sleep_for(passed_shutdown_time);

    server_status = ServerStatus::STOP_ACCEPTING_THREADS;
    std::cout << "server_status: STOP_ACCEPTING_THREADS" << std::endl;

    //This value multiplied by the milliseconds the loop below sleeps is the max time to wait, so 50 is 50 * 100 = 5000ms.
    unsigned int max_number_iterations = 50;

    for (unsigned int i = 0; i < max_number_iterations && 0 < num_threads_outstanding; i++) {
        std::cout << "num_threads_outstanding: " << num_threads_outstanding << '\n';
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }

    std::unique_lock<std::mutex> lock(server_shut_down_mutex);

    server_status = ServerStatus::SERVER_DOWN;
    std::cout << "server_status: SERVER_DOWN" << std::endl;

    shutDown(passed_shutdown_time);

    wait_for_server_shutdown.notify_all();
    lock.unlock();

    //Wait a little AFTER the shutdown as well to attempt for anything still running to end.
    std::this_thread::sleep_for(passed_shutdown_time);

}

std::string readSSLKeyIn(const std::string& file_name, const std::string& path = general_values::SSL_FILE_DIRECTORY) {
    std::ifstream fin(path + file_name);
    std::string returnVal;
    char c;
    while (fin.get(c)) {
        returnVal += c;
    }
    fin.close();
    return returnVal;
}

void GrpcServerImpl::Run(const std::function<void()>& server_started_callback) {

    LoginServiceImpl loginFunctionService;
    SMSVerificationServiceImpl smsVerificationService;
    LoginSupportFunctionsServiceImpl loginSupportFunctionsService;
    SendErrorToServerServiceImpl sendErrorToServerService;
    RequestFieldsServiceImpl requestFieldsService;
    SetFieldsServiceImpl setFieldsService;
    FindMatchesServiceImpl findMatchesServiceImpl;
    UserMatchOptionsImpl userMatchOptionsImpl;
    SetAdminFieldsImpl setAdminFieldsImpl;
    ChatRoomCommandsImpl chatRoomCommandsImpl;
    RetrieveServerLoadImpl retrieveServerLoadImpl;
    EmailSendingMessagesImpl emailSendingMessagesImpl;
    RequestAdminInfoImpl requestAdminInfoImpl;
    RequestStatisticsImpl requestStatisticsImpl;
    HandleFeedbackImpl handleFeedbackImpl;
    RequestUserAccountInfoServiceImpl requestUserAccountInfoServiceImpl;
    HandleReportsServiceImpl handleReportsServiceImpl;
    AdminChatRoomCommandsServiceImpl adminChatRoomCommandsServiceImpl;
    ManageServerCommandsImpl manageServerCommandsImpl;
    HandleErrorsImpl requestErrorsImpl;
    AdminEventCommandsServiceImpl adminEventCommandsServiceImpl;
    UserEventCommandsServiceImpl userEventCommandsServiceImpl;

#ifdef _DEBUG
    //NOTE: this is only used for testing
    SendPicturesForTestingServiceImpl sendPicturesForTestingServiceImpl;
#endif

    const std::string key = readSSLKeyIn("grpc_private_key.key");
    const std::string cert = readSSLKeyIn("grpc_public_cert.pem");

    //self-signed certificate, does not have a CA (certificate authority)
    grpc::SslServerCredentialsOptions sslOps(GRPC_SSL_REQUEST_CLIENT_CERTIFICATE_AND_VERIFY);

    sslOps.pem_key_cert_pairs.push_back(grpc::SslServerCredentialsOptions::PemKeyCertPair{key, cert});

    grpc::ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(current_server_uri,

#ifdef LG_TESTING
    grpc::InsecureServerCredentials()
    //grpc::SslServerCredentials(sslOps)
#else
    //grpc::InsecureServerCredentials()
    grpc::SslServerCredentials(sslOps)
#endif
    );

    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to a *synchronous* service.
    builder.RegisterService(&loginFunctionService);
    builder.RegisterService(&smsVerificationService);
    builder.RegisterService(&loginSupportFunctionsService);
    builder.RegisterService(&sendErrorToServerService);
    builder.RegisterService(&requestFieldsService);
    builder.RegisterService(&setFieldsService);
    builder.RegisterService(&findMatchesServiceImpl);
    builder.RegisterService(&userMatchOptionsImpl);
    builder.RegisterService(&setAdminFieldsImpl);
    builder.RegisterService(&chatRoomCommandsImpl);
    builder.RegisterService(&retrieveServerLoadImpl);
    builder.RegisterService(&emailSendingMessagesImpl);
    builder.RegisterService(&requestAdminInfoImpl);
    builder.RegisterService(&requestStatisticsImpl);
    builder.RegisterService(&handleFeedbackImpl);
    builder.RegisterService(&requestUserAccountInfoServiceImpl);
    builder.RegisterService(&handleReportsServiceImpl);
    builder.RegisterService(&adminChatRoomCommandsServiceImpl);
    builder.RegisterService(&manageServerCommandsImpl);
    builder.RegisterService(&requestErrorsImpl);
    builder.RegisterService(&adminEventCommandsServiceImpl);
    builder.RegisterService(&userEventCommandsServiceImpl);

#ifdef _DEBUG
    //NOTE: this is only used for testing
    builder.RegisterService(&sendPicturesForTestingServiceImpl);
#endif

    builder.AddChannelArgument(GRPC_ARG_MAX_CONNECTION_IDLE_MS, grpc_values::CONNECTION_IDLE_TIMEOUT_IN_MS);

    //NOTE: 100 is also the default value, simply being explicit here.
    builder.AddChannelArgument(GRPC_ARG_MAX_CONCURRENT_STREAMS, 100);

    builder.AddChannelArgument(GRPC_ARG_MAX_METADATA_SIZE, grpc_values::MAXIMUM_RECEIVING_META_DATA_SIZE);

    builder.SetMaxReceiveMessageSize(grpc_values::MAX_RECEIVE_MESSAGE_LENGTH);
    builder.SetMaxSendMessageSize(grpc_values::MAX_SEND_MESSAGE_LENGTH);

    //It seems that a thread is needed for every completion queue. So if new_max_threads=4 with
    // 1 completion queue, 3 threads remain for processing requests. Also note that this is
    // different from builder.SetSyncServerOption(grpc::ServerBuilder::MAX_POLLERS).
    //NOTE: If SetMaxThreads is set on the server, it will crash when attempting to set defaults. This means most
    // likely, it will crash with enough connections. Must leave it off for now. Also, it may degrade performance
    // https://github.com/grpc/grpc/issues/28646.
    //rq.SetMaxThreads((int)std::thread::hardware_concurrency());
    builder.SetResourceQuota(rq);

    builder.RegisterService(&service_);

    cq_ = builder.AddCompletionQueue();

    server_ = builder.BuildAndStart();

    server_status = ServerStatus::SERVER_RUNNING;

    std::cout << "Server listening on " << current_server_uri << std::endl;
    // Proceed to the server's main loop.
    HandleRpcs(server_started_callback);
}

ThreadPoolSuspend GrpcServerImpl::runNextCoroutine(
        bool ok_byte,
        CallDataCommand<ChatStreamContainerObject>* callDataCommandPtr
) {

    if (callDataCommandPtr->commandType == CallDataCommandTypes::FINISH) {
        //Do not allocate a 'reference' for the FINISH type, otherwise it will
        // run decrementReferenceCount() AFTER the object was deleted.
        callDataCommandPtr->currentObject->decrementReferenceCount();
        RUN_COROUTINE(callDataCommandPtr->currentObject->streamIsFinished);
        num_threads_outstanding--;
    } else {

        RunFunctionOnDestruction runFunctionOnDestruction(
                [&]() {
                    callDataCommandPtr->currentObject->decrementReferenceCount();

                    num_threads_outstanding--;
                }
        );

        //These values can be decremented before the function runs because there is still the reference count
        // from the runFunctionOnDestruction object above. This means that the ChatStreamContainerObject cannot
        // be deleted before the function runs.
        switch (callDataCommandPtr->commandType) {
            case CallDataCommandTypes::INITIALIZE:
                //controlled by initialization_complete boolean
                break;
            case CallDataCommandTypes::READ:
                callDataCommandPtr->currentObject->outstanding_read_in_completion_queue--;
                break;
            case CallDataCommandTypes::END_OF_VECTOR_WRITES:
            case CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING:
                callDataCommandPtr->currentObject->outstanding_write_in_completion_queue--;
                break;
            case CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END:
                callDataCommandPtr->currentObject->number_outstanding_write_to_back_of_queue_alarm--;
                break;
            case CallDataCommandTypes::FINISH:
                break;
            case CallDataCommandTypes::END_STREAM_ALARM_CALLED:
                callDataCommandPtr->currentObject->number_outstanding_time_out_stream_alarm--;
                break;
            case CallDataCommandTypes::CHAT_STREAM_CANCELLED_ALARM:
                callDataCommandPtr->currentObject->number_outstanding_chat_stream_cancelled--;
                break;
            case CallDataCommandTypes::NOTIFY_WHEN_DONE:
                break;
        }

        if (server_status == ServerStatus::SERVER_RUNNING) {
            RUN_COROUTINE(callDataCommandPtr->currentObject->runFunction, callDataCommandPtr->commandType, ok_byte);
            //callDataCommandPtr->currentObject->runFunction(callDataCommandPtr->commandType, ok_byte);
        } else { //ServerStatus::RETURN_SERVER_DOWN
            RUN_COROUTINE(callDataCommandPtr->currentObject->runServerShuttingDown, callDataCommandPtr->commandType);
        }
    }

    co_return;
}

void GrpcServerImpl::HandleRpcs(const std::function<void()>& server_started_callback) {

    // Spawn a new ChatStreamContainerObject instance to serve new clients.
    ChatStreamContainerObject::num_new_objects_created++;
    new ChatStreamContainerObject(&service_, cq_.get());
    void* tag;  // uniquely identifies a request.
    bool ok;
    bool completion_queue_active = true;

#ifdef LG_TESTING
    std::chrono::milliseconds timestamp = getCurrentTimestamp();
#endif

    server_started_callback();
    while (completion_queue_active) {
        // Block waiting to read the next event from the completion vector. The
        // event is uniquely identified by its tag, which in this case is the
        // memory address of a ChatStreamContainerObject instance.
        // The return value of Next should always be checked. This return value
        // tells us whether there is any kind of event or cq_ is shutting down.

#ifdef CHAT_STREAM_LOG
        std::cout << "Waiting on Next()\n";
#endif
#ifdef LG_TESTING
        total_server_run_time += getCurrentTimestamp().count() - timestamp.count();
#endif

        completion_queue_active = cq_->Next(&tag, &ok);
        if (!completion_queue_active) {
            break;
        }

        switch (server_status) {
            case ServerStatus::SERVER_RUNNING:
            case ServerStatus::RETURN_SERVER_DOWN: {

#ifdef CHAT_STREAM_LOG
                std::stringstream print_info_string;
                print_info_string << "ok: " << (ok ? "true" : "false") << " server_status: " << serverStatusToString(
                        server_status);

                std::cout << print_info_string.str() << " commandType: " << convertCallDataCommandTypesToString(
                        static_cast<CallDataCommand<ChatStreamContainerObject>*>(tag)->commandType) << '\n';
#endif

                auto* callDataCommandPtr = static_cast<CallDataCommand<ChatStreamContainerObject>*>(tag);

                //std::string command_type = printCallDataCommandTypes(callDataCommandPtr->commandType);
                callDataCommandPtr->currentObject->incrementReferenceCount();
                num_threads_outstanding++;

                thread_pool.submit_coroutine(runNextCoroutine(ok, callDataCommandPtr).handle);

                break;
            }
            case ServerStatus::STOP_ACCEPTING_THREADS:
            case ServerStatus::SERVER_DOWN: {

                //NOTE: the problem with the server shutdown is using the responder_ inside the function, if something
                // like Write() Read() or Finish() (or any of the other commands are called) AFTER the server is shut down
                // then there is a good chance the server will seg fault, so this will stop explicitly the clients that the
                // server is going down here

                //this will make it no longer accept messages, it will move down to the condition variable if SERVER_DOWN is
                // not yet set, if it is set then it will simply end
                completion_queue_active = false;
                break;
            }
        }
    }

    std::unique_lock<std::mutex> lock(server_shut_down_mutex);

    if (server_status != ServerStatus::SERVER_DOWN) {
        wait_for_server_shutdown.wait(lock);
    }

    lock.unlock();
}

void GrpcServerImpl::shutDown(const std::chrono::milliseconds& wait_time_before_shut_down) {
    //this will block until all the sync API is shutdown (the async should already be shut down)
    server_->Shutdown(std::chrono::system_clock::now() + wait_time_before_shut_down);
    // Always shutdown the completion vector after the server.
    cq_->Shutdown();
}

GrpcServerImpl::~GrpcServerImpl() {
    if (shut_down_thread) {
        shut_down_thread->join();
    }
#ifdef LG_TESTING
    std::cout << "total_server_run_time: " << total_server_run_time << '\n';
    std::cout << "total_number_message_left_in_queue_at_end: " << ChatStreamContainerObject::total_number_message_left_in_queue_at_end << "\n";

    std::cout << "\nvalues from ChatStreamContainerObject\n";
    std::cout << "total_chat_stream_inject_run_time: " << ChatStreamContainerObject::total_chat_stream_inject_run_time << "us\n";
    std::cout << "total_chat_stream_server_shutting_down_run_time: " << ChatStreamContainerObject::total_chat_stream_server_shutting_down_run_time << "us\n";

    std::cout << "\nvalues from inside ChatStreamContainerObject::runFunction()\n";
    std::cout << "total_chat_stream_run_function_end_of_vector_writes_run_time: "
        << ChatStreamContainerObject::total_chat_stream_run_function_end_of_vector_writes_run_time/(ChatStreamContainerObject::total_num_chat_stream_run_function_end_of_vector_writes_run==0?1:ChatStreamContainerObject::total_num_chat_stream_run_function_end_of_vector_writes_run.load())
        << "us(avg), " << ChatStreamContainerObject::total_chat_stream_run_function_end_of_vector_writes_run_time << "us(total), "
        << ChatStreamContainerObject::total_num_chat_stream_run_function_end_of_vector_writes_run << "(num)\n";

    std::cout << "total_chat_stream_run_function_write_with_vector_elements_run_time: "
    << ChatStreamContainerObject::total_chat_stream_run_function_write_with_vector_elements_run_time/(ChatStreamContainerObject::total_num_chat_stream_run_function_write_with_vector_elements_run==0?1:ChatStreamContainerObject::total_num_chat_stream_run_function_write_with_vector_elements_run.load())
    << "us(avg), " << ChatStreamContainerObject::total_chat_stream_run_function_write_with_vector_elements_run_time << "us(total), "
    << ChatStreamContainerObject::total_num_chat_stream_run_function_write_with_vector_elements_run << "(num)\n";

    std::cout << "total_chat_stream_run_function_read_run_time: "
    << ChatStreamContainerObject::total_chat_stream_run_function_read_run_time/(ChatStreamContainerObject::total_num_chat_stream_run_function_read_run==0?1:ChatStreamContainerObject::total_num_chat_stream_run_function_read_run.load())
    << "us(avg), " << ChatStreamContainerObject::total_chat_stream_run_function_read_run_time << "us(total), "
    << ChatStreamContainerObject::total_num_chat_stream_run_function_read_run << "(num)\n";

    std::cout << "total_chat_stream_run_function_alarm_queue_write_run_time: "
    << ChatStreamContainerObject::total_chat_stream_run_function_alarm_queue_write_run_time/(ChatStreamContainerObject::total_num_chat_stream_run_function_alarm_queue_write_run==0?1:ChatStreamContainerObject::total_num_chat_stream_run_function_alarm_queue_write_run.load())
    << "us(avg), " << ChatStreamContainerObject::total_chat_stream_run_function_alarm_queue_write_run_time << "us(total), "
    << ChatStreamContainerObject::total_num_chat_stream_run_function_alarm_queue_write_run << "(num)\n";

    std::cout << "total_chat_stream_run_function_finish_run_time: "
    << ChatStreamContainerObject::total_chat_stream_run_function_finish_run_time/(ChatStreamContainerObject::total_num_chat_stream_run_function_finish_run==0?1:ChatStreamContainerObject::total_num_chat_stream_run_function_finish_run.load())
    << "us(avg), " << ChatStreamContainerObject::total_chat_stream_run_function_finish_run_time << "us(total), "
    << ChatStreamContainerObject::total_num_chat_stream_run_function_finish_run << "(num)\n";

    std::cout << "total_chat_stream_run_function_done_run_time: "
    << ChatStreamContainerObject::total_chat_stream_run_function_done_run_time/(ChatStreamContainerObject::total_num_chat_stream_run_function_done_run==0?1:ChatStreamContainerObject::total_num_chat_stream_run_function_done_run.load())
    << "us(avg), " << ChatStreamContainerObject::total_chat_stream_run_function_done_run_time << "us(total), "
    << ChatStreamContainerObject::total_num_chat_stream_run_function_done_run << "(num)\n";

    std::cout << "\nnum_new_objects_created: " << ChatStreamContainerObject::num_new_objects_created << '\n';
    std::cout << "num_objects_deleted: " << ChatStreamContainerObject::num_objects_deleted << '\n';
    std::cout << "num_times_finish_called: " << ChatStreamContainerObject::num_times_finish_called << '\n';

    //Number of chat rooms 'left over' because they are never removed from map_of_chat_rooms_to_users.
    //std::cout << "\nmap_of_chat_rooms_to_users.size(): " << map_of_chat_rooms_to_users.size() << '\n';

#endif
    std::cout << "~GrpcServerImpl() called\n";
}
