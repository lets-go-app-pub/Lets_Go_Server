//
// Created by jeremiah on 5/8/21.
//
#pragma once

#include "coroutine_thread_pool.h"

//NOTE: Not everything uses this thread pool, notable exceptions are.
//1 thread for the async server
//2 thread for the change stream
//gRPC doesn't use this thread pool for the synchronous server
inline CoroutineThreadPool thread_pool;
