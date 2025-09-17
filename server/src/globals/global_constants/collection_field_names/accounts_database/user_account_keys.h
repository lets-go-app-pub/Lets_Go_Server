//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (USER_ACCOUNTS_COLLECTION_NAME)
namespace user_account_keys {

    inline const std::string STATUS = "iAs"; //int32; based on enum UserAccountStatus
    inline const std::string INACTIVE_MESSAGE = "sIm"; //string; message to be sent back for some inactive status ex: banned, suspended
    inline const std::string INACTIVE_END_TIME = "iEt"; //mongodb Date or null; if this is a date type this will be the time the user becomes un-suspended
    inline const std::string NUMBER_OF_TIMES_TIMED_OUT = "nTo"; //int32; number of times this account has been timed out (used as a counter to see when a time-out bans)
    inline const std::string INSTALLATION_IDS = "aIi"; //array of strings; all installation ids that have been logged into this account, this array has restricted size set by MAX_NUMBER_OF_INSTALLATION_IDS_STORED; NOTE: this array will be cleared in SMSVerification before the account is deleted, so never set this to empty
    inline const std::string LAST_VERIFIED_TIME = "dVt"; //mongoDB Date
    inline const std::string TIME_CREATED = "dCt"; //mongoDB Date

    inline const std::string ACCOUNT_ID_LIST = "sAI"; //array of strings; these are the accountIDs prefixed by FACEBOOK_ACCOUNT_ID_PREFIX or GOOGLE_ACCOUNT_ID_PREFIX

    inline const std::string SUBSCRIPTION_STATUS = "sUs"; //int32; An enum representing the current users' subscription tier. Follows UserSubscriptionStatus enum from UserSubscriptionStatus.proto.
    inline const std::string SUBSCRIPTION_EXPIRATION_TIME = "sEt"; //mongoDB Date; If the user is subscribed (SUBSCRIPTION_STATUS > NO_SUBSCRIPTION), this will be the time the subscription expires.

    inline const std::string ACCOUNT_TYPE = "aCt"; //int32; An enum representing the type of account this is (user or event types). Follows UserAccountType enum from UserAccountType.proto.

    inline const std::string PHONE_NUMBER = "sPn"; //string;
    inline const std::string FIRST_NAME = "sFn"; //string;
    inline const std::string BIO = "sBi"; //string;
    inline const std::string CITY = "sCn"; //string;
    inline const std::string GENDER = "sGe"; //string; "~"=unset, MALE_GENDER_VALUE=male, FEMALE_GENDER_VALUE=female, can also be "other"
    inline const std::string BIRTH_YEAR = "iBy"; //int32; -1=unset year format ex: 2012
    inline const std::string BIRTH_MONTH = "iBm"; //int32; -1=unset months are stored as values 1-12
    inline const std::string BIRTH_DAY_OF_MONTH = "iBd"; //int32; -1=unset day of month 1-xx
    inline const std::string BIRTH_DAY_OF_YEAR = "iBo"; //int32; this is the day of the year, taken from tm_yday, so it goes from [0-365]
    inline const std::string AGE = "iAg"; //int32; -1=unset day of month 1-xx
    inline const std::string EMAIL_ADDRESS = "sEa"; //string; the email address

    inline const std::string EMAIL_ADDRESS_REQUIRES_VERIFICATION = "bEv"; //bool //NOTE: this is accessed in web server

    inline const std::string OPTED_IN_TO_PROMOTIONAL_EMAIL = "pRo"; //bool; True if the user has opted in to receive promotional emails, false if not.

    inline const std::string PICTURES = "aPI"; //array of Documents; Object Id and timestamp for picture; NOTE: these could be set to null if there is no picture stored, and it may not be as large as NUMBER_PICTURES_STORED_PER_ACCOUNT, however it should never be larger

    //namespace contains keys for PICTURES documents
    namespace pictures {
        inline const std::string OID_REFERENCE = "dId"; //OID; the oid reference of the picture
        inline const std::string TIMESTAMP_STORED = "dTs"; //mongoDB Date; the timestamp the picture was stored
    }

    inline const std::string SEARCH_BY_OPTIONS = "sBo"; //int32, follows AlgorithmSearchOptions enum from SetFields.proto

    //NOTE: Empty time frame means 'anytime' these key names are shared with matching accounts.
    inline const std::string CATEGORIES = "aCa"; //array of documents containing: activity (type int32), timeframes(document of time (int64) and startStop (1 for start time -1 for stop time))

