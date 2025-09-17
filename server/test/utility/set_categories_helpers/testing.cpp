//
// Created by jeremiah on 6/20/22.
//
#include <utility_general_functions.h>
#include <fstream>
#include <account_objects.h>
#include <ChatRoomCommands.pb.h>
#include "gtest/gtest.h"
#include <google/protobuf/util/message_differencer.h>

#include <save_activity_time_frames.h>
#include <save_category_time_frames.h>
#include <sort_time_frames_and_remove_overlaps.h>
#include <user_account_keys.h>
#include <general_values.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetCategoriesHelpers : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_singleTimeFrame) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_start_time_frame(current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
    time_frame->set_stop_time_frame(time_frame->start_time_frame() + 60L*1000L);

    save_activity_time_frames(
        category_index_to_timeframes_map,
        account_activities_and_categories,
        category_activity_message,
        activity_index,
        category_index,
        current_timestamp
    );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 2);

        EXPECT_EQ(time_frames_arr[0].time.count(), category_activity_message.time_frame_array()[0].start_time_frame());
        EXPECT_EQ(time_frames_arr[0].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[1].time.count(), category_activity_message.time_frame_array()[0].stop_time_frame());
        EXPECT_EQ(time_frames_arr[1].startStopValue, -1);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    int time_frame_size = (int)std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end());
    ASSERT_EQ(time_frame_size, category_activity_message.time_frame_array_size()*2);

    for(int i = 0; i < time_frame_size; i++) {
        bsoncxx::document::view time_frame_doc = time_frames_array_doc[i].get_document().value;

        if(i % 2 == 0) { //start time
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[i/2].start_time_frame());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, 1);
        } else { //stop time
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[i/2].stop_time_frame());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);
        }
    }

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_noTimeframes) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 0);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 0);

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_tooManyTimeframes) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::chrono::milliseconds setting_timestamp = current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED;

    for(int i = 0; i < server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY + 1; i++) {
        auto time_frame = category_activity_message.add_time_frame_array();

        setting_timestamp += std::chrono::milliseconds{30L * 1000L};

        time_frame->set_start_time_frame(setting_timestamp.count());

        setting_timestamp += std::chrono::milliseconds{30L * 1000L};

        time_frame->set_stop_time_frame(setting_timestamp.count());
    }

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY*2);

        for(int i = 0; i < server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY*2; i++) {
            if(i % 2 == 0) { //start time
                EXPECT_EQ(time_frames_arr[i].time.count(), category_activity_message.time_frame_array()[i/2].start_time_frame());
                EXPECT_EQ(time_frames_arr[i].startStopValue, 1);
            } else { //stop time
                EXPECT_EQ(time_frames_arr[i].time.count(), category_activity_message.time_frame_array()[i/2].stop_time_frame());
                EXPECT_EQ(time_frames_arr[i].startStopValue, -1);
            }
        }
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view activity_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(activity_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(activity_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = activity_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    int time_frame_size = (int)std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end());
    ASSERT_EQ(time_frame_size, server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY*2);

    for(int i = 0; i < time_frame_size; i++) {
        bsoncxx::document::view time_frame_doc = time_frames_array_doc[i].get_document().value;

        if(i % 2 == 0) { //start time
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[i/2].start_time_frame());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, 1);
        } else { //stop time
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[i/2].stop_time_frame());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);
        }
    }
}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_overlappingTimeframes) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    std::chrono::milliseconds first_start_time{current_timestamp + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED};
    std::chrono::milliseconds first_stop_time{first_start_time.count() + 60L*1000L};
    std::chrono::milliseconds second_start_time{first_start_time.count() + 30L*1000L};
    std::chrono::milliseconds second_stop_time{first_start_time.count() + 90L*1000L};

    auto first_time_frame = category_activity_message.add_time_frame_array();

    first_time_frame->set_start_time_frame(first_start_time.count());
    first_time_frame->set_stop_time_frame(first_stop_time.count());

    auto second_time_frame = category_activity_message.add_time_frame_array();

    second_time_frame->set_start_time_frame(second_start_time.count());
    second_time_frame->set_stop_time_frame(second_stop_time.count());

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 2);

        EXPECT_EQ(time_frames_arr[0].time.count(), first_start_time.count());
        EXPECT_EQ(time_frames_arr[0].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[1].time.count(), second_stop_time.count());
        EXPECT_EQ(time_frames_arr[1].startStopValue, -1);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 2);

    bsoncxx::document::view time_frame_doc = time_frames_array_doc[0].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, first_start_time.count());
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, 1);

    time_frame_doc = time_frames_array_doc[1].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, second_stop_time.count());
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_categoryIndexAlreadyExistsInMap) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_start_time_frame(current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
    time_frame->set_stop_time_frame(time_frame->start_time_frame() + 60L*1000L);

    std::chrono::milliseconds previous_start_time{time_frame->stop_time_frame() + 120L*1000L};
    std::chrono::milliseconds previous_stop_time{previous_start_time.count() + 60L*1000L};

    category_index_to_timeframes_map.insert(std::make_pair(category_index, std::vector<TimeFrameStruct>{
        TimeFrameStruct(
                previous_start_time,
                1
                ),
        TimeFrameStruct(
                previous_stop_time,
                -1
                )
    }));

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 4);

        EXPECT_EQ(time_frames_arr[2].time.count(), category_activity_message.time_frame_array()[0].start_time_frame());
        EXPECT_EQ(time_frames_arr[2].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[3].time.count(), category_activity_message.time_frame_array()[0].stop_time_frame());
        EXPECT_EQ(time_frames_arr[3].startStopValue, -1);

        EXPECT_EQ(time_frames_arr[0].time.count(), previous_start_time.count());
        EXPECT_EQ(time_frames_arr[0].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[1].time.count(), previous_stop_time.count());
        EXPECT_EQ(time_frames_arr[1].startStopValue, -1);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    int time_frame_size = (int)std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end());
    ASSERT_EQ(time_frame_size, category_activity_message.time_frame_array_size()*2);

    for(int i = 0; i < time_frame_size; i++) {
        bsoncxx::document::view time_frame_doc = time_frames_array_doc[i].get_document().value;

        if(i % 2 == 0) { //start time
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[i/2].start_time_frame());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, 1);
        } else { //stop time
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[i/2].stop_time_frame());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);
        }
    }
}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeLessThanStopTime_startTimeTooEarly) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_start_time_frame(current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count() - 1);
    time_frame->set_stop_time_frame(current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count() + 60L*1000L);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 2);

        EXPECT_EQ(time_frames_arr[0].time.count(), -1);
        EXPECT_EQ(time_frames_arr[0].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[1].time.count(), category_activity_message.time_frame_array()[0].stop_time_frame());
        EXPECT_EQ(time_frames_arr[1].startStopValue, -1);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 1);

    bsoncxx::document::view time_frame_doc = time_frames_array_doc[0].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[0].stop_time_frame());
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeLessThanStopTime_startTimeTooLate) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_start_time_frame(current_timestamp.count() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count());
    time_frame->set_stop_time_frame(time_frame->start_time_frame() + 60L*1000L);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 0);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 0);
}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeLessThanStopTime_stopTimeTooEarly) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_stop_time_frame(time_frame->start_time_frame() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count() - 1);
    time_frame->set_start_time_frame(time_frame->stop_time_frame() - 60L*1000);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 0);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 0);
}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeLessThanStopTime_stopTimeTooLate) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_start_time_frame(current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());
    time_frame->set_stop_time_frame(current_timestamp.count() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count() + 60L*1000L);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 2);

        EXPECT_EQ(time_frames_arr[0].time.count(), category_activity_message.time_frame_array()[0].start_time_frame());
        EXPECT_EQ(time_frames_arr[0].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[1].time.count(), current_timestamp.count() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count());
        EXPECT_EQ(time_frames_arr[1].startStopValue, -1);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 2);

    bsoncxx::document::view time_frame_doc = time_frames_array_doc[0].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_activity_message.time_frame_array()[0].start_time_frame());
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, 1);

    time_frame_doc = time_frames_array_doc[1].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, current_timestamp.count() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count());
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeEqualToStopTime_timeIsOk) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    long used_time = current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count();

    time_frame->set_start_time_frame(used_time);
    time_frame->set_stop_time_frame(used_time);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 2);

        EXPECT_EQ(time_frames_arr[0].time.count(), used_time);
        EXPECT_EQ(time_frames_arr[0].startStopValue, 1);
        EXPECT_EQ(time_frames_arr[1].time.count(), used_time + 29L*1000L);
        EXPECT_EQ(time_frames_arr[1].startStopValue, -1);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 2);

    bsoncxx::document::view time_frame_doc = time_frames_array_doc[0].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, used_time);
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, 1);

    time_frame_doc = time_frames_array_doc[1].get_document().value;
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, used_time + 29L*1000L);
    EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, -1);

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeEqualToStopTime_timeTooEarly) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    time_frame->set_start_time_frame(current_timestamp.count() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count() - 1);
    time_frame->set_stop_time_frame(time_frame->start_time_frame());

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 0);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 0);
}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeEqualToStopTime_timeTooLate) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    long used_time = current_timestamp.count() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count() + 1;
    time_frame->set_start_time_frame(used_time);
    time_frame->set_stop_time_frame(used_time);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 0);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 0);

}

