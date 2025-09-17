//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include "Python.h"


//setup python modules
_ts* setupPythonModules();

//runs createIndex on all indexing to be done
void setupMongoDBIndexing();

//set up any mandatory documents in the database
void setupMandatoryDatabaseDocs();

//returns the boolean thread_started declared inside server_initialization_functions
bool getThreadStarted();

//signals map_of_chat_rooms_to_users to download the chat_room_id for the user of user_account_oid.
void downloadChatRoomForSpecificUser(
        const std::string& chat_room_id,
        const std::chrono::milliseconds& time_chat_room_last_observed,
        const std::string& user_account_oid,
        bool from_user_swiped_yes_on_event
);

void createAndStoreEventAdminAccount();
