//
// Created by jeremiah on 4/4/21.
//

#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/pipeline.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include <mutex>
#include <condition_variable>
#include <user_open_chat_streams.h>
#include <utility_general_functions.h>
#include <utility_chat_functions.h>
#include <store_mongoDB_error_and_exception.h>
#include <connection_pool_global_variable.h>
#include <build_debug_string_response.h>
#include <chat_room_values.h>
#include <bsoncxx/exception/exception.hpp>

#include "send_messages_implementation.h"
#include "database_names.h"
#include "collection_names.h"
#include "chat_room_shared_keys.h"
#include "chat_stream_container.h"
#include "chat_room_header_keys.h"
#include "chat_stream_container_object.h"

#include "chat_change_stream.h"
#include "chat_change_stream_values.h"
#include "messages_waiting_to_be_sent.h"
#include "chat_change_stream_helper_functions.h"
#include "chat_room_message_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

/** Can read src/utility/async_server/_documentation.md for how this relates to the chat stream. **/

std::atomic_bool continueChangeStream = true;
std::mutex change_stream_mutex;
std::atomic_bool thread_started = false;

long total_chat_change_stream_run_time = 0;

#ifdef LG_TESTING

struct TimeToSendMessagesStats {
    long largest_delay = -1;
    long total_number_messages_sent = 0;
    long total_delay_sending_messages = 0;

    long most_recent_message_sent = -1;
    std::string most_recent_message_uuid = "message_uuid_not_set";

    void store(long delayed_time) {
        if(delayed_time > largest_delay) {
            largest_delay = delayed_time;
        }
        total_delay_sending_messages += delayed_time;
        total_number_messages_sent++;
    }

    void store_most_recent_sent(
            long timestamp_stored,
            const std::string& message_uuid
    ) {
        if(timestamp_stored >= most_recent_message_sent) {
            most_recent_message_sent = timestamp_stored;
            most_recent_message_uuid = message_uuid;
        }
    }
};

inline std::unordered_map<std::string, TimeToSendMessagesStats> time_to_send_message_stats;

inline long largest_negative_gap_between_timestamps = -1;

#endif

