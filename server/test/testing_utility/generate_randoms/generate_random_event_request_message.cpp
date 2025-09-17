//
// Created by jeremiah on 3/16/23.
//
#include <iostream>
#include <fstream>

#include <mongocxx/client.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/exception/exception.hpp>

#include "generate_randoms.h"

#include "utility_general_functions.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "request_helper_functions.h"

#include "generate_random_account_info/generate_random_user_activities.h"
#include "generate_and_store_complete_account/generate_and_store_complete_account.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "event_request_message_is_valid.h"


//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

EventRequestMessage generateRandomEventRequestMessage(
        const EventChatRoomAdminInfo& chat_room_admin_info,
        const std::chrono::milliseconds& current_timestamp
) {
    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];

    EventRequestMessage event_info;

    event_info.set_event_title(gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE + 1));

    event_info.set_bio(gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO + 1));

    event_info.set_city(gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    //10e4 will give 4 decimal places of precision (NOTE: This method will not work for more because of
    // the precision of doubles).
    int precision = 10e4;

    //latitude must be a number -90 to 90
    double latitude = rand() % (90*precision);
    if(rand() % 2) {
        latitude *= -1;
    }
    latitude /= precision;

    //longitude must be a number -180 to 180
    double longitude = rand() % (180*precision);
    if(rand() % 2) {
        longitude *= -1;
    }
    longitude /= precision;

    event_info.set_location_longitude(longitude);
    event_info.set_location_latitude(latitude);

    event_info.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    event_info.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

    const int gender_val = (int)(rand() % 3);
    if(gender_val == 0) {
        event_info.add_allowed_genders(general_values::MALE_GENDER_VALUE);
    } else if(gender_val == 1) {
        event_info.add_allowed_genders(general_values::FEMALE_GENDER_VALUE);
    } else {
        event_info.add_allowed_genders(general_values::MALE_GENDER_VALUE);
        event_info.add_allowed_genders(general_values::FEMALE_GENDER_VALUE);
    }

    google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage> grpcCategoriesArray;
    google::protobuf::RepeatedPtrField<ServerActivityOrCategoryMessage> grpcActivitiesArray;

    //request categories and activities from database
    requestAllServerCategoriesActivitiesHelper(
            &grpcCategoriesArray,
            &grpcActivitiesArray,
            accounts_db);

    std::vector<ActivityStruct> activities = generateRandomActivities(
            event_info.min_allowed_age(),
            grpcCategoriesArray,
            grpcActivitiesArray
    );

    event_info.mutable_activity()->set_activity_index(activities[0].activityIndex);
    auto time_frame = event_info.mutable_activity()->add_time_frame_array();

    time_frame->set_start_time_frame(current_timestamp.count() + 60L * 60L * 1000L + (rand() % 10000000));
    time_frame->set_stop_time_frame(time_frame->start_time_frame() + 1000 + (rand() % 10000000));

    mongocxx::collection collection = mongo_cpp_client["Pictures_For_Testing"]["Pics"];

    srand(getCurrentTimestamp().count());

    int number_pictures = rand() % general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT + 1;

    std::vector<std::tuple<const int, const std::string, const std::string>> picture_and_index_vector;

    //222 pictures starting at index 0
    const int number_testing_pictures_in_file = 222;

    for (int j = 0; j < number_pictures; j++) {
        int pictureIndex = rand() % number_testing_pictures_in_file;

        try {
            auto pictureDocVal = collection.find_one(
                    document{}
                            << "_id" << pictureIndex
                            << finalize
            );

            picture_and_index_vector.emplace_back(
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

    for(const auto&[index, picture, thumbnail] : picture_and_index_vector) {
        auto pic_msg = event_info.add_pictures();

        pic_msg->set_file_size((int)picture.size());
        pic_msg->set_file_in_bytes(picture);
        pic_msg->set_thumbnail_size((int)thumbnail.size());
        pic_msg->set_thumbnail_in_bytes(thumbnail);
    }

    if(chat_room_admin_info.event_type != UserAccountType::USER_ACCOUNT_TYPE
       && rand() % 2 == 0) {
        std::ifstream qr_code_fin(general_values::PATH_TO_SRC + "/resources/qrcode_upload.wikimedia.org.png");

        std::string qr_code_in_bytes;
        int qr_code_size_in_bytes = 0;

        for(char c; qr_code_fin.get(c);) {
            qr_code_in_bytes += c;
            qr_code_size_in_bytes++;
        }

        qr_code_fin.close();

        event_info.set_qr_code_file_in_bytes(qr_code_in_bytes);
        event_info.set_qr_code_file_size(qr_code_size_in_bytes);

        //can be empty
        event_info.set_qr_code_message(gen_random_alpha_numeric_string(rand() % server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_QR_CODE_MESSAGE));
    }

    return event_info;
}