//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (ACCOUNTS_DATABASE_NAME) (INFO_STORED_AFTER_DELETION_COLLECTION_NAME)
namespace info_stored_after_deletion_keys {
    //planning to use this to store info about a phone number separate from the number
    //last SMS sent and number of swipes used are two examples
    inline const std::string PHONE_NUMBER = "pN"; //string
    inline const std::string TIME_SMS_CAN_BE_SENT_AGAIN = "tC"; //mongoDB Date Type; time when another SMS message is allowed to be sent for account authorization, this will ONLY be used if the verified account does not exist, otherwise TIME_SMS_CAN_BE_SENT_AGAIN is used
    inline const std::string TIME_EMAIL_CAN_BE_SENT_AGAIN = "tE"; //mongoDB Date Type; time when another SMS message is allowed to be sent for account authorization, this will ONLY be used if the verified account does not exist, otherwise TIME_EMAIL_CAN_BE_SENT_AGAIN is used
    inline const std::string COOL_DOWN_ON_SMS = "sC"; //int32; time until sms is off cool down NOTE: IN SECONDS; NOTE: This is meant to be generated and immediately extracted, it should never be relied on to be correct.

    inline const std::string NUMBER_SWIPES_REMAINING = "nSr"; //int32; the total number of swipes remaining for this phone number NOTE: works with NUMBER_SWIPES_REMAINING
    inline const std::string SWIPES_LAST_UPDATED_TIME = "sLu"; //int64 (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_SWIPES_UPDATED, so it is not the full timestamp NOTE: works with NUMBER_SWIPES_REMAINING

    inline const std::string NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS = "nFa"; //int32; the total number of times sms verification guesses have failed NOTE: works with FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME; Set as an int64 in order to make sure a user cannot input so many attempts, it loops back to zero.
    inline const std::string FAILED_SMS_VERIFICATION_LAST_UPDATE_TIME = "vLu"; //int64; (NOT MONGODB DATE TYPE); this is unixTimestamp/TIME_BETWEEN_VERIFICATION_ATTEMPTS, so it is not the full timestamp NOTE: works with NUMBER_FAILED_SMS_VERIFICATION_ATTEMPTS
}