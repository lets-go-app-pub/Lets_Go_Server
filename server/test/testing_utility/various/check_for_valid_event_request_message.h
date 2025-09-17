//
// Created by jeremiah on 3/19/23.
//

#pragma once

#include "EventRequestMessage.pb.h"

void checkForValidEventRequestMessage(
        const EventRequestMessage& valid_event_request_message,
        const std::function<bool(const EventRequestMessage& event_request)>& run_function
);