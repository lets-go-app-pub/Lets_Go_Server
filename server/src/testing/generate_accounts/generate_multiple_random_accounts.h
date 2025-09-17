//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <bsoncxx/oid.hpp>
#include <tbb/concurrent_unordered_set.h>

//Meant to be user for testing with multiThreadInsertAccounts(), it is cleared there.
inline tbb::concurrent_unordered_set<std::string> unique_phone_numbers;

//generates multiple accounts with randomized info stored inside
bsoncxx::oid insertRandomAccounts(int numAccountsToInsert, int threadIndex, bool use_unique_set = false);
