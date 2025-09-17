//
// Created by jeremiah on 3/20/21.
//

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <iostream>
#include <activity_struct.h>
#include <connection_pool_global_variable.h>

#include "generate_manual_accounts.h"

#include "server_initialization_functions.h"
#include "generate_random_account_info/generate_random_account_info.h"
#include "generate_and_store_complete_account/generate_and_store_complete_account.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

std::chrono::milliseconds getTimestampFromTimeAndDate(int month, int day, int hour, int min = 0, int sec = 0) {

    tm timeTm;

    timeTm.tm_year = 2020 - 1900;
    timeTm.tm_mon = month - 1;
    timeTm.tm_mday = day;
    timeTm.tm_hour = hour;
    timeTm.tm_min = min;
    timeTm.tm_sec = sec;
    timeTm.tm_isdst = -1;

    return std::chrono::milliseconds {timegm(&timeTm)};
}

void insertManualAccounts() {

    setupMongoDBIndexing();

    //222 pictures starting at index 0
    const int numberTestingPicturesInFile = 222;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::collection collection = mongoCppClient["Pictures_For_Testing"]["Pics"];

    int numAccountsToInsert = 2;
    for (int i = 0; i < numAccountsToInsert; i++) {

        std::cout << "Inserting Account Number: " << i + 1 << '\n';

        int numberPictures = rand() % general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT + 1;

        std::vector<std::tuple<const int, const std::string, const std::string>> pictureAndIndexVector;

        for (int j = 0; j < numberPictures; j++) {
            int pictureIndex = rand() % numberTestingPicturesInFile;

            auto pictureDocVal = collection.find_one(document{}
                                                             << "_id" << pictureIndex
                                                             << finalize);

            pictureAndIndexVector.emplace_back(
                    j,
                    pictureDocVal->view()["pic"].get_string().value.to_string(),
                    pictureDocVal->view()["thumbnail"].get_string().value.to_string()
            );
        }

        //theoretically this could make duplicates
        std::string phoneNumber = "+1";

        for (int j = 0; j < 10; j++) {
            phoneNumber += std::to_string(rand() % 10);
        }

        std::string gender;
        int randomGender = rand() % 3;
        switch (randomGender)
        {
            case 0:
                gender = general_values::MALE_GENDER_VALUE;
                break;
            case 1:
                gender = general_values::FEMALE_GENDER_VALUE;
                break;
            case 2:
                gender = pickRandomGenderOther();
                break;
            default:
                gender = "";
                break;
        }

        std::vector<std::string> genderRange;

        while (genderRange.empty()) {

            int useEveryone = rand() % 2;

            if (useEveryone == 1) {
                genderRange.push_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
            }
            else {
                int useMale = rand() % 2;
                if (useMale == 1) {
                    genderRange.push_back(general_values::MALE_GENDER_VALUE);
                }

                int useFemale = rand() % 2;
                if (useFemale == 1) {
                    genderRange.push_back(general_values::FEMALE_GENDER_VALUE);
                }

                int useGenderOther = rand() % 2;
                if (useGenderOther == 1) {
                    int numberGenderOthers = rand() % (
                            server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH - genderRange.size());

                    for (int j = 0; j < numberGenderOthers; j++) {
                        genderRange.emplace_back(pickRandomGenderOther());
                    }
                }
            }
        }

        if (i == 0) {

            std::vector<ActivityStruct> insertedActivities{

                    //multiple between timeframes
                    ActivityStruct(
                            0,
                            std::vector<TimeFrameStruct> {
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 23, 17),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 23, 18),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 25, 17),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 25, 18),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 26, 17),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 26, 18),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 27, 17),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 27, 18),
                                            -1
                                    ),
                            }
                    ),
                    ActivityStruct(
                            1,
                            std::vector<TimeFrameStruct> {
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 11, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 11, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 12, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 12, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 17, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 17, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 18, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 18, 30),
                                            -1
                                    ),
                            }
                    ),
                    ActivityStruct(4),
                    ActivityStruct(9),
            };

            generateAndStoreCompleteAccount(phoneNumber,
                                            "email@fake.first", 1992, 1, 1,
                                            pickRandomName(),
                                            general_values::MALE_GENDER_VALUE,
                                            pictureAndIndexVector,
                                            insertedActivities,
                                            -122, 37,
                                            18, 50,
                                            std::vector<std::string>{
                                                    general_values::MALE_GENDER_VALUE
                                            },
                                            50,
                                            pickRandomCity(), pickRandomBio());
        }
        else {

            std::vector<ActivityStruct> insertedActivities{

                    //multiple between timeframes
                    ActivityStruct(
                            0,
                            std::vector<TimeFrameStruct> {
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 15),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 16),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 16, 30),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 17, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 19, 30),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 20, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 21),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 24, 22),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 25, 10),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 20, 11),
                                            -1
                                    ),
                            }
                    ),
                    ActivityStruct(
                            1,
                            std::vector<TimeFrameStruct> {
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 11, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 11, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 12, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 12, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 17, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 17, 30),
                                            -1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 18, 00),
                                            1
                                    ),
                                    TimeFrameStruct(
                                            getTimestampFromTimeAndDate(8, 28, 18, 30),
                                            -1
                                    ),
                            }
                    ),
                    ActivityStruct(4),
                    ActivityStruct(9),
            };

            generateAndStoreCompleteAccount(phoneNumber,
                                            "email@fake.first", 1992, 1, 1,
                                            pickRandomName(),
                                            general_values::MALE_GENDER_VALUE,
                                            pictureAndIndexVector,
                                            insertedActivities,
                                            -122, 37,
                                            18, 50,
                                            std::vector<std::string>{
                                                    general_values::MALE_GENDER_VALUE
                                            },
                                            50,
                                            pickRandomCity(), pickRandomBio());

        }

    }

}