//
// Created by jeremiah on 1/31/22.
//

#pragma once

#include <string>
#include <vector>

void generateIndexingForAccounts(
        const std::vector<std::string>& account_oids,
        int start_index_to_update,
        int stop_index_to_update,
        int thread_index
);