void runLoopToSendMessages(
        std::deque<MessageWaitingToBeSent>& messages_waiting_to_be_sent,
        std::mutex& messages_waiting_to_be_sent_mutex,
        std::condition_variable& messages_waiting_to_be_sent_condition_variable,
        std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>>& cached_messages,
        size_t& cached_messages_size_in_bytes
) {

    //Will sleep until it has a new message to check. There is a short sleep after this function as well
    // because these messages must sit inside messages_waiting_to_be_sent for DELAY_FOR_MESSAGE_ORDERING
    // amount of time. However, if messages_waiting_to_be_sent.empty()==true then no reason for it to
    // spin.
    std::unique_lock<std::mutex> messages_lock(messages_waiting_to_be_sent_mutex);
    messages_waiting_to_be_sent_condition_variable.wait(
            messages_lock,
            [&messages_waiting_to_be_sent] {
                return !messages_waiting_to_be_sent.empty() || !continueChangeStream.load(std::memory_order_relaxed);
            }
    );

    //messages_lock needs to be locked when the loop condition is checked.
    while(!messages_waiting_to_be_sent.empty()) {

        try {
            std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

            //Can check .front() w/o checking .empty() because this is the only thread that removes from messages_waiting_to_be_sent.
            if ((current_timestamp - messages_waiting_to_be_sent.front().time_received) > chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING) {

                MessageWaitingToBeSent message_info = std::move(messages_waiting_to_be_sent.front());
                messages_waiting_to_be_sent.pop_front();

                //Unlock after the front element has been removed, this way the change stream thread can add values to messages_waiting_to_be_sent.
                messages_lock.unlock();

                ChatMessageToClient* responseMsg;

                if (!message_info.chat_to_client_response->has_return_new_chat_message()
                    || message_info.chat_to_client_response->return_new_chat_message().messages_list().empty()) {

                    const std::string error_string = "An empty message was passed to orderAndSendMessagesAfterDelay()\n";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "chat_room_id", message_info.chat_room_id,
                            "message_string",
                            message_info.chat_to_client_response->DebugString()
                    );

                    continue;
                }
                else {
                    //NOTE: This is pulled out again if a copy is made inside sendMessageToUsers() for a kDifferentUserJoinedMessage.
                    responseMsg = message_info.chat_to_client_response->mutable_return_new_chat_message()->mutable_messages_list(
                            0);
                }

                bool sentMessage = false;

                addOrRemoveChatRoomIdByMessageType(
                        *responseMsg,
                        [&](const std::string& account_oid) {

                            //Must get a reference to the user BEFORE inserting the chat room. Otherwise, the user could end in between
                            // insertUserOIDToChatRoomId() and sendMessageToUsers() and leak an object.
                            std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> found_chat_stream_value = user_open_chat_streams.find(
                                    account_oid);

                            //insert must come BEFORE so the user will be added from the map_of_chat_rooms_to_users THEN get the message.
                            if (found_chat_stream_value != nullptr) {
                                insertUserOIDToChatRoomId(
                                        message_info.chat_room_id,
                                        responseMsg->sent_by_account_id(),
                                        found_chat_stream_value->ptr()->getCurrentIndexValue()
                                );
                            }
#ifdef LG_TESTING
                            long time_to_send_message =
                                    getCurrentTimestamp().count() - message_info.time_received.count();
                            if (!isInvalidChatRoomId(message_info.chat_room_id)) {
                                time_to_send_message_stats[message_info.chat_room_id].store(time_to_send_message);
                                time_to_send_message_stats[message_info.chat_room_id].store_most_recent_sent(
                                        message_info.time_message_stored.count(),
                                        message_info.message_uuid);
                            }
#endif

                            sendMessageToUsers(
                                    message_info.chat_room_id,
                                    message_info.chat_to_client_response,
                                    responseMsg,
                                    cached_messages
                            );

                            sentMessage = true;
                        },
                        [&](const std::string& account_oid) {

                            //Must get a reference to the user BEFORE sending the message, otherwise the user could end in between
                            // sendMessageToUsers() and eraseUserOIDFromChatRoomId() and leak an object.
                            std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> found_chat_stream_value = user_open_chat_streams.find(
                                    account_oid);
#ifdef LG_TESTING
                            long time_to_send_message =
                                    getCurrentTimestamp().count() - message_info.time_received.count();
                            if (!isInvalidChatRoomId(message_info.chat_room_id)) {
                                time_to_send_message_stats[message_info.chat_room_id].store(time_to_send_message);
                                time_to_send_message_stats[message_info.chat_room_id].store_most_recent_sent(
                                        message_info.time_message_stored.count(),
                                        message_info.message_uuid);
                            }
#endif

                            sendMessageToUsers(
                                    message_info.chat_room_id,
                                    message_info.chat_to_client_response,
                                    responseMsg,
                                    cached_messages
                            );

                            //Must erase AFTER the message is sent or the user will not get the message because they were removed.
                            if (found_chat_stream_value != nullptr) {
                                eraseUserOIDFromChatRoomId(message_info.chat_room_id, account_oid,
                                                           found_chat_stream_value->ptr()->getCurrentIndexValue());
                            }
                            sentMessage = true;

                        }
                );

                //if message did not require adding or removing
                if (!sentMessage) {

#ifdef LG_TESTING
                    long time_to_send_message = getCurrentTimestamp().count() - message_info.time_received.count();
                    if (!isInvalidChatRoomId(message_info.chat_room_id)) {
                        time_to_send_message_stats[message_info.chat_room_id].store(time_to_send_message);
                        time_to_send_message_stats[message_info.chat_room_id].store_most_recent_sent(
                                message_info.time_message_stored.count(),
                                message_info.message_uuid);
                    }
#endif

                    sendMessageToUsers(
                            message_info.chat_room_id,
                            message_info.chat_to_client_response,
                            responseMsg,
                            cached_messages
                    );
                }

                //insert an empty deque for this chat room id if one does not exist
                auto cached_insert_result = cached_messages.insert(
                        std::pair<std::string, std::deque<MessageWaitingToBeSent>>(
                                message_info.chat_room_id,
                                std::deque<MessageWaitingToBeSent>()
                        )
                );

                if (cached_insert_result.second) { //insert was successful
                    cached_messages_size_in_bytes += chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
                }

                std::deque<MessageWaitingToBeSent>& messages_reference = cached_insert_result.first->second;

                //iterators tend to be faster than random access in a deque
                auto first_smaller_iterator_position = messages_reference.rbegin();
                for (;first_smaller_iterator_position != messages_reference.rend(); ++first_smaller_iterator_position) {
                    //want these ordered by timestamp_stored(time_message_stored) NOT time_time_received
                    //do not calculate message_uuid into this, if a message was a duplicate timestamp, allow this to handle
                    // any situations where one message relies on the other
                    if (first_smaller_iterator_position->time_message_stored < message_info.time_message_stored) {
                        break;
                    }
                }

                if (first_smaller_iterator_position == messages_reference.rbegin()) { //final element
                    cached_messages_size_in_bytes += message_info.current_message_size;

                    messages_reference.emplace_back(std::move(message_info));
                }
                else { //not final element

                    //Can think of iterators as 'between' the elements. A normal iterator (forward iterator) points forward, a reverse iterator points backwards.
                    // For example : list{4,5,6,7,8}; If I do list.begin() it is before the 4, so it points to 4 when de-referenced. However, a
                    // reverse iterator will point backwards and so it will point to list.rend(). It is important to understand this when calling
                    // .base() to convert a reverse iterator to a forward iterator.
                    const std::_Deque_iterator<MessageWaitingToBeSent, MessageWaitingToBeSent &, MessageWaitingToBeSent *> iterator_pos_to_insert_at = first_smaller_iterator_position.base();

                    iterateAndSendMessagesToTargetUsers(
                            message_info,
                            messages_reference,
                            iterator_pos_to_insert_at
                    );

                    cached_messages_size_in_bytes += message_info.current_message_size;

                    messages_reference.insert(
                            iterator_pos_to_insert_at,
                            std::move(message_info)
                    );
                }

                //refresh current timestamp after processing
                current_timestamp = getCurrentTimestamp();

                removeCachedMessagesOverMaxTime(
                        cached_messages,
                        cached_messages_size_in_bytes,
                        current_timestamp
                );

                removeCachedMessagesOverMaxBytes(
                        cached_messages,
                        cached_messages_size_in_bytes
                );

            }
            else {
                //Stop when time_received + global_value has not been reached yet. It is possible more
                // messages exist that have time_received available. However, if they are farther down
                // the list then time_message_stored is also later, and so they need to be delayed until
                // the front message time has been reached to be sent (for ordering purposes).
                break;
            }
        }
        catch (const mongocxx::exception& e) {
            //element that caused the error is removed, continue

            const std::string errorString = "An exception was thrown by a message when orderAndSendMessagesAfterDelay() was running.\n";

            storeMongoDBErrorAndException(
                __LINE__, __FILE__,
                std::optional<std::string>(), errorString,
                "messages_waiting_to_be_sent.size()", std::to_string(messages_waiting_to_be_sent.size()),
                "cached_messages.size()", std::to_string(cached_messages.size()),
                "cached_messages_size_in_bytes", std::to_string(cached_messages_size_in_bytes)
            );
        }

        //The lock needs to be locked for the while loop condition check.
        messages_lock.lock();
    }

}

