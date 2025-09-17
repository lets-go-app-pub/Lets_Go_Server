//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

//keys for (INFO_FOR_STATISTICS_DATABASE_NAME) (MATCH_ALGORITHM_STATISTICS_COLLECTION_NAME)
namespace match_algorithm_statistics_keys {
    inline const std::string TIMESTAMP_RUN = "Ts"; //mongoDB Date; timestamp the match was run and used as start time in algorithm
    inline const std::string END_TIME = "eT"; //mongoDB Date; end time in algorithm (not the time the algorithm took to run but the end point the algorithm uses to calculate matches)

    inline const std::string ALGORITHM_RUN_TIME = "rT"; //int64; Total time IN NANO SECONDS that the algorithm took to run.
    inline const std::string QUERY_ACTIVITIES_RUN_TIME  = "qA"; //int64; Total time IN NANO SECONDS that the query during the algorithm took to run.

    inline const std::string USER_ACCOUNT_DOCUMENT = "vA"; //document; the user account doc

    inline const std::string RESULTS_DOCS = "rS"; //array of documents; the returned document values, these are directly inserted from the algorithm results
}