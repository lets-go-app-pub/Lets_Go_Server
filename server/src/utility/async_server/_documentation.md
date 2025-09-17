## Chat Stream Documentation

This file will cover the chat stream implementation. It will focus around several files
including (but not limited to) listed below.
* chat_change_stream.h
* chat_change_stream_helper_functions.h
* chat_stream_container_object.h
* async_server.h
* coroutine_thread_pool.h
* coroutine_type.h
* coroutine_spin_lock.h

It does NOT cover things such as choices made for MessageSpecifics::MessageBodyCase (message type). Also, some
specifics are covered inside comments in the code.<br/>

'Fun' note: At the time of writing this the ChatStreamContainerObject is lock-free (it uses atomics and spin locks but no 
mutex locks).

## General Idea
The idea behind the chat stream is that it has a single ChatStreamContainerObject object which will
represent a single user. This object will be created when the user connects and destroyed when the
user disconnects. <br/>

This server was meant to be run in parallel with other program instances on other physical servers for horizontal scaling.<br/>

GrpcServerImpl is the primary server object and only one should be running at a time. This object is 
an implementation of the gRPC C++ Asynchronous server. From a gRPC standpoint, each ChatStreamContainerObject represents a
single bidirectional stream rpc with the server. Each ChatStreamContainerObject instance will be created and attached
to the GrpcServerImpl instance. The GrpcServerImpl instance will then use the ChatStreamContainerObject instances
to send messages. The ChatStreamContainerObject will be responsible for cleaning up any extra info it
saved before deleting itself when the rpc ends. <br/>

The ChatStreamContainerObject object has two primary jobs.
1. Sending newly received messages to the user (the connected device) from the chat rooms.
2. Requesting updates for specific messages.
   
In order to fulfill (1) a thread is permanently connected to the database (for the life of the server)
using what mongoDB calls a 'change stream'. Whenever a new message is stored inside the database,
the change stream thread will receive a signal from the database that the message has been sent. It will
then 'inject' the message into the ChatStreamContainerObject object. This will allow the new message
to be sent back to the user.

In order to fulfill (2) a requested message id (or group of requested message ids) is sent into the
server. The object will then look up the message inside the database and send back the requested
amount of information.

## Grpc Server
The GrpcServerImpl is the primary server. It will be started when the program is initialized and then
when it stops the program will end. A single instance is expected to run for the entire life of the program.
The server combines parts of the gRPC C++ asynchronous server with the gRPC C++ synchronous server. As a side note
the synchronous server can and will handle more than one rpc at a time (despite the name). However, the
asynchronous server is where the complexity arises, it also encompasses the chat stream.

#### Initialization
The server builder will initialize all the objects representing the synchronous server, set all relevant
options, and begin the asynchronous server. When starting the asynchronous server loop it will first create
the very first instance of ChatStreamContainerObject. It will then wait for messages to be sent from the
completion queue. 

#### Running
It will receive messages from the completion queue (variable cq_). These messages will be sent to the thread_pool so
that the server thread can go back to waiting at the completion queue. The 'tag' (a void* object) that is sent back 
from the completion queue is a pointer to a CallDataCommand object that is contained INSIDE a ChatStreamContainerObject
instance (look at CallDataCommand for more info). These CallDataCommand objects are used for two things.
1. A pointer to the containing ChatStreamContainerObject.
2. The command to run.

This 'tag' will then be sent to the thread pool to execute on a different thread.

#### Shut down
A shutdown for a server can be initiated from the desktop interface. Several steps will have to be passed in order
for the shutdown to complete. First it will disable the ability to accept new connections (no new bidirectional stream rpcs and
therefore no new ChatStreamContainerObject will be allowed to be created). It will then cancel the change
stream thread and sleep for a certain amount of time to allow the change stream to handle any outstanding messages. After that
it will cycle through and cancel all outstanding ChatStreamContainerObject. Another sleep will follow this in order to wait for
all objects to finish. Then the shutdown command will be sent to the grpc::Server and the completion queue allowing
the program to end.

NOTE: A single ChatStreamContainerObject will be leaked. However, the program will end immediately afterwards.

## Global Variables

There are three global variables that are used with the chat stream.
1. **map_of_chat_rooms_to_users**: A map of all chat rooms this server is currently connected to. Each chat room will have
 its own map of users and a unique identifier for the ChatStreamContainerObject that stored it. When a chat room is added
 to map_of_chat_rooms_to_users it will be stored forever (in order to use a lock free thread safe data structure). However, when
 a ChatStreamContainerObject is added to the chat room object it is expected to be removed when that ChatStreamContainerObject cleans up.
2. **user_open_chat_streams**: A MongoDBOIDContainer object instance. This object stores each active ChatStreamContainerObject so that
 references can be requested from other threads (the chat change stream thread specifically). Only one ChatStreamContainerObject can be
 stored for each user (made unique by the user oid).