//There is a small chance that messages returned from the chat change stream are not ordered
// by timestamp. The differences in time should be relatively minor. However,
void orderAndSendMessagesAfterDelay(
        std::deque<MessageWaitingToBeSent>& messages_waiting_to_be_sent,
        std::mutex& messages_waiting_to_be_sent_mutex,
        std::condition_variable& messages_waiting_to_be_sent_condition_variable
        ) {

    std::unordered_map<std::string, std::deque<MessageWaitingToBeSent>> cached_messages;
    size_t cached_messages_size_in_bytes = sizeof(cached_messages); // NOLINT(bugprone-sizeof-container)

    while(continueChangeStream.load(std::memory_order_relaxed)) {
        runLoopToSendMessages(
                messages_waiting_to_be_sent,
                messages_waiting_to_be_sent_mutex,
                messages_waiting_to_be_sent_condition_variable,
                cached_messages,
                cached_messages_size_in_bytes
        );
        std::this_thread::sleep_for(chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME);
    }

    long largest_delay = -1;
    long total_number_messages_sent = 0;
    long total_delay_sending_messages = 0;

#ifdef LG_TESTING
    for(const auto& val : time_to_send_message_stats) {
        if(val.second.largest_delay > largest_delay) {
            largest_delay = val.second.largest_delay;
        }
        total_number_messages_sent += val.second.total_number_messages_sent;
        total_delay_sending_messages += val.second.total_delay_sending_messages;
    }
#endif

    int total_messages_cached = 0;
    long final_cached_messages_size_in_bytes = sizeof(cached_messages);
    for(const auto& x : cached_messages) {
        final_cached_messages_size_in_bytes += chat_change_stream_values::SIZE_OF_MESSAGES_DEQUE_ELEMENT;
        for(const auto& y : x.second) {
            final_cached_messages_size_in_bytes += sizeof(MessageWaitingToBeSent);
            final_cached_messages_size_in_bytes += (long)y.chat_room_id.size();
            final_cached_messages_size_in_bytes += (long)y.message_uuid.size();
            final_cached_messages_size_in_bytes += (long)y.chat_to_client_response->ByteSizeLong();
        }
        total_messages_cached += (int)x.second.size();
    }

    std::stringstream ss;

    int num_bytes = final_cached_messages_size_in_bytes % 1024;

    final_cached_messages_size_in_bytes /= 1024;

    int num_kb = final_cached_messages_size_in_bytes % 1024;

    final_cached_messages_size_in_bytes /= 1024;

    ss
        << "calculated_cached_messages size: " << final_cached_messages_size_in_bytes << "Mb " << num_kb << "Kb " << num_bytes << "bytes\n";

    num_bytes = cached_messages_size_in_bytes % 1024;

    cached_messages_size_in_bytes /= 1024;

    num_kb = cached_messages_size_in_bytes % 1024;

    cached_messages_size_in_bytes /= 1024;

    ss
        << "running_cached_messages size: " << cached_messages_size_in_bytes << "Mb " << num_kb << "Kb " << num_bytes << "bytes\n";

    ss
        << "send_messages_largest_delay: " << largest_delay << " ms\n"
        << "send_messages_average_delay: " << (total_number_messages_sent==0? 0 : total_delay_sending_messages/total_number_messages_sent) << " ms\n"
        << "total_delay_sending_messages: " << total_delay_sending_messages << " ms\n"
        << "total_number_messages_sent: " << total_number_messages_sent << "\n"
        << "messages_waiting_to_be_sent.size() when cancelled: " << messages_waiting_to_be_sent.size() << '\n'
        << "cached_messages num chat rooms when cancelled: " << cached_messages.size() << '\n'
        << "cached_messages num messages when cancelled: " << total_messages_cached << '\n';

#ifdef LG_TESTING
    time_to_send_message_stats.clear();
#endif

    std::cout << ss.str() << std::flush;
}