    //Namespace contains keys for CATEGORIES documents.
    namespace categories {

        inline const std::string TYPE = "tYp"; //int32; enum representing what type of document this is, follows AccountCategoryType
        inline const std::string INDEX_VALUE = "iAc"; //int32; integer value of activity or category
        inline const std::string TIMEFRAMES = "aTf"; //array of documents containing: time (mongoDBDate), startStopValue; (1 is start time -1 if stop time)

        //namespace contains keys for TIMEFRAMES documents
        namespace timeframes {
            inline const std::string TIME = "dTf"; //int64; this is a timestamp in millis but NOT A mongoDB Date for algorithm efficiency; specific start or stop timestamp in timeframe
            inline const std::string START_STOP_VALUE = "iSs"; //int32; represents if the time is a start or stop time (1 is start time -1 if stop time)
        }
    }

    inline const std::string EVENT_VALUES = "eVv"; //document or does not exist; This will contain the values specific to an event. This document will only exist when ACCOUNT_TYPE is an event type.

    //Namespace contains keys for EVENT_VALUES document.
    namespace event_values {
        inline const std::string CREATED_BY = "cB"; //string; The account oid (in string form) that created the event if created by a user OR the admin NAME if created by an admin.
        inline const std::string CHAT_ROOM_ID = "cId"; //string; The chat room id for the event.
        inline const std::string EVENT_TITLE = "tI"; //string; The title of the event. This is useful to allow spaces in it which first name does not.
    }

    inline const std::string EVENT_EXPIRATION_TIME = "eEt"; //mongodb date; The time that this match can no longer be searched for. This field will exist regardless for the collection indexes.

    inline const std::string USER_CREATED_EVENTS = "uCe"; //array of documents; These are the events that this user has personally created.

    //Namespace contains keys for USER_CREATED_EVENTS document.
    namespace user_created_events {
        inline const std::string EVENT_OID = "oId"; //oid; The oid of the user created event.
        inline const std::string EXPIRATION_TIME = "eXt"; //mongodb date; The expiration time of the created event. Will follow EVENT_EXPIRATION_TIME of the event document (will never change to canceled).
        inline const std::string EVENT_STATE = "eVc"; //int32; roughly follows LetsGoEventStatus enum, it is only ever set to ONGOING or CANCELED (used to determine if the event was canceled)
    }

    //NOTE: If everything works correctly there should never be a duplicate
    // user still inside of OTHER_ACCOUNTS_MATCHED_WITH because un_match and client_message_to_server will remove
    // it inside a transaction, and join_chat_room is not allowed unless the MATCHING_OID_STRINGS is set
    // to null (an un_match or client_message_to_server occurred)
    //There is a check inside buildUpdateAccountsToMatchMade() when adding the element.
    //It is also checked on logout_function just in case.
    inline const std::string OTHER_ACCOUNTS_MATCHED_WITH = "aOa"; //array of documents; the other users this person has matched with and not yet opened a connection to; contains: oid of match (type string), timestamp(mongoDB date)

    //namespace contains keys for OTHER_ACCOUNTS_MATCHED_WITH documents
    namespace other_accounts_matched_with {
        inline const std::string OID_STRING = "sId"; //string; string representing oid of the match
        inline const std::string TIMESTAMP = "dTs"; //mongoDB date; timestamp in mongodb date format that match was made
    }

    inline const std::string MATCHING_ACTIVATED = "bAt"; //bool; true if account is actively taking matches, false if not (this could be mixed with STATUS however leaving it separate so that a bool can be added to turn off matches later)
    inline const std::string LOCATION = "dLo"; //document; follows the 2dSphere mongodb type "Point"

    //Used as a timed lock to disallow multiple runs of the findMatches() function (not the same as the
    // matching algorithm specifically). It will be set to the time that findMatches() can run again. When
    // the algorithm starts it will be set to TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS in the future. Then
    // when the algorithm ends it will be set to -1.
    //An important observation is that this cannot be run inside the transaction itself. That is because all operations
    // will be guaranteed to complete or fail and so it will not be properly 'set' until the transaction itself ends.
    //Also note that the algorithm has its own cool down related fields LAST_TIME_MATCH_ALGORITHM_RAN and
    // LAST_TIME_EMPTY_MATCH_RETURNED which are unrelated to this one.
    //Ideally want to keep this greater than matching_algorithm::TIMEOUT_FOR_ALGORITHM_AGGREGATION so that there will never
    // be 2 algorithms running for 1 user at a time.
    inline const std::string FIND_MATCHES_TIMESTAMP_LOCK = "tSl"; //mongoDBDate, timestamp the find_matches is 'available' to run again

