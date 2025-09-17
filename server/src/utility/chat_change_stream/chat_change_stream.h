//
// Created by jeremiah on 8/14/22.
//

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <chrono>
#include "ChatMessageToClientMessage.pb.h"
#include "ChatMessageStream.pb.h"

/** Can read src/utility/async_server/_documentation.md for how this relates to the chat stream. **/

//starts the change stream to monitor the chat room database
/** BLOCKING: do not call from main thread **/
void beginChatChangeStream();

//cancels the change stream, user after the gRPC loop finishes
void cancelChatChangeStream();

#ifdef LG_TESTING
void setThreadStartedToFalse();
#endif
