//
// Created by jeremiah on 10/26/21.
//

#pragma once

#include <memory>
#include <string>
#include <user_account_keys.h>
#include <vector>

#include "async_server.h"

struct ServerAddressList {
  const std::string ADDRESS;
  const int PORT;

  ServerAddressList(std::string &&_address, int _port)
      : ADDRESS(_address), PORT(_port) {}
};

#ifdef _RELEASE

// server address and port
// NOTE: Make sure that array index [0] in server info is always the 'best'
// server
//  to connect to. The most reliable, probably. This is because there is a place
//  that could (even though it probably never will) access index [0] directly
//  inside the Android client.
const static std::vector<ServerAddressList> GRPC_SERVER_ADDRESSES_URI{
    //        ServerAddressList("{redacted}", 50051), //linode
    //        ServerAddressList("{redacted}", 50051)  //hostwinds
    ServerAddressList("{redacted}", 50051), // linode
    ServerAddressList("{redacted}", 50051)  // hostwinds
};

#else

// server address and port
const static std::vector<ServerAddressList> GRPC_SERVER_ADDRESSES_URI{
    ServerAddressList("{redacted}", 50051),
    ServerAddressList("{redacted}", 50052)};

#endif

// value set on startup, just the address, not the port, used to search
// GRPC_SERVER_ADDRESSES_URI list
inline std::string current_server_address;

// the uri passed to the server builder, can look at function comments
//  for grpc::ServerBuilder::AddListeningPort() to see more info about accepted
//  uri formats
// inline std::string current_server_uri = "[{redacted}]:50051";
inline std::string current_server_uri = "[0:0:0:0:0:0:0:1]:50051";

inline std::unique_ptr<GrpcServerImpl> grpc_server_impl = nullptr;