3. **thread_pool**: This is a thread pool made to accept coroutines. It will also accept the more 'standard' tasks in the form of lambdas
 and is used in other areas of the program beyond just the chat stream. However, whenever the server retrieves a value from the completion 
 queue the tag is passed to this thread pool.

## Missing Messages

There is often a choice of either occasionally sending back a message that has already been received or missing the message completely. When
this choice is present, the solution should always be to get the occasional duplicate. There are two main issues that can cause a situation like this
to occur. 
1. The change stream thread can receive and send a message at any time. It will never (and should never) block for any substantial amount of time for
 a ChatStreamContainerObject to initialize or anything of the sort. It will put the message in a queue for the ChatStreamContainerObject and move on.
2. The change stream sends messages back in the order the database receives them. However, this is only loosely associated with the message
 field of timestamp stored (which is the order messages ideally go back to the clients in).

### Handling 1
There are brief breaks between certain things running with ChatStreamContainerObject instances. For example, when a new ChatStreamContainerObject initializes,
it must do so in a way that guarantees that no new messages are missed while it is doing the initialization. It must also request anything that the device
may have missed since its last connection. In order to do this it must request all messages after the time the client last updated while keeping track of any new
messages. **IMPORTANT: This can potentially lead to some duplicate messages**. However, it is better than the alternative of possibly missing messages.<br/>

Another place that messages may be missed is when a chat room is joined. This can happen because in order for a ChatStreamContainerObject instance
to receive messages it must be added to the map_of_chat_rooms_to_users. However, ideally each ChatStreamContainerObject would be responsible for adding and 
removing references to itself. Unfortunately in the time it would take for ChatStreamContainerObject to process, messages could be missed. The implemented solution
for this is that the change stream thread itself will store the ChatStreamContainerObject for specific message types (see addOrRemoveChatRoomIdByMessageType()).
This has the side effect that ChatStreamContainerObject may be set inside a chat room of map_of_chat_rooms_to_users without a local copy for the instance
of the chat room id it is responsible to remove because it has not extracted the message from its personal message queue yet. This will cause the instance to
not be able to remove the chat room id during cleanup. The solution for this is that during cleanup, the ChatStreamContainerObject must go through all messages
it did not send back from its personal queue during cleanup and make sure that it did not miss any messages which add chat rooms.

### Handling 2
The reason messages must go back in order is because of update_time (the timestamp_stored of the most recently received message by the client). This update_time is
then sent to the server and all messages at or after update_time-TIME_TO_REQUEST_PREVIOUS_MESSAGES will be requested and sent back to the client. This means that if
timestamps are out of order and a later message is sent back before an earlier timestamp, there is a chance that the earlier timestamp could be missed. 

Unfortunately, order of messages from the server is not rigorously guaranteed. Therefor, there are windows that it will request backwards in order to search for missed messages.
The first window is when joinChatRoom() is called. When the kDifferentUserJoinedMessage is received by the change stream, it will request messages backwards 
TIME_TO_REQUEST_PREVIOUS_MESSAGES amount of time in case a message was missed and send them back to the user. The second window is when a ChatStreamContainerObject
initializes. It will request the messages backwards by TIME_TO_REQUEST_PREVIOUS_MESSAGES in case the user missed any messages during that time. It should also have 
a list of message uuids passed from the client during that time that it will exclude from the search. This should help to minimize duplicate messages.

The change stream on mongoDB sends back messages based on the order they are stored in the server (the timing of which can be a bit complex when considering transactions and replica
sets). However, it is certain that if a timestamp is generated separately, then it could be stored by the server in a different order than the timestamp itself reflects. The
timestamps are generated by the database itself (using the $$NOW command). However, this leads to other complications such as more database calls because the documents
have to be accessed a second time to store the generated timestamp (e.g. viewed_time and activity_time). Also, while it helps, it still does not guarantee order and so the
change stream itself will cache the messages for DELAY_FOR_MESSAGE_ORDERING amount of time and send them back in order by timestamp. This will give a window where, as long as the
messages are close, they will all be properly ordered. As a note if DELAY_FOR_MESSAGE_ORDERING is increased too much then the user will have noticeable lag between messages.

In order to promote order of the messages a few things are done. First the change stream relies on a single thread to send back the messages ordered by timestamp_stored. Then this
is injected to each ChatStreamContainerObject by a single thread to the instances' personal queue. The messages are then popped from the queue in order and gRPC guarantees order
of the rpcs. So the connection from the change stream to the client will have order by timestamp_stored 'guaranteed'.

Another problem occurs when messages that add/remove a user are received out of order. This can cause some problems with the chat rooms map. For example if
a kick message occurs then a join message occurs for the same user the final state of the user should be inside the chat room. However, if they are received
backwards by the change stream then the kick can occur after the join and the final state of the user will be not inside the chat room. The function
iterateAndSendMessagesToTargetUsers() handles these issues. However, the basic idea is that when a message that adds/removes a user is received, the change stream
will go back through the message cache and find if any messages were received that also modified the user's state inside the chat room. It will then iterate through 
any such messages and make sure that the final user state in the chat room is correct.