TEST_F(SetCategoriesHelpers, saveActivityTimeFrames_startTimeGreaterThanStopTime) {
    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    bsoncxx::builder::basic::array account_activities_and_categories;

    int activity_index = 5;
    int category_index = 2;

    CategoryActivityMessage category_activity_message;

    category_activity_message.set_activity_index(activity_index);
    auto time_frame = category_activity_message.add_time_frame_array();

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    time_frame->set_start_time_frame(current_timestamp.count()  + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count() - 1);
    time_frame->set_stop_time_frame(time_frame->start_time_frame() - 60L*1000L);

    save_activity_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories,
            category_activity_message,
            activity_index,
            category_index,
            current_timestamp
            );

    ASSERT_EQ(category_index_to_timeframes_map.size(), 1);

    for(const auto& [category_index_val, time_frames_arr]: category_index_to_timeframes_map) {
        ASSERT_EQ(time_frames_arr.size(), 0);
    }

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), 1);

    bsoncxx::document::view category_ele = categories_doc[0].get_document().value;
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::ACTIVITY_TYPE);
    EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), activity_index);

    bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
    ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), 0);

}

TEST_F(SetCategoriesHelpers, saveCategoryTimeFrames_twoCategories) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds{60L*1000L};

    category_index_to_timeframes_map.insert(
        std::make_pair(1, std::vector<TimeFrameStruct>{})
    );

    category_index_to_timeframes_map.insert(
        std::make_pair(2, std::vector<TimeFrameStruct>{
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    ),
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
        })
    );

    bsoncxx::builder::basic::array account_activities_and_categories;

    save_category_time_frames(
        category_index_to_timeframes_map,
        account_activities_and_categories
    );

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), category_index_to_timeframes_map.size());

    int i = 0;
    for (auto&[category_index, category_time_frames] : category_index_to_timeframes_map) {
        bsoncxx::document::view category_ele = categories_doc[i].get_document().value;
        EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::CATEGORY_TYPE);
        EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), category_index);

        bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
        ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), category_time_frames.size());

        int j = 0;
        for(const auto& time_frame_ele : time_frames_array_doc) {
            bsoncxx::document::view time_frame_doc = time_frame_ele.get_document().value;
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_time_frames[j].time.count());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, category_time_frames[j].startStopValue);
            j++;
        }
        i++;
    }
}

