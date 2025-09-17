//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>
#include <chrono>
#include "get_environment_variable.h"
#include "bsoncxx/types.hpp"

namespace general_values {

    inline const std::string APP_NAME = "LetsGo";

    inline const unsigned int MINIMUM_ACCEPTED_ANDROID_VERSION = 3; //minimum accepted version for login to be successful NOTE: NEVER MAKE THIS 0 FOR gRPC DEFAULT REASONS
    inline const unsigned int MINIMUM_ACCEPTED_DESKTOP_INTERFACE_VERSION = 1; //minimum accepted version for login to be successful NOTE: NEVER MAKE THIS 0 FOR gRPC DEFAULT REASONS

    inline const std::chrono::milliseconds TIME_BETWEEN_ACCOUNT_VERIFICATION = std::chrono::milliseconds{183L*24L*60L*60L*1000L}; //this is the time between account verifications required; this is half a year (each SMS costs $$$, a longer time is les $$$)

#ifdef LG_TESTING
    inline std::chrono::milliseconds TIME_BETWEEN_TOKEN_VERIFICATION = std::chrono::milliseconds{2*60L*1000L};//std::chrono::milliseconds{60L*30L*1000L}; //this is the time until the logged in account expires; NOTE: if changing this value, must drop the index from loggedInCollection to re-apply it
#else
    //TODO: set to a more reasonable time (like 30 minutes, may want to take into account TIME_CHAT_STREAM_STAYS_ACTIVE and NUMBER_TIMES_CHAT_STREAM_REFRESHES_BEFORE_RESTART)
    inline const std::chrono::milliseconds TIME_BETWEEN_TOKEN_VERIFICATION = std::chrono::milliseconds{30L*60L*1000L}; //this is the time until the logged in account expires; NOTE: if changing this value, must drop the index from loggedInCollection to re-apply it
#endif
    inline const std::chrono::milliseconds TIME_UNTIL_VERIFICATION_CODE_EXPIRES = std::chrono::milliseconds{5L * 60L * 1000L}; //this is the time until the SMS verification code sent expires; Should be short for security purposes; Must be longer than TIME_UNTIL_VERIFICATION_CODE_EXPIRES NOTE: if changing this value, must drop the index from pendingCollection to re-apply it
    inline const std::chrono::milliseconds TIME_UNTIL_PENDING_ACCOUNT_REMOVED = std::chrono::milliseconds{24L * 60L * 60L * 1000L}; //this is the time until the pending account is removed by mongoDB, Must be longer than TIME_UNTIL_VERIFICATION_CODE_EXPIRES NOTE: there are other functions that can remove or update the pending account
    [[maybe_unused]] inline const std::chrono::milliseconds TIME_UNTIL_EMAIL_VERIFICATION_EXPIRES = std::chrono::milliseconds{2L*60L*60L*1000L}; //this is the time until the page to verify the email expires
    inline const std::chrono::seconds TIME_UNTIL_EMAIL_VERIFICATION_ACCOUNT_REMOVED = std::chrono::seconds{60L*60L*24L*3L}; //this is the time until the mongoDB object holding the email verification info is removed
    [[maybe_unused]] inline const std::chrono::milliseconds TIME_UNTIL_ACCOUNT_RECOVERY_EXPIRES = std::chrono::milliseconds{60L*60L*2L*1000L}; //this is the time until the page to attempt account recovery expires
    inline const std::chrono::seconds TIME_UNTIL_ACCOUNT_RECOVERY_REMOVED = std::chrono::seconds{60L * 60L * 24L * 3L}; //this is the time until the mongoDB object holding the account recovery info is removed

    inline const std::chrono::milliseconds TIME_BETWEEN_SERVER_SHUT_DOWN_STATES = std::chrono::milliseconds{1000L * 30L}; //this is the time the server sleeps between its shutdown states before forcing things to end
    inline const std::chrono::milliseconds TWENTY_TWENTY_ONE_START_TIMESTAMP = std::chrono::milliseconds{51L * 365L * 24L * 60L * 60L * 1000L}; //this should be 51 years after 1970 so the start of 2021

    inline const std::chrono::milliseconds TIME_AVAILABLE_TO_SELECT_TIME_FRAMES = std::chrono::milliseconds{ 60L * 60L * 24L * 7L * 3L * 1000L }; //this is the time that the user can select forward time frames (in milliseconds)

    //gender values
    inline const std::string MALE_GENDER_VALUE = "Male";
    inline const std::string FEMALE_GENDER_VALUE = "Female";
    inline const std::string MATCH_EVERYONE_GENDER_RANGE_VALUE = "Everyone";
    inline const std::string EVENT_GENDER_VALUE = "3vnT~{"; //This is the value an events gender will be set to.

    //Value the event age is set to by default, used for matching purposes.
    inline const int EVENT_AGE_VALUE = 98000; //Want an age that is impossible otherwise. NOTE: -1 is the default age value during account creation.

    //account IDs used
    inline const std::string FACEBOOK_ACCOUNT_ID_PREFIX = "fb_";
    inline const std::string GOOGLE_ACCOUNT_ID_PREFIX = "go_";

    inline const unsigned int EMAIL_VERIFICATION_AND_ACCOUNT_RECOVERY_VERIFICATION_CODE_LENGTH = 30; //number of alphanumeric chars in verification code for email verification and account recovery; NOTE: must also be updated on web server
    inline const std::string WEB_SERVER_HOME_PAGE = "https://letsgoapp.site/";