### Other Info

A general idea is that if TIME_TO_REQUEST_PREVIOUS_MESSAGES is requested backwards, then there will be no missed message (a multi-node replica set increases the
potential out of order time here by quite a bit). However, if TIME_TO_REQUEST_PREVIOUS_MESSAGES is increased then overhead of requesting duplicate messages will 
also increase with it. The other variable of note is that if messages are delayed for DELAY_FOR_MESSAGE_ORDERING before passing through the change stream then
they will be mostly ordered (however it creates a delay for users receiving messages). These two variables are the primary levers for making adjustments if
missing messages becomes a problem. There are a few others inside chat_change_stream_values.h as well.

There is also an odd situation that can occur where a user gets a kicked message back with a flag of 'only_store_message' or 'do_not_update_user_state'. For example say
a user starts a new chat stream and during initialization it updates a chat room. If this chat room first requests the chat rooms list from the database showing that
the user is still inside the chat room.  Then the user is kicked BEFORE messages are requested from the database, the kicked message can be sent back with 
'do_not_update_user_state'. This means the user will be kicked, however, their device will never register they are kicked and so while it isn't specifically a 
missed message it misses the functionality associated with that message. In order to prevent this the user chat rooms are compared before and after the messages
are requested. If they do not match, the initialization chunk will re-run (it should be rare). The same situation can occur with joinChatRoom(). However, in this
case it can be prevented by not requesting any messages after the kDifferentUserJoined message during the function call (the user cannot be kicked
before they join).

Another (less relevant to the server) issue with missing messages occurs if a user sends a message to the server and stores the timestamp_stored returned for their own message as the
update_time. This can cause a missed message if the other message was stored before theirs. If they attempt to connect at this point then the initialization of the chat stream will
have the most recent update_time and so the potential for missed messages is increased. In order to prevent this, the update_time is not updated until a is sent back
to the client. This message will be sent back in place of the actual message for the sending user (the sending user will already have the message, they sent it after all).

A situation which can occur on the client due to latency can also cause missed messages. If the client joins a chat room from the server. Then before the client has started downloading
the chat room, the kDifferentUserJoinedMessage is received by the change stream, the client can miss the previous messages sent from the message cache as well as any new messages
because the device will skip messages for chat rooms it is not a part of. This is a problem that must be fixed on the client side. In Android the fix was to have a 'temporarily
joined chat room' object that can receive and store messages until the join chat room function has completed. After that it is cleaned up and any messages it stores are processed.

## Chat Change Stream

The change stream will connect to the database when the program starts. It will endlessly cycle until continueChangeStream is set to false (this is done in server shutdown). It accepts
messages and headers from the database CHAT_ROOMS_DATABASE_NAME (the chat rooms). New chat room headers that are matches will be sent back to the matching users. New messages will be sent 
out to any users currently inside the respective chat room. <br/>

The messages have a slight delay to promote a more correct order this should provide a better user experience (see 'Missing Messages' header for more info). The change stream threads
are also responsible for adding and removing chat rooms to avoid missed messages (again see 'Missing Messages'). Note that it only sends back 
AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE to the clients in order to save bandwidth. This should be enough to display a majority of messages. However,
no need to send back full info for everything can let the device request more.<br/>

The change stream also has a message_cache associated with it. This will cache messages for the last MAX_TIME_TO_CACHE_MESSAGES (or until it gets too large, at which point it will be
trimmed). The cache will be used for two things. First, to tell if messages were sent back in their 'proper' order (ordered by the timestamp the message was stored). And second, when
a user joins a chat room it will send back the previous few seconds of messages for that chat room (see 'Missing Messages' header for more info).<br/>

Note: Throughout this document the change stream is often referred to as 'change stream thread'. When this phrase is used it actually refers to the two threads that
make up the system mentioned here.  

## CoroutineThreadPool

The global variable thread_pool is a thread pool of type CoroutineThreadPool that can also take coroutines. It can take normal 'tasks' in the form of functors however the 
ChatStreamContainerObject uses coroutines. The coroutine ThreadPoolSuspend is defined in coroutine_type.h, this is the only type of coroutine that the thread pool 
takes. A few notes on using ThreadPoolSuspend and coroutines in general (including some pitfalls).
* Exceptions are not normally propagated inside coroutines. However, the coroutine ThreadPoolSuspend will propagate them if RUN_COROUTINE is used to call it.
* **If passing callables (such as lambdas) or rvalues they must be passed by value (ThreadSafeWritesWaitingToProcessVector has this problem). Otherwise, when the coroutine
 suspends (co_await is called) the rvalue will be removed. Then when it resumes the reference will no longer be valid.**
