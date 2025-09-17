//
// Created by jeremiah on 3/20/21.
//

#include <iostream>

#include <mongocxx/uri.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/exception/exception.hpp>

#include <utility_general_functions.h>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <request_helper_functions.h>

#include "server_initialization_functions.h"
#include "generate_multiple_random_accounts.h"

#include "generate_random_account_info/generate_random_account_info.h"
#include "generate_random_account_info/generate_random_user_activities.h"
#include "generate_and_store_complete_account/generate_and_store_complete_account.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"

#include <tbb/concurrent_unordered_set.h>

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/**
 *
 * NOTE: Min age is not taken into account for activities and so it is possible that
 * a user generates all activities that they are too young for causing verification function
 * to fail.
 *
 * Returns the most recently inserted account OID. If only 1 account is inserted, it will be that
 * account oid.
 *
 * **/
bsoncxx::oid insertRandomAccounts(int numAccountsToInsert, int threadIndex, bool use_unique_set) {

    srand(threadIndex + time(nullptr));

    //222 pictures starting at index 0
    const int numberTestingPicturesInFile = 222;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongoCppClient[database_names::ACCOUNTS_DATABASE_NAME];

    google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage> grpcCategoriesArray;
    google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage> grpcActivitiesArray;

    //request categories and activities from database
    if (!requestAllServerCategoriesActivitiesHelper(
            &grpcCategoriesArray,
            &grpcActivitiesArray,
            accounts_db)) {
        std::cout << "Error extracting categories and/or activities\n";
        return bsoncxx::oid("000000000000000000000000");
    }

    bsoncxx::oid most_recent_account_oid("000000000000000000000000");

    mongocxx::collection collection = mongoCppClient["Pictures_For_Testing"]["Pics"];

    for (int i = 0; i < numAccountsToInsert; i++) {

        srand(getCurrentTimestamp().count());

        if ((numAccountsToInsert > 20) & (i % 20 == 0)) {
            std::cout << std::string("Inserting Account Number: " + std::to_string(i + 1) + "\n");
        }

        int numberPictures = rand() % general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT + 1;

        std::vector<std::tuple<const int, const std::string, const std::string>> pictureAndIndexVector;

        for (int j = 0; j < numberPictures; j++) {
            int pictureIndex = rand() % numberTestingPicturesInFile;

            try {
                auto pictureDocVal = collection.find_one(
                    document{}
                        << "_id" << pictureIndex
                    << finalize
                );

                pictureAndIndexVector.emplace_back(
                        j,
                        pictureDocVal->view()["pic"].get_string().value.to_string(),
                        pictureDocVal->view()["thumbnail"].get_string().value.to_string()
                );
            }
            catch (const mongocxx::logic_error& e) {
                std::cout << "Mongocxx exception: " << e.what() << '\n';
            }
            catch (bsoncxx::exception& e) {
                std::cout << "Bsoncxx exception: " << e.what() << '\n';
            }
            catch (std::exception& e) {
                std::cout << "std exception: " << e.what() << '\n';
            }
        }

        std::string phoneNumber = "+1";

        if(use_unique_set) {

            bool unique = false;

            while(!unique) {
                phoneNumber = "+1";

                for (int j = 0; j < 10; j++) {
                    //do not allow area code to start with 0 or 1
                    phoneNumber += std::to_string(j == 0 ? rand() % 8 + 2 : rand() % 10);
                }

                //Do not allow an area code of 555
                while(phoneNumber[2] == '5' && phoneNumber[3] == '5' && phoneNumber[4] == '5') {
                    for (int j = 2; j < 5; j++) {
                        //do not allow area code to start with 0 or 1
                        phoneNumber[j] = j == 2 ? (char)('2' + rand() % 8) : (char)('0' + rand() % 10);
                    }
                }

                //prevents duplicates
                unique = unique_phone_numbers.insert(phoneNumber).second;
            }
        } else {
            for (int j = 0; j < 10; j++) {
                //do not allow area code to start with 0 or 1
                phoneNumber += std::to_string(j == 0 ? rand() % 8 + 2 : rand() % 10);
            }

            //Do not allow an area code of 555
            while(phoneNumber[2] == '5' && phoneNumber[3] == '5' && phoneNumber[4] == '5') {
                for (int j = 2; j < 5; j++) {
                    //do not allow area code to start with 0 or 1
                    phoneNumber[j] = j == 2 ? (char)('2' + rand() % 8) : (char)('0' + rand() % 10);
                }
            }
        }

        std::string gender;
        int randomGender = rand() % 2;
        switch (randomGender) {
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

            int useMale = rand() % 2;
            if (useMale == 1) {
                genderRange.push_back(general_values::MALE_GENDER_VALUE);
            }

            int useFemale = rand() % 2;
            if (useFemale == 1) {
                genderRange.push_back(general_values::FEMALE_GENDER_VALUE);
            }

//            int useEveryone = rand() % 2;
//
//            if (useEveryone == 1) {
//                genderRange.push_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
//            } else {
//                int useMale = rand() % 2;
//                if (useMale == 1) {
//                    genderRange.push_back(general_values::MALE_GENDER_VALUE);
//                }
//
//                int useFemale = rand() % 2;
//                if (useFemale == 1) {
//                    genderRange.push_back(general_values::FEMALE_GENDER_VALUE);
//                }
//
//                int useGenderOther = rand() % 2;
//                if (useGenderOther == 1) {
//                    int numberGenderOthers = rand() % (
//                            server_parameter_restrictions::NUMBER_GENDER_USER_CAN_MATCH_WITH - genderRange.size());
//
//                    for (int j = 0; j < numberGenderOthers; j++) {
//                        genderRange.emplace_back(pickRandomGenderOther());
//                    }
//                }
//            }
        }

        int maxDistance = (rand() % server_parameter_restrictions::MAXIMUM_ALLOWED_DISTANCE + 1);

        int birthYear = 2020 - (rand() % 67 + 13);
        int birthMonth = rand() % 12 + 1;
        int birthDay = rand() % 29 + 1;

        int age = calculateAge(getCurrentTimestamp(), birthYear, birthMonth, birthDay);

        if (age < server_parameter_restrictions::LOWEST_ALLOWED_AGE) { //if the age is under 13
            birthMonth = 1;
            birthDay = 1;
            age = calculateAge(getCurrentTimestamp(), birthYear, birthMonth, birthDay);
        }

        int minAge;
        int maxAge;

        if (13 <= age && age <= 15) {
            minAge = rand() % 5 + 13;
            maxAge = rand() % (1 + 17 - minAge) + minAge;
        } else if (16 <= age && age <= 17) {
            minAge = rand() % (5 + (age - 15)) + 13;
            maxAge = rand() % ((1 + age + 2) - minAge) + minAge;
        } else if (18 <= age && age <= 19) {
            minAge = rand() % 80 + (18 - age - 16);
            maxAge = rand() % (98 - minAge + 15) + minAge;
        } else {
            minAge = rand() % 80 + 18;
            maxAge = rand() % (98 - minAge + 15) + minAge;
        }

        //double longitude = -122.099088292; //somewhere in california
        double longitude = -111.960205859; //somewhere in phoenix

        int numDigits = rand() % 8 + 1;
        for (double j = 2; j < numDigits; j++) {

            double digit = rand() % 10;
            longitude -= digit / pow(10, j);
        }

        //double latitude = 37.3773622787;  //somewhere in california
        double latitude = 33.3741437765;  //somewhere in phoenix

        numDigits = rand() % 8 + 1;
        for (double j = 2; j < numDigits; j++) {

            double digit = rand() % 10;
            latitude += digit / pow(10, j);;
        }

        std::string city = "~";

        if (rand() % 3 != 0) {
            city = pickRandomCity();
        }

        std::string bio = "~";

        if (rand() % 3 != 0) {
            bio = pickRandomBio();
        }

        std::string most_recent_account_oid_str =
                        generateAndStoreCompleteAccount(phoneNumber,
                                                        "email@fake.first", birthYear, birthMonth, birthDay,
                                                        pickRandomName(),
                                                        gender,
                                                        pictureAndIndexVector,
                                                        generateRandomActivities(
                                                                age,
                                                                grpcCategoriesArray,
                                                                grpcActivitiesArray
                                                        ),
                                                        longitude, latitude,
                                                        minAge, maxAge,
                                                        genderRange,
                                                        maxDistance,
                                                        city, bio);

        if(!isInvalidOIDString(most_recent_account_oid_str)) {
            most_recent_account_oid = bsoncxx::oid{most_recent_account_oid_str};
        }

    }

    return most_recent_account_oid;

}