    inline const std::string LAST_TIME_FIND_MATCHES_RAN = "dFr"; //mongoDBDate, timestamp of the last time this account was used (last time the algorithm was called from this account). Will be set to Date(-1) for anything that is an event ACCOUNT_TYPE.

    inline const std::string AGE_RANGE = "aAr"; //document; contains fields for min and max age range this user will accept (it is a document for indexing purposes, arrays cannot index by the array index ex. AGE_RANGE.0 does not use the index)

    //namespace contains keys for AGE_RANGE document
    namespace age_range {
        inline const std::string MIN = "mIn"; //int32; min age range this user will accept
        inline const std::string MAX = "mAx"; //int32; min age range this user will accept
    }

    inline const std::string GENDERS_RANGE = "aGr"; //array of strings; list of genders this user matches with; values can be MALE_GENDER_VALUE for male FEMALE_GENDER_VALUE for female or the name of the selected gender (can be multiple gender)
    inline const std::string MAX_DISTANCE = "iMd"; //int32; max Distance this account accepts in miles

    inline const std::string PREVIOUSLY_MATCHED_ACCOUNTS = "aPm"; //array of documents; stores the matching account oID that it was previously matched with, the most recent time matched and the number of times matched

    //namespace contains keys for PREVIOUSLY_MATCHED_ACCOUNTS documents
    namespace previously_matched_accounts {
        inline const std::string OID = "oId"; //oid; account previously matched; using matching account oID
        inline const std::string TIMESTAMP = "dPt"; //mongoDB Date; most recent time account was matched
        inline const std::string NUMBER_TIMES_MATCHED = "iPa"; //int32; total number of times the account has been matched with; NOTE: NEVER LET THIS BE 0
    }

    inline const std::string LAST_TIME_EMPTY_MATCH_RETURNED = "dLe"; //mongoDB Date; this is the timestamp of the last time an empty match array was returned, it is reset to -1 when any match criteria is changed; used with TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND
    inline const std::string LAST_TIME_MATCH_ALGORITHM_RAN = "dMn"; //mongoDB Date; this is the timestamp of the last time the match algorithm ran, it prevents it from being spammed; used with TIME_BETWEEN_ALGORITHM_RUNS; this is actually updated in 2 places in case an error occurs during algorithm runtime

    inline const std::string INT_FOR_MATCH_LIST_TO_DRAW_FROM = "iDf"; //int32; this value can be anything between 1 and whatever number is max, if it is at max it will be reset to 0 (this number is used with NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER to decide whether to draw from 'other users said yes' list or 'algorithm match' list)
    inline const std::string TOTAL_NUMBER_MATCHES_DRAWN = "iMr"; //int32; this value will start at 0 and be incremented each time a match is returned

    //lists storing potential matches for this account
    inline const std::string HAS_BEEN_EXTRACTED_ACCOUNTS_LIST = "aHe"; //array of documents; list of accounts a device has requested (does not contain ACTIVITY_STATISTICS)
    inline const std::string ALGORITHM_MATCHED_ACCOUNTS_LIST = "aMa"; //array of documents; list of accounts this user has algorithmically been matched with
    inline const std::string OTHER_USERS_MATCHED_ACCOUNTS_LIST = "aOu"; //array of documents; list of accounts that selected 'yes' on this user (does not contain ACTIVITY_STATISTICS)

    //namespace contains keys for HAS_BEEN_EXTRACTED_ACCOUNTS_LIST, ALGORITHM_MATCHED_ACCOUNTS_LIST and OTHER_USERS_MATCHED_ACCOUNTS_LIST documents
    namespace accounts_list {
        inline const std::string OID = "oIe"; //oID; account OID of the matched account (this is for both the matching account document and the verified account document)
        inline const std::string POINT_VALUE = "dPv"; //double; point value of the matched account
        inline const std::string DISTANCE = "dDs"; //double; distance in miles of the matched account
        inline const std::string EXPIRATION_TIME = "dEx"; //mongoDB Date; expiration time of the matched account (this is the raw expiration time, it does not have TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED factored in)
        inline const std::string MATCH_TIMESTAMP = "dTs"; //mongoDB Date; timestamp this match was made, used to compare against account timestamps to see if match still valid
        inline const std::string FROM_MATCH_ALGORITHM_LIST = "bFm"; //bool; true if from ALGORITHM_MATCHED_ACCOUNTS_LIST false if from OTHER_USERS_MATCHED_ACCOUNTS_LIST
        inline const std::string ACTIVITY_STATISTICS = "aCs"; //array of documents; final match result containing some potentially useful values for this match (only actually stored in algorithm array, just used for statistics)
        inline const std::string SAVED_STATISTICS_OID = "oSs"; //oID; this is the oid to the location where the saved statistics are stored; NOTE: if an error occurs this field could not exist
    }

