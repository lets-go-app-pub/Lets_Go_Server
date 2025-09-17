//
// Created by jeremiah on 8/27/22.
//

#pragma once

#include <string>
#include "chat_rooms_objects.h"
#include "UpdateOtherUserMessages.pb.h"
#include "AccountState.pb.h"

//this will force all info to be sent back
static inline OtherUserInfoForUpdates buildDummyClientInfo() {
    OtherUserInfoForUpdates dummy_client_info;
    dummy_client_info.set_account_state(AccountStateInChatRoom(-1));
    return dummy_client_info;
}

void build_updateSingleChatRoomMemberNotInChatRoom_comparison(
        long current_timestamp_used,
        const std::string& user_account_oid,
        const std::string& chat_room_id,
        const ChatRoomHeaderDoc& header_doc,
        const UpdateOtherUserResponse& update_response_msg,
        const OtherUserInfoForUpdates& client_info = buildDummyClientInfo()
);

void build_updateSingleOtherUserOrEvent_comparison(
        long current_timestamp_used,
        const std::string& user_account_oid,
        const std::string& chat_room_id,
        const ChatRoomHeaderDoc& header_doc,
        const UpdateOtherUserResponse& update_response_msg,
        const OtherUserInfoForUpdates& other_user,
        HowToHandleMemberPictures handle_member_pictures = HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO
);

void build_buildBasicUpdateOtherUserResponse_comparison(
        long current_timestamp_used,
        AccountStateInChatRoom user_account_state,
        const std::string& user_account_oid,
        long user_last_activity_time,
        const UpdateOtherUserResponse& update_response_msg
);