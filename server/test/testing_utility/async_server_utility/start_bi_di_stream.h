//
// Created by jeremiah on 6/27/22.
//

#pragma once

#include <thread>
#include <ChatMessageStream.grpc.pb.h>
#include "testing_client.h"

std::jthread startBiDiStream(
        std::shared_ptr<TestingClient> client,
        const UserAccountDoc& user_info,
        const std::function<void(
                const grpc_stream_chat::ChatToClientResponse& response)>& callback_when_message_received,
        const std::string& chat_rooms_string = "",
        const TestingStreamOptions& testingStreamOptions = TestingStreamOptions(),
        const std::function<void()>& callback_when_completed = nullptr
);
