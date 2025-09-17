//
// Created by jeremiah on 11/18/21.
//

#pragma once

#include <string>

namespace error_values {
    //extract errors specific
    inline const long MAXIMUM_NUMBER_ALLOWED_BYTES_TO_REQUEST_ERROR_MESSAGE = 1024L * 1024L * 1024L * 5L; //5Gb total

    //extract errors ~specific~
    inline const std::string EXTRACT_ERRORS_TOTAL_BYTES_KEY = "b_tot_key"; //used for trailing metadata inside extract errors, stores a long
    inline const std::string EXTRACT_ERRORS_EXTRACTED_BYTES_KEY = "b_ext_key"; //used for trailing metadata inside extract errors, stores an int
    inline const std::string EXTRACT_ERRORS_TOTAL_DOCS_KEY = "d_tot_key"; //used for trailing metadata inside extract errors, stores a long
    inline const std::string EXTRACT_ERRORS_EXTRACTED_DOCS_KEY = "d_ext_key"; //used for trailing metadata inside extract errors, stores an int
    inline const int EXTRACT_ERRORS_MAX_SEARCH_RESULTS = 1525192; //use inside extract errors see function for more details
}