* Coroutines by nature are not destroyed when they go out of scope. The handle must be stored and .destroy() called (CoroutineThreadPool and RUN_COROUTINE will handle this).
* The first time a handle is acquired it will not run anything in the coroutine because initial_suspend() returns std::suspend_always.
* .destroy() must always be called because final_suspend() returns std::suspend_always.
* If using nested coroutines .destroy() will not propagate to 'inner' coroutines (if using CoroutineThreadPool with RUN_COROUTINE this will be handled automatically).<br/>

Coroutine handles are expected to be passed into the thread pool. Then, if another coroutine is called from inside the passed coroutine, it can be called using the
RUN_COROUTINE macro. This will handle several things automatically (exceptions, storing child handles, propagating SUSPEND etc...). When SUSPEND is called inside
a coroutine running inside the thread pool it will co_yield. This will pause execution of the coroutine and return to the thread pool. The thread pool will then
put it in the back of the queue.<br/>

The value of coroutines in this thread pool (inspired by Kotlin suspend functions) is that the threads can use SUSPEND in place of say a spin lock or a mutex.
This way threads never have to permanently spin or sleep inside the thread pool. It seems to be a reasonable alternative to refactoring the code in order to
achieve the same result inside the thread pool. CoroutineSpinlock is also set up to allow data structures (ideally fast operations, it is a spin lock) used inside
coroutines or outside coroutine to spin while waiting (note that this locking method is not fair). 

## ChatStreamContainerObject

This class is meant to 'live and die' with a gRPC bidirectional rpc. When a new rpc starts for a user a new instance is created
for them and the old instance is destroyed. For more general information on what this class is built to accomplish read the 
'General Idea' header.

### Expected Input
An initial metadata is expected to be passed when the rpc starts. The data required is the data inside 
grpc_stream_chat::InitialLoginMessageRequest. The keys for each value can also be found inside chat_stream_container.h. There are
four keys passed and all are expected to be valid
+ CURRENT_ACCOUNT_ID
+ LOGGED_IN_TOKEN
+ LETS_GO_VERSION
+ INSTALLATION_ID

The fifth key is CHAT_ROOM_VALUES which is a bit more complex. Each chat room the client is currently inside must be stored with 
some information. However, all of this information is a single string. Each value is separated by CHAT_ROOM_VALUES_DELIMITER 
and the string is expected to end with a chat room delimiter. The expected order is
1. chat_room_id (expected to be a valid chat room id)
2. last_time_updated (expected to be a 64bit number -1 or greater converted to a string)
3. last_time_viewed (expected to be a 64bit number -1 or greater converted to a string)
4. number_recent_message_uuids (should be a 32bit number 0 or greater converted to a string; the number of recent_message_uuids)
5. recent_message_uuids (can be empty, however there must be the number of uuids reflected by number_recent_message_uuids)

So an example CHAT_ROOM_VALUES string might be.<br/>
string = chat_room_id_1 + CHAT_ROOM_VALUES_DELIMITER + last_time_updated_1 + CHAT_ROOM_VALUES_DELIMITER + last_time_viewed_1 + CHAT_ROOM_VALUES_DELIMITER + 2 + CHAT_ROOM_VALUES_DELIMITER + most_recent_message_uuid_1 + CHAT_ROOM_VALUES_DELIMITER + most_recent_message_uuid_2 + CHAT_ROOM_VALUES_DELIMITER;

An example CHAT_ROOM_VALUES string with no message_uuids would be.<br/>
string = chat_room_id_1 + CHAT_ROOM_VALUES_DELIMITER + last_time_updated_1 + CHAT_ROOM_VALUES_DELIMITER + last_time_viewed_1 + CHAT_ROOM_VALUES_DELIMITER + 0 + CHAT_ROOM_VALUES_DELIMITER;

The client is expected to provide all uuids back TIME_TO_REQUEST_PREVIOUS_MESSAGES amount of time for each chat room. However,
the server has a maximum metadata size set of MAXIMUM_RECEIVING_META_DATA_SIZE and so the metadata cannot go over this limit
(leaving a Kb of extra for the first four <key, value> pairs seems to work in testing). Once it reaches a certain point, it
is recommended to skip adding any further uuids. This will request some extra duplicate messages from the server, however it
will avoid returning INVALID_PARAMETER_PASSED and potentially crashing the client.

### Final Output
Trailing metadata is sent back when the stream finishes. However, it is important to note that this is **NOT GUARANTEED BY GRPC**. The stream could end 
unexpectedly and not allow gRPC the chance to send it back.
<br/><br/>
Trailing metadata keys can also be found inside chat_stream_container.h. There are three metadata key value pairs.
+ REASON_STREAM_SHUT_DOWN_KEY (should always be sent back)
+ RETURN_STATUS_KEY (will be sent back if REASON_STREAM_SHUT_DOWN_KEY==RETURN_STATUS_ATTACHED)
+ OPTIONAL_INFO_OF_CANCELLING_STREAM (optional info, currently only used when REASON_STREAM_SHUT_DOWN_KEY==STREAM_CANCELED_BY_ANOTHER_STREAM, it
 will send back the installation_id of the device that canceled the stream)

