//
// Created by jeremiah on 3/19/21.
//
#pragma once

#include <chrono>

struct TimeFrameStruct {
    std::chrono::milliseconds time = std::chrono::milliseconds{-1}; //time for the start/stop of this timeframe
    int startStopValue = 0; //-1 for stop; 1 for start
    bool fromUser = true;

    TimeFrameStruct(std::chrono::milliseconds _time, int _startStopValue) :
            time(_time), startStopValue(_startStopValue) {}

    bool operator== (const TimeFrameStruct& other) const {
        return (time == other.time
                && startStopValue == other.startStopValue
                && fromUser == other.fromUser);
    }

    bool operator!= (const TimeFrameStruct& other) const {
        return !(*this == other);
    }
};

