//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (INFO_FOR_STATISTICS_DATABASE_NAME) (INDIVIDUAL_MATCH_STATISTICS_COLLECTION_NAME)
namespace individual_match_statistics_keys {
    inline const std::string STATUS_ARRAY = "aS"; //array of documents containing, STATUS and RESPONSE_TIMESTAMP, this could be empty or have up to 2 elements if 1 user swipes yes then another user swipes on that (the 2nd element will be the 2nd to swipe)

    //namespace contains keys for STATUS_ARRAY documents
    namespace status_array {

        //NOTE: if the extracted item expires, then even if the user sees and swipes on this it will never be updated
        inline const std::string STATUS = "sT"; //utf8; essentially works like an enum for different cases; listed below

        //NOTE: This does NOT represent a document, it is essentially an enum representing possible values of STATUS.
        namespace status_enum {
            inline const std::string YES = "y"; //this is a possible value of STATUS
            inline const std::string NO = "n"; //this is a possible value of STATUS
            inline const std::string BLOCK = "b"; //this is a possible value of STATUS
            inline const std::string REPORT = "r"; //this is a possible value of STATUS
            //variations of UNKNOWN are reserved, so put an underscore in the middle
            inline const std::string UN_KNOWN = "u"; //this is a possible value of STATUS
        }

        inline const std::string RESPONSE_TIMESTAMP = "rT"; //mongoDB Date; timestamp the match was returned from the device;
    }

    inline const std::string SENT_TIMESTAMP = "tS"; //array of mongoDB Date; timestamps the match was sent to the device; array index will align with STATUS_ARRAY (if arrays are the same size)

    inline const std::string DAY_TIMESTAMP = "tD"; //int64; the day number (from unix timestamp point of January 1970...) used for indexing and works with field GENERATED_FOR_DAY; this is updated whenever the document is sent back to the user

    inline const std::string USER_EXTRACTED_LIST_ELEMENT_DOCUMENT = "eD"; //document; document storing the 'extracted' list element from the match (a single result of the pipeline with some extra info added)

    inline const std::string MATCHED_USER_ACCOUNT_DOCUMENT = "uV"; //document; document of matched user account at the time the algorithm ran (the calling user account is inside of MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME for when the algorithm was run)
}