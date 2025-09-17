//
// Created by jeremiah on 3/25/22.
//

#include <save_activity_time_frames.h>

#include "server_parameter_restrictions.h"

#include <matching_algorithm.h>
#include <bsoncxx/array/value.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <user_account_keys.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <general_values.h>

#include "sort_time_frames_and_remove_overlaps.h"
#include "utility_general_functions.h"

//Will add the category (by category_index) to the map if an element does not already exist for it. After
// that it will trim any problems with the timeframes (overlaps, invalid time frames etc.). It will then save
// the activity to the bsoncxx::array and any timeframes to the vector with the passed category_index.
void save_activity_time_frames(
        std::map<int, std::vector<TimeFrameStruct>>& category_index_to_timeframes_map,
        bsoncxx::builder::basic::array& account_activities_and_categories,
        const CategoryActivityMessage& activity,
        const int& activity_index,
        const int& category_index,
        const std::chrono::milliseconds& current_timestamp
) {

    //NOTE: this will do nothing if category_index already exists
    category_index_to_timeframes_map.insert(std::make_pair(category_index, std::vector<TimeFrameStruct>{}));

    const std::chrono::milliseconds end_seconds =
            current_timestamp + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES; //final value possible to be set for time frame

    const int num_time_frames =
            activity.time_frame_array_size() > server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY ?
            server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY :
            activity.time_frame_array_size();

    std::vector<TimeFrameStruct> time_frames;
    const auto& time_frame = activity.time_frame_array();
    for (int j = 0; j < num_time_frames; ++j) {
        std::chrono::milliseconds start_time = std::chrono::milliseconds{time_frame[j].start_time_frame()};
        std::chrono::milliseconds stop_time = std::chrono::milliseconds{time_frame[j].stop_time_frame()};

        if (start_time < stop_time) {

            if (start_time < current_timestamp +
                             matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED) { //start time is less than min allowed time

                //NOTE: the -1 will be removed before it is inserted into the array
                // however it makes some things leading up to that point simpler
                start_time = std::chrono::milliseconds{-1};
            } else if (start_time >= end_seconds) { //start time is greater than or equal to max allowed time
                continue;
            }

            if (stop_time < current_timestamp +
                            matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED) { //stop time is less than min allowed time
                continue;
            } else if (stop_time >= end_seconds) { //stop time is greater than or equal to max allowed time
                stop_time = end_seconds;
            }

        }
        else if (start_time == stop_time) { //if start time is same as stop time

            if (start_time < current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED
                || start_time >= end_seconds) { // both times are outside the allowed time frame
                continue;
            } else { // both times are inside the allowed time frame

                //NOTE: this is done because if stopTime == startTime then the algorithm will count it as an overlap of 0
                // if it matches with another user and so the point value of this overlap will also be 0
                // to fix this, because the android device minimum amount of time unit is minutes however the server works in
                // current_timestamp can add 29 so there will be overlap and the minutes won't change
                stop_time += std::chrono::milliseconds{29L * 1000L};
            }
        } else { //if start time is greater than stop time
            continue;
        }

        //NOTE: an important point is that a start AND a stop time are always inserted here
        time_frames.emplace_back(TimeFrameStruct(
                start_time, 1
        ));
        time_frames.emplace_back(TimeFrameStruct(
                stop_time, -1
        ));
    }

    //add this TimeFrameStruct to the categories array so that it can be used as well
    const bsoncxx::array::value time_frame_mongo_db_array = sort_time_frames_and_remove_overlaps(
            time_frames,
            [&](TimeFrameStruct& val) {
                category_index_to_timeframes_map[category_index].emplace_back(val);
            },
            [&]() {
                category_index_to_timeframes_map[category_index].clear();
            }
    );

    account_activities_and_categories.append(
        bsoncxx::builder::stream::document{}
            << user_account_keys::categories::TYPE << AccountCategoryType::ACTIVITY_TYPE
            << user_account_keys::categories::INDEX_VALUE << activity_index
            << user_account_keys::categories::TIMEFRAMES << time_frame_mongo_db_array
        << bsoncxx::builder::stream::finalize
    );
}