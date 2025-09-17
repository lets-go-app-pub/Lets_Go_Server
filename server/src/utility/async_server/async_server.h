//
// Created by jeremiah on 5/6/21.
//
#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <mutex>
#include <condition_variable>
#include <atomic>

#include <grpcpp/impl/codegen/completion_queue.h>
#include <ChatMessageStream.grpc.pb.h>
#include <general_values.h>
#include <coroutine_type.h>
#include <call_data_command.h>
#include <grpcpp/resource_quota.h>

class ChatStreamContainerObject;

/** Can read src/utility/async_server/_documentation.md for how this relates to the chat stream. **/

class GrpcServerImpl final {
public:

    void beginShutdownOnSeparateThread();

    //NOTE: There is a bit of overhead with a lambda parameter that is only used for testing. However,
    // Run() is only called once, so it should be negligible.
    void Run(const std::function<void()>& server_started_callback = [](){});

    virtual ~GrpcServerImpl();

private:

    ThreadPoolSuspend runNextCoroutine(
            bool ok_byte,
            CallDataCommand<ChatStreamContainerObject>* callDataCommandPtr
            );

    /** Only run a single thread in HandleRpcs (or at least keep each service confined to a thread). This
     * is because when cleaning up a service, it is important to know that there are no more calls
     * (Read(), Write() etc...) that have not at least had their thread initialized after Finish() is
     * returned to the ChatStreamContainerObject. Otherwise the thread could attempt to reference a deleted pointer. **/
    void HandleRpcs(const std::function<void()>& server_started_callback);

    enum class ServerStatus {
        SERVER_RUNNING, //server is up and running
        RETURN_SERVER_DOWN, //server is waiting for threads to finishBiDiStream while sending server down to any that are not complete
        STOP_ACCEPTING_THREADS, //server will not return any more new connections to this server
        SERVER_DOWN //server has been ShutDown
    };

    static std::string serverStatusToString(const ServerStatus& serverStatus) {
        switch (serverStatus) {
            case ServerStatus::SERVER_RUNNING:
                return "SERVER_RUNNING";
            case ServerStatus::RETURN_SERVER_DOWN:
                return "RETURN_SERVER_DOWN";
            case ServerStatus::STOP_ACCEPTING_THREADS:
                return "STOP_ACCEPTING_THREADS";
            case ServerStatus::SERVER_DOWN:
                return "SERVER_DOWN";
        }

        return "{Unknown Value}";
    }

    std::atomic<ServerStatus> server_status = ServerStatus::STOP_ACCEPTING_THREADS;
    std::atomic_int num_threads_outstanding = 0;

    std::shared_ptr<grpc::ServerCompletionQueue> cq_ = nullptr;
    std::shared_ptr<grpc::Server> server_ = nullptr;
    grpc_stream_chat::StreamChatService::AsyncService service_;
    grpc::ResourceQuota rq;

    std::mutex server_shut_down_mutex;
    std::condition_variable wait_for_server_shutdown;

    std::atomic_bool shut_down_started = false;
    std::unique_ptr<std::thread> shut_down_thread = nullptr;

    //simple class to run the passed function when destructor is called
    class RunFunctionOnDestruction {
    public:
        RunFunctionOnDestruction() = delete;

        [[maybe_unused]] explicit RunFunctionOnDestruction(std::function<void()>&& _function_to_run) : function_to_run(
                std::move(_function_to_run)) {}

        ~RunFunctionOnDestruction() {
#ifdef CHAT_STREAM_LOG
            std::cout << "Running ~RunFunctionOnDestruction() (from async_server)\n";
#endif
            function_to_run();
        }

    private:
        const std::function<void()> function_to_run;
    };

    void shutDown(const std::chrono::milliseconds& wait_time_before_shut_down);

#ifdef LG_TESTING

    long total_server_run_time = 0;

public:
#endif
    void blockWhileWaitingForShutdown(const std::chrono::milliseconds& shutdown_time);

};