TEST_F(SetCategoriesHelpers, saveCategoryTimeFrames_overlappingTimeFrames) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds{60L*1000L};
    std::chrono::milliseconds second_time_frame_start = first_time_frame_start + std::chrono::milliseconds{60L*1000L};
    std::chrono::milliseconds second_time_frame_stop = first_time_frame_start + std::chrono::milliseconds{120L*1000L};

    category_index_to_timeframes_map.insert(
            std::make_pair(2, std::vector<TimeFrameStruct>{
                TimeFrameStruct(
                        first_time_frame_start,
                        1
                        ),
                TimeFrameStruct(
                        first_time_frame_stop,
                        -1
                        ),
                TimeFrameStruct(
                        second_time_frame_start,
                        1
                        ),
                TimeFrameStruct(
                        second_time_frame_stop,
                        -1
                        )
            })
    );

    bsoncxx::builder::basic::array account_activities_and_categories;

    save_category_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories
            );

    std::map<int, std::vector<TimeFrameStruct>> generated_category_index_to_timeframes_map;

    generated_category_index_to_timeframes_map.insert(
            std::make_pair(2, std::vector<TimeFrameStruct>{
                TimeFrameStruct(
                        first_time_frame_start,
                        1
                        ),
                TimeFrameStruct(
                        second_time_frame_stop,
                        -1
                        )
            })
            );

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), generated_category_index_to_timeframes_map.size());

    int i = 0;
    for (auto&[category_index, category_time_frames] : generated_category_index_to_timeframes_map) {
        bsoncxx::document::view category_ele = categories_doc[i].get_document().value;
        EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::CATEGORY_TYPE);
        EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), category_index);

        bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
        ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), category_time_frames.size());

        int j = 0;
        for(const auto& time_frame_ele : time_frames_array_doc) {
            bsoncxx::document::view time_frame_doc = time_frame_ele.get_document().value;
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_time_frames[j].time.count());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, category_time_frames[j].startStopValue);
            j++;
        }
        i++;
    }
}

