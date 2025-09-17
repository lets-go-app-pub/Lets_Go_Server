//
// Created by jeremiah on 3/25/22.
//

#include "sort_time_frames_and_remove_overlaps.h"

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <user_account_keys.h>

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

//Sorts the time frames passed in time_frames and removes any overlaps between them.
//Expects each time_frame to have a 'pair' this means that there should be a start AND a stop
// time for each time frame inside the time_frames vector.
bsoncxx::array::value sort_time_frames_and_remove_overlaps(
        std::vector<TimeFrameStruct>& time_frames,
        const std::function<void(TimeFrameStruct&)>& run_with_append,
        const std::function<void()>& clear_append_timeframes
        ) {


    std::sort(std::begin(time_frames), std::end(time_frames),
              [](const TimeFrameStruct& lhs, const TimeFrameStruct& rhs) -> bool {
        if (lhs.time == rhs.time) {
            //NOTE: If the times are equal then the start times must come first. It is different when the
            // algorithm is running because the algorithm is comparing userA timeframes to userB timeframes.
            // This function is simply removing overlaps from userA timeframes.
            return rhs.startStopValue < lhs.startStopValue;
        }

        return lhs.time < rhs.time;
    });

    auto time_frame_mongoDB_array = bsoncxx::builder::basic::array{};
    //organize the array so there are no overlapping timeframes
    int nesting_value = 0;
    for (size_t j = 0; j < time_frames.size(); j++) {

        //NOTE: This is checked for the sake of completion, however it should never happen.
        if(time_frames[j].startStopValue != 1) {
            continue;
        }

        if (time_frames[j].time != std::chrono::milliseconds{
            -1}) { //If timeFrame is -1 then it will be removed the first time the algorithm runs, there is no reason to store it.

            time_frame_mongoDB_array.append(document{}
                    << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{time_frames[j].time.count()}
                    << user_account_keys::categories::timeframes::START_STOP_VALUE << time_frames[j].startStopValue
                << finalize);
        }

        //This must go OUTSIDE the above condition block or when categories are saved from activities
        // there can be an uneven number of start to stop values.
        run_with_append(time_frames[j]);

        nesting_value++;

        //Move to next relevant stop time.
        while (nesting_value > 0 && j < time_frames.size()) {
            j++;
            //startStopValue will be either -1 or 1
            nesting_value += time_frames[j].startStopValue;
        }

        if(j >= time_frames.size() || nesting_value < 0) {
            time_frame_mongoDB_array.clear();
            clear_append_timeframes();
            break;
        }

        run_with_append(time_frames[j]);

        //upsert stop time
        time_frame_mongoDB_array.append(document{}
                << user_account_keys::categories::timeframes::TIME << bsoncxx::types::b_int64{time_frames[j].time.count()}
                << user_account_keys::categories::timeframes::START_STOP_VALUE << time_frames[j].startStopValue
            << finalize);

    }

    return time_frame_mongoDB_array.extract();
}