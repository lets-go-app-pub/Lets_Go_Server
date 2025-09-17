//
// Created by jeremiah on 3/20/21.
//

#include <vector>
#include <set>
#include <utility_general_functions.h>

#include "generate_random_user_activities.h"

#include "server_parameter_restrictions.h"
#include "general_values.h"

std::vector<ActivityStruct> generateRandomActivities(
        int age,
        const google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>& grpcCategoriesArray,
        const google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>& grpcActivitiesArray
) {

    std::vector<bool> activity_valid(grpcActivitiesArray.size(), true);

    for (int i = 0; i < grpcActivitiesArray.size(); i++) {
        const auto& activity = grpcActivitiesArray[i];
        if (grpcCategoriesArray.size() < activity.category_index()
            || grpcCategoriesArray[activity.category_index()].min_age() > age
            || activity.min_age() > age) {
            activity_valid[i] = false;
        }
    }

    std::vector<ActivityStruct> categories;

    //This must be at least 1 exe: a value from 1-4 if 5 possible activities.
    int numberActivities = rand() % (server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT - 1) + 1;

    std::set<int> uniqueActivities = std::set<int>();

    while (uniqueActivities.size() < (size_t) numberActivities) {
        int activity_index = (rand() % ((int)activity_valid.size()-1))+1;
        if(activity_valid[activity_index]) {
            uniqueActivities.insert(activity_index);
        }
    }

    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp(); //get the seconds as a long long so it is stored as an int64 in mongoDB
    const std::chrono::milliseconds endTimestamp = currentTimestamp +
                                                   general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES; //final value possible to be set for time frame

    for (int i : uniqueActivities) {

        //gives a 25% chance to be anytime
        int useSpecificTime = rand() % 4;
        ActivityStruct singleCategoryMongoDBArray;
        std::vector<TimeFrameStruct> timeFrames;

        if (useSpecificTime == 0) {} //if anytime then do nothing, an empty array is expected
        else {

            int numberTimeFrames = rand() % (server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY + 1);

            //auto singleCategoryTimeFrame = bsoncxx::builder::basic::array{};
            std::vector<MyPairStruct> singleCategoryTimeFrame;

            //creates the times and stores them inside the vector singleCategoryTimeFrame
            //these times are randomly generated from the min possible time to max possible time, no organization
            //the addition to numberTimeFrames is because with the elimination of overlaps it is rare to actually hit the max number of time frames
            for (int j = 0; j < numberTimeFrames + 3; j++) {
                if (j == 0 && rand() % 3 == 0) { //33% chance to start at -1

                    int stopTime = rand() % (general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count()) + 1;

                    std::chrono::milliseconds stopTimeFrame = currentTimestamp + std::chrono::milliseconds{stopTime};
                    singleCategoryTimeFrame.emplace_back(std::chrono::milliseconds{-1}, stopTimeFrame);

                } else {
                    int startTime = rand() % general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count();

                    std::chrono::milliseconds startTimeFrame = currentTimestamp + std::chrono::milliseconds{startTime};

                    int stopTime = rand() % (general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES -
                                             std::chrono::milliseconds{startTime}).count();
                    std::chrono::milliseconds stopTimeFrame = startTimeFrame + std::chrono::milliseconds{stopTime};

                    singleCategoryTimeFrame.emplace_back(startTimeFrame, stopTimeFrame);
                }
            }

            std::vector<TimeFrameValue> timeFramesVector;

            //adds more values to the previously stored values and runs a check to filer invalid data
            for (auto a : singleCategoryTimeFrame) {

                std::chrono::milliseconds stopTime = a.stopTime;
                std::chrono::milliseconds startTime = a.startTime;

                if (startTime <= stopTime && currentTimestamp < stopTime && stopTime <= endTimestamp) {

                    timeFramesVector.emplace_back(TimeFrameValue(stopTime, true, false));

                    if (currentTimestamp < startTime) { //if start time is in the future
                        timeFramesVector.emplace_back(TimeFrameValue(startTime, false, false));
                    } else { //if startTime has already passed
                        timeFramesVector.emplace_back(TimeFrameValue(std::chrono::milliseconds{-1L}, false, false));
                    }

                } else if (startTime > stopTime) {
                    std::cout << "ERROR: startTime > stopTime\n";
                    std::cout << "startTime: " << getDateTimeStringFromTimestamp(startTime) << "\n";
                    std::cout << "stopTime: " << getDateTimeStringFromTimestamp(stopTime) << "\n";
                    std::cout << "seconds: " << getDateTimeStringFromTimestamp(currentTimestamp) << "\n";
                } else if (stopTime >= endTimestamp) {
                    std::cout << "ERROR: stopTime >= endSeconds\n";
                    std::cout << "startTime: " << getDateTimeStringFromTimestamp(startTime) << "\n";
                    std::cout << "stopTime: " << getDateTimeStringFromTimestamp(stopTime) << "\n";
                    std::cout << "seconds: " << getDateTimeStringFromTimestamp(currentTimestamp) << "\n";
                } else if (currentTimestamp > stopTime) {
                    std::cout << "ERROR: seconds > stopTime\n";
                    std::cout << "startTime: " << getDateTimeStringFromTimestamp(startTime) << "\n";
                    std::cout << "stopTime: " << getDateTimeStringFromTimestamp(stopTime) << "\n";
                    std::cout << "seconds: " << getDateTimeStringFromTimestamp(currentTimestamp) << "\n";
                }
            }

            //sort vector for organizing array
            std::sort(std::begin(timeFramesVector), std::end(timeFramesVector),
                      [](const TimeFrameValue& lhs, const TimeFrameValue& rhs) -> bool {
                          if (lhs.timeStamp == rhs.timeStamp) {
                              //if lhs is start this will always return false
                              //so that says right < left which means start comes before stop
                              return lhs.isStopTime < rhs.isStopTime;
                          }
                          return lhs.timeStamp < rhs.timeStamp;
                      });

            int nestingValue = 0;
            int timeFramesInserted = 0;
            //organize the array so there are no overlapping timeframes and store them to the document for this activity
            //the extra parameter guarantees that only the desired number of time frames can be inserted
            for (size_t j = 0; j < timeFramesVector.size() && timeFramesInserted <= numberTimeFrames; j++) {

                timeFrames.emplace_back(
                        timeFramesVector[j].timeStamp, 1
                );
                nestingValue++;

                while (nestingValue > 0) {
                    j++;
                    if (timeFramesVector[j].isStopTime) {
                        nestingValue--;
                    } else {
                        nestingValue++;
                    }

                }

                timeFrames.emplace_back(
                        timeFramesVector[j].timeStamp, -1
                );

                timeFramesInserted++;
            }
        }

        categories.emplace_back(
                i, timeFrames
        );

    }

    return categories;

}

std::ostream& operator<<(std::ostream& out, const TimeFrameValue& c) {
    if (c.isClientTime)
        out << "Client; ";
    else
        out << "Match; ";

    if (c.isStopTime)
        out << "Stop Time: ";
    else
        out << "Start Time: ";

    out << getDateTimeStringFromTimestamp(c.timeStamp);

    return out;
}