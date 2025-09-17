//
// Created by jeremiah on 5/10/21.
//
#pragma once

#include <grpcpp/create_channel.h>
#include <ChatMessageStream.grpc.pb.h>
#include <account_objects.h>
#include <chat_stream_container.h>
#include <gtest/gtest.h>
#include <general_values.h>
#include "thread"
#include "grpc_values.h"

class TestingStreamOptions {

    friend class TestingClient;

    enum TestingOptions {
        NONE,
        SKIP_STATUS_CHECK,
        CANCEL_STREAM,
        RUN_DEADLINE,
        DROP_CHANNEL
    };

    std::chrono::milliseconds delay_initialization_time = std::chrono::milliseconds{-1};
    std::chrono::milliseconds deadline_time = std::chrono::milliseconds{-1};
    TestingOptions testingOptions = TestingOptions::NONE;

public:
    void setCancelStream() {
        testingOptions = TestingOptions::CANCEL_STREAM;
    }

    void setRunDeadline(const std::chrono::milliseconds& _deadline_time) {
        testingOptions = TestingOptions::RUN_DEADLINE;
        this->deadline_time = _deadline_time;
    }

    void setDropChannel() {
        testingOptions = TestingOptions::DROP_CHANNEL;
    }

    void setSkipStatusCheck() {
        testingOptions = TestingOptions::SKIP_STATUS_CHECK;
    }

    void setInitializationDelay(const std::chrono::milliseconds& initialization_delay) {
        delay_initialization_time = initialization_delay;
    }

};

inline std::atomic_int index_testing_client = 0;
inline bool print_stream_stuff = true;

class TestingClient {
public:
    void startBiDiStream(
            const UserAccountDoc& user_info,
            const std::function<void(
                    const grpc_stream_chat::ChatToClientResponse& response)>& callback_when_message_received,
            const std::function<void()>& callback_when_started,
            const std::function<void()>& callback_when_completed,
            const std::string& chat_rooms_string,
            const TestingStreamOptions& testingStreamOptions
    ) {

        num_times_start_called++;

        context = new grpc::ClientContext;

        context->AddMetadata(chat_stream_container::initial_metadata::CURRENT_ACCOUNT_ID, user_info.current_object_oid.to_string());
        context->AddMetadata(chat_stream_container::initial_metadata::LOGGED_IN_TOKEN, user_info.logged_in_token);
        context->AddMetadata(chat_stream_container::initial_metadata::LETS_GO_VERSION, std::to_string(general_values::MINIMUM_ACCEPTED_ANDROID_VERSION));
        context->AddMetadata(chat_stream_container::initial_metadata::INSTALLATION_ID, user_info.installation_ids.front());
        context->AddMetadata(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES, chat_rooms_string);

        if (testingStreamOptions.testingOptions == TestingStreamOptions::RUN_DEADLINE) {
            std::chrono::time_point deadline_time =
                    std::chrono::system_clock::now() + testingStreamOptions.deadline_time;
            context->set_deadline(deadline_time);
        }

        reader_writer = bi_di_stub->StreamChatRPC(context);

        callback_when_started();

        if (testingStreamOptions.testingOptions == TestingStreamOptions::CANCEL_STREAM) {
            //allow the server to initialize
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            cancel_called = true;
            context->TryCancel();
        } else if (testingStreamOptions.testingOptions == TestingStreamOptions::DROP_CHANNEL) {
            //allow the server to initialize
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            channel.reset();
        }

        if(print_stream_stuff) {
            std::cout << std::string("Starting BiDi stream " + std::to_string(personal_index)) << std::endl;
        }

        grpc_stream_chat::ChatToClientResponse response;
        while (reader_writer->Read(&response)) {
            callback_when_message_received(response);
        }

        //this needs to be called in order to access trailing metadata & status
        grpc::Status icons_status = reader_writer->Finish();

        if(callback_when_completed) {
            callback_when_completed();
        }

        switch (testingStreamOptions.testingOptions) {
            case TestingStreamOptions::NONE:
                EXPECT_TRUE(icons_status.ok());
                break;
            case TestingStreamOptions::SKIP_STATUS_CHECK:
            case TestingStreamOptions::CANCEL_STREAM:
            case TestingStreamOptions::RUN_DEADLINE:
            case TestingStreamOptions::DROP_CHANNEL:
                break;
        }

        auto trailing_meta_data = context->GetServerTrailingMetadata();

        //Three things that can be returned by metadata
        auto shut_down_reason = trailing_meta_data.find(chat_stream_container::trailing_metadata::REASON_STREAM_SHUT_DOWN_KEY);

        //metadata will not be received if something happens like the deadline is exceeded
        if (shut_down_reason != trailing_meta_data.end()) {

            trailing_meta_data_stream_down_reasons =  grpc_stream_chat::StreamDownReasons(
                std::stoi(
                    std::string(shut_down_reason->second.data(), shut_down_reason->second.length())
                )
            );

            auto installation_id_metadata = trailing_meta_data.find(chat_stream_container::trailing_metadata::OPTIONAL_INFO_OF_CANCELLING_STREAM);

            if(installation_id_metadata != trailing_meta_data.end()) {
                trailing_meta_data_optional_info = std::string(installation_id_metadata->second.data(), installation_id_metadata->second.length());
            }

            switch(trailing_meta_data_stream_down_reasons) {
                case grpc_stream_chat::UNKNOWN_STREAM_STOP_REASON:
                    break;
                case grpc_stream_chat::RETURN_STATUS_ATTACHED: {
                    auto return_status_attached = trailing_meta_data.find(chat_stream_container::trailing_metadata::RETURN_STATUS_KEY);

                    //metadata will not be received if something happens like the deadline is exceeded
                    EXPECT_NE(return_status_attached, trailing_meta_data.end());
                    if (return_status_attached != trailing_meta_data.end()) {
                        trailing_meta_data_return_status = ReturnStatus(
                            std::stoi(
                                std::string(return_status_attached->second.data(), return_status_attached->second.length())
                            )
                        );
                    }
                    break;
                }
                case grpc_stream_chat::STREAM_TIMED_OUT:
                    if(print_stream_stuff) {
                        std::cout << "STREAM_TIMED_OUT\n";
                    }
                    break;
                case grpc_stream_chat::STREAM_CANCELED_BY_ANOTHER_STREAM:
                    EXPECT_NE(installation_id_metadata, trailing_meta_data.end());
                    break;
                case grpc_stream_chat::SERVER_SHUTTING_DOWN:
                    break;
                case grpc_stream_chat::StreamDownReasons_INT_MIN_SENTINEL_DO_NOT_USE_:
                    break;
                case grpc_stream_chat::StreamDownReasons_INT_MAX_SENTINEL_DO_NOT_USE_:
                    break;
            }
        }

        if(print_stream_stuff) {
            std::cout << std::string("StreamDownReason: ").
            append(grpc_stream_chat::StreamDownReasons_Name(trailing_meta_data_stream_down_reasons)).
            append(" ReturnStatus: ").
            append(ReturnStatus_Name(trailing_meta_data_return_status)).
            append("\n");
        }

        reader_writer = nullptr;
        delete context;
        context = nullptr;

        if(print_stream_stuff) {
            std::cout << std::string("Finishing BiDi stream " + std::to_string(personal_index)) << std::endl;
        }
    }