void beginChatChangeStream() {

    continueChangeStream = true;

    //This is just used to check if continueChangeStream has been set. A different boolean
    // is used to avoid making continueChangeStream atomic.
    thread_started = true;

    //NOTE: Conceptually an unordered_map<std::string, std::deque<MessageWaitingToBeSent>> could be used for batching
    // messages together by chat room id. However, there is a problem that when accounts must be added or removed
    // (insertUserOIDToChatRoomId() or eraseUserOIDFromChatRoomId()), the references could need to be held
    // for a relatively long time. Also, the messages must be ordered, yet eraseUserOIDFromChatRoomId() must be
    // called after sending the message.
    //NOTE: Expects values to be inserted in order.
    std::deque<MessageWaitingToBeSent> messages_waiting_to_be_sent;
    std::mutex messages_waiting_to_be_sent_mutex;
    std::condition_variable messages_waiting_to_be_sent_condition_variable;

#ifdef LG_TESTING
    struct TimestampTestingStuff {
        long latest_timestamp = -1;
        long largest_negative_gap_between_timestamps = -1;
        long num_messages = 0;
        long num_messages_out_of_order = 0;
        long total_gap = 0;

        long largest_time_to_get_back_to_change_stream = -1;
        long time_to_get_back_to_change_stream = 0;

        void process(long passed_timestamp) {
            num_messages++;
            if(passed_timestamp > latest_timestamp) {
                latest_timestamp = passed_timestamp;
            } else {
                num_messages_out_of_order++;
                long gap = latest_timestamp - passed_timestamp;
                if(gap > largest_negative_gap_between_timestamps) {
                    largest_negative_gap_between_timestamps = gap;
                }
                total_gap += gap;
            }
            long temp_time_to_get_back_to_change_stream = getCurrentTimestamp().count() - passed_timestamp;
            time_to_get_back_to_change_stream += temp_time_to_get_back_to_change_stream;
            if(temp_time_to_get_back_to_change_stream > largest_time_to_get_back_to_change_stream) {
                largest_time_to_get_back_to_change_stream = temp_time_to_get_back_to_change_stream;
            }
        }
    };

    std::map<std::string, TimestampTestingStuff> timestamp_testing_stuff;
#endif

    std::jthread order_messages_thread(
            orderAndSendMessagesAfterDelay,
            std::ref(messages_waiting_to_be_sent),
            std::ref(messages_waiting_to_be_sent_mutex),
            std::ref(messages_waiting_to_be_sent_condition_variable)
    );

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database chatRoomDB = mongoCppClient[database_names::CHAT_ROOMS_DATABASE_NAME];

    mongocxx::pipeline pipe;

    pipe.match(
        document{}

            << "operationType" << "insert"
            << "$expr" << open_document

                //make sure that this was inserted to a chat_room_ collection
                //$substrCP will return an empty string if index 0 is too large (the command will not segfault).
                // It will also bring the size down to the length of the string if it is too long. So this
                // will not error even with odd inputs.
                //NOTE: The $and statement inside mongoDB does not short circuit. If these protections were not
                // here it would not be valid to check the string length before running this.
                << "$eq" << open_array
                    << collection_names::CHAT_ROOM_ID_
                    << open_document
                        << "$substrCP" << open_array
                              << "$ns.coll"
                              << 0
                              << (int)collection_names::CHAT_ROOM_ID_.size()
                        << close_array
                    << close_document
                << close_array

            << close_document
        << finalize
    );

    pipe.project(
        document{}
            << "fullDocument" << open_document
                << "$cond" << open_document

                    //if: document is the chat room header
                    << "if" << open_document
                        << "$eq" << open_array
                            << "$fullDocument._id" << chat_room_header_keys::ID
                        << close_array
                    << close_document

                    //then: only project the OIDs, the matching array and the last active time
                    << "then" << open_document
                        << "_id" << "$fullDocument._id"
                        << chat_room_header_keys::MATCHING_OID_STRINGS << "$fullDocument." + chat_room_header_keys::MATCHING_OID_STRINGS
                        << chat_room_shared_keys::TIMESTAMP_CREATED << "$fullDocument." + chat_room_shared_keys::TIMESTAMP_CREATED
//                      << ACCOUNTS_IN_CHAT_ROOM << open_document
//                          << "$map" << open_document
//                              << "input" << "$fullDocument." + ACCOUNTS_IN_CHAT_ROOM
//                              << "as" << "userInfo"
//                              << "in" << "$$userInfo." + ACCOUNT_OID
//                          << close_document
//                      << close_document
                    << close_document

                    //else: project the entire message
                    << "else" << "$fullDocument"

                    << close_document
                << close_document

            //project the collection name (includes chatRoomId)
            << "coll" << "$ns.coll"
            << "operationType" << 1
        << finalize
    );

    std::cout << "Chat change stream initialized." << std::endl;;

    const std::chrono::seconds begin_change_stream_start_time = std::chrono::duration_cast<std::chrono::seconds>(getCurrentTimestamp());

    bsoncxx::builder::basic::document resumeToken;
    bsoncxx::document::view resumeTokenView;
    while (continueChangeStream.load(std::memory_order_relaxed)) {
#ifdef LG_TESTING
        std::chrono::milliseconds start_timestamp = getCurrentTimestamp();
#endif
        std::unique_lock<std::mutex> lock(change_stream_mutex);

        //The memory order on the lock will prevent this from being moved before it.
        if(!continueChangeStream.load(std::memory_order_relaxed)) {
            break;
        }

        mongocxx::options::change_stream options;

        // Time until the inner loop finishes (this means it can happen sooner). This time can also be waited
        // for before the change stream can be shut down
        const std::chrono::milliseconds await_time{chat_change_stream_values::CHAT_CHANGE_STREAM_AWAIT_TIME};
        options.max_await_time(await_time);
        if (!resumeTokenView.empty()) {
            //options.resume_after() does not allow the stream to restart after an 'invalidate' event
            // options.start_after() does.
            options.start_after(resumeTokenView);
        } else {
            //If the initial operation time is not specified, then it can get into an odd loop where a message is sent while
            // it is sleeping during CHAT_CHANGE_STREAM_SLEEP_TIME. Then it comes back to the top and starts at the end missing
            // any messages that may have been sent during that time (assuming it does not have a resumeToken yet).
            options.start_at_operation_time(bsoncxx::types::b_timestamp{1, (uint32_t)begin_change_stream_start_time.count()});
        }

        static const size_t CHAT_ROOM_ID_SIZE = collection_names::CHAT_ROOM_ID_.length();
        static const size_t CHAT_ROOM_COLLECTION_MIN_SIZE = CHAT_ROOM_ID_SIZE + chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS - 1;
        static const size_t CHAT_ROOM_COLLECTION_MAX_SIZE = CHAT_ROOM_ID_SIZE + chat_room_values::CHAT_ROOM_ID_NUMBER_OF_DIGITS;
        mongocxx::change_stream change_stream = chatRoomDB.watch(pipe, options);

        try {

#ifdef LG_TESTING
            total_chat_change_stream_run_time += getCurrentTimestamp().count() - start_timestamp.count();
#endif
            //this loop breaks after 'chat_change_stream_await_time' millis
            for (const bsoncxx::document::view& event : change_stream) {
#ifdef LG_TESTING
                start_timestamp = getCurrentTimestamp();
#endif
                bsoncxx::stdx::optional<bsoncxx::document::view> resume_token = change_stream.get_resume_token();

                //The resume token is passed inside a bsoncxx::document::view which is a pointer, it goes out of scope
                // if the outer loop here loops, so storing the data here.
                //Make sure to get token before doing anything that could cause an exception. This way the document will
                // be skipped after the exception is caught.
                resumeToken.clear();
                resumeToken.append(bsoncxx::builder::concatenate_doc{resume_token.value()});
                resumeTokenView = resumeToken.view();

                if(!event["coll"]
                    || event["coll"].type() != bsoncxx::type::k_utf8
                    || !event["fullDocument"]
                    || event["fullDocument"].type() != bsoncxx::type::k_document) {
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(),
                            std::string("An invalid document was returned from change stream."),
                            "doc", event
                    );
                    continue;
                }

                std::string chat_room_id = event["coll"].get_string().value.to_string();

                if(chat_room_id.size() < CHAT_ROOM_COLLECTION_MIN_SIZE || CHAT_ROOM_COLLECTION_MAX_SIZE < chat_room_id.size()) {
                    const std::string error_string = "The extracted chat room Id was an invalid size.";
                    storeMongoDBErrorAndException(
                            __LINE__, __FILE__,
                            std::optional<std::string>(), error_string,
                            "chatRoomId", chat_room_id,
                            "doc", event
                    );

                    continue;
                }

                //extract only the chat room Id
                chat_room_id.erase(0, CHAT_ROOM_ID_SIZE);

                //extract the document itself
                bsoncxx::document::view full_document = event["fullDocument"].get_document();
                std::string document_id;
                auto documentIdElement = full_document["_id"];
                if (documentIdElement &&
                        documentIdElement.type() == bsoncxx::type::k_utf8
                        ) { //if element exists and is type utf8
                    document_id = documentIdElement.get_string().value.to_string();
                }
                else { //if element does not exist or is not type utf8 or null
                    logElementError(__LINE__, __FILE__,
                                    documentIdElement,
                                    full_document, bsoncxx::type::k_utf8, "_id",
                                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id);

                    continue;
                }

                if(document_id != chat_room_header_keys::ID) { //if document is a message
                    std::shared_ptr<grpc_stream_chat::ChatToClientResponse> chatToClientResponse =
                            std::make_shared<grpc_stream_chat::ChatToClientResponse>();

                    ChatMessageToClient* responseMsg = chatToClientResponse->mutable_return_new_chat_message()->add_messages_list();

                    //dummy_oid is acceptable here because the only thing of value the
                    // oid is used for is checking if the message is deleted and if the
                    // message was just sent it cannot be deleted yet
                    //sending back ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE because any message that
                    // is an 'active' message type will be the latest message inside the chat
                    // room
                    //this will not run any database access, pictures message only returns the
                    // picture oid and differentUserJoinedChatRoom will not do a database access
                    // if extractUserInfoObjects == nullptr
                    if (!convertChatMessageDocumentToChatMessageToClient(
                            full_document, chat_room_id,
                            chat_stream_container::CHAT_CHANGE_STREAM_PASSED_STRING_TO_CONVERT, false, responseMsg,
                            AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE,
                            DifferentUserJoinedChatRoomAmount::SKELETON,
                            false, nullptr)
                            ) {
                        continue;
                    }

                    bool isDeleted = false;

                    switch(responseMsg->message().message_specifics().message_body_case()) {
                        case MessageSpecifics::kTextMessage:
                            isDeleted = responseMsg->message().message_specifics().text_message().active_message_info().is_deleted();
                            break;
                        case MessageSpecifics::kPictureMessage:
                            isDeleted = responseMsg->message().message_specifics().picture_message().active_message_info().is_deleted();
                            break;
                        case MessageSpecifics::kLocationMessage:
                            isDeleted = responseMsg->message().message_specifics().location_message().active_message_info().is_deleted();
                            break;
                        case MessageSpecifics::kMimeTypeMessage:
                            isDeleted = responseMsg->message().message_specifics().mime_type_message().active_message_info().is_deleted();
                            break;
                        case MessageSpecifics::kInviteMessage:
                            isDeleted = responseMsg->message().message_specifics().invite_message().active_message_info().is_deleted();
                            break;

                        case MessageSpecifics::kEditedMessage:
                        case MessageSpecifics::kDeletedMessage:
                        case MessageSpecifics::kUserKickedMessage:
                        case MessageSpecifics::kUserBannedMessage:
                        case MessageSpecifics::kDifferentUserJoinedMessage:
                        case MessageSpecifics::kDifferentUserLeftMessage:
                        case MessageSpecifics::kUpdateObservedTimeMessage:
                        case MessageSpecifics::kThisUserJoinedChatRoomStartMessage:
                        case MessageSpecifics::kThisUserJoinedChatRoomMemberMessage:
                        case MessageSpecifics::kThisUserJoinedChatRoomFinishedMessage:
                        case MessageSpecifics::kThisUserLeftChatRoomMessage:
                        case MessageSpecifics::kUserActivityDetectedMessage:
                        case MessageSpecifics::kChatRoomNameUpdatedMessage:
                        case MessageSpecifics::kChatRoomPasswordUpdatedMessage:
                        case MessageSpecifics::kNewAdminPromotedMessage:
                        case MessageSpecifics::kNewPinnedLocationMessage:
                        case MessageSpecifics::kChatRoomCapMessage:
                        case MessageSpecifics::kMatchCanceledMessage:
                        case MessageSpecifics::kNewUpdateTimeMessage:
                        case MessageSpecifics::kHistoryClearedMessage:
                        case MessageSpecifics::kLoadingMessage:
                        case MessageSpecifics::MESSAGE_BODY_NOT_SET:
                            break;
                    }

                    if(isDeleted) {
                        const std::string error_string = "Change stream function received a deleted message. It should ONLY receive inserted messages.\n";
                        storeMongoDBErrorAndException(
                                __LINE__, __FILE__,
                                std::optional<std::string>(), error_string,
                                "message_received", buildDebugStringResponse(responseMsg)
                                );
                        continue;
                    }

#ifdef LG_TESTING
                    if(!isInvalidChatRoomId(responseMsg->message().standard_message_info().chat_room_id_message_sent_from())
                        && responseMsg->timestamp_stored() > 0
                            ) {
                        timestamp_testing_stuff[responseMsg->message().standard_message_info().chat_room_id_message_sent_from()].process(responseMsg->timestamp_stored());
                    }
#endif

                    const MessageSpecifics::MessageBodyCase message_type = responseMsg->message().message_specifics().message_body_case();

                    std::vector<MessageTarget> message_targets;

                    const AddOrRemove add_or_remove_user = addOrRemoveUserBasedOnMessageType(message_type);

                    switch (add_or_remove_user) {
                        case MESSAGE_TARGET_NOT_SET:
                            break;
                        case MESSAGE_TARGET_ADD:
                            message_targets.emplace_back(
                                add_or_remove_user,
                                responseMsg->sent_by_account_id()
                            );
                            break;
                        case MESSAGE_TARGET_REMOVE:
                            switch(message_type) {
                                case MessageSpecifics::kUserKickedMessage:
                                    message_targets.emplace_back(
                                        add_or_remove_user,
                                        responseMsg->message().message_specifics().user_kicked_message().kicked_account_oid()
                                    );
                                    break;
                                case MessageSpecifics::kUserBannedMessage:
                                    message_targets.emplace_back(
                                        add_or_remove_user,
                                        responseMsg->message().message_specifics().user_banned_message().banned_account_oid()
                                    );
                                    break;
                                case MessageSpecifics::kDifferentUserLeftMessage:
                                    message_targets.emplace_back(
                                        add_or_remove_user,
                                        responseMsg->sent_by_account_id()
                                    );
                                    break;
                                case MessageSpecifics::kMatchCanceledMessage:
                                    message_targets.emplace_back(
                                        add_or_remove_user,
                                        responseMsg->sent_by_account_id()
                                    );
                                    message_targets.emplace_back(
                                        add_or_remove_user,
                                        responseMsg->message().message_specifics().match_canceled_message().matched_account_oid()
                                    );
                                    break;
                                default:
                                    break;
                            }
                            break;
                    }

                    //If this message is from the user swiping yes on an event, send back the event chat room to them.
                    //This is set up here to avoid the standard delay of the stream.
                    if(message_type == MessageSpecifics::kDifferentUserJoinedMessage) {

                        bsoncxx::document::view message_specifics_document;

                        const auto message_specifics_document_element = full_document[chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT];
                        if (message_specifics_document_element
                            && message_specifics_document_element.type() == bsoncxx::type::k_document) { //if element exists and is type document
                            message_specifics_document = message_specifics_document_element.get_document().value;
                        } else { //if element does not exist or is not type document
                            logElementError(
                                    __LINE__, __FILE__,
                                    message_specifics_document_element, full_document,
                                    bsoncxx::type::k_document, chat_room_message_keys::MESSAGE_SPECIFICS_DOCUMENT,
                                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                            );
                            continue;
                        }

                        const auto user_joined_from_event_element = message_specifics_document[chat_room_message_keys::message_specifics::USER_JOINED_FROM_EVENT];
                        if(user_joined_from_event_element) {

                            std::cout << "contains USER_JOINED_FROM_EVENT element" << std::endl;

                            //Keep this reference alive while sending the message. Otherwise, the ChatStreamContainerObject could finish
                            // in between insertUserOIDToChatRoomId and sending the message.
                            std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> found_chat_stream_values = user_open_chat_streams.find(responseMsg->sent_by_account_id());

                            if (found_chat_stream_values != nullptr) { //if foundChatStreamValues exists in map
                                insertUserOIDToChatRoomId(
                                        chat_room_id,
                                        responseMsg->sent_by_account_id(),
                                        found_chat_stream_values->ptr()->getCurrentIndexValue()
                                );
                            }

                            downloadChatRoomForSpecificUser(
                                chat_room_id,
                                std::chrono::milliseconds{-1L},
                                responseMsg->sent_by_account_id(),
                                true
                            );

                            if (user_joined_from_event_element.type() != bsoncxx::type::k_string) { //if element is not type string
                                logElementError(
                                    __LINE__, __FILE__,
                                    user_joined_from_event_element, message_specifics_document,
                                    bsoncxx::type::k_int32, chat_room_message_keys::message_specifics::USER_JOINED_ACCOUNT_STATE,
                                    database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                                );

                                //NOTE: Ok to continue here in order to send back the message to other users.
                            }
                        }
                    }

                    std::scoped_lock<std::mutex> messages_lock(messages_waiting_to_be_sent_mutex);

                    std::chrono::milliseconds time_received = getCurrentTimestamp();

                    std::chrono::milliseconds time_response_stored{responseMsg->timestamp_stored()};
                    auto message_to_be_sent = MessageWaitingToBeSent(
                            chat_room_id,
                            responseMsg->message_uuid(),
                            message_type,
                            std::move(chatToClientResponse),
                            time_response_stored,
                            time_received,
                            std::move(message_targets)
                    );

                    //Inserting values in a sorted order is significantly faster than running std::sort() inside
                    // order_messages_thread.
                    if(messages_waiting_to_be_sent.empty()
                        || messages_waiting_to_be_sent.back().time_message_stored < time_response_stored) {
                        messages_waiting_to_be_sent.emplace_back(std::move(message_to_be_sent));
                    } else {
                        bool message_inserted = false;

                        //iterating across a deque is faster if using the iterator itself
                        for (auto it = messages_waiting_to_be_sent.rbegin(); it != messages_waiting_to_be_sent.rend(); ++it) {
                            if (it->time_message_stored < time_response_stored
                                || (it->time_message_stored == time_response_stored
                                    && it->message_uuid < responseMsg->message_uuid())
                                    ) {
                                messages_waiting_to_be_sent.insert(
                                        it.base(),
                                        std::move(message_to_be_sent)
                                );
                                message_inserted = true;
                                break;
                            }
                        }

                        if(!message_inserted) {
                            //NOTE: There is a suppressed warning here. However, if message_to_be_sent is moved message_inserted
                            // will be true. So the warning is irrelevant.
                            messages_waiting_to_be_sent.emplace_front(std::move(message_to_be_sent)); // NOLINT(bugprone-use-after-move)
                        }
                    }

                    messages_waiting_to_be_sent_condition_variable.notify_one();

                }
                else { //if document is a header

                    //NOTE: The header document had most of its information trimmed.

                    //This block means that the chat room was just created and the first user (or users in case of
                    // a match) were put inside

                    bsoncxx::array::view match_made_array;

                    const auto matching_oid_string_element = full_document[chat_room_header_keys::MATCHING_OID_STRINGS];
                    if (matching_oid_string_element
                        && matching_oid_string_element.type() == bsoncxx::type::k_array
                            ) { //if element exists and is type utf8
                        match_made_array = matching_oid_string_element.get_array().value;
                    } else if (matching_oid_string_element
                                && matching_oid_string_element.type() != bsoncxx::type::k_null
                            ) { //if element does not exist or is not type utf8 or null
                        logElementError(
                            __LINE__, __FILE__,
                            matching_oid_string_element, full_document,
                            bsoncxx::type::k_array, chat_room_header_keys::MATCHING_OID_STRINGS,
                            database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                        );
                        continue;
                    }

                    //All new headers are sent to the change stream, only a few
                    if (!match_made_array.empty()) { //if this is a match made chat room

                        std::chrono::milliseconds match_created_time;

                        const auto created_time_element = full_document[chat_room_shared_keys::TIMESTAMP_CREATED];
                        if (created_time_element
                            && created_time_element.type() == bsoncxx::type::k_date) { //if element exists and is type array
                            match_created_time = created_time_element.get_date().value;
                        } else { //if element does not exist or is not type array
                            logElementError(
                                __LINE__, __FILE__,
                                created_time_element, full_document,
                                bsoncxx::type::k_date, chat_room_shared_keys::TIMESTAMP_CREATED,
                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id
                            );

                            continue;
                        }

                        std::vector<std::string> match_user_oids;

                        for (const auto& ele : match_made_array) {
                            if (ele.type() == bsoncxx::type::k_utf8) {
                                match_user_oids.emplace_back(ele.get_string().value.to_string());
                            } else {
                                logElementError(__LINE__, __FILE__,
                                                matching_oid_string_element,
                                                full_document, bsoncxx::type::k_utf8,
                                                chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM,
                                                database_names::CHAT_ROOMS_DATABASE_NAME, collection_names::CHAT_ROOM_ID_ + chat_room_id);

                                continue;
                            }
                        }

                        for (const auto& oid : match_user_oids) {
                            //NOTE: these messages are only used to pass information to the ChatStreamObject
                            // that it needs to run a function, the message itself is not sent to anyone

                            //Keep this reference alive while sending the message. Otherwise, the ChatStreamContainerObject could finish
                            // in between insertUserOIDToChatRoomId and sending the message.
                            std::shared_ptr<ReferenceWrapper<ChatStreamContainerObject>> found_chat_stream_values = user_open_chat_streams.find(oid);

                            if (found_chat_stream_values != nullptr) { //if foundChatStreamValues exists in map
                                insertUserOIDToChatRoomId(
                                        chat_room_id,
                                        oid,
                                        found_chat_stream_values->ptr()->getCurrentIndexValue()
                                );
                            }

                            downloadChatRoomForSpecificUser(
                                chat_room_id,
                                match_created_time,
                                oid,
                                false
                            );
                        }

                        if (match_user_oids.size() != 2) {

                            std::string errorString = "A chat room was detected as 'match_made' however there were not 2 userOIDs inside the array.\n";
                            errorString += "MatchUserOIDs Vector:\n";
                            for (size_t i = 0; i < match_user_oids.size(); i++) {
                                errorString += "index " + std::to_string(i) + ": ";
                                errorString += match_user_oids[i] + '\n';
                            }
                            errorString += '\n';

                            storeMongoDBErrorAndException(
                                    __LINE__, __FILE__,
                                    std::optional<std::string>(), errorString,
                                    "document", full_document
                            );

                            //NOTE: need to continue here to avoid memory leak if a chat room Id is not saved to chatRoomIdsUserIsPartOf
                        }
                    }
                }

                //NOTE: the responseMessage was moved into the ChatMessageToClientWrapper so don't try to use it
#ifdef LG_TESTING
                total_chat_change_stream_run_time += getCurrentTimestamp().count() - start_timestamp.count();
#endif
            }
        }
        catch (const mongocxx::exception& e) {
            std::cout << "Chat change stream mongocxx::exception: " << e.what() << '\n';
            std::string exception_code = e.code().message();
            int exception_value = e.code().value();

            //if an exception with the resume token occurs, clear it and continue
            // missing a few messages is better than the change stream permanently stopping
            if (exception_code == "generic server error"
                && (exception_value == 9
                    || exception_value == 280
                    || exception_value == 40649
                    || exception_value == 50811)
            ) {
                resumeToken.clear();
                resumeTokenView = resumeToken;

                //empty resume token
//            Exception e.what(): invalid empty resume token: generic server error
//            Exception code.message: generic server error
//            Exception code.value: 40649
//            Can replicate this by sending in "" as the resume token

                //invalid hex string
//            Exception e.what(): resume token string was not a valid hex string: generic server error
//            Exception code.message: generic server error
//            Exception code.value: 9
//            Can replicate this by adding or removing a char to the hex string

                //token not found
//            Exception e.what(): cannot resume stream; the resume token was not found. {_data: "8260687B2D000000022B022C0100296E5A100420EA7287C1204D83A4FFDA70F869EC5646645F6964006460687B2D8FAF53467F4CD2B00004"}: generic server error
//            Exception code.message: generic server error
//            Exception code.value: 280
//            Can replicate this by creating a token for 1 database, then inserting it to the change stream of another

                //invalid token
//            Exception e.what(): KeyString format error: Unknown type: 7: generic server error
//            Exception code.message: generic server error
//            Exception code.value: 50811
//            Can replicate this by replacing the final char of a token (sometimes, sometimes it will send back #280)
            }

            //if an exception occurred, don't spam the thread, wait before trying again
            std::this_thread::sleep_for(chat_change_stream_values::CHAT_CHANGE_STREAM_SLEEP_TIME);
        }
        catch (const bsoncxx::exception& e) {
            std::cout << "Chat change stream bsoncxx::exception: " << e.what() << '\n';
            std::string exception_code = e.code().message();

            std::optional<std::string> dummyExceptionString;
            storeMongoDBErrorAndException(__LINE__, __FILE__,
                                          dummyExceptionString, exception_code);

            //if an exception occurred, don't spam the thread, wait before trying again
            std::this_thread::sleep_for(chat_change_stream_values::CHAT_CHANGE_STREAM_SLEEP_TIME);
        }
        //NOTE: MUST have the explicit unlock() function called here. If it is not
        // there does not seem to be enough time between the lock being unlocked between
        // scopes and the new lock being created for cancelChatChangeStream() to ever
        // be called.
        lock.unlock();
    }

    messages_waiting_to_be_sent_condition_variable.notify_all();
    order_messages_thread.join();

    std::cout << "\nChat change stream completed.\n";
    std::cout << "total_chat_change_stream_run_time: " << total_chat_change_stream_run_time << '\n';

