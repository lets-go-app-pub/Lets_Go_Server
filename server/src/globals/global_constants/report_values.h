//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <chrono>

namespace report_values {
    inline const int MAX_NUMBER_OF_REPORTED_USERS_ADMIN_CAN_REQUEST_NOTIFIED = 5; //admin can request at most this many of reports at a time
    inline const int NUMBER_OF_REPORTS_BEFORE_ADMIN_NOTIFIED = 1; //when this number of reports is reached for a user, the admin will receive this as a 'report'

    inline const int NUMBER_TIMES_TIMED_OUT_BEFORE_BAN = 4; //when the user is timed out by an admin this many times, they will be baned (ex: if 4 then when NUMBER_OF_TIMES_TIMED_OUT == 4 and a time-out is received, the user will be banned); NOTE: TIME_OUT_TIMES is coupled to this and this is expected to be the size
    inline const std::chrono::milliseconds TIME_OUT_TIMES[NUMBER_TIMES_TIMED_OUT_BEFORE_BAN]{
            std::chrono::milliseconds{4 * 60 * 60 * 1000},  //4 hours
            std::chrono::milliseconds{24 * 60 * 60 * 1000}, //24 hours
            std::chrono::milliseconds{3 * 24 * 60 * 60 * 1000}, //3 days
            std::chrono::milliseconds{7 * 24 * 60 * 60 * 1000}  //1 week
    }; //NUMBER_TIMES_TIMED_OUT_BEFORE_BAN is the size for this array
    static_assert(NUMBER_TIMES_TIMED_OUT_BEFORE_BAN == sizeof(TIME_OUT_TIMES)/sizeof(std::chrono::milliseconds));

    inline const std::chrono::hours MINIMUM_TIME_FOR_SUSPENSION = std::chrono::hours{1}; //the minimum amount of time a user can be suspended for (NOTE IN HOURS)
    inline const std::chrono::hours MAXIMUM_TIME_FOR_SUSPENSION = std::chrono::hours{365*24}; //the maximum amount of time a user can be suspended for (NOTE IN HOURS)
    inline const std::chrono::milliseconds CHECK_OUT_TIME = std::chrono::milliseconds{10L * 60L * 1000L}; //the amount of time before a report can be requested again after requested once

    inline const int MAX_NUMBER_OF_TIMES_CAN_SPAM_REPORTS = 5;
}