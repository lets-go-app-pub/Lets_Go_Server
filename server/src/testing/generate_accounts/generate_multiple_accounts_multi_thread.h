//
// Created by jeremiah on 3/20/21.
//
#pragma once

//essentially runs generateMultipleRandomAccounts in a multi-threaded way
void multiThreadInsertAccounts(int numberAccountsToGenerate = 5000, int numberOfThreads = std::thread::hardware_concurrency(), bool generateIndexingAccountValues = true);
