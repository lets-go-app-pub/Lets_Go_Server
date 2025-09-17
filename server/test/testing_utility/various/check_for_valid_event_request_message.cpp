//
// Created by jeremiah on 3/19/23.
//

#include <gtest/gtest.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>

#include "check_for_valid_event_request_message.h"
#include "utility_general_functions.h"
#include "server_parameter_restrictions.h"
#include "general_values.h"
#include "connection_pool_global_variable.h"
#include "database_names.h"
#include "collection_names.h"
#include "activities_info_keys.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void checkForValidEventRequestMessage(
        const EventRequestMessage& valid_event_request_message,
        const std::function<bool(const EventRequestMessage& event_request)>& run_function
) {

    EventRequestMessage event_request_message;

    auto runCheck = [&](const std::function<void()>& setup_error) {
        event_request_message.CopyFrom(valid_event_request_message);

        setup_error();

        bool successful = run_function(event_request_message);

        EXPECT_FALSE(successful);
    };

    event_request_message.CopyFrom(valid_event_request_message);

    runCheck(
            [&]() {
                event_request_message.set_event_title("");
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_event_title(
                        gen_random_alpha_numeric_string(
                                server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE + 1 +
                                rand() % 100
                        )
                );
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_bio("");
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_bio(
                        gen_random_alpha_numeric_string(
                                server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO + 1 +
                                rand() % 100
                        )
                );
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_city("");
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_city(
                        gen_random_alpha_numeric_string(
                                server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 +
                                rand() % 100
                        )
                );
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_location_longitude(181);
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_location_latitude(91);
            }
    );

    runCheck(
            [&]() {
                event_request_message.clear_allowed_genders();
            }
    );

    //only invalid genders inside gender_range
    runCheck(
            [&]() {
                event_request_message.clear_allowed_genders();
                event_request_message.add_allowed_genders(general_values::EVENT_GENDER_VALUE);
                event_request_message.add_allowed_genders("");
                event_request_message.add_allowed_genders(
                        gen_random_alpha_numeric_string(
                                server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1 +
                                rand() % 100
                        )
                        );
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE - 1);
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_min_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1);
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_max_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE - 1);
            }
    );

    runCheck(
            [&]() {
                event_request_message.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1);
            }
    );

    //min age must be <= max age
    runCheck(
            [&]() {
                event_request_message.set_min_allowed_age(55);
                event_request_message.set_max_allowed_age(54);
            }
    );

    runCheck(
            [&]() {
                event_request_message.clear_activity();
            }
    );

    runCheck(
            [&]() {
                event_request_message.mutable_activity()->set_activity_index(0);
            }
    );

    //event_info->activity().time_frame_array_size() != 1
    runCheck(
            [&]() {
                event_request_message.mutable_activity()->clear_time_frame_array();
            }
    );

    //event_info->activity().time_frame_array_size() != 1
    runCheck(
            [&]() {
                auto time_frame_array = event_request_message.mutable_activity()->add_time_frame_array();
                time_frame_array->set_start_time_frame(event_request_message.activity().time_frame_array(0).stop_time_frame() + 1000 + rand() % 100000);
                time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame() + 1000 + rand() % 100000);
            }
    );

    //start_time <= current_timestamp
    runCheck(
            [&]() {
                 event_request_message.mutable_activity()->mutable_time_frame_array(0)->set_start_time_frame(getCurrentTimestamp().count() - 60 * 60 * 1000);
            }
    );

    //invalid activity index
    runCheck(
            [&]() {
                event_request_message.mutable_activity()->set_activity_index(10000000);
            }
    );

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection activities_info_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

    bsoncxx::stdx::optional<bsoncxx::document::value> optional_activities_doc = activities_info_collection.find_one(
            document{}
                << "_id" << activities_info_keys::ID
            << finalize
    );

    if(!optional_activities_doc) {
        EXPECT_TRUE(false);
        return;
    }

    const bsoncxx::document::view activities_doc = optional_activities_doc->view();

    const bsoncxx::array::view categories_array = activities_doc[activities_info_keys::CATEGORIES].get_array().value;

    //find an activity that requires a higher min age
    int category_index = 0;
    for(const auto& category : categories_array) {
        const bsoncxx::document::view category_doc = category.get_document().value;
        const int min_age = category_doc[activities_info_keys::categories::MIN_AGE].get_int32().value;
        if(min_age > server_parameter_restrictions::LOWEST_ALLOWED_AGE && category_index > 0) {
            break;
        }
        category_index++;
    }

    const bsoncxx::array::view activities_array = activities_doc[activities_info_keys::ACTIVITIES].get_array().value;

    //find an activity that requires a higher min age
    int activity_index = 0;
    int final_activity_index = 0;
    int final_activity_of_category_index = 0;
    for(const auto& activity : activities_array) {
        const bsoncxx::document::view activity_doc = activity.get_document().value;
        const int min_age = activity_doc[activities_info_keys::activities::MIN_AGE].get_int32().value;
        if(min_age > server_parameter_restrictions::LOWEST_ALLOWED_AGE
            && activity_index > 0
            && final_activity_index ==0
        ) {
            final_activity_index = activity_index;

            if(final_activity_of_category_index != 0) {
                break;
            }
        }

        const int extracted_category_index = activity_doc[activities_info_keys::activities::CATEGORY_INDEX].get_int32().value;

        if(extracted_category_index == category_index
            && final_activity_of_category_index == 0) {
            final_activity_of_category_index = activity_index;

            if(final_activity_index != 0) {
                break;
            }
        }

        activity_index++;
    }

    if(final_activity_index == 0
        || final_activity_of_category_index == 0) {
        std::cout << "final_activity_index: " << final_activity_index << '\n';
        std::cout << "final_activity_of_category_index: " << final_activity_of_category_index << '\n';
        EXPECT_TRUE(false);
        return;
    }

    //activity min age too high for min_allowed_age
    runCheck(
            [&]() {
                event_request_message.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
                event_request_message.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);
                event_request_message.mutable_activity()->set_activity_index(final_activity_index);
            }
    );

    //category min age too high for min_allowed_age
    runCheck(
            [&]() {
                event_request_message.set_min_allowed_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
                event_request_message.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);
                event_request_message.mutable_activity()->set_activity_index(final_activity_of_category_index);
            }
    );

    runCheck(
            [&]() {
                event_request_message.clear_pictures();
            }
    );

    runCheck(
            [&]() {
                event_request_message.mutable_pictures(0)->set_thumbnail_size(0);
                event_request_message.mutable_pictures(0)->set_thumbnail_in_bytes("");
            }
    );

    runCheck(
            [&]() {
                std::string too_long_thumbnail_in_bytes = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES + 1 + rand() % 1000);
                event_request_message.mutable_pictures(0)->set_thumbnail_size(too_long_thumbnail_in_bytes.size());
                event_request_message.mutable_pictures(0)->set_thumbnail_in_bytes(std::move(too_long_thumbnail_in_bytes));
            }
    );

    runCheck(
            [&]() {
                event_request_message.mutable_pictures(0)->set_thumbnail_size(
                        event_request_message.pictures(0).thumbnail_size() + 1
                );
            }
    );

    runCheck(
            [&]() {
                event_request_message.mutable_pictures(0)->set_file_size(0);
                event_request_message.mutable_pictures(0)->set_file_in_bytes("");
            }
    );

    runCheck(
            [&]() {
                std::string too_long_file_in_bytes = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES + 1 + rand() % 1000);
                event_request_message.mutable_pictures(0)->set_file_size(too_long_file_in_bytes.size());
                event_request_message.mutable_pictures(0)->set_file_in_bytes(std::move(too_long_file_in_bytes));
            }
    );

    runCheck(
            [&]() {
                event_request_message.mutable_pictures(0)->set_file_size(
                        event_request_message.pictures(0).file_size() + 1
                );
            }
    );

    //too many pictures sent
    runCheck(
            [&]() {
                while(event_request_message.pictures().size() < general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT+1) {
                    auto pic = event_request_message.add_pictures();
                    pic->CopyFrom(event_request_message.pictures(0));
                }
            }
    );

    if(event_request_message.qr_code_file_size() != 0) {
        runCheck(
                [&]() {
                    std::string too_long_qr_code = gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_QR_CODE_SIZE_IN_BYTES + 1 + rand() % 1000);
                    event_request_message.set_qr_code_file_size(too_long_qr_code.size());
                    event_request_message.set_qr_code_file_in_bytes(std::move(too_long_qr_code));
                }
        );

        runCheck(
                [&]() {
                    event_request_message.set_qr_code_file_size(
                            event_request_message.qr_code_file_size() + 1
                    );
                }
        );

        runCheck(
                [&]() {
                    event_request_message.set_qr_code_message(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_QR_CODE_MESSAGE + 1 + rand() % 1000));
                }
        );
    }

}
