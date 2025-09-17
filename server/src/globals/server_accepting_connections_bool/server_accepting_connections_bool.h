//
// Created by jeremiah on 5/16/21.
//
#pragma once

#include <atomic>

inline std::atomic_bool server_accepting_connections(true);

//Set inside the client interface inside '/etc/systemd/system/clientInterface.service'. This is meant to be returned
// when a manual shutdown is run.
inline const int SERVER_SHUTDOWN_REQUESTED_RETURN_CODE = 255;

//If this is set to anything besides SERVER_SHUTDOWN_REQUESTED_RETURN_CODE, the service build into the client
// interface linux servers will automatically restart it.
inline int SHUTDOWN_RETURN_CODE = 0;
