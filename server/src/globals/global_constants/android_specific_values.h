//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include <chrono>
#include "chat_stream_container.h"

namespace android_specific_values {
    inline const std::chrono::milliseconds TIME_BETWEEN_CHAT_MESSAGE_INVITE_EXPIRATION = std::chrono::milliseconds{
            2L * 24L * 60L * 60L * 1000L}; //used on Android client to decide time before chat message invite type expires
    inline const std::chrono::milliseconds INFO_HAS_NOT_BEEN_OBSERVED_BEFORE_CLEANED = std::chrono::milliseconds{
            7L * 24L * 60L * 60L * 1000L}; //amount of time before messages and members will have their 'extra' info trimmed for memory management on Android client

    inline const int HIGHEST_DISPLAYED_AGE = 80; //this is the highest age shown on the Android slider

    inline const std::string APP_URL_STRING_FOR_SHARING = "https://play.google.com/store/apps/details?id=site.letsgoapp.letsgo"; //URL sent when the user 'Shares' the app
    inline const unsigned int NUMBER_TIMES_CHAT_STREAM_REFRESHES_BEFORE_RESTART = 3; //this is the number of times the chat stream will run refreshChatStream() before it will restart the stream
    inline const std::chrono::milliseconds CHAT_ROOM_STREAM_DEADLINE_TIME =
            (1 + NUMBER_TIMES_CHAT_STREAM_REFRESHES_BEFORE_RESTART) *
            chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE; //this is the time before the chat stream will hit its deadline and time out (the plus 1 is because of the initial wait time before a single refresh is done)

    inline const unsigned int ACTIVITY_ICON_BORDER_WIDTH = 8; //this is the number of pixels around an unselected activity icon that the border is thick
    inline const unsigned int ACTIVITY_ICON_PADDING = 40; //this is the number of pixels in the padding around the activity icon; NOTE: this is NOT related to the android 'padding' attribute
    inline const std::string ACTIVITY_ICON_COLOR = "#4890E1"; //this is the color of the unselected activity icon an well as border

    //NOTE: not currently in use by Android (it uses the background color of the layout it is inside)
    inline const std::string ACTIVITY_ICON_BACKGROUND_COLOR = "#E6EBEF"; //this is the background color of the unselected activity icon

    //Picture values
    inline const int IMAGE_QUALITY_VALUE = 50; //this is the quality that the image will retain when compressed (must be a number between 0 and 100)
    inline const int PICTURE_MAXIMUM_CROPPED_SIZE_PX = 2048; //this is the maximum height or width pictures will be cropped to
    inline const int PICTURE_THUMBNAIL_MAXIMUM_CROPPED_SIZE_PX = 256; //this is the maximum height or width in pixels the thumbnail will be saved as

    //TODO: set this to something more reasonable (like ten minutes)
    inline const long TIME_BETWEEN_UPDATING_SINGLE_USER =
            10L * 60L * 1000L; //time between checking individual users for picture updates (mainly used when user joins a chat room and updateChatRoom() is called)
    inline const long TIME_BETWEEN_UPDATING_SINGLE_USER_FUNCTION_RUNNING =
            60L * 1000L; //time between updateSingleChatRoomMemberInfo() inside ApplicationRepository.kt running for a specific user

    //ImageMessageValues
    inline const int CHAT_MESSAGE_IMAGE_THUMBNAIL_WIDTH = 256; //this is the width in pixels of the thumbnail that will be sent when a reply is used for a chat message
    inline const int CHAT_MESSAGE_IMAGE_THUMBNAIL_HEIGHT = 256; //this is the height in pixels of the thumbnail that will be sent when a reply is used for a chat message
}