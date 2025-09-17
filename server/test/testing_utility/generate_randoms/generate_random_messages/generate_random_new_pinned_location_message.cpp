//
// Created by jeremiah on 3/17/23.
//
//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"

grpc_chat_commands::SetPinnedLocationRequest generateRandomNewPinnedLocationMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& message_uuid,
        const double longitude,
        const double latitude
) {

    std::string actual_message_uuid;

    if(isInvalidUUID(message_uuid)) {
        actual_message_uuid = generateUUID();
    } else {
        actual_message_uuid = message_uuid;
    }

    grpc_chat_commands::SetPinnedLocationRequest set_pinned_location_request;
    grpc_chat_commands::SetPinnedLocationResponse set_pinned_location_response;

    setupUserLoginInfo(
            set_pinned_location_request.mutable_login_info(),
            account_oid,
            logged_in_token,
            installation_id
            );

    // the precision of doubles).
    int precision = 10e4;

    double generated_longitude;
    if(longitude == chat_room_values::PINNED_LOCATION_DEFAULT_LONGITUDE) {
        //longitude must be a number -180 to 180
        generated_longitude = rand() % (180*precision);
        if(rand() % 2) {
            generated_longitude *= -1;
        }
        generated_longitude /= precision;
    } else {
        generated_longitude = longitude;
    }

    double generated_latitude;
    if(latitude == chat_room_values::PINNED_LOCATION_DEFAULT_LATITUDE) {
        //latitude must be a number -90 to 90
        generated_latitude = rand() % (90*precision);
        if(rand() % 2) {
            generated_latitude *= -1;
        }
        generated_latitude /= precision;
    } else {
        generated_latitude = latitude;
    }

    set_pinned_location_request.set_chat_room_id(chat_room_id);

    set_pinned_location_request.set_new_pinned_longitude(generated_longitude);
    set_pinned_location_request.set_new_pinned_latitude(generated_latitude);

    set_pinned_location_request.set_message_uuid(actual_message_uuid);

    setPinnedLocation(&set_pinned_location_request, &set_pinned_location_response);

    return set_pinned_location_request;
}