    inline const std::string TIME_SMS_CAN_BE_SENT_AGAIN = "dIs"; //mongoDB Date; time when another SMS message is allowed to be sent for account authorization
    inline const std::string NUMBER_SWIPES_REMAINING = "iSr"; //int32; the total number of swipes remaining for this phone number
    inline const std::string SWIPES_LAST_UPDATED_TIME = "iSt"; //int64 (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_SWIPES_UPDATED, so it is not the full timestamp

    inline const std::string LOGGED_IN_TOKEN = "sLt"; //string; for now this just an OID converted to a string however, in the future it can become a more secure token, so leaving the type as utf8
    inline const std::string LOGGED_IN_TOKEN_EXPIRATION = "dEt"; //mongoDB Date; login token expiration time
    inline const std::string LOGGED_IN_INSTALLATION_ID = "sDi"; //string; ID of device used to log into this account

    //These 2 fields are for returning things to the user when using the findOneAndUpdate operation.
    inline const std::string COOL_DOWN_ON_SMS_RETURN_MESSAGE = "iCs"; //int32; time until sms is off cool down NOTE: IN SECONDS
    inline const std::string LOGGED_IN_RETURN_MESSAGE = "iRm"; //int32; follows ReturnStatus enum, or LoginFunctionResultValuesEnum depending on what calls it; this value not meant to be updated outside attempting to log in

    inline const std::string TIME_EMAIL_CAN_BE_SENT_AGAIN = "aEC"; //mongoDB Date; time the email comes off cool down

    inline const std::string LAST_TIME_DISPLAYED_INFO_UPDATED = "dIu"; //mongoDB Date; last time any of the displayed info for this account (the info shown on Android) was updated. Note age, location (for max distance) and user pictures will not be updated if this if checked.

    inline const std::string BIRTHDAY_TIMESTAMP = "dTb"; //mongoDB dates; timestamp for last time birthday was updated (in seconds)
    inline const std::string GENDER_TIMESTAMP = "dTg"; //mongoDB dates; timestamp for last time gender was updated (in seconds)
    inline const std::string FIRST_NAME_TIMESTAMP = "dTn"; //mongoDB dates; timestamp for last time first name was updated (in seconds)
    inline const std::string BIO_TIMESTAMP = "dTi"; //mongoDB dates; timestamp for last time user bio was updated (in seconds)
    inline const std::string CITY_NAME_TIMESTAMP = "dTm"; //mongoDB dates; timestamp for last time user city name was updated (in seconds)
    inline const std::string POST_LOGIN_INFO_TIMESTAMP = "dTc"; //mongoDB dates; timestamp for last time any of the 'post login' info was updated (info collected after login including bio, city name, age range, gender range, max distance) (in seconds)
    inline const std::string EMAIL_TIMESTAMP = "dTe"; //mongoDB dates; timestamp for last time email was updated (this also includes if email verified bool was changed) (in seconds)
    inline const std::string CATEGORIES_TIMESTAMP = "dTs"; //mongoDB Date; timestamp for last time categories was updated

    inline const std::string CHAT_ROOMS = "aUp"; //array of documents; contains info about chat rooms this user is present inside

    //namespace contains keys for CHAT_ROOMS documents
    namespace chat_rooms {
        inline const std::string CHAT_ROOM_ID = "sIe"; //string; chat room id
        inline const std::string LAST_TIME_VIEWED = "dTv"; //mongoDB dates; last time this user viewed the chat room. NOTE: This value can be a little off for two reasons. 1) It is often derived from client devices. 2) It can be slightly behind when messages are inserted (this will only have an effect if the account is loaded on a different device).
        inline const std::string EVENT_OID = "eId"; //oid or does not exist; Only present if this is an event chat room. The event oid.
    }

