//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

/**
 * When adding fields to this file, make sure to add values to buildUserStatisticsDocument() function.
 * **/
//keys for (ACCOUNTS_DATABASE_NAME) (USER_ACCOUNT_STATISTICS_COLLECTION_NAME);
namespace user_account_statistics_keys {

    //NOTE: _id fields matches the _id from USER_ACCOUNTS_COLLECTION_NAME, this is separated into a different document to avoid overflow

    inline const std::string DOCUMENT_TIMESTAMP = "t"; //mongodb Date; the time each document was updated (most documents below contain this value)

    inline const std::string LOGIN_TIMES = "lT"; //array of documents

    //namespace contains keys for LOGIN_TIMES documents
    namespace login_times {
        inline const std::string INSTALLATION_ID = "i"; //string; the installation ID that logged in here
        inline const std::string DEVICE_NAME = "d"; //string; the name of the device that logged in here
        inline const std::string API_NUMBER = "a"; //int32; the api number that logged in here
        inline const std::string LETS_GO_VERSION = "l"; //int32; the lets go version that logged in here
        inline const std::string CALLER_URI = "u"; //string; the uri (the address and port) of the function caller
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string TIMES_MATCH_OCCURRED = "aM"; //array of documents

    //namespace contains keys for TIMES_MATCH_OCCURRED documents
    namespace times_match_occurred {
        inline const std::string MATCHED_OID = "o"; //oid; account oid this user matched with
        inline const std::string CHAT_ROOM_ID = "c"; //string; chat room users were put into
        inline const std::string MATCH_TYPE = "y"; //int32 or does not exist; follows MatchType enum from MatchTypeEnum.proto (this was added later, it may not exist on older statistics docs)
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string LOCATIONS = "lO"; //array of documents

    //namespace contains keys for LOCATIONS documents
    namespace locations {
        inline const std::string LOCATION = "n"; //array; array has 2 elements of type double, 1st is longitude, 2nd is latitude (to follow mongoDB type Point convention)
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string PHONE_NUMBERS = "pN"; //array of documents

    //namespace contains keys for PHONE_NUMBERS documents
    namespace phone_numbers {
        inline const std::string PHONE_NUMBER = "n"; //string; phone number
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string NAMES = "nA"; //array of documents

    //namespace contains keys for NAMES documents
    namespace names {
        inline const std::string NAME = "n"; //string; username
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string BIOS = "bI"; //array of documents

    //namespace contains keys for BIOS documents
    namespace bios {
        inline const std::string BIOS = "b"; //string; bio
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string CITIES = "cI"; //array of documents

    //namespace contains keys for CITIES documents
    namespace cities {
        inline const std::string CITY = "c"; //string; city
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string GENDERS = "gE"; //array of documents

    //namespace contains keys for GENDERS documents
    namespace genders {
        inline const std::string GENDER = "g"; //string; gender
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string BIRTH_INFO = "bD"; //array of documents

    //namespace contains keys for BIRTH_INFO documents
    namespace birth_info {
        inline const std::string YEAR = "y"; //int32; birth year
        inline const std::string MONTH = "m"; //int32; birth month
        inline const std::string DAY_OF_MONTH = "d"; //int32; birth day of month
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string EMAIL_ADDRESSES = "eM"; //array of documents

    //namespace contains keys for EMAIL_ADDRESSES documents
    namespace email_addresses {
        inline const std::string EMAIL = "e"; //string; email address
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string CATEGORIES = "cA"; //array of documents

    //namespace contains keys for CATEGORIES documents
    namespace categories {
        inline const std::string ACTIVITIES_AND_CATEGORIES = "c"; //array of documents; categories
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string AGE_RANGES = "aR"; //array of documents

    //namespace contains keys for AGE_RANGES documents
    namespace age_ranges {
        inline const std::string AGE_RANGE = "a"; //array of 2 int32 values; min and max age range (does not follow AGE_RANGE from user account values, is not a document)
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string GENDER_RANGES = "gR"; //array of documents; documents contain GENDER_RANGES_GENDER_RANGE & TIMESTAMP_STORED

    //namespace contains keys for GENDER_RANGES documents
    namespace gender_ranges {
        inline const std::string GENDER_RANGE = "g"; //array of strings; list of genders this user matches with follows GENDERS_RANGE
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string MAX_DISTANCES = "mD"; //array of documents

    //namespace contains keys for MAX_DISTANCES documents
    namespace max_distances {
        inline const std::string MAX_DISTANCE = "m"; //int32; max Distance this account accepts in miles follows MAX_DISTANCE
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string SMS_SENT_TIMES = "sS"; //array of mongoDB Dates; times that this account had sms sent to it

    inline const std::string EMAIL_SENT_TIMES = "eS"; //array of documents

    //namespace contains keys for EMAIL_SENT_TIMES documents
    namespace email_sent_times {
        inline const std::string EMAIL_ADDRESS = "e"; //string; email address email will be sent TO
        inline const std::string EMAIL_PREFIX = "p"; //string; email prefix email will be sent FROM
        inline const std::string EMAIL_SUBJECT = "s"; //string; email message subject
        inline const std::string EMAIL_CONTENT = "c"; //string; email message content
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    //used on the web server
    inline const std::string EMAILS_VERIFIED = "eV"; //array of documents

    //namespace contains keys for EMAILS_VERIFIED documents
    namespace emails_verified {
        inline const std::string EMAILS_VERIFIED_EMAIL = "e"; //string; email address
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    //used on the web server
    inline const std::string ACCOUNT_RECOVERY_TIMES = "rT"; //array of documents

    //namespace contains keys for ACCOUNT_RECOVERY_TIMES documents
    namespace account_recovery_times {
        inline const std::string PREVIOUS_PHONE_NUMBER = "p"; //string; previously used phone number
        inline const std::string NEW_PHONE_NUMBER = "n"; //string; updated phone number
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string ACCOUNT_LOGGED_OUT_TIMES = "lI"; //array of mongoDB Dates; times that this account logged out

    inline const std::string ACCOUNT_SEARCH_BY_OPTIONS = "sO"; //array of documents

    //namespace contains keys for ACCOUNT_SEARCH_BY_OPTIONS documents
    namespace search_by_options {
        inline const std::string NEW_SEARCH_BY_OPTIONS = "n"; //int; updated search by option, follows user_account_keys::SEARCH_BY_OPTIONS
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }

    inline const std::string OPTED_IN_TO_PROMOTIONAL_EMAIL = "pE"; //array of documents

    //namespace contains keys for OPTED_IN_TO_PROMOTIONAL_EMAIL documents
    namespace opted_in_to_promotional_email {
        inline const std::string NEW_OPTED_IN_TO_PROMOTIONAL_EMAIL = "n"; //bool; updated search by option, follows user_account_keys::SEARCH_BY_OPTIONS
        inline const std::string TIMESTAMP = DOCUMENT_TIMESTAMP; //mongodb Date; time the document was updated
    }
}