//
// Created by jeremiah on 3/25/22.
//

#pragma once

#include <bsoncxx/array/value.hpp>
#include <time_frame_struct.h>
#include <vector>

bsoncxx::array::value sort_time_frames_and_remove_overlaps(
        std::vector<TimeFrameStruct>& time_frames,
        const std::function<void(TimeFrameStruct&)>& run_with_append = [](TimeFrameStruct&){},
        const std::function<void()>& clear_append_timeframes = [](){}
        );