    void sendBiDiMessage(grpc_stream_chat::ChatToServerRequest& request) {
        std::scoped_lock<std::mutex> lock(write_mutex);
        if (reader_writer) {
            reader_writer->Write(request);
        }
    }

    void finishBiDiStream() {
        std::scoped_lock<std::mutex> lock(write_mutex);
        if (context) {
            cancel_called = true;
            context->TryCancel();
        }
    }

    grpc_stream_chat::StreamDownReasons trailing_meta_data_stream_down_reasons = grpc_stream_chat::StreamDownReasons::StreamDownReasons_INT_MIN_SENTINEL_DO_NOT_USE_;
    std::string trailing_meta_data_optional_info = "~";
    ReturnStatus trailing_meta_data_return_status = ReturnStatus::ReturnStatus_INT_MAX_SENTINEL_DO_NOT_USE_;

    TestingClient() {
        grpc::ChannelArguments args;
        //this is to make the channel 'unique' (otherwise GRPC_ARG_MAX_CONCURRENT_STREAMS can become an issue because
        // grpc seems to naturally re-use the non-unique channels behind the scenes).
        args.SetInt("a", personal_index);
        //The 'send' is because it is a 'send' from the server and a 'receive' from the client.
        args.SetMaxReceiveMessageSize(grpc_values::MAX_SEND_MESSAGE_LENGTH);

        //This way of creating a channel is done because if the standard grpc::CreateChannel() is used, it will return the
        // same channel to all clients. When this happens then the value GRPC_ARG_MAX_CONCURRENT_STREAMS becomes an issue
        // with a large number of clients opening connections on the same channel.
        channel = grpc::CreateCustomChannel("localhost:50051", grpc::InsecureChannelCredentials(), args);
        bi_di_stub = grpc_stream_chat::StreamChatService::NewStub(channel);
    }

private:

    int personal_index = index_testing_client.fetch_add(1);
    std::atomic_bool cancel_called = false;
    int num_times_start_called = 0; //for testing, got a segfault at Read() once
    std::mutex write_mutex;
    grpc::ClientContext* context = nullptr;
    std::shared_ptr<grpc::Channel> channel = nullptr;
    std::unique_ptr<grpc_stream_chat::StreamChatService::Stub> bi_di_stub = nullptr;
//    std::unique_ptr<testing_functions::TestingFunctionsService::Stub> testing_stub = testing_functions::TestingFunctionsService::NewStub(channel);
    std::unique_ptr<grpc::ClientReaderWriter<grpc_stream_chat::ChatToServerRequest, grpc_stream_chat::ChatToClientResponse>> reader_writer = nullptr;

};
