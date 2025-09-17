//
// Created by jeremiah on 3/25/22.
//

#pragma once

#include <vector>
#include <map>

#include <bsoncxx/builder/basic/array.hpp>

#include "time_frame_struct.h"

//This function will save the categories and timeframes from the std::map to the bsoncxx::array ignoring overlapping timeframes.
//Expects the categories saved to the 'category_index_to_timeframes_map' map with their respective timeframes. Each timeframe
// is expected to have a start and a stop timeframe.
void save_category_time_frames(
        std::map<int, std::vector<TimeFrameStruct>>& category_index_to_timeframes_map,
        bsoncxx::builder::basic::array& account_activities_and_categories
        );