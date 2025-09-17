//
// Created by jeremiah on 3/25/22.
//

#include "save_category_time_frames.h"

#include <bsoncxx/array/value.hpp>
#include <bsoncxx/builder/stream/document.hpp>

#include <user_account_keys.h>
#include <AccountCategoryEnum.grpc.pb.h>

#include "sort_time_frames_and_remove_overlaps.h"

//This function will save the categories and timeframes from the std::map to the bsoncxx::array ignoring overlapping timeframes.
//Expects the categories saved to the 'category_index_to_timeframes_map' map with their respective timeframes. Each timeframe
// is expected to have a start and a stop timeframe.
void save_category_time_frames(
        std::map<int, std::vector<TimeFrameStruct>>& category_index_to_timeframes_map,
        bsoncxx::builder::basic::array& account_activities_and_categories
) {
    for (auto&[category_index, category_time_frames] : category_index_to_timeframes_map) {

        bsoncxx::array::value time_frame_mongoDB_array = sort_time_frames_and_remove_overlaps(category_time_frames);

        account_activities_and_categories.append(
                bsoncxx::builder::stream::document{}
                    << user_account_keys::categories::TYPE << AccountCategoryType::CATEGORY_TYPE
                    << user_account_keys::categories::INDEX_VALUE << category_index
                    << user_account_keys::categories::TIMEFRAMES << time_frame_mongoDB_array
                << bsoncxx::builder::stream::finalize);
    }
}