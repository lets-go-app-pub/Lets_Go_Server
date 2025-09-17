//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <utility>

#include "time_frame_struct.h"

struct ActivityStruct {

    int activityIndex = 0;
    std::vector<TimeFrameStruct> timeFrames{};
    std::chrono::milliseconds totalTime = std::chrono::milliseconds{0};

    ActivityStruct() = default;

    ActivityStruct(int _activityIndex, std::vector<TimeFrameStruct> _timeFrames, const std::chrono::milliseconds& _totalTime) :
            activityIndex(_activityIndex), timeFrames(std::move(_timeFrames)), totalTime(_totalTime) {}

    ActivityStruct(int _activityIndex, std::vector<TimeFrameStruct> _timeFrames) :
            activityIndex(_activityIndex), timeFrames(std::move(_timeFrames)) {}

    explicit ActivityStruct(int _activityIndex) :
            activityIndex(_activityIndex){}

    bool operator== (const ActivityStruct& other) const {

        if (activityIndex != other.activityIndex)
            return false;

        if (totalTime != other.totalTime)
            return false;

        if (timeFrames.size() != other.timeFrames.size())
            return false;

        for (size_t i = 0; i < timeFrames.size(); i++) {
            if (timeFrames[i] != other.timeFrames[i]) {
                return false;
            }
        }

        return true;
    }

    bool operator!= (const ActivityStruct& other) const {
        return !(*this == other);
    }
};
