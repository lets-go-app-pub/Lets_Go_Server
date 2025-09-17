//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include "activity_struct.h"

class TimeFrameValue {

public:
    std::chrono::milliseconds timeStamp{};
    bool isStopTime;
    bool isClientTime;

    TimeFrameValue(std::chrono::milliseconds _timeStamp, bool _isStopTime, bool _isClientTime):
            timeStamp(_timeStamp), isStopTime(_isStopTime), isClientTime(_isClientTime) {}

    friend std::ostream& operator << (std::ostream& out, const TimeFrameValue& c);
};

struct MyPairStruct
{
    std::chrono::milliseconds startTime;
    std::chrono::milliseconds stopTime;

    MyPairStruct(const std::chrono::milliseconds& a, const std::chrono::milliseconds& b)
            : startTime(a), stopTime(b) {}
};

//generates an array of randomized activities
std::vector<ActivityStruct> generateRandomActivities(
        int age,
        const google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>& grpcCategoriesArray,
        const google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>& grpcActivitiesArray
        );