//
// Created by jeremiah on 3/25/22.
//

#pragma once

#include <map>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>

#include <CategoryTimeFrame.grpc.pb.h>

#include <time_frame_struct.h>

void save_activity_time_frames(
        std::map<int, std::vector<TimeFrameStruct>>& category_index_to_timeframes_map,
        bsoncxx::builder::basic::array& account_activities_and_categories,
        const CategoryActivityMessage& activity,
        const int& activity_index, const int& category_index,
        const std::chrono::milliseconds& current_timestamp
        );