//
// Created by jeremiah on 8/25/22.
//

#include <optional>

#include "chat_room_commands_helper_functions.h"
#include "general_values.h"
#include "utility_testing_functions.h"
#include "server_parameter_restrictions.h"
#include "store_mongoDB_error_and_exception.h"

bool filterAndStoreSingleUserToBeUpdated(
        const OtherUserInfoForUpdates& member,
        const AccountStateInChatRoom account_state_in_chat_room,
        const std::chrono::milliseconds& current_timestamp,
        OtherUserInfoForUpdates& error_checked_member,
        bool insert_result
) {

    if (
            isInvalidOIDString(member.account_oid())
            || !AccountStateInChatRoom_IsValid(account_state_in_chat_room)
            || !insert_result
            ) {

        const std::string errorString = "Invalid chat room info was passed to filterAndStoreSingleUserToBeUpdated()\n";
        storeMongoDBErrorAndException(
                __LINE__, __FILE__, std::optional<std::string>(),
                errorString,
                "ObjectID_used", member.account_oid(),
                "account_state", convertAccountStateInChatRoomToString(account_state_in_chat_room),
                "insert_result", insert_result ? "true" : "false"
        );

        return false;
    }

    error_checked_member.set_account_oid(member.account_oid());
    error_checked_member.set_account_state(account_state_in_chat_room);

    if(member.thumbnail_index_number() < 0) {
        error_checked_member.set_thumbnail_index_number(0);
    } else if(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT <= member.thumbnail_index_number()) {
        error_checked_member.set_thumbnail_index_number(general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT - 1);
    } else {
        error_checked_member.set_thumbnail_index_number(member.thumbnail_index_number());
    }

    if(member.thumbnail_size_in_bytes() < 0) {
        error_checked_member.set_thumbnail_size_in_bytes(0);
    } else {
        error_checked_member.set_thumbnail_size_in_bytes(member.thumbnail_size_in_bytes());
    }

    if (member.thumbnail_timestamp() > current_timestamp.count()) {
        error_checked_member.set_thumbnail_timestamp(current_timestamp.count());
    } else if (member.thumbnail_timestamp() < general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count()) {
        error_checked_member.set_thumbnail_timestamp(-1L);
    } else {
        error_checked_member.set_thumbnail_timestamp(member.thumbnail_timestamp());
    }

    if (member.first_name().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME) {
        error_checked_member.set_first_name("");
    } else {
        error_checked_member.set_first_name(member.first_name());
    }

    if (member.age() > server_parameter_restrictions::HIGHEST_ALLOWED_AGE) {
        error_checked_member.set_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);
    } else if (member.age() < server_parameter_restrictions::LOWEST_ALLOWED_AGE) {
        error_checked_member.set_age(server_parameter_restrictions::LOWEST_ALLOWED_AGE);
    } else {
        error_checked_member.set_age(member.age());
    }

    if (member.member_info_last_updated_timestamp() > current_timestamp.count()) {
        error_checked_member.set_member_info_last_updated_timestamp(current_timestamp.count());
    } else if (member.member_info_last_updated_timestamp() <
               general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count()) {
        error_checked_member.set_member_info_last_updated_timestamp(-1L);
    } else {
        error_checked_member.set_member_info_last_updated_timestamp(
                member.member_info_last_updated_timestamp());
    }

    std::set<int> index_numbers;
    for (const auto& picture: member.pictures_last_updated_timestamps()) {
        auto result = index_numbers.insert(picture.index_number());

        if(!result.second || picture.index_number() < 0 || general_values::NUMBER_PICTURES_STORED_PER_ACCOUNT <= picture.index_number() ) {
            continue;
        }

        int index_number = picture.index_number();
        long last_updated_timestamp;

        if (picture.last_updated_timestamp() > current_timestamp.count()) {
            last_updated_timestamp = current_timestamp.count();
        } else if (picture.last_updated_timestamp() <
                   general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count()) {
            last_updated_timestamp = general_values::TWENTY_TWENTY_ONE_START_TIMESTAMP.count();
        } else {
            last_updated_timestamp = picture.last_updated_timestamp();
        }

        auto added_picture_msg = error_checked_member.add_pictures_last_updated_timestamps();
        added_picture_msg->set_index_number(index_number);
        added_picture_msg->set_last_updated_timestamp(last_updated_timestamp);
    }

    if(LetsGoEventStatus_IsValid(member.event_status())) {
        error_checked_member.set_event_status(member.event_status());
    } else {
        error_checked_member.set_event_status(LetsGoEventStatus::UNKNOWN_EVENT_STATUS);
    }

    if(member.event_title().size() > server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE) {
        error_checked_member.set_event_title("");
    } else {
        error_checked_member.set_event_title(member.event_title());
    }

    return true;
}

std::vector<OtherUserInfoForUpdates> filterAndStoreListOfUsersToBeUpdated(
        const google::protobuf::RepeatedPtrField<OtherUserInfoForUpdates>& chat_room_member_info,
        const std::chrono::milliseconds& currentTimestamp
) {

    std::set<std::string> account_oids_stored;
    std::vector<OtherUserInfoForUpdates> client_member_info;
    //error checking chat_room_member_info
    for (const auto& member: chat_room_member_info) {

        auto insert_result = account_oids_stored.insert(member.account_oid());

        OtherUserInfoForUpdates error_checked_member;

        if(!filterAndStoreSingleUserToBeUpdated(
            member,
            member.account_state(),
            currentTimestamp,
            error_checked_member,
            insert_result.second
        )) {
            continue;
        }

        client_member_info.emplace_back(std::move(error_checked_member));

    }

    return client_member_info;
}