#ifdef LG_TESTING
    long total_gap_between_timestamps = 0;
    long total_num_messages = 0;
    long total_num_messages_out_of_order = 0;
    long largest_time_to_get_back_to_change_stream = -1;
    long time_to_get_back_to_change_stream = 0;

    for(const auto& x : timestamp_testing_stuff) {
        if(x.second.largest_negative_gap_between_timestamps > largest_negative_gap_between_timestamps) {
            largest_negative_gap_between_timestamps = x.second.largest_negative_gap_between_timestamps;
        }
        if(x.second.largest_time_to_get_back_to_change_stream > largest_time_to_get_back_to_change_stream) {
            largest_time_to_get_back_to_change_stream = x.second.largest_time_to_get_back_to_change_stream;
        }
        total_gap_between_timestamps += x.second.total_gap;
        total_num_messages += x.second.num_messages;
        total_num_messages_out_of_order += x.second.num_messages_out_of_order;
        time_to_get_back_to_change_stream += x.second.time_to_get_back_to_change_stream;
    }

    if(total_num_messages > 0) {
        std::cout << "largest_negative_gap_between_timestamps: " << largest_negative_gap_between_timestamps << "ms\n";
        std::cout << "average_negative_gap_between_timestamps: " << (total_gap_between_timestamps * 1000) / total_num_messages << "us\n";
        std::cout << "total_num_messages: " << total_num_messages << "\n";
        std::cout << "total_num_messages_out_of_order: " << total_num_messages_out_of_order << "\n";

        std::cout << "\nlargest_time_to_get_back_to_change_stream: " << largest_time_to_get_back_to_change_stream << " ms\n";
        std::cout << "average_time_to_get_back_to_change_stream: " << time_to_get_back_to_change_stream / total_num_messages << " ms\n";
    }
