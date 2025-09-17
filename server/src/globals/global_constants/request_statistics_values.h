//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <chrono>

namespace request_statistics_values {
    inline const unsigned int MAXIMUM_NUMBER_DAYS_TO_SEARCH = 365; //used when requesting statistics, maximum number of days user can go back in time for the search
    inline const long MILLISECONDS_IN_ONE_DAY = 24L * 60L * 60L * 1000L; //used when requesting statistics
    inline const std::chrono::milliseconds COOL_DOWN_BETWEEN_REQUESTING_AGE_GENDER_STATISTICS = std::chrono::milliseconds{30L * 1000L};
}