### Initialization
* The first thing a ChatStreamContainerObject will do inside the constructor is to attach itself to the AsyncService with an INITIALIZE tag. When a new
bidirectional rpc is started the instance will be called with INITIALIZE. INITIALIZE will in turn call initializeObject() which will create a new
ChatStreamContainerObject() and attach it to the AsyncService to be used for the next rpc (and so on indefinitely).
* If any errors happen during initialization after this point it will immediately finish the object and send back the trailing metadata to the client.
* After 'new ChatStreamContainerObject()' is called it will extract the initial metadata and do a basic login.
* Next it will upsert itself to the user_open_chat_streams. This will cancel a previous stream if it existed inside the data structure. Also, importantly
it will create its own unique index number 'current_index_value' during the update (see the code for details about why it must come at this point). **'current_index_value'
is guaranteed to be larger than any previous ChatStreamContainerObject instances for this user_oid**. This fact is used when removing and storing values inside 
user_open_chat_streams and map_of_chat_rooms_to_users.
* Next any new message and chat room states that require updated are sent back to the client. An important step happens here inside extractChatRooms(). The
local variable chat_room_ids_user_is_part_of as well as map_of_chat_rooms_to_users are populated with the chat rooms that this user is currently inside. This
instance will be responsible for cleaning itself out of all map_of_chat_rooms_to_users values. chat_room_ids_user_is_part_of should hold a copy of everywhere
that it exists or existed at (see 'Missing Messages' header under Handling 1 for more info).
* After that initialization_complete is set to true, Read() is sent into the completion queue to await any new requests and all initialization messages as well
as any messages that were injected are sent back to the user.

### Joining And Leaving Chat Rooms
The global variable map_of_chat_rooms_to_users stores each chat room that this server has seen since it started and the users currently inside each chat room 
(look under 'Global Variables' header for more info). The variable is used by the change stream thread to find which users to attempt to send messages to.
Whenever it receives a message it will access the chat room that the message is sent from and iterate through all users and then inject the message to the
respective ChatStreamContainerObject (the sending user will receive a kNewUpdateTimeMessage instead). The chat room objects are never removed from
map_of_chat_rooms_to_users. The user oids ARE however removed from each chat room, and it is the ChatStreamContainerObjects' job to keep track of which
need to be removed when the instance ends.<br/>

There are two times that a user oid will be added or removed from map_of_chat_rooms_to_users. The first is when the change stream thread receives a message and
the message type reflects a join/leave message. The change stream must handle this to avoid missing messages or storing extra messages (see 'Missing Messages' header for 
more info). Whenever a message of this type is sent to the ChatStreamContainerObject instance it must also keep track of these messages that have been added in order to
remove them on cleanup. It does this using the local variable chat_room_ids_user_is_part_of along with its own unique index number current_index_value.<br/>

When ChatStreamContainerObject initializes it will request all chat rooms the user is currently a part of from the database and add them to both map_of_chat_rooms_to_users and 
chat_room_ids_user_is_part_of. The instance will then receive any messages from the change stream thread and will mimic the storage of the change thread. This will allow the object 
to have a copy of all chat rooms that must be removed. This requires that certain criteria are fulfilled in order to prevent any user_oids being left in map_of_chat_rooms_to_users.
* The change stream thread can only store messages to the map if ChatStreamContainerObject has not called Finish() yet. 
* The ChatStreamContainerObject instance must receive the message if the change stream thread stored it. 
* All messages that are injected to must be checked before cleanup can complete inside the ChatStreamContainerObject instance.<br/>

Another issue is what happens when one ChatStreamContainerObject instance is created for a user while another is still active and stored inside
user_open_chat_streams. What happens is that the new ChatStreamContainerObject instance will call endStream() on the old one and take its place.
However, this means that there is a possibility that events could play out like so. 
1. First instance adds values to map_of_chat_rooms_to_users during initialization.
2. Second instance calls endStream() on first instance and adds the chat rooms to map_of_chat_rooms_to_users (they already exist, so it does nothing).
3. First instance cleans up and removes everything inside chat_room_ids_user_is_part_of, removing all chat rooms that second instance requires to receive messages.<br/>

In order to fix this problem a unique increasing index is used. Each ChatStreamContainerObject when added to user_open_chat_streams will calculate its own unique index
as it is being added. This is important to make sure that **the most recent version of ChatStreamContainerObject will always have a higher current_index_value**. This way
map_of_chat_rooms_to_users can store a copy of the current_index_value of the instance that stored the value. Then if erase is called, it will only remove the oid from
map_of_chat_rooms_to_users if it is less than or equal to the calling instances' current_index_value. This also allows each ChatStreamContainerObject to only look at
messages that add chat rooms to the map and not messages that remove them. This cuts down on a fair number of potential problems with detecting when an element should 
be removed from chat_room_ids_user_is_part_of. Instead, during cleanup it will just attempt to remove every chat room it was ever a part of.<br/>

