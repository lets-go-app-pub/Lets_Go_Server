//
// Created by jeremiah on 3/18/21.
//

#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <global_bsoncxx_docs.h>
#include <AccountCategoryEnum.grpc.pb.h>
#include <utility_testing_functions.h>


#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"

#include "request_helper_functions.h"
#include "database_names.h"
#include "collection_names.h"
#include "activities_info_keys.h"
#include "user_account_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

bool requestBirthdayHelper(const bsoncxx::document::view& userAccountDocView, BirthdayMessage* response) {

    auto birthYearElement = userAccountDocView[user_account_keys::BIRTH_YEAR];
    if (!birthYearElement || birthYearElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, birthYearElement,
                        userAccountDocView, bsoncxx::type::k_int32, user_account_keys::BIRTH_YEAR,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    auto birthMonthElement = userAccountDocView[user_account_keys::BIRTH_MONTH];
    if (!birthMonthElement
        || birthMonthElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, birthMonthElement,
                        userAccountDocView, bsoncxx::type::k_int32, user_account_keys::BIRTH_MONTH,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    auto birthDayOfMonthElement = userAccountDocView[user_account_keys::BIRTH_DAY_OF_MONTH];
    if (!birthDayOfMonthElement
        || birthDayOfMonthElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, birthDayOfMonthElement,
                        userAccountDocView, bsoncxx::type::k_int32, user_account_keys::BIRTH_DAY_OF_MONTH,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    auto storedAgeElement = userAccountDocView[user_account_keys::AGE];
    if (!storedAgeElement
        || storedAgeElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, storedAgeElement,
                        userAccountDocView, bsoncxx::type::k_int32, user_account_keys::AGE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    response->set_birth_year(birthYearElement.get_int32().value);
    response->set_birth_month(birthMonthElement.get_int32().value);
    response->set_birth_day_of_month(birthDayOfMonthElement.get_int32().value);
    response->set_age(storedAgeElement.get_int32().value);

    return true;
}

bool requestEmailHelper(const bsoncxx::document::view& userAccountDocView, EmailMessage* response) {

    auto emailAddressElement = userAccountDocView[user_account_keys::EMAIL_ADDRESS];
    if (!emailAddressElement
        || emailAddressElement.type() != bsoncxx::type::k_utf8) { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, emailAddressElement,
                        userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::EMAIL_ADDRESS,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    auto requiresEmailAddressVerificationElement = userAccountDocView[user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION];
    if (!requiresEmailAddressVerificationElement
        || requiresEmailAddressVerificationElement.type() != bsoncxx::type::k_bool) { //if element does not exist or is not type bool
        logElementError(__LINE__, __FILE__, requiresEmailAddressVerificationElement,
                        userAccountDocView, bsoncxx::type::k_bool, user_account_keys::EMAIL_ADDRESS_REQUIRES_VERIFICATION,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    response->set_requires_email_verification(requiresEmailAddressVerificationElement.get_bool().value);
    response->set_email(emailAddressElement.get_string().value.to_string());

    return true;
}

bool requestGenderHelper(const bsoncxx::document::view& userAccountDocView, std::string* response) {

    auto genderElement = userAccountDocView[user_account_keys::GENDER];
    if (genderElement && genderElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        *response = genderElement.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, genderElement,
                        userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::GENDER,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    return true;
}

bool requestNameHelper(const bsoncxx::document::view& userAccountDocView, std::string* response) {

    auto firstNameElement = userAccountDocView[user_account_keys::FIRST_NAME];
    if (firstNameElement && firstNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        *response = firstNameElement.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, firstNameElement,
                        userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::FIRST_NAME,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    return true;
}

bool requestPostLoginInfoHelper(const bsoncxx::document::view& userAccountDocView, PostLoginMessage* response) {

    std::string userBio;
    std::string userCity;
    int minAge;
    int maxAge;
    int maxDistance;
    std::vector<std::string> genderRangeVector;

    auto userBioElement = userAccountDocView[user_account_keys::BIO];
    if (userBioElement && userBioElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        userBio = userBioElement.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, userBioElement,
                        userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::BIO,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    auto userCityElement = userAccountDocView[user_account_keys::CITY];
    if (userCityElement && userCityElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
        userCity = userCityElement.get_string().value.to_string();
    } else { //if element does not exist or is not type utf8
        logElementError(__LINE__, __FILE__, userCityElement,
                        userAccountDocView, bsoncxx::type::k_utf8, user_account_keys::CITY,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    if (!extractMinMaxAgeRange(userAccountDocView, minAge, maxAge)) {
        return false;
    }

    auto maxDistanceElement = userAccountDocView[user_account_keys::MAX_DISTANCE];
    if (maxDistanceElement &&
        maxDistanceElement.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        maxDistance = maxDistanceElement.get_int32().value;
    } else { //if element does not exist or is not type int32
        logElementError(__LINE__, __FILE__, maxDistanceElement,
                        userAccountDocView, bsoncxx::type::k_int32, user_account_keys::MAX_DISTANCE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    auto genderRangeElement = userAccountDocView[user_account_keys::GENDERS_RANGE];
    if (genderRangeElement &&
        genderRangeElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
        bsoncxx::array::view genderRange = genderRangeElement.get_array().value;

        for (auto gender: genderRange) {
            if (gender.type() == bsoncxx::type::k_utf8) {
                genderRangeVector.push_back(gender.get_string().value.to_string());
            } else {
                logElementError(__LINE__, __FILE__, genderRangeElement,
                                userAccountDocView, bsoncxx::type::k_utf8,
                                user_account_keys::GENDERS_RANGE,
                                database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

                return false;
            }
        }
    } else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, genderRangeElement,
                        userAccountDocView, bsoncxx::type::k_array, user_account_keys::GENDERS_RANGE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    response->set_user_bio(std::move(userBio));
    response->set_user_city(std::move(userCity));
    response->set_min_age(minAge);
    response->set_max_age(maxAge);
    response->set_max_distance(maxDistance);

    for (std::string gender: genderRangeVector) {
        response->add_gender_range(std::move(gender));
    }

    return true;
}

//returns 1 if successful
//returns 0 if failed
//returns 2 if current_object_oid not found
//NOTE: this function is fairly similar to buildCategoriesArrayForCurrentUser() however the way it handles
// timestamps and storing them is different and so it is a separate function
bool requestCategoriesHelper(const bsoncxx::document::view& userAccountDocView,
                             google::protobuf::RepeatedPtrField<CategoryActivityMessage>* activityArray) {

    auto categoriesElement = userAccountDocView[user_account_keys::CATEGORIES];
    if (!categoriesElement
        || categoriesElement.type() != bsoncxx::type::k_array) { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, categoriesElement,
                        userAccountDocView, bsoncxx::type::k_array, user_account_keys::CATEGORIES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    bsoncxx::array::view categoriesOuterArray = categoriesElement.get_array().value;

    for (const auto& category: categoriesOuterArray) {

        if (category.type() != bsoncxx::type::k_document) { //if element is not type document
            logElementError(__LINE__, __FILE__, categoriesElement,
                            userAccountDocView, bsoncxx::type::k_document, user_account_keys::CATEGORIES,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            return false;
        }

        bsoncxx::document::view singleActivity = category.get_document().value;

        auto activityTypeElement = singleActivity[user_account_keys::categories::TYPE];
        if (!activityTypeElement
            || activityTypeElement.type() != bsoncxx::type::k_int32) { //if element does not exist or is wrong type
            logElementError(__LINE__, __FILE__, activityTypeElement,
                            singleActivity, bsoncxx::type::k_int32, user_account_keys::categories::TYPE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            return false;
        }

        //skip category type documents
        if(activityTypeElement.get_int32().value == AccountCategoryType::CATEGORY_TYPE) {
            continue;
        }

        //store the activity inside the response
        CategoryActivityMessage* singleCategory = activityArray->Add();

        auto activityIndexElement = singleActivity[user_account_keys::categories::INDEX_VALUE];
        if (activityIndexElement &&
        activityIndexElement.type() == bsoncxx::type::k_int32) { //if element exists and is correct type
            singleCategory->set_activity_index(activityIndexElement.get_int32().value);
        } else { //if element does not exist or is wrong type
            logElementError(__LINE__, __FILE__, activityIndexElement,
                            singleActivity, bsoncxx::type::k_int32, user_account_keys::categories::INDEX_VALUE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            return false;
        }

        auto timeFramesArrayElement = singleActivity[user_account_keys::categories::TIMEFRAMES];
        if (!timeFramesArrayElement
            || timeFramesArrayElement.type() != bsoncxx::type::k_array) { //if element does not exist or is wrong type
            logElementError(__LINE__, __FILE__, timeFramesArrayElement,
                            singleActivity, bsoncxx::type::k_array, user_account_keys::categories::TIMEFRAMES,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

            return false;
        }

        bsoncxx::array::view timeFrames{timeFramesArrayElement.get_array().value};
        CategoryTimeFrameMessage* gRPCTimeFrameMessage = nullptr;
        int firstElement = true;

        for (const auto& singleTime : timeFrames) {

            if (singleTime.type() != bsoncxx::type::k_document) { //if element is wrong type
                logElementError(
                        __LINE__, __FILE__,
                        timeFramesArrayElement, singleActivity,
                        bsoncxx::type::k_document,
                        user_account_keys::categories::TIMEFRAMES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );

                return false;
            }

            //store the time inside the response
            bsoncxx::document::view timeView = singleTime.get_document().value;
            long long time;
            int startStopTime;

            auto timeElement = timeView[user_account_keys::categories::timeframes::TIME];
            if (timeElement
                && timeElement.type() == bsoncxx::type::k_int64) { //if element exists and is correct type
                time = timeElement.get_int64().value;
            } else { //if element does not exist or is wrong type
                logElementError(
                        __LINE__, __FILE__,
                        timeElement, timeView,
                        bsoncxx::type::k_int64, user_account_keys::categories::timeframes::TIME,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );

                return false;
            }

            auto startStopTimeElement = timeView[user_account_keys::categories::timeframes::START_STOP_VALUE];
            if (startStopTimeElement
                && startStopTimeElement.type() == bsoncxx::type::k_int32) { //if element exists and is correct type
                startStopTime = startStopTimeElement.get_int32().value;
            } else { //if element does not exist or is wrong type
                logElementError(
                        __LINE__, __FILE__,
                        startStopTimeElement, timeView,
                        bsoncxx::type::k_int32, user_account_keys::categories::timeframes::START_STOP_VALUE,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                );

                return false;
            }

            if (firstElement) { //if this is the first element

                firstElement = false;
                if (startStopTime == -1) { //if first element is a stop time

                    //it is valid for the first element to be a stop time, so create a pair to be returned
                    gRPCTimeFrameMessage = singleCategory->add_time_frame_array();

                    gRPCTimeFrameMessage->set_start_time_frame(-1L);
                    gRPCTimeFrameMessage->set_stop_time_frame(time);

                    gRPCTimeFrameMessage = nullptr;
                } else if (startStopTime == 1) { //if first element is a start time

                    //add a new gRPC array element and set the start time
                    gRPCTimeFrameMessage = singleCategory->add_time_frame_array();
                    gRPCTimeFrameMessage->set_start_time_frame(time);
                    gRPCTimeFrameMessage->set_stop_time_frame(-1L); //set a default value
                } else { //startStopTime was not 1 or -1

                    std::string nullPtrStr = "storedPointer";

                    if (gRPCTimeFrameMessage == nullptr) {
                        nullPtrStr = "nullptr";
                    }

                    std::string errorString = "First element of startStopTime from categories array was invalid value '";
                    errorString += user_account_keys::categories::TIMEFRAMES;
                    errorString += "'";

                    std::string values = "time: ";
                    values += getDateTimeStringFromTimestamp(std::chrono::milliseconds{time});
                    values += " startStopTime: ";
                    values += std::to_string(startStopTime);
                    values += " gRPCTimeFrameMessage: ";
                    values += nullPtrStr;

                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), errorString,
                            "database", database_names::ACCOUNTS_DATABASE_NAME,
                            "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                            "userAccountDocView", userAccountDocView,
                            "Values", values
                    );

                    return false;
                }
            } else if (startStopTime == -1 &&
            gRPCTimeFrameMessage != nullptr) { //if time is a stop time

                //set the time then put the pointer to the gRPC element to null
                gRPCTimeFrameMessage->set_stop_time_frame(time);
                gRPCTimeFrameMessage = nullptr;
            } else if (startStopTime == 1) { //if time is a start time

                //add a new gRPC array element and set the start time
                gRPCTimeFrameMessage = singleCategory->add_time_frame_array();
                gRPCTimeFrameMessage->set_start_time_frame(time);
                gRPCTimeFrameMessage->set_stop_time_frame(-1L); //set a default value
            } else { //this means that startStopTime was -1 and the pointer was null OR that startStopTime was an unknown value

                std::string nullPtrStr = "storedPointer";

                if (gRPCTimeFrameMessage == nullptr) {
                    nullPtrStr = "nullptr";
                }

                std::string errorString = "First element of startStopTime from categories array was invalid value '";
                errorString += user_account_keys::categories::TIMEFRAMES;
                errorString += "'";

                std::string values = "time: ";
                values += getDateTimeStringFromTimestamp(std::chrono::milliseconds{time});
                values += " startStopTime: ";
                values += std::to_string(startStopTime);
                values += " gRPCTimeFrameMessage: ";
                values += nullPtrStr;

                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), errorString,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "userAccountDocView", userAccountDocView,
                        "Values", values
                );

                return false;
            }

        }
    }

    return true;

}

bool requestOutOfDateIconIndexValues(
        const google::protobuf::RepeatedField<google::protobuf::int64>& iconTimestamps,
        google::protobuf::RepeatedField<google::protobuf::int64>* indexToBeUpdated,
        mongocxx::database& accountsDB
) {

    mongocxx::collection iconsInfoCollection = accountsDB[collection_names::ICONS_INFO_COLLECTION_NAME];

    bsoncxx::document::value findOutdatedIndex = buildFindOutdatedIconIndexes(iconTimestamps);

    mongocxx::options::find findOpts;

    findOpts.projection(
            document{}
                    << "_id" << 1
            << finalize
    );

    mongocxx::stdx::optional<mongocxx::cursor> findIndexCursor;
    try {
        findIndexCursor = iconsInfoCollection.find(findOutdatedIndex.view(), findOpts);
    } catch (const mongocxx::logic_error& e) {
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), e.what(),
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ACTIVITIES_INFO_COLLECTION_NAME
        );

        return false;
    }

    if(findIndexCursor) { //cursor found
        for (const auto& doc : *findIndexCursor) {
            //index
            auto indexElement = doc["_id"];
            if (indexElement &&
                    indexElement.type() == bsoncxx::type::k_int64) { //if element exists and is type int64
                *indexToBeUpdated->Add() = indexElement.get_int64().value;
            } else { //if element does not exist or is not type int64
                logElementError(
                        __LINE__, __FILE__,
                        indexElement,doc,
                        bsoncxx::type::k_int64, "_id",
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::ICONS_INFO_COLLECTION_NAME
                );

                continue;
            }
        }
    } else { //cursor not found
        const std::string error_string = "Cursor was not found after a find was performed and no exception was returned.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ACTIVITIES_INFO_COLLECTION_NAME
        );
        return false;
    }

    return true;
}

bool requestAllServerCategoriesActivitiesHelper(
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* grpcCategoriesArray,
        google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage>* grpcActivitiesArray,
        mongocxx::database& accountsDB) {

    mongocxx::collection activitiesInfoCollection = accountsDB[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> activitiesCategoriesResult;
    std::optional<std::string> activitiesCategoriesExceptionString;
    try {
        activitiesCategoriesResult = activitiesInfoCollection.find_one(
                document{}
                        << "_id" << activities_info_keys::ID
                << finalize
        );
    } catch (const mongocxx::logic_error& e) {
        activitiesCategoriesExceptionString = e.what();
    }

    if(activitiesCategoriesResult) {

        const bsoncxx::document::view activitiesCategoriesDocView = *activitiesCategoriesResult;
        bsoncxx::array::view categoriesArrayView;
        bsoncxx::array::view activitiesArrayView;

        auto categoriesArrayElement = activitiesCategoriesDocView[activities_info_keys::CATEGORIES];
        if (categoriesArrayElement && categoriesArrayElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
            categoriesArrayView = categoriesArrayElement.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(
                    __LINE__, __FILE__,
                    categoriesArrayElement, activitiesCategoriesDocView,
                    bsoncxx::type::k_array, activities_info_keys::CATEGORIES,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
            );
            return false;
        }

        auto activitiesArrayElement = activitiesCategoriesDocView[activities_info_keys::ACTIVITIES];
        if (activitiesArrayElement && activitiesArrayElement.type() == bsoncxx::type::k_array) { //if element exists and is type array
            activitiesArrayView = activitiesArrayElement.get_array().value;
        } else { //if element does not exist or is not type array
            logElementError(
                    __LINE__, __FILE__,
                    activitiesArrayElement, activitiesCategoriesDocView,
                    bsoncxx::type::k_array, activities_info_keys::ACTIVITIES,
                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
            );
            return false;
        }

        int index = 0;

        //store categories from array to response
        for(const auto& categoryElement : categoriesArrayView) {

            if(categoryElement.type() == bsoncxx::type::k_document) {

                const bsoncxx::document::view categoryDocView = categoryElement.get_document().value;

                ServerActivityOrCategoryMessage* gRPCCategory = grpcCategoriesArray->Add();

                //index
                gRPCCategory->set_index(index);

                //order number
                auto orderNumberElement = categoryDocView[activities_info_keys::categories::ORDER_NUMBER];
                if (orderNumberElement
                    && orderNumberElement.type() == bsoncxx::type::k_double) { //if element exists and is type double
                    gRPCCategory->set_order_number(orderNumberElement.get_double().value);
                } else { //if element does not exist or is not type double
                    logElementError(
                            __LINE__, __FILE__,
                            orderNumberElement, categoryDocView,
                            bsoncxx::type::k_double, activities_info_keys::categories::ORDER_NUMBER,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //name
                auto displayNameElement = categoryDocView[activities_info_keys::categories::DISPLAY_NAME];
                if (displayNameElement
                    && displayNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    gRPCCategory->set_display_name(displayNameElement.get_string().value.to_string());
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            displayNameElement, categoryDocView,
                            bsoncxx::type::k_utf8, activities_info_keys::categories::DISPLAY_NAME,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //icon display name
                auto iconDisplayNameElement = categoryDocView[activities_info_keys::categories::ICON_DISPLAY_NAME];
                if (iconDisplayNameElement
                    && iconDisplayNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    gRPCCategory->set_icon_display_name(iconDisplayNameElement.get_string().value.to_string());
                } else { //if element does not exist or is not type utf8
                    logElementError(__LINE__, __FILE__, iconDisplayNameElement,
                                    categoryDocView, bsoncxx::type::k_utf8, activities_info_keys::categories::DISPLAY_NAME,
                                    database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME);

                    return false;
                }

                //min_age
                auto minAgeElement = categoryDocView[activities_info_keys::categories::MIN_AGE];
                if (minAgeElement
                    && minAgeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type utf8
                    gRPCCategory->set_min_age(minAgeElement.get_int32().value);
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            minAgeElement, categoryDocView,
                            bsoncxx::type::k_int32, activities_info_keys::categories::MIN_AGE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //color
                auto colorElement = categoryDocView[activities_info_keys::categories::COLOR];
                if (colorElement
                    && colorElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    gRPCCategory->set_color(colorElement.get_string().value.to_string());
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            colorElement, categoryDocView,
                            bsoncxx::type::k_utf8, activities_info_keys::categories::COLOR,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

            } else { //array element was not a document
                const std::string error_string = "An element inside the categories array was not type document.";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::ACTIVITIES_INFO_COLLECTION_NAME,
                        "type", convertBsonTypeToString(categoryElement.type())
                );
                return false;
            }

            index++;
        }

        index = 0;

        //store activities from array to response
        for(const auto& activityElement : activitiesArrayView) {

            if(activityElement.type() == bsoncxx::type::k_document) {

                const bsoncxx::document::view activityDocView = activityElement.get_document().value;

                ServerActivityOrCategoryMessage* gRPCCategory = grpcActivitiesArray->Add();

                //index
                gRPCCategory->set_index(index);

                //name
                auto displayNameElement = activityDocView[activities_info_keys::activities::DISPLAY_NAME];
                if (displayNameElement &&
                    displayNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    gRPCCategory->set_display_name(displayNameElement.get_string().value.to_string());
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            displayNameElement, activityDocView,
                            bsoncxx::type::k_utf8, activities_info_keys::activities::DISPLAY_NAME,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //icon display name
                auto iconDisplayNameElement = activityDocView[activities_info_keys::activities::ICON_DISPLAY_NAME];
                if (iconDisplayNameElement &&
                iconDisplayNameElement.type() == bsoncxx::type::k_utf8) { //if element exists and is type utf8
                    gRPCCategory->set_icon_display_name(iconDisplayNameElement.get_string().value.to_string());
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            iconDisplayNameElement, activityDocView,
                            bsoncxx::type::k_utf8, activities_info_keys::activities::DISPLAY_NAME,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //min_age
                auto minAgeElement = activityDocView[activities_info_keys::activities::MIN_AGE];
                if (minAgeElement &&
                    minAgeElement.type() == bsoncxx::type::k_int32) { //if element exists and is type utf8
                    gRPCCategory->set_min_age(minAgeElement.get_int32().value);
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            minAgeElement, activityDocView,
                            bsoncxx::type::k_int32, activities_info_keys::activities::MIN_AGE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //category_index
                auto categoryIndexElement = activityDocView[activities_info_keys::activities::CATEGORY_INDEX];
                if (categoryIndexElement &&
                    categoryIndexElement.type() == bsoncxx::type::k_int32) { //if element exists and is type utf8
                    gRPCCategory->set_category_index(categoryIndexElement.get_int32().value);
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            categoryIndexElement, activityDocView,
                            bsoncxx::type::k_int32, activities_info_keys::activities::MIN_AGE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

                //icon_index
                auto iconIndexElement = activityDocView[activities_info_keys::activities::ICON_INDEX];
                if (iconIndexElement &&
                    iconIndexElement.type() == bsoncxx::type::k_int32) { //if element exists and is type utf8
                    gRPCCategory->set_icon_index(iconIndexElement.get_int32().value);
                } else { //if element does not exist or is not type utf8
                    logElementError(
                            __LINE__, __FILE__,
                            iconIndexElement, activityDocView,
                            bsoncxx::type::k_int32, activities_info_keys::activities::ICON_INDEX,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::ACTIVITIES_INFO_COLLECTION_NAME
                    );
                    return false;
                }

            } else { //array element was not a document
                const std::string error_string = "An element inside the activities array was not type document.";
                storeMongoDBErrorAndException(
                        __LINE__, __FILE__,
                        std::optional<std::string>(), error_string,
                        "database", database_names::ACCOUNTS_DATABASE_NAME,
                        "collection", collection_names::USER_ACCOUNTS_COLLECTION_NAME,
                        "type", convertBsonTypeToString(activityElement.type())
                );
                return false;
            }

            index++;
        }

    } else {
        const std::string error_string = "Could not find ID inside ACTIVITIES_INFO_COLLECTION_NAME, this should ALWAYS exist.";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                activitiesCategoriesExceptionString, error_string,
                "database", database_names::ACCOUNTS_DATABASE_NAME,
                "collection", collection_names::ACTIVITIES_INFO_COLLECTION_NAME
        );
        return false;
    }

    return true;
}

//helper function for requestPicture; sends back pictures requested by picture index values inside requestedIndexes
//NOTE: Order relative to requestedIndexes is NOT guaranteed for the pictures being returned.
bool requestPicturesHelper(
        std::set<int>& requestedIndexes,
        mongocxx::database& accountsDB,
        const bsoncxx::oid& userAccountOID,
        const bsoncxx::document::view& userAccountDocView,
        mongocxx::client& mongoCppClient,
        mongocxx::collection& userAccountsCollection,
        const std::chrono::milliseconds& currentTimestamp,
        const std::function<void(int)>& setPictureEmptyResponse,
        const std::function<void(
                 const bsoncxx::oid& /*picture_oid*/,
                 std::string&& /*picture_byte_string*/,
                 int /*picture_size*/,
                 int /*index_number*/,
                 const std::chrono::milliseconds& /*picture_timestamp*/,
                 bool /*extracted_from_deleted_pictures*/,
                 bool /*references_removed_after_delete*/
        )>& setPictureToResponse,
        mongocxx::client_session* session,
        mongocxx::database* deletedDB //If searching deleted pictures is not wanted; set to null.
) {

    auto picturesElement = userAccountDocView[user_account_keys::PICTURES];
    if (picturesElement
        && picturesElement.type() == bsoncxx::type::k_array) { //if element exists and is type array

        bsoncxx::array::view pictureReferenceArray{picturesElement.get_array().value};

        std::set<int> non_empty_indexes;
        bsoncxx::builder::basic::array requestedPictureOid;
        for(int indexOfArray : requestedIndexes) {
            auto pictureElement = pictureReferenceArray[indexOfArray];

            if (pictureElement && pictureElement.type() == bsoncxx::type::k_document) {
                non_empty_indexes.insert(indexOfArray);
                auto pictureDocument = pictureElement.get_document().value;
                bsoncxx::oid picture_oid;

                //extract picture oid
                auto pictureOIDElement = pictureDocument[user_account_keys::pictures::OID_REFERENCE];
                if (pictureOIDElement &&
                    pictureOIDElement.type() == bsoncxx::type::k_oid) { //if element exists and is type oid
                    picture_oid = pictureOIDElement.get_oid().value;
                    requestedPictureOid.append(picture_oid);
                } else { //if element does not exist or is not type oid
                    logElementError(
                            __LINE__, __FILE__,
                            pictureOIDElement, pictureDocument,
                            bsoncxx::type::k_oid, user_account_keys::pictures::OID_REFERENCE,
                            database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME
                    );
                    return false;
                }
            } else { //if type is not document

                //this means there is no picture at this index
                setPictureEmptyResponse(indexOfArray);
            }
        }

        if(!requestedPictureOid.view().empty()) {
            //this function stores all requested pictures to a response
            if(!extractAllPicturesToResponse(mongoCppClient, accountsDB, userAccountsCollection, currentTimestamp,
                                         userAccountDocView, requestedPictureOid.view(), non_empty_indexes,
                                         userAccountOID, session, setPictureToResponse,
                                         setPictureEmptyResponse, deletedDB)) {
                return false;
            }
        }

    } else { //if element does not exist or is not type array
        logElementError(__LINE__, __FILE__, picturesElement,
                        userAccountDocView, bsoncxx::type::k_array, user_account_keys::PICTURES,
                        database_names::ACCOUNTS_DATABASE_NAME, collection_names::USER_ACCOUNTS_COLLECTION_NAME);

        return false;
    }

    return true;
}