#endif

}

void cancelChatChangeStream() {
    std::unique_lock<std::mutex> lock(change_stream_mutex);
    //if change stream is still running, unlock it
    continueChangeStream = false;
}

bool getThreadStarted() {
    //if change stream is still running, unlock it
    return thread_started;
}

void downloadChatRoomForSpecificUser(
        const std::string& chat_room_id,
        const std::chrono::milliseconds& time_chat_room_last_observed,
        const std::string& user_account_oid,
        bool from_user_swiped_yes_on_event
        ) {
    auto first_chat_room_oid = map_of_chat_rooms_to_users.find(chat_room_id);

    if (first_chat_room_oid != map_of_chat_rooms_to_users.end()) { //if the chat room exists

        //No delay needed, a match made is a new chat room being created. This means there should be no chance of out
        // of order messages.
        first_chat_room_oid->second.sendMessageToSpecificUser(
                //Copy by value to send with the lambda (don't copy anything large).
                [chat_room_id, time_chat_room_last_observed, from_user_swiped_yes_on_event, user_account_oid] (
                        std::vector<std::shared_ptr<grpc_stream_chat::ChatToClientResponse>>& reply_vector
                ) {

                    StoreAndSendMessagesToClientBiDiStream<TypeOfChatToClientToUse::CHAT_TO_CLIENT_TYPE_NEW_MESSAGE> store_messages_to_vector(reply_vector);

                    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
                    mongocxx::database chat_room_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

                    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];
                    mongocxx::collection chat_room_collection = chat_room_db[collection_names::CHAT_ROOM_ID_ + chat_room_id];

                    const bsoncxx::document::value dummy_user_doc = document{}
                            << "matchUserOID" << user_account_oid
                            << "NOTE" << "dummy document from begin_chat_change_stream; match_made"
                            << "Line_number" << __LINE__
                            << finalize;

                    sendNewChatRoomAndMessages(
                            mongo_cpp_client,
                            accounts_db,
                            user_accounts_collection,
                            chat_room_collection,
                            dummy_user_doc.view(),
                            chat_room_id,
                            time_chat_room_last_observed,
                            getCurrentTimestamp(),
                            user_account_oid,
                            &store_messages_to_vector,
                            HowToHandleMemberPictures::REQUEST_ALL_PICTURE_INFO,
                            AmountOfMessage::COMPLETE_MESSAGE_INFO,
                            false,
                            false,
                            from_user_swiped_yes_on_event
                    );

                    store_messages_to_vector.finalCleanup();
                },
                user_account_oid
        );
    }
}

#ifdef LG_TESTING
void setThreadStartedToFalse() {
    thread_started = false;
}
#endif