TEST_F(SetCategoriesHelpers, saveCategoryTimeFrames_sameTimeFrames) {

    std::map<int, std::vector<TimeFrameStruct>> category_index_to_timeframes_map;
    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds{60L*1000L};

    category_index_to_timeframes_map.insert(
            std::make_pair(2, std::vector<TimeFrameStruct>{
                TimeFrameStruct(
                        first_time_frame_start,
                        1
                        ),
                TimeFrameStruct(
                        first_time_frame_stop,
                        -1
                        ),
                TimeFrameStruct(
                        first_time_frame_start,
                        1
                        ),
                TimeFrameStruct(
                        first_time_frame_stop,
                        -1
                        )
            })
            );

    bsoncxx::builder::basic::array account_activities_and_categories;

    save_category_time_frames(
            category_index_to_timeframes_map,
            account_activities_and_categories
            );

    std::map<int, std::vector<TimeFrameStruct>> generated_category_index_to_timeframes_map;

    generated_category_index_to_timeframes_map.insert(
            std::make_pair(2, std::vector<TimeFrameStruct>{
                TimeFrameStruct(
                        first_time_frame_start,
                        1
                        ),
                TimeFrameStruct(
                        first_time_frame_stop,
                        -1
                        )
            })
            );

    bsoncxx::array::view categories_doc = account_activities_and_categories.view();

    ASSERT_EQ(std::distance(categories_doc.begin(), categories_doc.end()), generated_category_index_to_timeframes_map.size());

    int i = 0;
    for (auto&[category_index, category_time_frames] : generated_category_index_to_timeframes_map) {
        bsoncxx::document::view category_ele = categories_doc[i].get_document().value;
        EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::TYPE].get_int32().value), AccountCategoryType::CATEGORY_TYPE);
        EXPECT_EQ(AccountCategoryType(category_ele[user_account_keys::categories::INDEX_VALUE].get_int32().value), category_index);

        bsoncxx::array::view time_frames_array_doc = category_ele[user_account_keys::categories::TIMEFRAMES].get_array().value;
        ASSERT_EQ(std::distance(time_frames_array_doc.begin(), time_frames_array_doc.end()), category_time_frames.size());

        int j = 0;
        for(const auto& time_frame_ele : time_frames_array_doc) {
            bsoncxx::document::view time_frame_doc = time_frame_ele.get_document().value;
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value, category_time_frames[j].time.count());
            EXPECT_EQ(time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value, category_time_frames[j].startStopValue);
            j++;
        }
        i++;
    }
}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_singleTimeFrame) {

    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
        TimeFrameStruct(
                current_timestamp,
                1
        )
    );

    time_frames.emplace_back(
        TimeFrameStruct(
            current_timestamp + std::chrono::milliseconds {30*1000},
            -1
        )
    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
    );

    std::sort(time_frames.begin(), time_frames.end(), [](const TimeFrameStruct& lhs, const TimeFrameStruct& rhs){
        return lhs.time < rhs.time;
    });

    ASSERT_EQ(appended_time_frames.size(), time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
            time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
            time_frames[i].time.count()
        );

        EXPECT_EQ(
            time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
            time_frames[i].startStopValue
        );

        i++;
    }

}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_overlappingTimeFrames) {

    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {60*1000};
    std::chrono::milliseconds second_time_frame_start = first_time_frame_start + std::chrono::milliseconds {30*1000};
    std::chrono::milliseconds second_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {90*1000};
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
        TimeFrameStruct(
            first_time_frame_start,
            1
        )
    );

    time_frames.emplace_back(
        TimeFrameStruct(
            first_time_frame_stop,
            -1
        )
    );

    time_frames.emplace_back(
        TimeFrameStruct(
            second_time_frame_start,
            1
        )
    );

    time_frames.emplace_back(
        TimeFrameStruct(
            second_time_frame_stop,
            -1
        )
    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
            );

    std::vector<TimeFrameStruct> generated_time_frames;

    generated_time_frames.emplace_back(
        TimeFrameStruct(
            first_time_frame_start,
            1
        )
    );

    generated_time_frames.emplace_back(
        TimeFrameStruct(
            second_time_frame_stop,
            -1
        )
    );

    ASSERT_EQ(appended_time_frames.size(), generated_time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], generated_time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), generated_time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                generated_time_frames[i].time.count()
                );

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
                generated_time_frames[i].startStopValue
                );

        i++;
    }
}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_manyOverlappingTimeFrames) {

    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {120*1000};
    std::chrono::milliseconds second_time_frame_start = first_time_frame_start + std::chrono::milliseconds {30*1000};
    std::chrono::milliseconds second_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {90*1000};
    std::chrono::milliseconds third_time_frame_start = first_time_frame_start + std::chrono::milliseconds {45*1000};
    std::chrono::milliseconds third_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {90*1000};
    std::chrono::milliseconds fourth_time_frame_start = first_time_frame_start + std::chrono::milliseconds {20*1000};
    std::chrono::milliseconds fourth_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {60*1000};
    std::chrono::milliseconds fifth_time_frame_start = first_time_frame_start + std::chrono::milliseconds {30*1000};
    std::chrono::milliseconds fifth_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {90*1000};
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    third_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    third_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    fourth_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    fourth_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    fifth_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    fifth_time_frame_stop,
                    -1
                    )
                    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
            );

    std::vector<TimeFrameStruct> generated_time_frames;

    generated_time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    generated_time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    ASSERT_EQ(appended_time_frames.size(), generated_time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], generated_time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), generated_time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                generated_time_frames[i].time.count()
                );

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
                generated_time_frames[i].startStopValue
                );

        i++;
    }
}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_startStopTimeOverlap) {

    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {60*1000};
    std::chrono::milliseconds second_time_frame_start = first_time_frame_stop;
    std::chrono::milliseconds second_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {90*1000};
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_stop,
                    -1
                    )
                    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
            );

    std::vector<TimeFrameStruct> generated_time_frames;

    generated_time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    generated_time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_stop,
                    -1
                    )
                    );

    ASSERT_EQ(appended_time_frames.size(), generated_time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], generated_time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), generated_time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                generated_time_frames[i].time.count()
                );

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
                generated_time_frames[i].startStopValue
                );

        i++;
    }
}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_missingStartTime) {
    std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = current_timestamp + std::chrono::milliseconds {30*1000};
    std::chrono::milliseconds second_time_frame_start = current_timestamp + std::chrono::milliseconds {60*1000};
    std::chrono::milliseconds second_time_frame_stop = current_timestamp + std::chrono::milliseconds {90*1000};
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_stop,
                    -1
                    )
                    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
            );

    std::vector<TimeFrameStruct> generated_time_frames;

    generated_time_frames.emplace_back(
        TimeFrameStruct(
            second_time_frame_start,
            1
        )
    );

    generated_time_frames.emplace_back(
        TimeFrameStruct(
            second_time_frame_stop,
            -1
        )
    );

    ASSERT_EQ(appended_time_frames.size(), generated_time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], generated_time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), generated_time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                generated_time_frames[i].time.count()
                );

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
                generated_time_frames[i].startStopValue
                );

        i++;
    }
}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_missingStopTime) {

    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds second_time_frame_start = first_time_frame_start + std::chrono::milliseconds {30*1000};
    std::chrono::milliseconds second_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {90*1000};
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    second_time_frame_stop,
                    -1
                    )
                    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
            );

    std::vector<TimeFrameStruct> generated_time_frames;

    ASSERT_EQ(appended_time_frames.size(), generated_time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], generated_time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), generated_time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                generated_time_frames[i].time.count()
                );

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
                generated_time_frames[i].startStopValue
                );

        i++;
    }
}