    inline const std::string OTHER_USERS_BLOCKED = "aUb"; //array of documents; list of accounts blocked for user, contains VERIFIED_OTHER_USERS_BLOCKED_OID and TIMESTAMP_BLOCKED

    //namespace contains keys for OTHER_USERS_BLOCKED documents
    namespace other_users_blocked {
        inline const std::string OID_STRING = "sId"; //string; oid of account that is blocked for this user
        inline const std::string TIMESTAMP_BLOCKED = "dTs"; //mongoDB date; time block occurred
    }

    inline const std::string NUMBER_TIMES_SWIPED_YES = "iNy"; //int32; number of times this user has swiped 'yes' on others
    inline const std::string NUMBER_TIMES_SWIPED_NO = "iNn"; //int32; number of times this user has swiped 'no' on others
    inline const std::string NUMBER_TIMES_SWIPED_BLOCK = "iNb"; //int32; number of times this user has swiped 'block' on others
    inline const std::string NUMBER_TIMES_SWIPED_REPORT = "iNr"; //int32; number of times this user has swiped 'report' on others

    inline const std::string NUMBER_TIMES_OTHERS_SWIPED_YES = "iOy"; //int32; number of times others have swiped 'yes' on this user
    inline const std::string NUMBER_TIMES_OTHERS_SWIPED_NO = "iOn"; //int32; number of times others have swiped 'no' on this user
    inline const std::string NUMBER_TIMES_OTHERS_SWIPED_BLOCK = "iOb"; //int32; number of times others have swiped 'block' on this user
    inline const std::string NUMBER_TIMES_OTHERS_SWIPED_REPORT = "iOr"; //int32; number of times others have swiped 'report' on this user

    inline const std::string NUMBER_TIMES_SENT_ACTIVITY_SUGGESTION = "iSu"; //int32; number of times this user has sent an 'activity suggestion'
    inline const std::string NUMBER_TIMES_SENT_BUG_REPORT = "iBr"; //int32; number of times this user has sent a 'bug report'
    inline const std::string NUMBER_TIMES_SENT_OTHER_SUGGESTION = "iOs"; //int32; number of times this user has sent an 'other suggestion'

    inline const std::string NUMBER_TIMES_SPAM_FEEDBACK_SENT = "iTs"; //int32; number of times the user feedback has been set as spam by an admin
    inline const std::string NUMBER_TIMES_SPAM_REPORTS_SENT = "iTr"; //int32; number of times the user reports have been set as spam by an admin

    inline const std::string NUMBER_TIMES_REPORTED_BY_OTHERS_IN_CHAT_ROOM = "iCr"; //int32; number of times others have reported this user from chat room (swipes is NUMBER_TIMES_OTHERS_SWIPED_BLOCK)
    inline const std::string NUMBER_TIMES_BLOCKED_BY_OTHERS_IN_CHAT_ROOM= "iCb"; //int32; number of times others have blocked this user from chat room (swipes is NUMBER_TIMES_OTHERS_SWIPED_BLOCK)
    inline const std::string NUMBER_TIMES_THIS_USER_REPORTED_FROM_CHAT_ROOM = "iCe"; //int32; number of times this user has swiped report on other users from chat room (swipes is NUMBER_TIMES_SWIPED_REPORT)
    inline const std::string NUMBER_TIMES_THIS_USER_BLOCKED_FROM_CHAT_ROOM = "iCl"; //int32; number of times this user has swiped block on other users from chat room (swipes is NUMBER_TIMES_SWIPED_BLOCK)

    inline const std::string DISCIPLINARY_RECORD = "dRk"; //array of documents; container for records of disciplinary action, stored whenever STATUS is updated

    //namespace contains keys for DISCIPLINARY_RECORD documents
    namespace disciplinary_record {
        inline const std::string SUBMITTED_TIME = "dRs"; //mongoDB Date; timestamp this action took place
        inline const std::string END_TIME = "dRe"; //mongoDB Date; timestamp this action ends (used for suspensions)
        inline const std::string ACTION_TYPE = "dRt"; //int32; type of action taken follows DisciplinaryActionTypeEnum
        inline const std::string REASON = "dRr"; //string; the message for why the action was taken
        inline const std::string ADMIN_NAME = "dRn"; //string; name of admin that send the discipline
    }

}

namespace user_account_collection_index {
    inline const std::string ALGORITHM_INDEX_NAME = "algo_idx"; //name of the algorithm index for the ACCOUNTS_DATABASE_NAME.USER_ACCOUNTS_COLLECTION_NAME collection
}