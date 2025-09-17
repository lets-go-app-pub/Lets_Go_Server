//
// Created by jeremiah on 8/15/22.
//

#pragma once

#include <deque>
#include "messages_waiting_to_be_sent.h"
#include "chat_room_values.h"

namespace chat_change_stream_values {

    inline std::chrono::milliseconds CHAT_CHANGE_STREAM_AWAIT_TIME{30L * 1000L}; //Time the chat change stream awaits for information before re-connecting. Also used on server shut down.
    inline std::chrono::milliseconds CHAT_CHANGE_STREAM_SLEEP_TIME{50L}; //Time the chat change stream sleeps between awaiting. Used so the thread doesn't spin endlessly if it cannot connect.

    inline std::chrono::milliseconds DELAY_FOR_MESSAGE_ORDERING{1L * 1000L}; //When each message is received it will delay for this amount of time before being sent back.
    inline std::chrono::milliseconds MESSAGE_ORDERING_THREAD_SLEEP_TIME{2L}; //Each time the order_messages_thread completes a cycle, this is the sleep time. If this is too large, messages will be sent in fairly large 'batches' instead of more individually. If it is too small then it can 'spin' and waste resources. (the value is arbitrary atm)
    inline std::chrono::milliseconds TIME_TO_REQUEST_PREVIOUS_MESSAGES{20L * 1000L}; //When the change stream thread receives a kDifferentUserJoinedMessage, it will request BACK this far in order to get any possible missed messages. extractChatRooms AND the client will use the variable as well. NOTE: This should ALWAYS be smaller than MAX_TIME_TO_CACHE_MESSAGES.

    inline const std::chrono::milliseconds MAX_TIME_TO_CACHE_MESSAGES{60L * 1000L}; //The amount of time to cache messages. This will allow order to be checked before sending and previous messages to be sent without checking the database. (can also be trimmed by size, see MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES) NOTE: This should ALWAYS be larger than TIME_TO_REQUEST_PREVIOUS_MESSAGES.
    inline const size_t MAX_SIZE_FOR_MESSAGE_CACHE_IN_BYTES = 100L * 1024L * 1024L; //100Mb; The max size (dynamically allocated memory) the cached_messages variable inside order_messages_thread can grow to before it is trimmed. (can also be trimmed by time, see MAX_TIME_TO_CACHE_MESSAGES)

    //NOTE: All elements won't be exactly chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS, however it will be close enough to get a good estimate.
    inline const size_t SIZE_OF_MESSAGES_DEQUE_ELEMENT = sizeof(std::deque<MessageWaitingToBeSent>) + chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS; //Size of a cached_message element without calculating dynamic allocation inside MessageWaitingToBeSent.
}