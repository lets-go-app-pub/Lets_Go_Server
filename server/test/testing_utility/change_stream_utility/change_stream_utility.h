//
// Created by jeremiah on 8/19/22.
//

#pragma once

#include <chrono>

inline void spinUntilNextSecond() {
    //Must sleep to get past the current second. This is because the chat stream will start requesting at
    // the current second (using start_at_operation_time()). If the second has not passed the change stream will
    // receive the kChatRoomCapMessage, kDifferentUserJoinedMessage etc... This is a problem because the async server
    // is not actually started and so only one message can be sent into each ChatStreamContainerObject.
    using namespace std::chrono;
    const seconds starting_count = duration_cast<seconds>(system_clock::now().time_since_epoch());
    while(starting_count == duration_cast<seconds>(system_clock::now().time_since_epoch())) {}
}