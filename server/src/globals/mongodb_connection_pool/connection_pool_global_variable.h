//
// Created by jeremiah on 5/13/21.
//
#pragma once

#include <mongocxx/pool.hpp>
#include "mongodb_values.h"

class ThreadPoolWrapper {
public:
    mongocxx::pool::entry acquire() {
        return mongocxx_client_pool->acquire();
    }

    //The thread pool cannot be initialized until mongocxx::instance has been started
    // this wrapper allows for the initialization to be called.
    void init() {
        mongocxx_client_pool = std::make_unique<mongocxx::pool>(mongodb_values::URI);
    }

private:
    std::unique_ptr<mongocxx::pool> mongocxx_client_pool = nullptr;
};

inline ThreadPoolWrapper mongocxx_client_pool;