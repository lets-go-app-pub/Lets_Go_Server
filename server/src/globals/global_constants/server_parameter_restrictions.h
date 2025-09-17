//
// Created by jeremiah on 11/18/21.
//

#pragma once

namespace server_parameter_restrictions {
    inline const int NUMBER_ACTIVITIES_STORED_PER_ACCOUNT = 5; //this is the max number of activities each account can store
    inline const int NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY = 5; //this is the max number of time frames each activity can store
    inline const int NUMBER_GENDER_USER_CAN_MATCH_WITH = 4; //this is the number of genders the user is allowed to match with, it is essentially a list of strings

    inline const int MAXIMUM_NUMBER_BYTES_TEXT_MESSAGE_REPLY = 255; //this is the maximum number of allowed bytes (utf-8 encoded) for a text message reply, any more than this, and it will be trimmed
    inline const int MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE = 255; //this is the maximum number of allowed bytes (utf-8 encoded) for a text message with only ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE, any more than this, and it will be trimmed
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES = 1023; //this is the maximum number of allowed bytes (utf-8 encoded) for simple strings such as name and gender
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_FIRST_NAME = 24; //this is the maximum number of allowed bytes in an utf-8 encoded string for the users first name
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_EVENT_TITLE = 48; //this is the maximum number of allowed bytes in an utf-8 encoded string for the users first name
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_USER_BIO = 400; //this is the maximum number of allowed characters (in utf-8 assuming ascii) for the users bio
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_USER_QR_CODE_MESSAGE = 128; //this is the maximum number of allowed characters (in utf-8 assuming ascii) for the users bio
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_USER_FEEDBACK = 400; //this is the maximum number of allowed characters (in utf-8 assuming ascii) for the users bio
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_USER_MESSAGE = 10000; //this is the maximum number of allowed characters (in utf-8 assuming ascii) for a chat text message
    inline const int MINIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE = 10; //this is the minimum number of allowed characters (in utf-8 assuming ascii) for the inactive message
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_INACTIVE_MESSAGE = 50; //this is the maximum number of allowed characters (in utf-8 assuming ascii) for an inactive message to the user
    inline const int MINIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON = 10; //this is the minimum number of allowed characters (in utf-8 assuming ascii) for the inactive message
    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_MODIFY_ERROR_MESSAGE_REASON = 100; //this is the maximum number of allowed characters (in utf-8 assuming ascii) for an inactive message to the user

    inline const int MAXIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE = 25000; //this is the maximum number of allowed characters for the error message
    inline const int MINIMUM_NUMBER_ALLOWED_BYTES_ERROR_MESSAGE = 2; //this is the maximum number of allowed characters for the error message
    inline const int HIGHEST_ALLOWED_AGE = 120; //this is the maximum allowed age the user can be; NOTE: this will need extra work, can't just change variable
    inline const int LOWEST_ALLOWED_AGE = 13; //this is the minimum value age of the user can be; NOTE: this will need extra work, can't just change variable ex. find_needed_verification_info.cpp
    inline const int DEFAULT_USER_AGE_IF_ERROR = 35; //used with logout function and testing; NOTE: Setting an age that should be fairly generic. Don't want an edge case and don't want to set the user too young.

    inline const int DEFAULT_MAX_DISTANCE = 75; //the default max distance the user will match with in miles
    inline const int MINIMUM_ALLOWED_DISTANCE = 1; //this is the minimum distance a user can match using
    inline const int MAXIMUM_ALLOWED_DISTANCE = 100; //this is the maximum distance a user can match using

    inline const int MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE = 1; //minimum number of chars in activity or category name
    inline const int MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE = 26; //maximum number of chars in activity or category name
    //NOTE: these are not sent back to android so that if a new version of android comes out the icons will not change size with it and
    // the older ones will still work (backwards compatability issues)
    //NOTE: android will scale an image DOWN (poorly) to make it fit but not UP, sort of anyway, if it can be converted to a VectorDrawable then it
    // will scale in any way (an .svg file can be converted in Android Studio)
    inline const unsigned int ACTIVITY_ICON_WIDTH_IN_PIXELS = 256; //width of the icons used for displaying on android in pixels
    inline const unsigned int ACTIVITY_ICON_HEIGHT_IN_PIXELS = ACTIVITY_ICON_WIDTH_IN_PIXELS; //height of the icons used for displaying on android in pixels

    inline const int MAXIMUM_CHAT_MESSAGE_PICTURE_SIZE_IN_BYTES = 3 * 1024 * 1024; //the maximum size in bytes a picture can be
    inline const int MAXIMUM_CHAT_MESSAGE_THUMBNAIL_SIZE_IN_BYTES = 1 * 1024 * 1024; //the maximum size in bytes a picture can be

    inline const int MAXIMUM_PICTURE_SIZE_IN_BYTES = 3 * 1024 * 1024; //the maximum size in bytes a picture can be
    inline const int MAXIMUM_PICTURE_THUMBNAIL_SIZE_IN_BYTES = 512 * 1024; //the maximum size in bytes a picture thumbnail can be

    inline const int MAXIMUM_QR_CODE_SIZE_IN_BYTES = 3 * 1024 * 1024; //the maximum size in bytes a QR Code image for an event can be
}