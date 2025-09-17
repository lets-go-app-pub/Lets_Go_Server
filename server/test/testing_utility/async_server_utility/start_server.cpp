//
// Created by jeremiah on 5/11/21.
//

#include <condition_variable>
#include "start_server.h"

std::jthread startServer(std::shared_ptr<GrpcServerImpl> grpcServerImpl) {

    std::condition_variable cond;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    auto server_started_callback = [&](){
        mutex.unlock();
        cond.notify_all();
    };

    std::jthread t1{[&](){
        mutex.lock();
        grpcServerImpl->Run(server_started_callback);
    }};

    cond.wait(lock);
    lock.unlock();

    return t1;
}