TEST_F(SetCategoriesHelpers, sortTimeFramesAndRemoveOverlaps_sameTimeFrames) {

    std::chrono::milliseconds first_time_frame_start = getCurrentTimestamp();
    std::chrono::milliseconds first_time_frame_stop = first_time_frame_start + std::chrono::milliseconds {60*1000};
    std::vector<TimeFrameStruct> time_frames;

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
            )
    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    std::vector<TimeFrameStruct> appended_time_frames;

    auto run_with_append = [&](TimeFrameStruct& time_frame){
        appended_time_frames.emplace_back(time_frame);
    };

    auto clear_append_timeframes = [&](){
        appended_time_frames.clear();
    };

    bsoncxx::array::value sorted_timeframes = sort_time_frames_and_remove_overlaps(
            time_frames,
            run_with_append,
            clear_append_timeframes
            );

    std::vector<TimeFrameStruct> generated_time_frames;

    generated_time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_start,
                    1
                    )
                    );

    generated_time_frames.emplace_back(
            TimeFrameStruct(
                    first_time_frame_stop,
                    -1
                    )
                    );

    ASSERT_EQ(appended_time_frames.size(), generated_time_frames.size());
    int i = 0;
    for(; i < (int)appended_time_frames.size(); i++) {
        EXPECT_EQ(appended_time_frames[i], generated_time_frames[i]);
    }

    bsoncxx::array::view sorted_timeframes_view = sorted_timeframes.view();

    ASSERT_EQ(std::distance(sorted_timeframes_view.begin(), sorted_timeframes_view.end()), generated_time_frames.size());

    i = 0;
    for(const auto& ele : sorted_timeframes_view) {
        bsoncxx::document::view time_frame_doc = ele.get_document().value;

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::TIME].get_int64().value,
                generated_time_frames[i].time.count()
                );

        EXPECT_EQ(
                time_frame_doc[user_account_keys::categories::timeframes::START_STOP_VALUE].get_int32().value,
                generated_time_frames[i].startStopValue
                );

        i++;
    }
}