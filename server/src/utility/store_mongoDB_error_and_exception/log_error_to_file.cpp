//
// Created by jeremiah on 11/5/21.
//

#include <file_IO_mutex_lock.h>
#include "store_mongoDB_error_and_exception.h"
#include "general_values.h"

void logErrorToFile(const std::string& error_message) {
    std::ofstream fileOutputStream;

    const std::string current_timestamp_string = getDateTimeStringFromTimestamp(getCurrentTimestamp());

    //I/O is not thread safe
    std::scoped_lock<std::mutex> lock(mutex_file_io_lock);
    fileOutputStream.open(general_values::ERROR_LOG_OUTPUT, std::ios_base::app);

    if (fileOutputStream) {
        fileOutputStream << "\n[" << current_timestamp_string << "] " << error_message;
        fileOutputStream.close();
    } else {
        std::cout << "Failed to open file " << general_values::ERROR_LOG_OUTPUT << '\n';
    }
}