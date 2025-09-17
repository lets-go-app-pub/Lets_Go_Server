//
// Created by jeremiah on 5/30/22.
//

#include <mongocxx/instance.hpp>
#include <connection_pool_global_variable.h>
#include <gtest/gtest.h>
#include <accepted_mime_types.h>
#include <server_initialization_functions.h>
#include "chat_stream_container_object.h"

int main(int argc, char** argv) {

    srand((unsigned) time(nullptr) * getpid());

    [[maybe_unused]] mongocxx::instance mongo_cpp_instance{}; // This should be done only once.
    mongocxx_client_pool.init();

    for (const auto& mime_type : accepted_mime_types) {
        if (mime_type.size() > largest_mime_type_size) {
            largest_mime_type_size = mime_type.size();
        }
    }

    _ts* embedded_python_thread_state = setupPythonModules();

    //indexing must come before database docs to enforce unique indexing
    setupMongoDBIndexing();
    setupMandatoryDatabaseDocs();

    ::testing::InitGoogleTest(&argc, argv);
    int tests_return = RUN_ALL_TESTS();

    //NOTE: this must be released AFTER all Python code has completed
    PyEval_RestoreThread(embedded_python_thread_state);
    Py_Finalize();

    return tests_return;
}