    inline const std::string EMAIL_AUTHORIZATION_REGEX = R"(^[^@\s]+@[^@\s\.]+\.[^@\.\s]+$)"; //used to validate emails, accepts in the form x@y.z can not have whitespace
#ifdef LG_TESTING
    inline const std::string ERROR_LOG_OUTPUT = get_environment_variable("ERROR_LOG_OUTPUT_DIRECTORY_LETS_GO") + "ErrorLog.txt"; //if a mongoDB error (or any error really) fails and throws an exception the error will be written to this file instead
#else
    inline const std::string ERROR_LOG_OUTPUT = get_environment_variable("ERROR_LOG_OUTPUT_DIRECTORY_LETS_GO") + "ErrorLog.txt"; //if a mongoDB error (or any error really) fails and throws an exception the error will be written to this file instead
#endif
    inline const std::string GET_SINGLE_MESSAGE_PASSED_STRING_TO_CONVERT = "get_single_message"; //passed to convertChatMessageDocumentToChatMessageToClient from getSingleMessage to notify that it is not a specific user doing the conversion

    inline const int DEFAULT_AGE_RANGE_PLUS_MINUS_ADULTS = 7; //this is the default age range for adults it will be +/- their age (it will not go under 18 or over max age) NOTE: leave this larger than 2
    inline const int DEFAULT_AGE_RANGE_PLUS_MINUS_CHILDREN = 2; //this is the default age range for children it will be +/- their age (it will not go under 13 there are instances of it going to 20 if they are 18) NOTE: leave this smaller than 4

    inline const int VERIFICATION_CODE_NUMBER_OF_DIGITS = 6; //the number of digits in the login SMS verification code (also in android)
    inline const int NUMBER_PICTURES_STORED_PER_ACCOUNT = 3; //this is the number of pictures each account can store; NOTE: this is made to increase, however never decrease it

    inline const int MAX_NUMBER_OF_INSTALLATION_IDS_STORED = 10; //this is the maximum number of Installation IDs that can be stored in a verified account

    inline const std::chrono::milliseconds TIME_BETWEEN_SENDING_SMS = std::chrono::milliseconds{30L*1000L}; //this is the time between sending SMS to the same phone number
    inline const std::chrono::milliseconds TIME_BETWEEN_SENDING_EMAILS = std::chrono::milliseconds{30L*1000L}; //this is the time between sending emails to the same phone number

    inline const int SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP = 8; //This will be the number of times different functions (notable CoroutineSpinLock) 'spin' before sleeping or suspending. The optimal value is based on the processor and OS.

    //Stored inside /etc/environment
    inline const std::string PYTHON_PATH_STRING = get_environment_variable("PYTHON_FILES_DIRECTORY_LETS_GO");
    inline const std::string MONGODB_URI_STRING = get_environment_variable("MONGODB_REPLICA_SET_URI_LETS_GO");
    inline const std::string SSL_FILE_DIRECTORY = get_environment_variable("GRPC_SSL_KEY_DIRECTORY_LETS_GO");
    inline const std::string PATH_TO_SRC = get_environment_variable("LETS_GO_PATH_TO_SRC");

    //See sms_verification _documentation.md for more details.
    inline const std::chrono::milliseconds TIME_BETWEEN_VERIFICATION_ATTEMPTS = std::chrono::milliseconds{60L * 60L * 1000L}; //each account will get MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS failed sms verification attempts (in milliseconds), then they will be reset; NOTE: DO NOT SET TO 0
    inline const int MAXIMUM_NUMBER_FAILED_VERIFICATION_ATTEMPTS = 20; //number of failed sms verification attempts 'allowed' during TIME_BETWEEN_VERIFICATION_ATTEMPTS

    //See login_function _documentation.md for more details.
    inline const std::chrono::milliseconds TIME_BETWEEN_SMS_VERIFICATION_MESSAGES_BY_INSTALLATION_ID = std::chrono::milliseconds{60L * 60L * 1000L}; //each account will get MAXIMUM_NUMBER_SMS_MESSAGES_SENT_BY_INSTALLATION_ID failed sms verification attempts (in milliseconds), then they will be reset; NOTE: DO NOT SET TO 0
    inline const int MAXIMUM_NUMBER_SMS_MESSAGES_SENT_BY_INSTALLATION_ID = 20; //number of sms verification messages 'allowed' during TIME_BETWEEN_SMS_VERIFICATION_MESSAGES_BY_INSTALLATION_ID

    inline const long long NUMBER_BIGGER_THAN_UNIX_TIMESTAMP_MS = 95617602000000; //simply need a number larger than the unix timestamp will ever be for the algorithm, this is the year 5000

    inline const long EVENT_DEFAULT_LAST_TIME_FIND_MATCHES_RAN = -1; //The default value of events for the LAST_TIME_FIND_MATCHES_RAN field.

    //These will be stored in user_account_values::EVENT_EXPIRATION_TIME to pass information.
    namespace event_expiration_time_values {
        //USER_ACCOUNT is always greater than the current timestamp. This is done on purpose so the algorithm can
        // match with the user accounts using $gt.
        const bsoncxx::types::b_date USER_ACCOUNT{std::chrono::milliseconds{NUMBER_BIGGER_THAN_UNIX_TIMESTAMP_MS}}; //Largest 64 bit integer value

        //EVENT_CANCELED is always less than the current timestamp. This is done on purpose so the algorithm will never
        // match with these accounts.
        const bsoncxx::types::b_date EVENT_CANCELED{std::chrono::milliseconds{-2}};
    }

    //This is the time after a match expires that it is removed from the device and from USER_CREATED_EVENTS. This is
    // only relevant to user created matches from USER_CREATED_EVENTS. Not to events stored as matches or inside chat
    // rooms.
    inline const std::chrono::milliseconds TIME_AFTER_EXPIRED_MATCH_REMOVED_FROM_DEVICE{0};

}