There is also one other edge case worth mentioning here. When the change stream thread receives a kDifferentUserJoined message type, instead of sending to all users except
the sender of the message (where a normal message sends a kNewUpdateTimeMessage), it will send a version of it back to ALL users. This is because otherwise the change stream
thread will add the oid to map_of_chat_rooms_to_users, however the sending user ChatStreamContainerObject instance will not receive it causing an oid to be left dangling
inside map_of_chat_rooms_to_users. The ChatStreamContainerObject will handle this by checking for when this case arises, adding the oid to chat_room_ids_user_is_part_of
and then sending kNewUpdateTimeMessage to the user instead.

### Injecting Messages

The most complicated part of injecting messages is that as soon as the ChatStreamContainerObject instance is added to user_open_chat_streams, it can receive injected messages
at any time. This causes some problems of missing messages (read 'Missing Messages' header for more details). It also causes issues with outstanding references held, this is
exaggerated even more by the use of coroutines. The major issue is that injectStreamResponse() runs on the change stream thread and so it should do minimal (ideally zero, however
it does one at the moment) operations that require blocking. The solution to this is to send it to a thread pool. However, because the ChatStreamContainerObject instance
could clean up and delete itself before the coroutine actually executes, a reference must be held. This means that a shared pointer must be passed by value to the
coroutine. Unsure if this parameter could be changed by the compiler to a const reference and so there is a volatile sink in order to prevent this possibility.

### End Stream Time & Refreshing

In order to guarantee that a ChatStreamContainerObject instance cannot somehow never receive a signal to remove itself each one has an Alarm time_out_stream_alarm. This 
alarm will be set to a specific amount of time and when it expires the ChatStreamContainerObject will call beginFinish(). This alarm time can be refreshed by the client
by sending in a ChatToServerRequest for it (if the refresh request is too close to alarm expiration the request will fail).The ChatStreamContainerObject instance
can technically be refreshed infinitely. However, according to Google [here](https://www.youtube.com/watch?v=Naonb2XD_2Q&t=317s) rpcs being occasionally restarted
has various benefits.<br/>

This means the recommendation is to refresh a limited number of times and then re-connect. For example the android client refreshes three times, then it will
start a new instance.

### Requesting Message Updates

Message updates can be requested in batches. Each request can only send for a single chat room, however multiple messages can be requested. No login info is needed
because that was required when the bidirectional rpc started. Each message requires the message_uuid as well as the 'amount' of message to send back. There are several
features that are required or that the server will guarantee.
* The client will expect a response from this no matter what happens with the message_uuids and
 request_status set. Any request_status outside INTERMEDIATE_MESSAGE_LIST is expected to be the
 last group of messages to be sent. Both the server and client rely on this being true.
* If an error of some kind happens or the user was not inside the chat room
 then simply send back the overall message.
* IMPORTANT: If a UUID was invalid or an error occurred when extracting the message, the
 client expects a message containing the messageUUID as well as the RequestStatus and ReturnStatus.
 EVERY UUID PASSED SHOULD RECEIVE A RESPONSE WITH A RETURN STATUS.<br/>
 Possible Return Values<br/>
 -ReturnStatus::SUCCESS; Everything was extracted as expected.<br/>
 -ReturnStatus::VALUE_NOT_SET; An error occurred with extracting (invalid uuid or message not found).<br/>
 -ReturnStatus::UNKNOWN; Other circumstance, such as too many messages passed in or invalid amount of message.
* If a message was successfully extracted, the user expects the info along with a returnStatus set to SUCCESS.
* Expected to maintain order of messages. The order they are sent in inside ChatToServerRequest is the order they will
 be returned inside ChatToClientResponse.
* Only one 'batch' of messages can be requested per stream at a time represented by a kRequestFullMessageInfo
 type message. Each 'batch' can request a maximum of maximum number of MAX_NUMBER_MESSAGES_USER_CAN_REQUEST
 messages. If another message update request is sent on the same rpc then CURRENTLY_PROCESSING_UPDATE_REQUEST 
 will be returned to the client.

### Stream Ending

There are a few ways for the stream to end. (see 'Final Output' above for more details)
1) An error occurs making the ChatStreamContainerObject instance run beginFinish(). Inside the trailing metadata this 
 will return StreamDownReasons::RETURN_STATUS_ATTACHED. A return status will also be returned showing the error.
2) The alarm time_out_stream_alarm could expire (as opposed to the alarm being canceled). This will return 
 StreamDownReasons::STREAM_TIMED_OUT (see 'End Stream Time & Refreshing' header for more info).
