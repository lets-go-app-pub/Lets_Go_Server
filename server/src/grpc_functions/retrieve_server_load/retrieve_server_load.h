//
// Created by jeremiah on 4/18/21.
//
#pragma once

#include <RetrieveServerLoad.grpc.pb.h>
#include <number_chat_streams_running.h>
#include <store_mongoDB_error_and_exception.h>
#include <grpc_values.h>
#include "server_accepting_connections_bool.h"
#include <bsoncxx/builder/stream/document.hpp>

using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;

class RetrieveServerLoadImpl final : public retrieve_server_load::RetrieveServerLoadService::Service {
public:

    //Protects last_time_run variable so condition can be checked.
    CoroutineSpinlock spin_lock;

    //Make sure this is longer than the estimated time to ping the server, otherwise there is no point.
    const std::chrono::milliseconds TIME_BETWEEN_REQUESTS_MS = std::chrono::milliseconds{30 * 1000};

    std::atomic<std::chrono::nanoseconds> ping_time_to_server = std::chrono::nanoseconds{-1};
    std::chrono::milliseconds last_time_run = std::chrono::milliseconds{-1};

    grpc::Status RetrieveServerLoadRPC(
            grpc::ServerContext*,
            const retrieve_server_load::RetrieveServerLoadRequest* request,
            retrieve_server_load::RetrieveServerLoadResponse* response
    ) override {

#ifndef _RELEASE
        std::cout << "Starting Retrieve Server Load...\n";
#endif // _RELEASE

        if (!server_accepting_connections) {
            response->set_accepting_connections(false);
            response->set_num_clients(-1);
        } else {

            //The client uses this RetrieveServerLoad call in order to get an idea of latency to this server. So
            // A good way to give a more realistic round trip latency is to either ping the database or sleep for
            // the time it takes to ping the database.
            delayForDatabasePing();

            int load = -1;

            if (request->request_num_clients()) {
                load = number_chat_streams_running;

                if (load < 0) {

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(),
                            std::string("number_chat_streams_running was set to a value less than 0."),
                            "number_chat_streams_running", std::to_string(number_chat_streams_running)
                    );
                    load = 0;

                    if (number_chat_streams_running < 0) {
                        number_chat_streams_running = 0;
                    }
                }
            }

            response->set_accepting_connections(true);
            response->set_num_clients(load);
        }

#ifndef _RELEASE
        std::cout << "Finishing Retrieve Server Load...\n";
#endif // _RELEASE

        return grpc::Status::OK;
    }

private:
    void delayForDatabasePing() {

        std::chrono::nanoseconds instance_ping_time = ping_time_to_server;
        {
            const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
            SpinlockWrapper spin_lock_wrapper{spin_lock};

            if(current_timestamp - last_time_run > TIME_BETWEEN_REQUESTS_MS) { //Recalculate ping time to server
                last_time_run = current_timestamp;
            } else {
                instance_ping_time = ping_time_to_server;
            }
        }

        if(instance_ping_time > std::chrono::milliseconds{-1}) {
            //Sleep to replicate ping.
            std::this_thread::sleep_for(instance_ping_time);
        } else {
            //Run an actual ping and save the resulting time.
            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

            mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

            const std::chrono::nanoseconds start_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch());

            //Hello is essentially a ping to the server. Some basic info is returned, but it isn't viewed.
            accounts_db.run_command(
                document{}
                    << "hello" << 1
                << finalize
            );

            const std::chrono::nanoseconds end_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch());

            ping_time_to_server = end_time - start_time;
        }
    }

};
