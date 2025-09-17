//
// Created by jeremiah on 6/27/22.
//

#include <condition_variable>
#include "start_bi_di_stream.h"

/** send the 2 std::function parameters as actual parameters, if auto is used to catch a lambda something
 * weird happens where this function finishes and returns the thread, then if callback_when_message_received or
 * callback_when_finished are invoked, it can seg fault **/
std::jthread startBiDiStream(
        std::shared_ptr<TestingClient> client,
        const UserAccountDoc& user_info,
        const std::function<void(
                const grpc_stream_chat::ChatToClientResponse& response)>& callback_when_message_received,
        const std::string& chat_rooms_string,
        const TestingStreamOptions& testingStreamOptions,
        const std::function<void()>& callback_when_completed
) {

    std::condition_variable cond;
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);

    auto server_started_callback = [&]() {
        cond.notify_all();
        mutex.unlock();
    };

    std::jthread t1{[&, callback_when_completed_copy = callback_when_completed]() {

        mutex.lock();
        client->startBiDiStream(
                user_info,
                callback_when_message_received,
                server_started_callback,
                callback_when_completed_copy,
                chat_rooms_string,
                testingStreamOptions
        );
    }};

    cond.wait(lock);
    lock.unlock();

    return t1;
}