3) If the server is shutting down it will return StreamDownReasons::SERVER_SHUTTING_DOWN. The client will simply need
 to start a new bidirectional rpc with a different server.
4) If a bidirectional rpc is started for User A when another bidirectional rpc is already running it will cancel the
 previous rpc. When this happens StreamDownReasons::STREAM_CANCELED_BY_ANOTHER_STREAM will be returned. Some optional
 info will also be returned with the installation_id of the device that started the new stream. 
5) Other reasons such as the client can call cancel on the rpc. These will usually simply send back 
 StreamDownReasons::UNKNOWN_STREAM_STOP_REASON.

### Read 

The reads represent the internal::AsyncReaderInterface<R>, part of the grpc::ServerAsyncReaderWriterInterface used inside
ChatStreamContainerObject. The major point is that **this interface is NOT ASYNCHRONOUS**. What that means in this context
is that when Read() is called from responder_, ChatStreamContainerObject must wait for Read() (in the form of READ_TAG)
to return from the completion queue in order to call it again.<br/><br/>
Enforcing this for Read() is fairly straightforward by calling Read() at the end of initialization. It is then only
called again when READ_TAG is returned from the completion queue.

### Write

The writes represent the internal::AsyncWriterInterface<W>, part of the grpc::ServerAsyncReaderWriterInterface used inside
ChatStreamContainerObject. The major point is that **this interface is NOT ASYNCHRONOUS**. What that means in this context
is that when Write() is called from responder_, ChatStreamContainerObject must wait for Write() (there are three different
tags related to write) to return from the completion queue in order to call it again.<br/>

Write() is a bit more difficult than Read(). One problem is that when Write() is called it adds the value to the front of the
completion queue instead of the back. Because each ChatStreamContainerObject instance will only have one Write() outstanding
at a time, this will not have an effect the order of messages the client sees. However, it is reasonable to make it at
least a little 'more' fair by sending the new messages to the back of the completion queue. This is important to remember
because all operations for the async server are inside the completion queue. This means that if all clients push every 
Write() to the front of the queue, they will out-compete in a sense commands such as Read() and alarms. In order to overcome
this issue an alarm is used to send each message to the back of the completion queue instead of the front (with the async server
alarms naturally go to the back). This is not true if multiple responses are inside a single writes_waiting_to_process element 
(more on this below). <br/>

Another problem occurs because of writes_waiting_to_process (the local queue for ChatStreamContainerObject). Each element in the
queue can store a vector of response messages. However, only one outstanding Write() (one element of the vector, not the queue) 
can be sent back to the client at a time. In order to handle this the remaining elements are concatenated to the front of
writes_waiting_to_process and will be sent immediately. Normally messages are sent to the back of the completion queue in order
to enforce fairness. This situation is an exception, Write() will be called immediately until the vector elements have been used
up, then the next Write() will be sent to the back of the queue.

There are three TAGS and three functions that each have a unique purpose. It is important to recognize that only ONE of each function
should be running at any given time because the internal::AsyncWriterInterface is not asynchronous.
* FUNCTIONS
1) writeVectorElement<true>() This is called when an element from writes_waiting_to_process has more than one 
 message to send back to the client. It breaks the idea of sending the next Write() to the back of the completion queue. It
 will run the Write() immediately.
2) writeVectorElement<false>() This is the 'basic' way to start write. It will write the next message inside writes_waiting_to_process and
 then if there were extra responses inside the writes_waiting_to_process element it will queue up writeVectorElement<true>(). If 
 there were no extra responses inside the writes_waiting_to_process element it will call runWriteCallbackFunction().
3) runWriteCallbackFunction() This will be called when a writeVectorElement() has sent all
 responses from an element of writes_waiting_to_process. This function will send the next Write() to the back of the completion
 queue in order to promote fairness between ChatStreamContainerObject instances (they share a single completion queue).
* TAGS
1) CallDataCommandTypes::END_OF_VECTOR_WRITES; Signals that a single element of writes_waiting_to_process has had all responses
 sent back to the client. This means that either the writes are complete (writes_waiting_to_process is empty) or the next Write()
 should go to the back of the completion queue to promote fairness.
2) CallDataCommandTypes::WRITE_WITH_VECTOR_ELEMENTS_REMAINING; Signals that a single element of writes_waiting_to_process has responses
 remaining to be sent back to the client. This means that the rest of the responses of the vector need to be written back using 
 Write() as soon as possible.
3) CallDataCommandTypes::ALARM_QUEUE_WRITE_AT_END; Uses an alarm to send the next Write() to the back of the completion queue.

### Outstanding References

This system has a straightforward problem with it. The ChatStreamContainerObject instance is responsible for cleaning itself up.
This means not only cleaning up the chat rooms the user is a part of but the last command inside cleanupObject() is 'delete this;'.
This is an issue because other threads could be holding a reference to this object and attempt to access it after it has been 
deallocated.<br/>

