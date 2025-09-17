//
// Created by jeremiah on 5/11/21.
//
#pragma once

#include <thread>
#include <async_server.h>

std::jthread startServer(std::shared_ptr<GrpcServerImpl> grpcServerImpl);