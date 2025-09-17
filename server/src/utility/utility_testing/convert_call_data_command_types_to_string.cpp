//
// Created by jeremiah on 4/20/22.
//

#include <utility_testing_functions.h>

//prints the enum Status for CallDataCommandTypes
std::string convertCallDataCommandTypesToString(CallDataCommandTypes callDataCommandTypes) {
    std::string returnString = "Error: invalid value";

    switch (callDataCommandTypes)
    {
        case CallDataCommandTypes::INITIALIZE:
            returnString = "INITIALIZE";
            break;
        case CallDataCommandTypes::READ:
            returnString = "READ";
            break;
        case CallDataCommandTypes::END_OF_VECTOR_WRITES:
            returnString = "END_OF_VECTOR_WRITES";
            break;
        case CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING:
            returnString = "WRITE_WITH_VECTOR_ELEMENTS_REMAINING";
            break;
        case CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END:
            returnString = "ALARM_QUEUE_WRITE_AT_END";
            break;
        case CallDataCommandTypes::FINISH:
            returnString = "FINISH";
            break;
        case CallDataCommandTypes::END_STREAM_ALARM_CALLED:
            returnString = "END_STREAM_ALARM_CALLED";
            break;
        case CallDataCommandTypes::CHAT_STREAM_CANCELLED_ALARM:
            returnString = "CHAT_STREAM_CANCELLED_ALARM";
            break;
        case CallDataCommandTypes::NOTIFY_WHEN_DONE:
            returnString = "NOTIFY_WHEN_DONE";
            break;
    }

    return returnString;
}