The issue can be broken down into two parts in order to solve it. 
1) The first part is that other threads can request a copy of the ChatStreamContainerObject instance from 
 user_open_chat_streams. Notably the change stream thread will do this. In order to keep track of this a variable named 
 reference_count will be incremented when a request is taken out and decremented when the request goes out of scope (see
 ReferenceWrapper for details). This way the ChatStreamContainerObject instance can prevent new references from being
 created then spin until reference_count hits zero.
2) The second part is the completion queue. There is the possibility that a TAG still exists somewhere inside it that holds a pointer
 to this object. Unlike the above reference_count variable, only a pointer is sent into the queue not a whole object and so a destructor
 constructor combo cannot be relied on. However, there is a point about the completion queue that can be taken advantage of. **Anything
 that goes into the completion queue will be sent back**, except grpc::ServerContext::AsyncNotifyWhenDone which is not used (can look
 at constructor of ChatStreamContainerObject comments for more info on this). This means that variables can be incremented for the
 outstanding commands and the function can spin until all references inside the completion queue have been sent back.

### Finalizing

In order for a ChatStreamContainerObject instance to complete there are several steps that it must always go through.
1) The instance will set service_completed to true. This will not allow anything such as Write() or Read() to start which can cause
 data races if run at the same time as Finish().
2) The instance will then wait until run_writer_atomic is zero. run_writer_atomic will be incremented before service_completed is checked
 and decremented afterwards. Waiting will guarantee that nothing is inside a block that has can interfere with Finish() such as Read()
 or Write(). It will also guarantee that a reference is not in the process of being requested from retrieveReferenceWrapperForCallData().
3) All alarms are canceled, the instance then waits until the alarms have been sent back from the completion queue as well as initialization has
 completed. The alarms must be waited for here because if the completion queue gets an alarm that has completed after Finish() is called, it 
 can crash. This wait will also guarantee that initialize has at least started (reference_count will be incremented to reflect that, so the 
 outstanding reference will be waited for in step 5). Initialization is necessary to prime the AsyncService with the next instance of
 ChatStreamContainerObject.
4) It will call Finish() on the rpc (Finish should only be called once).
5) When FINISH_TAG is returned it will wait for any outstanding references that still require cleaned up. This includes references from
   retrieveReferenceWrapperForCallData() as well as references inside the completion queue (see 'Outstanding References' header for details). 
6) The instance will erase itself from user_open_chat_streams. The element inside user_open_chat_streams will only be erased if its
 stored pointer matches the 'this' value. There is no possibility of a double pointer being stored because every pointer should be removed from
 the data structure before it deletes itself.
7) All messages remaining that were not sent inside writes_waiting_to_process will be iterated through. The purpose of this is to guarantee
 that the local copy of chat room ids 'chat_room_ids_user_is_part_of' for this instance is up-to-date. Any messages that do not update this
 are completely discarded. Because Finish() has already been called, the rpc is dead and no more messages can be sent back.
8) chat_room_ids_user_is_part_of is used to remove any possible chat rooms from map_of_chat_rooms_to_users. If a more recent value of
 current_index_value (a higher number) is stored with the account_oid, this instance will ignore that account_oid and leave it to the 
 more recent instance to clean up.

### Replacing Old Instance With New Instance

A few simple rules for the ChatStreamContainerObject.
* Each user (determined by user_oid) can only have one active ChatStreamContainerObject. As a side note, technically if different physical servers
 are running, each server can have its own instance.
* Each object is responsible to clean up after itself.<br/>

This means that whenever a new ChatStreamContainerObject instance (bidirectional rpc) starts and a previous one is running, something must happen to 
one of them. What happens is that the new instance will overwrite the old instance (the new instance will replace the reference inside 
user_open_chat_streams and call endStream() on the old instance). However, each instance is also responsible for cleaning up after itself. This can
lead to an issue of the old instance cleaning up new instance values. For example say the old instance stores some chat room ids it is responsible
for removing from map_of_chat_rooms_to_users. If the new instance also stores them before the old instance runs clean up, the old instance can
clean up the chat room ids added by the new instance. This will disallow the new instance from receiving messages for any of the chat rooms the 
old instance has removed. In order to avoid this a unique index variable is used for each ChatStreamContainerObject instance named 
current_index_value. This value will increase with each new instance stored inside user_open_chat_streams. This allows the instances to store 
their unique index for each chat room they are responsible for. They can then remove the chat room only if the stored index is less than or equal
to their own index.

### Memory Ordering

Memory order inside a mutex is not an issue (the atomic operations inside the mutex are usually called with
std::memory_order_relaxed). However, there may be potential for memory order problems in the
future, and I have no idea how to guarantee that there won't be (without completely restricting it). It is worth
noting here because if a bizarre bug occurs I may want to check the assembly to see if the compiler optimized in a
bug for the multithreading environment.
