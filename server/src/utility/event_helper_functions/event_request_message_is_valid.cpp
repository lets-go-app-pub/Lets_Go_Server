//
// Created by jeremiah on 3/8/23.
//

#include <mongocxx/collection.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include "event_request_message_is_valid.h"
#include "server_parameter_restrictions.h"

#include "utility_general_functions.h"
#include "activities_info_keys.h"
#include "general_values.h"
#include "global_bsoncxx_docs.h"
#include "connection_pool_global_variable.h"
#include "store_mongoDB_error_and_exception.h"

EventRequestMessageIsValidReturnValues eventRequestMessageIsValid(
        EventRequestMessage* event_info,
        mongocxx::collection& activities_info_collection,
        const std::chrono::milliseconds& current_timestamp
) {

    EventRequestMessageIsValidReturnValues return_values{ReturnStatus::SUCCESS, "", true};

    if (event_info->event_title().empty()
        || event_info->event_title().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE) {
        const std::string error_string = "Event title " + event_info->event_title() +
                                         " is an invalid length.\nMust be between 1 and " +
                                         std::to_string(
                                                 server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE) +
                                         " characters.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (event_info->bio().empty() ||
        event_info->bio().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO) {
        const std::string error_string = "Bio " + event_info->event_title() +
                                         " is an invalid length.\nMust be between 1 and " +
                                         std::to_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO) +
                                         " characters.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (event_info->city().empty() ||
        event_info->city().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) {
        const std::string error_string = "City " + event_info->event_title() +
                                         " is an invalid length.\nMust be between 1 and " +
                                         std::to_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES) +
                                         " characters.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (isInvalidLocation(event_info->location_longitude(), event_info->location_latitude())) {
        const std::string error_string = "Invalid location passed, events must have a location.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    extractRepeatedGenderProtoToVector(
            event_info->allowed_genders(),
            return_values.gender_range_vector,
            return_values.user_matches_with_everyone
    );

    if (return_values.gender_range_vector.empty()) {
        const std::string error_string = "At least one valid gender must be passed.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    //More checks are below after the activity and category are extracted.
    if (event_info->min_allowed_age() < server_parameter_restrictions::LOWEST_ALLOWED_AGE
        || event_info->min_allowed_age() > server_parameter_restrictions::HIGHEST_ALLOWED_AGE) {
        const std::string error_string = "Minimum allowed age must be between " +
                                         std::to_string(server_parameter_restrictions::LOWEST_ALLOWED_AGE) +
                                         " and " + std::to_string(server_parameter_restrictions::HIGHEST_ALLOWED_AGE) +
                                         ".";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (event_info->max_allowed_age() < server_parameter_restrictions::LOWEST_ALLOWED_AGE
        || event_info->max_allowed_age() > server_parameter_restrictions::HIGHEST_ALLOWED_AGE) {
        const std::string error_string = "Maximum allowed age must be between " +
                                         std::to_string(server_parameter_restrictions::LOWEST_ALLOWED_AGE) +
                                         " and " + std::to_string(server_parameter_restrictions::HIGHEST_ALLOWED_AGE) +
                                         ".";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (event_info->min_allowed_age() > event_info->max_allowed_age()) {
        const std::string error_string = "Minimum allowed age (" + std::to_string(event_info->min_allowed_age()) +
                                         ") cannot be larger than maximum allowed age (" +
                                         std::to_string(event_info->max_allowed_age()) +
                                         ").";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (!event_info->has_activity()
        && event_info->activity().activity_index() != 0) {
        const std::string error_string = "Request does not contain an activity.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (event_info->activity().time_frame_array_size() != 1) {
        const std::string error_string =
                "Activity must contain exactly one time frame.\n" + event_info->activity().DebugString();
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    return_values.event_start_time = std::chrono::milliseconds{event_info->activity().time_frame_array(0).start_time_frame()};
    return_values.event_stop_time = std::chrono::milliseconds{event_info->activity().time_frame_array(0).stop_time_frame()};

    if (return_values.event_start_time > return_values.event_stop_time) {
        const std::string error_string =
                "Event stop time <" +
                getDateTimeStringFromTimestamp(return_values.event_stop_time)
                + "> must come after event start time <" +
                getDateTimeStringFromTimestamp(return_values.event_start_time)
                + ">.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (return_values.event_start_time <= current_timestamp) {
        const std::string error_string = "Event start time must come after current time.\npassed start time: " +
                                         getDateTimeStringFromTimestamp(return_values.event_start_time);
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (return_values.event_stop_time <= current_timestamp) {
        const std::string error_string = "Event stop time must come after current time.\npassed stop time: " +
                                         getDateTimeStringFromTimestamp(return_values.event_stop_time);
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    bsoncxx::builder::basic::array index_numbers_array_value; //used to search for activities
    index_numbers_array_value.append(event_info->activity().activity_index());

    const bsoncxx::array::view index_numbers_array_view = index_numbers_array_value.view();
    static const std::string PROJECTED_ARRAY_NAME = "pAn";

    mongocxx::stdx::optional<mongocxx::cursor> find_activities_cursor;
    try {
        mongocxx::pipeline find_activities;

        find_activities.match(
                document{}
                        << "_id" << activities_info_keys::ID
                << finalize
        );

        const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                PROJECTED_ARRAY_NAME,
                index_numbers_array_view,
                event_info->min_allowed_age()
        );

        find_activities.project(project_activities_by_index.view());

        find_activities_cursor = activities_info_collection.aggregate(find_activities);
    }
    catch (const mongocxx::logic_error& e) {
        const std::string error_string =
                "Exception occurred when attempting to extract index.\nmessage: " + std::string(e.what());

        const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                PROJECTED_ARRAY_NAME,
                index_numbers_array_view,
                event_info->min_allowed_age()
        );

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "index_numbers_array_view", index_numbers_array_view,
                "user_age", event_info->min_allowed_age(),
                "project_activities_by_index", project_activities_by_index
        );

        return {ReturnStatus::LG_ERROR, error_string, false};
    }

    if (!find_activities_cursor || find_activities_cursor->begin() == find_activities_cursor->end()) {
        const std::string error_string =
                "When finding activities, cursor came back unset or empty. This most likely means the activity index"
                " added was invalid. For example, the minimum age requirement could be too young for that activity.";

        const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                PROJECTED_ARRAY_NAME,
                index_numbers_array_view,
                event_info->min_allowed_age()
        );

        storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), error_string,
                "index_numbers_array_view", index_numbers_array_view,
                "user_age", event_info->min_allowed_age(),
                "project_activities_by_index", project_activities_by_index
        );

        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    bsoncxx::array::view projected_categories_array_value;

    //should only have 1 element (included a matched stage searching by _id)
    for (const auto& projected_category_doc: *find_activities_cursor) {
        auto categories_array_element = projected_category_doc[PROJECTED_ARRAY_NAME];
        if (categories_array_element
            && categories_array_element.type() == bsoncxx::type::k_array) { //if element exists and is type array
            projected_categories_array_value = categories_array_element.get_array().value;
        } else { //if element does not exist or is not type array
            const std::string error_string = "Element did not exist or was not type array when extracting"
                                             " PROJECTED_ARRAY_NAME. This most likely means the min age was too young for the activity.";

            const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                    PROJECTED_ARRAY_NAME,
                    index_numbers_array_view,
                    event_info->min_allowed_age()
            );

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "index_numbers_array_view", index_numbers_array_view,
                    "user_age", event_info->min_allowed_age(),
                    "project_activities_by_index", project_activities_by_index
            );

            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }
    }

    int num_categories = 0;
    for (const auto& category_doc: projected_categories_array_value) {
        auto category_index_element = category_doc[activities_info_keys::activities::CATEGORY_INDEX];
        if (category_index_element
            && category_index_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
            return_values.category_index = category_index_element.get_int32().value;
        } else { //if element does not exist or is not type int32
            const std::string error_string = "Element did not exist or was not type array when extracting CATEGORY_INDEX."
                                             " Most likely means min age is lower than allowed by activity.";

            const bsoncxx::document::value project_activities_by_index = buildProjectActivitiesByIndex(
                    PROJECTED_ARRAY_NAME,
                    index_numbers_array_view,
                    event_info->min_allowed_age()
            );

            storeMongoDBErrorAndException(
                    __LINE__, __FILE__,
                    std::optional<std::string>(), error_string,
                    "index_numbers_array_view", index_numbers_array_view,
                    "user_age", event_info->min_allowed_age(),
                    "project_activities_by_index", project_activities_by_index
            );

            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }
        num_categories++;
    }

    if (num_categories != 1) {
        const std::string error_string =
                "Only one activity and therefore one category should be present. However, found " +
                std::to_string(num_categories) + " categories.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (return_values.category_index < 1) {
        const std::string error_string = "Invalid category index was found of " + std::to_string(return_values.category_index) + ".";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    if (event_info->pictures().empty()) {
        const std::string error_string = "Must pass in at least one picture.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    //Check for invalid or corrupt pictures.
    //picture_oids is expected to be the same size as request->pictures()
    for (const auto& picture_message: event_info->pictures()) {
        const int thumbnail_size = picture_message.thumbnail_size();
        if (thumbnail_size <= 0 ||
            server_parameter_restrictions::MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES < thumbnail_size) {
            const std::string error_string =
                    "Generated thumbnail for index " + std::to_string(return_values.picture_oids.size()) + " was invalid.";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }

        if ((int) picture_message.thumbnail_in_bytes().length() != thumbnail_size) { //thumbnail is corrupt
            const std::string error_string = "Generated thumbnail for index " + std::to_string(return_values.picture_oids.size()) +
                                             " was corrupt. Please try submitting the event again.";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }

        const int file_size = picture_message.file_size();
        if (file_size <= 0 || server_parameter_restrictions::MAXIMUM_PICTURE_SIZE_IN_BYTES < file_size) {
            const std::string error_string =
                    "Picture for index " + std::to_string(return_values.picture_oids.size()) + " was invalid.";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }

        if ((int) picture_message.file_in_bytes().length() != file_size) { //picture is corrupt
            const std::string error_string = "Picture for index " + std::to_string(return_values.picture_oids.size()) +
                                             " was corrupt. Please try submitting the event again.";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }

        return_values.picture_oids.emplace_back();
        return_values.pictures_array.append(
                document{}
                        << user_account_keys::pictures::OID_REFERENCE << return_values.picture_oids.back()
                        << user_account_keys::pictures::TIMESTAMP_STORED << bsoncxx::types::b_date{current_timestamp}
                << finalize
        );
    }

    if (return_values.picture_oids.size() > general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) {
        const std::string error_string =
                "Maximum of "
                + std::to_string(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT) +
                " pictures allowed to be passed in.";
        return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
    }

    //Must finish filling up the PICTURES array with null values.
    const int num_null_pics = general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT - (int) return_values.picture_oids.size();
    for (int i = 0; i < num_null_pics; ++i) {
        return_values.pictures_array.append(bsoncxx::types::b_null{});
    }

    //NOTE: No QR Code is valid.
    const int qr_code_file_size = event_info->qr_code_file_size();
    if (qr_code_file_size != 0) {

        return_values.qr_code_is_set = true;

        if (server_parameter_restrictions::MAXIMUM_QR_CODE_SIZE_IN_BYTES < qr_code_file_size) {
            const std::string error_string = "QR Code image was too large.";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }

        if ((int) event_info->qr_code_file_in_bytes().length() != qr_code_file_size) { //image is corrupt
            const std::string error_string = "QR Code image was corrupt. Please try submitting the event again.";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }

        if (server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_QR_CODE_MESSAGE <
            event_info->qr_code_message().size()) {
            const std::string error_string =
                    "QR Code message was too long. Maximum allowed bytes (characters) is " +
                    std::to_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_USER_QR_CODE_MESSAGE) +
                    ".";
            return {ReturnStatus::INVALID_PARAMETER_PASSED, error_string, false};
        }
    }

    return return_values;
}
