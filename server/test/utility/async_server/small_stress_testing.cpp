//
// Created by jeremiah on 7/14/22.
//
#include <utility_general_functions.h>
#include <server_parameter_restrictions.h>
#include <android_specific_values.h>
#include <general_values.h>
#include <fstream>
#include <generate_multiple_random_accounts.h>
#include <mongocxx/pool.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/collection.hpp>
#include <connection_pool_global_variable.h>
#include <database_names.h>
#include <collection_names.h>
#include <account_objects.h>
#include <reports_objects.h>
#include <ChatRoomCommands.pb.h>
#include <chat_room_commands.h>
#include <setup_login_info.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "chat_message_stream.h"
#include "chat_room_commands_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include <generate_random_messages.h>
#include <async_server.h>
#include <start_server.h>
#include <testing_client.h>
#include <start_bi_di_stream.h>
#include <chat_stream_container_object.h>

#include <chat_room_message_keys.h>
#include <generate_multiple_accounts_multi_thread.h>
#include <tbb/concurrent_unordered_map.h>
#include <send_messages_implementation.h>
#include "chat_change_stream_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ServerStressTest : public ::testing::Test {
protected:
    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }
};

struct ChatRoomData {
    tbb::concurrent_unordered_set<std::string> message_received_uuid{};

    std::mutex recent_message_received_mutex;
    std::vector<std::pair<long, std::string>> recently_received_message_uuids;
};

void saveMessageToDataStructure(
        const google::protobuf::RepeatedPtrField<ChatMessageToClient>& messages_list,
        std::unordered_map<std::string, std::unique_ptr<ChatRoomData>>& user_chat_rooms_messages_received,
        std::atomic_int& num_times_duplicate_message_returned,
        std::atomic_int& number_new_messages_received,
        tbb::concurrent_unordered_set<std::string>& message_uuid_received_or_sent
) {
    for (const auto& msg: messages_list) {
        message_uuid_received_or_sent.insert(msg.message_uuid());
        number_new_messages_received++;
        if (msg.message().message_specifics().message_body_case() == MessageSpecifics::MessageBodyCase::kTextMessage) {
            auto chat_room_data = user_chat_rooms_messages_received.find(
                    msg.message().standard_message_info().chat_room_id_message_sent_from());
            EXPECT_TRUE(chat_room_data != user_chat_rooms_messages_received.end());
            if (chat_room_data != user_chat_rooms_messages_received.end()) {
                auto result = chat_room_data->second->message_received_uuid.insert(
                        msg.message_uuid()
                );
                if (!result.second) {
                    while (!chat_room_data->second->message_received_uuid.contains(msg.message_uuid())) {
                        chat_room_data->second->message_received_uuid.insert(msg.message_uuid());
                    }
                    num_times_duplicate_message_returned++;
                }

                long timestamp_stored = msg.timestamp_stored();

                std::scoped_lock<std::mutex> lock(chat_room_data->second->recent_message_received_mutex);
                if (chat_room_data->first != msg.message().standard_message_info().chat_room_id_message_sent_from()) {
                    std::cout << "CHAT ROOM DOES NOT MATCH\n";
                }
                chat_room_data->second->recently_received_message_uuids.emplace_back(
                        timestamp_stored, msg.message_uuid()
                );
            }
        }
    }
}

/* This test is not very representative of a 'real' situation. Each 'user' must run multiple threads while the device also
 * runs the server and the database. The purpose is more just to do a larger test to find inconsistencies between threads
 * within the server. It can however provide some loose profiling information.
 *
 * Some of the values are printed at the end of the test to see run times.
 *
 * Will 'randomly' choose 1 of 4 actions
 * 1) Send a text message.
 * 2) Refresh chat stream.
 * 3) Request update for a current text message.
 * 4) Restart the bi-di stream with a new stream.
**/
TEST_F(ServerStressTest, small_stress_test) {

    const std::chrono::milliseconds original_time_to_request_previous_messages = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES;

    //If TIME_TO_REQUEST_PREVIOUS_MESSAGES gets too large, it will request ALL messages for the entire timeframe when
    // it restarts the bi-di stream. So setting this down to a more 'reasonable' number for testing.
    chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES = std::chrono::milliseconds{800};

    for (int l = 0; l < 1; l++) {

        ChatStreamContainerObject::num_new_objects_created = 0;
        ChatStreamContainerObject::num_objects_deleted = 0;
        ChatStreamContainerObject::num_times_finish_called = 0;

        std::cout << "COUNTER: " << l << std::endl;

        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        const std::chrono::milliseconds test_start_time = getCurrentTimestamp();

        struct UserValues {
            UserAccountDoc user_account;
            std::vector<std::string> joined_chat_rooms;
            long extra_time_to_send_back_messages = 0;

            UserValues() = delete;

            explicit UserValues(
                    UserAccountDoc&& _user_account
            ) : user_account(std::move(_user_account)) {}
        };

        /** These parameters control the function.**/

        const int num_clients = 50; //number of users/clients (each user will have a thread)
        const int num_chat_rooms = 10; //total number of chat rooms (outside the first user, each following user will randomly join some)
        const std::chrono::milliseconds total_time_to_send_messages{
                30 * 1000}; //total time that the users will randomly send messages/run operations
        const std::chrono::milliseconds total_time_to_wait_to_complete =
                chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING +
                chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME + std::chrono::milliseconds{
                        15 * 1000
        }; //after users have 'finished' running operations, this is how long for the threads to sleep while the server catches up
        const std::chrono::milliseconds total_time_chat_stream_active{
                30 * 60 * 1000}; //temp TIME_CHAT_STREAM_STAYS_ACTIVE variable value
        const std::chrono::milliseconds sleep_time_between_client_actions{15L};

        std::atomic_int num_messages_sent = 0;
        std::atomic_int num_messages_received = 0;
        std::atomic_int num_refresh_operations = 0;
        std::atomic_int num_message_updates_requested = 0;
        std::atomic_int num_times_stream_restarted = 0;
        std::atomic_int num_times_duplicate_initialize_messages_received = 0;
        std::atomic_int num_times_duplicate_return_new_message_received = 0;
        std::atomic_int num_times_duplicate_message_returned_sending = 0;

        std::vector<std::unique_ptr<std::jthread>> threads(num_clients);
        std::vector<std::unique_ptr<UserValues>> user_accounts(num_clients);
        std::vector<grpc_chat_commands::CreateChatRoomResponse> chat_rooms(num_chat_rooms);
        std::unordered_map<std::string, std::atomic_int> chat_rooms_messages_received;

        std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

        std::atomic_long total_client_run_time = 0;

        const std::chrono::milliseconds original_time_between_token_verification = general_values::TIME_BETWEEN_TOKEN_VERIFICATION;
        general_values::TIME_BETWEEN_TOKEN_VERIFICATION = std::chrono::milliseconds{30L * 60L * 1000L};

        {
            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;
            mongocxx::collection accounts_collection = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME][collection_names::USER_ACCOUNTS_COLLECTION_NAME];

            multiThreadInsertAccounts(
                    num_clients,
                    (int) std::thread::hardware_concurrency() - 1,
                    false
            );

            auto cursor = accounts_collection.find(
                    document{} << finalize
            );

            int i = 0;
            for (const bsoncxx::document::view& doc: cursor) {
                UserAccountDoc account_doc;
                account_doc.convertDocumentToClass(doc);
                user_accounts[i] = std::make_unique<UserValues>(std::move(account_doc));
                i++;
            }
        }

        for (int j = 0; j < num_chat_rooms; j++) {
            if (num_chat_rooms > 50 && j > 0) {
                //There is a bottleneck inside create chat rooms when it requests a specific value from
                // the shared document. This may avoid the 'congestion' here.
                std::this_thread::sleep_for(std::chrono::milliseconds{20});
            }

            std::cout << "num_running: " << j << '\n';

            grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
            grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

            setupUserLoginInfo(
                    create_chat_room_request.mutable_login_info(),
                    user_accounts[0]->user_account.current_object_oid,
                    user_accounts[0]->user_account.logged_in_token,
                    user_accounts[0]->user_account.installation_ids.front()
            );

            createChatRoom(&create_chat_room_request, &create_chat_room_response);
            ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

            chat_rooms_messages_received[create_chat_room_response.chat_room_id()] = 0;
            user_accounts[0]->joined_chat_rooms.emplace_back(create_chat_room_response.chat_room_id());
            chat_rooms[j] = std::move(create_chat_room_response);
        }

        int number_join_chat_threads = num_clients < (int) std::thread::hardware_concurrency() ? num_clients
                                                                                               : (int) std::thread::hardware_concurrency();
        std::vector<std::jthread> join_chat_threads;

        //skip first account, it is already a part of all chat rooms
        int remainder = (num_clients - 1) % number_join_chat_threads;
        int total = (num_clients - 1) / number_join_chat_threads;
        int start = 1;

        for (int i = 0; i < number_join_chat_threads; i++) {
            int passed_remained = 0;
            if (remainder > 0) {
                passed_remained = 1;
                remainder--;
            }
            join_chat_threads.emplace_back(
                    [&chat_rooms, &user_accounts, start, end = (start + total + passed_remained)]() {
                        std::cout << std::string(
                                "join_chat_room start: " + std::to_string(start) + " end: " + std::to_string(end) +
                                "\n");
                        for (int j = start; j < end; j++) {
                            for (int k = 0; k < num_chat_rooms; ++k) {
                                if (rand() % 2) {

                                    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

                                    join_chat_room_request.set_chat_room_id(chat_rooms[k].chat_room_id());
                                    join_chat_room_request.set_chat_room_password(chat_rooms[k].chat_room_password());

                                    setupUserLoginInfo(
                                            join_chat_room_request.mutable_login_info(),
                                            user_accounts[j]->user_account.current_object_oid,
                                            user_accounts[j]->user_account.logged_in_token,
                                            user_accounts[j]->user_account.installation_ids.front()
                                    );

                                    auto rw = grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse>();

                                    joinChatRoom(&join_chat_room_request, &rw);
                                    ASSERT_FALSE(rw.write_params.empty());
                                    ASSERT_EQ(rw.write_params.front().msg.chat_room_status(),
                                              grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED);

                                    user_accounts[j]->joined_chat_rooms.emplace_back(chat_rooms[k].chat_room_id());
                                }
                            }
                        }
                    }
            );

            start += total + passed_remained;
        }

        for (auto& thread: join_chat_threads) {
            thread.join();
        }

        join_chat_threads.clear();

        bool no_chat_rooms = true;

        //start at index 1, index 0 exists in every chat room
        for (int i = 1; i < (int) user_accounts.size(); i++) {
            if (!user_accounts[i]->joined_chat_rooms.empty()) {
                no_chat_rooms = false;
                break;
            }
        }

        //make sure there are always at least 2 users that can send messages to each other (user 0 and 1 in this case)
        if (no_chat_rooms && user_accounts.size() > 1 && !user_accounts[0]->joined_chat_rooms.empty()) {
            grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

            join_chat_room_request.set_chat_room_id(user_accounts[0]->joined_chat_rooms[0]);

            std::string chat_room_password;

            for (const auto& chat_room_response: chat_rooms) {
                if (chat_room_response.chat_room_id() == user_accounts[0]->joined_chat_rooms[0]) {
                    chat_room_password = chat_room_response.chat_room_password();
                    break;
                }
            }

            join_chat_room_request.set_chat_room_password(chat_room_password);

            setupUserLoginInfo(
                    join_chat_room_request.mutable_login_info(),
                    user_accounts[1]->user_account.current_object_oid,
                    user_accounts[1]->user_account.logged_in_token,
                    user_accounts[1]->user_account.installation_ids.front()
            );

            auto rw = grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse>();

            joinChatRoom(&join_chat_room_request, &rw);
            ASSERT_FALSE(rw.write_params.empty());
            ASSERT_EQ(rw.write_params.front().msg.chat_room_status(),
                      grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED);
            ASSERT_FALSE(rw.write_params.front().msg.messages_list().empty());
            ASSERT_EQ(rw.write_params.front().msg.messages_list()[0].return_status(),
                      ReturnStatus::SUCCESS);

            user_accounts[1]->joined_chat_rooms.emplace_back(user_accounts[0]->joined_chat_rooms[0]);
        }

        std::jthread server_thread = startServer(grpcServerImpl);

        //Start chat change stream last AFTER chat room has been created and user's joined it. This way
        // it will not get the new chat room message.
        std::jthread chat_change_stream_thread = std::jthread(
                [&]() {
                    beginChatChangeStream();
                }
        );

        const std::chrono::milliseconds original_time_chat_stream_stays_active = chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
        const std::chrono::milliseconds original_time_chat_stream_refresh_allowed = chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;

        chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = total_time_chat_stream_active;
        chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{
                (long) ((double) total_time_chat_stream_active.count() * .95)};
        print_stream_stuff = false;

        //allow server and change stream to start
        std::this_thread::sleep_for(std::chrono::milliseconds{50});

        const std::chrono::milliseconds loop_start_time = getCurrentTimestamp();
        const std::chrono::milliseconds end_time = loop_start_time + total_time_to_send_messages;

        std::atomic_int clients_passed_send_loop = 0;

        std::atomic_long time_spent_sending_messages = 0;
        std::atomic_long time_spent_refreshing_stream = 0;
        std::atomic_long time_spent_updating_messages = 0;
        std::atomic_long time_spent_refreshing_bi_di_stream = 0;

        std::cout << "About to start primary loop." << std::endl;
        for (int k = 0; k < num_clients; ++k) {
            threads[k] = std::make_unique<std::jthread>(
                    [&, k]() {

                        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

                        std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

                        //All chat rooms are added upfront, the values INSIDE ChatRoomData must be asynchronous.
                        std::unordered_map<std::string, std::unique_ptr<ChatRoomData>> user_chat_rooms_messages_received;

                        std::string chat_rooms_string;

                        for (const std::string& chat_room_id: user_accounts[k]->joined_chat_rooms) {
                            user_chat_rooms_messages_received.insert(
                                    std::make_pair(chat_room_id, std::make_unique<ChatRoomData>())
                            );
                            chat_rooms_string += std::string(chat_room_id).
                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                    append(std::to_string(current_timestamp.count())).
                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                    append(std::to_string(current_timestamp.count())).
                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                    append(std::to_string(0)).
                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
                        }

                        std::chrono::milliseconds most_recent_received_timestamp{-1};

                        int number_times_refresh_called = 0;
                        int number_times_refresh_returned = 0;

                        std::mutex message_updates_mutex;
                        std::mutex refresh_mutex;

                        std::vector<std::string> requested_message_updates;
                        std::vector<std::string> received_message_updates;

                        //ensure only 1 message (or group of messages) can be requested at a time
                        std::atomic_bool outstanding_message_request = false;
                        std::atomic_int number_new_messages_received_or_sent = 0;
                        tbb::concurrent_unordered_set<std::string> message_uuid_received_or_sent;
                        std::atomic_int num_outstanding_refresh_messages = 0;

                        std::atomic_bool initialize_completed_message_called = false;

                        const std::function<void(
                                const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
                                const grpc_stream_chat::ChatToClientResponse& response) {
                            const std::chrono::milliseconds lambda_start = getCurrentTimestamp();
                            num_messages_received++;
                            if (response.server_response_case() ==
                                grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage) {
                                saveMessageToDataStructure(
                                        response.return_new_chat_message().messages_list(),
                                        user_chat_rooms_messages_received,
                                        num_times_duplicate_return_new_message_received,
                                        number_new_messages_received_or_sent,
                                        message_uuid_received_or_sent
                                );
                            } else if (
                                    response.server_response_case() ==
                                    grpc_stream_chat::ChatToClientResponse::kRefreshChatStreamResponse
                                    ) {
                                std::unique_lock<std::mutex> lock(refresh_mutex);
                                if (num_outstanding_refresh_messages > 0) {
                                    number_times_refresh_returned++;
                                    num_outstanding_refresh_messages--;
                                }
                            } else if (
                                    response.server_response_case() ==
                                    grpc_stream_chat::ChatToClientResponse::kRequestFullMessageInfoResponse
                                    ) {
                                std::unique_lock<std::mutex> lock(message_updates_mutex);
                                if (outstanding_message_request) {
                                    outstanding_message_request = false;
                                    for (const auto& val: response.request_full_message_info_response().full_messages().full_message_list()) {
                                        //There is an edge case where the message_uuid is remove at the same time as this lambda runs.
                                        // Checking to make sure the message was requested just in case
                                        received_message_updates.emplace_back(val.message_uuid());
                                    }
                                    lock.unlock();
                                    EXPECT_EQ(
                                            response.request_full_message_info_response().request_status(),
                                            grpc_stream_chat::RequestFullMessageInfoResponse::FINAL_MESSAGE_LIST
                                    );
                                }
                            } else if (
                                    response.server_response_case() ==
                                    grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse
                                    ) {
                                saveMessageToDataStructure(
                                        response.initial_connection_messages_response().messages_list(),
                                        user_chat_rooms_messages_received,
                                        num_times_duplicate_initialize_messages_received,
                                        number_new_messages_received_or_sent,
                                        message_uuid_received_or_sent
                                );
                            } else if (
                                    response.server_response_case() ==
                                    grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse
                                    ) {
                                initialize_completed_message_called = true;
                            }

                            const std::chrono::milliseconds timestamp = getCurrentTimestamp();
                            total_client_run_time += timestamp.count() - lambda_start.count();
                            most_recent_received_timestamp = timestamp;
                        };

                        TestingStreamOptions testingStreamOptions;
                        testingStreamOptions.setSkipStatusCheck();

                        std::shared_ptr<std::jthread> bi_di_thread =
                                std::make_shared<std::jthread>(
                                        startBiDiStream(
                                                client,
                                                user_accounts[k]->user_account,
                                                callback_when_message_received,
                                                chat_rooms_string,
                                                testingStreamOptions
                                        )
                                );

                        for (
                                int i = 0;
                                !initialize_completed_message_called.load(std::memory_order_relaxed);
                                ++i
                                ) {
                            if (i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
                                static const timespec ns = {0, 1}; //1 nanosecond
                                nanosleep(&ns, nullptr);
                                i = 0;
                            }
                        }

                        //the int is the ChatStreamContainerObject ptr, the long is the most recent timestamp it returned
                        tbb::concurrent_unordered_map<unsigned int, long> chat_stream_container_largest_timestamp;
                        std::atomic_long current_largest_returned_timestamp;

                        total_client_run_time += getCurrentTimestamp().count() - current_timestamp.count();

                        if (k == num_clients - 1) {
                            std::cout << "Finished initialization on " << std::to_string(k) << " , took " <<
                                    getCurrentTimestamp().count() - loop_start_time.count() << "ms." << std::endl;
                        }

                        //pause while all threads have their bi-di streams started
                        std::this_thread::sleep_for(
                                std::chrono::milliseconds{num_clients < 100 ? 100 : (long) (num_clients)});
                        const std::chrono::milliseconds operation_loop_start_time = getCurrentTimestamp();
                        for (std::chrono::milliseconds timestamp = getCurrentTimestamp();
                             timestamp < end_time; timestamp = getCurrentTimestamp()) {

                            int choice = rand() % 30;
                            const std::chrono::milliseconds choices_start_time = getCurrentTimestamp();

                            if (choice == 0) {
                                //do nothing
                            }
                            else if (choice < 16 &&
                                       !user_accounts[k]->joined_chat_rooms.empty()) { //send a text message to a chat room
                                //get random chat room that this user is part of
                                const std::string chat_room_id = user_accounts[k]->joined_chat_rooms[rand() %
                                                                                                     user_accounts[k]->joined_chat_rooms.size()];

                                const std::string text_message_uuid = generateUUID();

                                auto chat_room_data = user_chat_rooms_messages_received.find(
                                        chat_room_id
                                );

                                EXPECT_TRUE(chat_room_data != user_chat_rooms_messages_received.end());
                                if (chat_room_data != user_chat_rooms_messages_received.end()) {

                                    //This must be set because this user will not receive its own message
                                    // from the change chat stream. Also, it should be set before the message is
                                    // sent, otherwise the message can be received before this insert is called.
                                    auto result = chat_room_data->second->message_received_uuid.insert(
                                            text_message_uuid);
                                    if (!result.second) {
                                        while (!chat_room_data->second->message_received_uuid.contains(
                                                text_message_uuid)) {
                                            chat_room_data->second->message_received_uuid.insert(text_message_uuid);
                                        }
                                        num_times_duplicate_message_returned_sending++;
                                    }

                                    auto [text_message_request, text_message_response] = generateRandomTextMessage(
                                            user_accounts[k]->user_account.current_object_oid,
                                            user_accounts[k]->user_account.logged_in_token,
                                            user_accounts[k]->user_account.installation_ids.front(),
                                            chat_room_id,
                                            text_message_uuid,
                                            gen_random_alpha_numeric_string(
                                                    rand() % 100 +
                                                    server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE +
                                                    1)
                                    );

                                    EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

                                    chat_rooms_messages_received[chat_room_id]++;
                                }

                                num_messages_sent++;
                                number_new_messages_received_or_sent++;
                                message_uuid_received_or_sent.insert(text_message_uuid);
                                time_spent_sending_messages += (getCurrentTimestamp() - choices_start_time).count();
                            }
                            else if (choice < 22) { //refresh
                                grpc_stream_chat::ChatToServerRequest request;
                                request.mutable_refresh_chat_stream();

                                client->sendBiDiMessage(request);
                                std::unique_lock<std::mutex> lock(refresh_mutex);
                                number_times_refresh_called++;
                                num_outstanding_refresh_messages++;
                                lock.unlock();
                                num_refresh_operations++;
                                time_spent_refreshing_stream += (getCurrentTimestamp() - choices_start_time).count();
                            }
                            else if (choice < 28 && !user_accounts[k]->joined_chat_rooms.empty() &&
                                       !outstanding_message_request) { //request_message_update

                                //get random chat room that this user is part of
                                const std::string chat_room_id = user_accounts[k]->joined_chat_rooms[rand() %
                                                                                                     user_accounts[k]->joined_chat_rooms.size()];

                                mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                                mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                                mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
                                mongocxx::collection chat_room_collection = chat_rooms_db[
                                        collection_names::CHAT_ROOM_ID_ +
                                        chat_room_id];

                                mongocxx::pipeline pipeline;

                                pipeline.match(
                                        document{}
                                                << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kTextMessage
                                                << finalize
                                );

                                pipeline.project(
                                        document{}
                                                << "_id" << 1
                                                << finalize
                                );

                                pipeline.sample(1);

                                auto cursor = chat_room_collection.aggregate(pipeline);

                                std::optional<bsoncxx::document::value> text_message;
                                for (const bsoncxx::document::view& doc: cursor) {
                                    text_message = bsoncxx::document::value(doc);
                                    break;
                                }

                                if (text_message) { //message was found (it is OK if not found, there could be no text messages that exist)

                                    const std::string extracted_message_uuid = text_message->view()["_id"].get_string().value.to_string();

                                    grpc_stream_chat::ChatToServerRequest request;
                                    request.mutable_request_full_message_info()->set_chat_room_id(chat_room_id);
                                    auto messages_list_request = request.mutable_request_full_message_info()->add_message_uuid_list();
                                    messages_list_request->set_message_uuid(extracted_message_uuid);
                                    messages_list_request->set_amount_of_messages_to_request(
                                            (rand() % 2) ? AmountOfMessage::COMPLETE_MESSAGE_INFO
                                                         : AmountOfMessage::ENOUGH_TO_DISPLAY_AS_FINAL_MESSAGE);

                                    client->sendBiDiMessage(request);

                                    std::unique_lock<std::mutex> lock(message_updates_mutex);
                                    requested_message_updates.emplace_back(extracted_message_uuid);
                                    outstanding_message_request = true;
                                    lock.unlock();
                                    num_message_updates_requested++;
                                }
                                time_spent_updating_messages += (getCurrentTimestamp() - choices_start_time).count();
                            }
                            else if (choice == 29) { //cancel chat stream and start a new one
                                num_times_stream_restarted++;

                                //NOTE: This possibility takes significantly longer to run than the others, so it has the lowest
                                // chance of being run.

                                chat_rooms_string.clear();
                                const std::chrono::milliseconds earliest_timestamp_to_request = getCurrentTimestamp() - chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES;
                                for (const std::string& chat_room_id: user_accounts[k]->joined_chat_rooms) {
                                    auto chat_room_data = user_chat_rooms_messages_received.find(chat_room_id);
                                    EXPECT_TRUE(chat_room_data != user_chat_rooms_messages_received.end());

                                    if (chat_room_data != user_chat_rooms_messages_received.end()) {

                                        std::unique_lock<std::mutex> lock(
                                                chat_room_data->second->recent_message_received_mutex);

                                        chat_room_data->second->recently_received_message_uuids.erase(
                                                std::remove_if(
                                                        chat_room_data->second->recently_received_message_uuids.begin(),
                                                        chat_room_data->second->recently_received_message_uuids.end(),
                                                        [&](const std::pair<long, std::string>& val) -> bool {
                                                            return val.first < earliest_timestamp_to_request.count();
                                                        }),
                                                chat_room_data->second->recently_received_message_uuids.end());

                                        std::sort(chat_room_data->second->recently_received_message_uuids.begin(),
                                                  chat_room_data->second->recently_received_message_uuids.end(),
                                                  [](const std::pair<long, std::string>& lhs, const std::pair<long, std::string>& rhs){
                                            return lhs.first < rhs.first;
                                        });

                                        long last_updated_time =
                                                chat_room_data->second->recently_received_message_uuids.empty() ?
                                                -1L
                                                : chat_room_data->second->recently_received_message_uuids.back().first;

                                        chat_rooms_string += std::string(chat_room_id).
                                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                                append(std::to_string(last_updated_time)).
                                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                                append(std::to_string(last_updated_time)).
                                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                                append(std::to_string(chat_room_data->second->recently_received_message_uuids.size())).
                                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

                                        for(int l = 0; l < (int)chat_room_data->second->recently_received_message_uuids.size(); ++l) {
                                            chat_rooms_string
                                                .append(chat_room_data->second->recently_received_message_uuids[l].second)
                                                .append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
                                        }

                                        lock.unlock();
                                    }
                                }

                                std::shared_ptr<TestingClient> client_two = std::make_shared<TestingClient>();

                                initialize_completed_message_called = false;

                                std::shared_ptr<std::jthread> bi_di_thread_2 =
                                        std::make_shared<std::jthread>(
                                                startBiDiStream(
                                                        client_two,
                                                        user_accounts[k]->user_account,
                                                        callback_when_message_received,
                                                        chat_rooms_string,
                                                        testingStreamOptions
                                                )
                                        );

                                bi_di_thread->join();
                                std::atomic_store(&bi_di_thread, bi_di_thread_2);
                                std::atomic_store(&client, client_two);
                                time_spent_refreshing_bi_di_stream += (getCurrentTimestamp() -
                                                                       choices_start_time).count();
                                //remove any passed refresh that were cancelled with the server
                                std::unique_lock<std::mutex> refresh_lock(refresh_mutex);
                                number_times_refresh_called -= num_outstanding_refresh_messages;
                                num_outstanding_refresh_messages = 0;
                                refresh_lock.unlock();

                                //remove any passed update requests that were called from the server
                                std::unique_lock<std::mutex> update_lock(message_updates_mutex);
                                if (outstanding_message_request) {
                                    outstanding_message_request = false;
                                    requested_message_updates.pop_back();
                                }
                                update_lock.unlock();

//                                for(
//                                        int i = 0, j = 0;
//                                        !initialize_completed_message_called.load(std::memory_order_relaxed);
//                                        ++i, ++j
//                                        ) {
//                                    if(i == general_values::SPINLOCK_NUMBER_TIMES_TO_LOOP_BEFORE_SLEEP) {
//                                        static const timespec ns = { 0,1 }; //1 nanosecond
//                                        nanosleep(&ns, nullptr);
//                                        i = 0;
//                                    }
//                                    if(j % 10000000 == 0) {
//                                        std::cout << std::string(std::to_string(k) + " loop stuck\n") << std::flush;
//                                    }
//                                }

                            }
                            // else {}
                            // Skipping testing these.
                            // Server shutdown (slow, would require coordination between threads and server).
                            // Inject_message (this is already done by the change stream).
                            // Join & leave chat rooms (slow, will need a thread safe local data structure for this).

                            total_client_run_time += getCurrentTimestamp().count() - timestamp.count();
                            std::this_thread::sleep_for(std::chrono::milliseconds{
                                    rand() % sleep_time_between_client_actions.count() + 10
                            });
                        }

                        int temp_passed_send_loop = clients_passed_send_loop.fetch_add(1);
                        std::cout << std::string("clients finished: " + std::to_string(temp_passed_send_loop + 1) + "\n") << std::flush;
                        if (temp_passed_send_loop == num_clients - 1) {
                            std::cout << "All clients have finished operations. Sleeping for catchup." << std::endl;
                            std::cout << "Operations took " << (getCurrentTimestamp() -
                                                                operation_loop_start_time).count() << "ms to run." << std::endl;
                        }

                        //sleep to make sure all messages are received
                        if (total_time_to_wait_to_complete.count() > 0) {
                            const std::chrono::milliseconds sleep_start_timestamp = getCurrentTimestamp();
                            std::this_thread::sleep_for(total_time_to_wait_to_complete);
                            const std::chrono::milliseconds sleep_stop_timestamp = getCurrentTimestamp();
                            //If a message was received AFTER sleep was started (the messages were lagging).
                            if (sleep_start_timestamp < most_recent_received_timestamp) {
                                user_accounts[k]->extra_time_to_send_back_messages =
                                        most_recent_received_timestamp.count() - sleep_start_timestamp.count();
                            }
                        }

                        client->finishBiDiStream();

                        bi_di_thread->join();

                        EXPECT_EQ(
                                user_chat_rooms_messages_received.size(),
                                user_accounts[k]->joined_chat_rooms.size()
                        );

                        EXPECT_EQ(
                                number_times_refresh_called,
                                number_times_refresh_returned
                        );

                        EXPECT_EQ(
                                requested_message_updates.size(),
                                received_message_updates.size()
                        );

                        if (requested_message_updates.size() == received_message_updates.size()) {
                            std::sort(requested_message_updates.begin(), requested_message_updates.end());
                            std::sort(received_message_updates.begin(), received_message_updates.end());
                            for (int i = 0; i < (int) received_message_updates.size(); i++) {

                                bool messages_match = requested_message_updates[i] == requested_message_updates[i];
                                EXPECT_TRUE(messages_match);
                                if (!messages_match) {
                                    //if this happens they will be out of order and spam
                                    break;
                                }
                            }
                        }

                        for (const auto& [chat_room_id, set_of_messages]: user_chat_rooms_messages_received) {
                            auto ptr = chat_rooms_messages_received.find(chat_room_id);
                            if (ptr != chat_rooms_messages_received.end()) {
                                EXPECT_EQ(ptr->second, set_of_messages->message_received_uuid.size());
                                if (ptr->second != (int) set_of_messages->message_received_uuid.size()) {
                                    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                                    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

                                    mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];
                                    mongocxx::collection chat_room_collection = chat_rooms_db[
                                            collection_names::CHAT_ROOM_ID_ + chat_room_id];

                                    size_t document_count = chat_room_collection.count_documents(
                                            document{}
                                                    << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kTextMessage
                                                    << finalize
                                    );

                                    std::stringstream output_str;

                                    output_str << "user: " << user_accounts[k]->user_account.current_object_oid.to_string() << " actual_count: " << document_count << '\n';
                                    output_str << "number_new_messages_received_or_sent: " << number_new_messages_received_or_sent << '\n';

                                    if (ptr->second > (int) set_of_messages->message_received_uuid.size()
                                        && ptr->second - set_of_messages->message_received_uuid.size() < 10) {

                                        bsoncxx::builder::basic::array extracted_uuid;
                                        extracted_uuid.append("iDe");

                                        for (const std::string& message_uuid: set_of_messages->message_received_uuid) {
                                            extracted_uuid.append(message_uuid);
                                        }

                                        auto cursor = chat_room_collection.find(
                                                document{}
                                                        << "_id" << open_document
                                                        << "$nin" << extracted_uuid
                                                        << close_document
                                                        << chat_room_message_keys::MESSAGE_TYPE << MessageSpecifics::MessageBodyCase::kTextMessage
                                                        << finalize
                                        );

                                        for (const bsoncxx::document::view& doc: cursor) {
                                            std::string message_uuid = doc["_id"].get_string().value.to_string();
                                            long message_timestamp = doc[chat_room_shared_keys::TIMESTAMP_CREATED].get_date().value.count();
                                            output_str << "\nMissed message_uuid: " << message_uuid << (message_uuid_received_or_sent.contains(
                                                    message_uuid) ? " was " : " was NOT ") << "sent back.\n";

//                                            auto message_doc = chat_room_collection.find_one(
//                                                    document{}
//                                                            << "_id" << message_uuid
//                                                            << finalize
//                                            );
//
//                                            output_str << "message_doc\n" << makePrettyJson(message_doc->view());

                                            size_t num_docs_same_timestamp = chat_room_collection.count_documents(
                                                    document{}
                                                            << chat_room_shared_keys::TIMESTAMP_CREATED << bsoncxx::types::b_date{
                                                            std::chrono::milliseconds{message_timestamp}}
                                                            << finalize
                                            );

                                            size_t num_docs_larger_timestamp = chat_room_collection.count_documents(
                                                    document{}
                                                            << chat_room_shared_keys::TIMESTAMP_CREATED << open_document
                                                            << "$gt" << bsoncxx::types::b_date{
                                                            std::chrono::milliseconds{message_timestamp}}
                                                            << close_document
                                                            << finalize
                                            );

                                            output_str << num_docs_larger_timestamp << " num_docs_larger_timestamp.\n";

                                            output_str << "timestamp_of_message                    : " << message_timestamp << '\n';

                                            if (num_docs_same_timestamp > 1) {
                                                output_str << num_docs_same_timestamp << " documents with this timestamp.\n";
                                            }

                                        }
                                    }

                                    std::cout << output_str.str() << std::endl;
                                    std::cout << std::endl;
                                }
                            }
                        }
                    }
            );
        }

        std::cout << "small_stress_testing.cpp JOINING client threads\n";
        for (auto& thread: threads) {
            thread->join();
        }
        threads.clear();

        std::cout << "SHUTTING_DOWN" << std::endl;
        grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{200});
        server_thread.join();
        chat_change_stream_thread.join();

        std::cout << "SHUTTING_DOWN_COMPLETE" << std::endl;

        EXPECT_EQ(map_of_chat_rooms_to_users.size(), num_chat_rooms);
        for (const auto& x: map_of_chat_rooms_to_users) {
            EXPECT_TRUE(x.second.map.empty());
        }

        EXPECT_EQ(user_open_chat_streams.num_values_stored, 0);

        std::cout << "num_messages_sent: " << num_messages_sent << '\n';
        std::cout << "num_messages_received: " << num_messages_received << '\n';
        std::cout << "num_refresh_operations: " << num_refresh_operations << '\n';
        std::cout << "num_message_updates_requested: " << num_message_updates_requested << '\n';
        std::cout << "num_times_stream_restarted: " << num_times_stream_restarted << '\n';
        std::cout << "num_times_duplicate_message_returned_sending: " << num_times_duplicate_message_returned_sending << '\n';
        std::cout << "num_times_duplicate_initialize_messages_received: " << num_times_duplicate_initialize_messages_received << '\n';
        std::cout << "num_times_duplicate_return_new_message_received: " << num_times_duplicate_return_new_message_received << '\n';

        std::cout << "\ntotal_client_run_time: " << total_client_run_time << '\n';
        std::cout << "time_spent_sending_messages: " << time_spent_sending_messages << '\n';
        std::cout << "time_spent_refreshing_stream: " << time_spent_refreshing_stream << '\n';
        std::cout << "time_spent_updating_messages: " << time_spent_updating_messages << '\n';
        std::cout << "time_spent_refreshing_bi_di_stream: " << time_spent_refreshing_bi_di_stream << "\n\n";

        long largest_lag_time = 0; //this will probably be user[0], they are a member of every chat room
        long average_lag_time = 0;
        for (const auto& acct: user_accounts) {
            average_lag_time += acct->extra_time_to_send_back_messages;
            largest_lag_time =
                    acct->extra_time_to_send_back_messages > largest_lag_time ? acct->extra_time_to_send_back_messages
                                                                              : largest_lag_time;
        }

        //These values show how long after the messages stopped being sent it took the server to 'catch up'.
        std::cout << "average_lag_time: " << (user_accounts.empty() ? 0 : average_lag_time /
                                                                          user_accounts.size()) << '\n';
        std::cout << "largest_lag_time: " << largest_lag_time << '\n';

        std::cout << "thread_pool.num_jobs_outstanding: " << thread_pool.num_jobs_outstanding() << '\n';

        std::cout << "total_test_run_time: " << (getCurrentTimestamp() - test_start_time).count() << '\n';

        std::cout << "delay_for_message_ordering: " << chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING.count() << '\n';

        EXPECT_TRUE(
                ChatStreamContainerObject::num_new_objects_created == ChatStreamContainerObject::num_objects_deleted
                ||
                ChatStreamContainerObject::num_new_objects_created == ChatStreamContainerObject::num_objects_deleted +
                                                                      1 //an object could be created and 'inside' the completion queue that has never been used
        );

        chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = original_time_chat_stream_stays_active;
        chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = original_time_chat_stream_refresh_allowed;
        general_values::TIME_BETWEEN_TOKEN_VERIFICATION = original_time_between_token_verification;
        print_stream_stuff = true;

        clearDatabaseAndGlobalsForTesting();
    }

    chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES = original_time_to_request_previous_messages;
}

enum SimpleMessageInfoFrom {
    SIMPLE_MESSAGE_INFO_FROM_UNKNOWN,
    SIMPLE_MESSAGE_INFO_FROM_DIFFERENT_USER_JOINED,
    SIMPLE_MESSAGE_INFO_FROM_NEW_MESSAGE,
    SIMPLE_MESSAGE_INFO_FROM_INITIALIZE,
    SIMPLE_MESSAGE_INFO_FROM_JOIN_CHAT_ROOM,
    SIMPLE_MESSAGE_INFO_FROM_SENT_MESSAGE,
    SIMPLE_MESSAGE_INFO_FROM_MANUALLY_EXTRACTED,
};

std::string convertSimpleMessageInfoFromToString(SimpleMessageInfoFrom val) {

    std::string return_str = "Invalid SimpleMessageInfoFrom type";

    switch (val) {
        case SIMPLE_MESSAGE_INFO_FROM_UNKNOWN:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_UNKNOWN";
            break;
        case SIMPLE_MESSAGE_INFO_FROM_DIFFERENT_USER_JOINED:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_DIFFERENT_USER_JOINED";
            break;
        case SIMPLE_MESSAGE_INFO_FROM_NEW_MESSAGE:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_NEW_MESSAGE";
            break;
        case SIMPLE_MESSAGE_INFO_FROM_INITIALIZE:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_INITIALIZE";
            break;
        case SIMPLE_MESSAGE_INFO_FROM_JOIN_CHAT_ROOM:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_JOIN_CHAT_ROOM";
            break;
        case SIMPLE_MESSAGE_INFO_FROM_SENT_MESSAGE:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_SENT_MESSAGE";
            break;
        case SIMPLE_MESSAGE_INFO_FROM_MANUALLY_EXTRACTED:
            return_str = "SIMPLE_MESSAGE_INFO_FROM_MANUALLY_EXTRACTED";
            break;
    }

    return return_str;
}

struct SimpleMessageInfo {
    std::string send_by_oid;
    std::string message_uuid;
    long timestamp_stored = -1;
    MessageSpecifics::MessageBodyCase message_type = MessageSpecifics::MessageBodyCase::MESSAGE_BODY_NOT_SET;
    SimpleMessageInfoFrom message_came_from = SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_UNKNOWN;

    SimpleMessageInfo() = default;

    SimpleMessageInfo(
            std::string _send_by_oid,
            std::string _message_uuid,
            long _timestamp_stored,
            MessageSpecifics::MessageBodyCase _message_type,
            SimpleMessageInfoFrom _message_came_from
    ) : send_by_oid(std::move(_send_by_oid)),
        message_uuid(std::move(_message_uuid)),
        timestamp_stored(_timestamp_stored),
        message_type(_message_type),
        message_came_from(_message_came_from) {}

    //all equality operator overloads work ONLY on message_uuid
    bool operator==(const SimpleMessageInfo& rhs) const {
        return message_uuid == rhs.message_uuid;
    }

    bool operator!=(const SimpleMessageInfo& rhs) const {
        return !(rhs == *this);
    }

    bool operator<(const SimpleMessageInfo& rhs) const {
        return message_uuid < rhs.message_uuid;
    }

    bool operator>(const SimpleMessageInfo& rhs) const {
        return rhs < *this;
    }

    bool operator<=(const SimpleMessageInfo& rhs) const {
        return !(rhs < *this);
    }

    bool operator>=(const SimpleMessageInfo& rhs) const {
        return !(*this < rhs);
    }
};

struct ChatRoomJoinedLeftTimes {
    enum ReasonLeft {
        REASON_LEFT_NOT_SET,
        REASON_LEFT_LEFT_CHAT_ROOM,
        REASON_LEFT_KICKED_CHAT_ROOM,
        REASON_LEFT_BANNED_CHAT_ROOM,
        REASON_LEFT_CHAT_STREAM_INITIALIZATION
    };

    static std::string convertReasonLeftToString(ReasonLeft reason_left) {
        std::string return_value;

        switch (reason_left) {
            case REASON_LEFT_NOT_SET:
                return_value = "REASON_LEFT_NOT_SET";
                break;
            case REASON_LEFT_LEFT_CHAT_ROOM:
                return_value = "REASON_LEFT_LEFT_CHAT_ROOM";
                break;
            case REASON_LEFT_KICKED_CHAT_ROOM:
                return_value = "REASON_LEFT_KICKED_CHAT_ROOM";
                break;
            case REASON_LEFT_BANNED_CHAT_ROOM:
                return_value = "REASON_LEFT_BANNED_CHAT_ROOM";
                break;
            case REASON_LEFT_CHAT_STREAM_INITIALIZATION:
                return_value = "REASON_LEFT_CHAT_STREAM_INITIALIZATION";
                break;
        }

        if (return_value.empty()) {
            return_value = "Invalid value passed " + std::to_string(reason_left);
        }

        return return_value;
    }

    long joined_time;
    long left_time = -1;
    ReasonLeft reason_left = ReasonLeft::REASON_LEFT_NOT_SET;

    std::set<SimpleMessageInfo> messages_during_timeframe;

    std::string most_recent_uuid;
    long most_recent_message_stored_time = -1;

    ChatRoomJoinedLeftTimes() = delete;

    explicit ChatRoomJoinedLeftTimes(long _joined_time) : joined_time(_joined_time) {}
};

struct UserChatRoomInfo {
    std::string chat_room_id;
    std::vector<ChatRoomJoinedLeftTimes> times_joined_left;

    //lock this when accessing times_joined_left
    std::mutex times_joined_left_mutex;
};

struct UserInsideChatRoomInfo {
    int user_index = -1;
    AccountStateInChatRoom account_state = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;

    UserInsideChatRoomInfo() = default;

    explicit UserInsideChatRoomInfo(
            int _user_index,
            AccountStateInChatRoom _account_state
    ) : user_index(_user_index), account_state(_account_state) {}
};

struct GlobalChatRoomInfo {
    grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

    //oid of admin
    std::string admin_account_oid;

    //A list of the users for this chat room. When a user leaves it will set their account state
    // to ACCOUNT_STATE_NOT_IN_CHAT_ROOM.
    //This list will NOT contain the ACCOUNT_STATE_IS_ADMIN user.
    tbb::concurrent_unordered_map<std::string, std::atomic<UserInsideChatRoomInfo>> users_info;

    tbb::concurrent_vector<SimpleMessageInfo> messages;
};

void printCompareSimpleMessageInfoError(
        const std::string& type,
        int index,
        const SimpleMessageInfo& message,
        mongocxx::collection& chat_room_collection
) {
    std::stringstream ss;

    ss
            << "Missing " << type << " message.\n"
            << "index: " << index << '\n'
            << "message_uuid: " << message.message_uuid << '\n'
            << "timestamp_stored: " << message.timestamp_stored << '\n'
            << "message_type: " << convertMessageBodyTypeToString(message.message_type) << '\n'
            << "chat_room_collection.name(): " << chat_room_collection.name() << '\n';

    std::cout << ss.str() << std::endl;

    auto message_doc = chat_room_collection.find_one(
            document{}
                    << "_id" << message.message_uuid
                    << finalize
    );

    if (message_doc) {
        std::cout << makePrettyJson(message_doc->view()) << std::endl;
    }
}

bool compareSimpleMessageInfo(
        const std::vector<SimpleMessageInfo>& generated_messages,
        int& generated_index,
        const std::vector<SimpleMessageInfo>& extracted_messages_from_database,
        int& extracted_messages_from_database_index,
        mongocxx::collection& chat_room_collection,
        const std::string& current_user_account_oid
) {
    bool success = true;
    if (generated_messages[generated_index].timestamp_stored >
        extracted_messages_from_database[extracted_messages_from_database_index].timestamp_stored) {
        //ignore messages sent by current user
        if (current_user_account_oid !=
            extracted_messages_from_database[extracted_messages_from_database_index].send_by_oid) {
            printCompareSimpleMessageInfoError(
                    "generated_1",
                    extracted_messages_from_database_index,
                    extracted_messages_from_database[extracted_messages_from_database_index],
                    chat_room_collection
            );
            success = false;
        }
        extracted_messages_from_database_index++;
    } else if (generated_messages[generated_index].timestamp_stored <
               extracted_messages_from_database[extracted_messages_from_database_index].timestamp_stored) {
        //ignore messages sent by current user
        if (current_user_account_oid != generated_messages[generated_index].send_by_oid) {
            printCompareSimpleMessageInfoError(
                    "extracted_1",
                    generated_index,
                    generated_messages[extracted_messages_from_database_index],
                    chat_room_collection
            );
            success = false;
        }
        generated_index++;
    } else { //generated_messages[generated_index].timestamp_stored == extracted_messages_from_database[extracted_messages_from_database_index].timestamp_stored
        if (generated_messages[generated_index].message_uuid <
            extracted_messages_from_database[extracted_messages_from_database_index].message_uuid) {
            if (current_user_account_oid != generated_messages[generated_index].send_by_oid) {
                printCompareSimpleMessageInfoError(
                        "extracted_2",
                        generated_index,
                        generated_messages[extracted_messages_from_database_index],
                        chat_room_collection
                );
                success = false;
            }
            generated_index++;
        } else if (generated_messages[generated_index].message_uuid >
                   extracted_messages_from_database[extracted_messages_from_database_index].message_uuid) {
            //ignore messages sent by current user
            if (current_user_account_oid !=
                extracted_messages_from_database[extracted_messages_from_database_index].send_by_oid) {
                printCompareSimpleMessageInfoError(
                        "generated_2",
                        extracted_messages_from_database_index,
                        extracted_messages_from_database[extracted_messages_from_database_index],
                        chat_room_collection
                );
                success = false;
            }
            extracted_messages_from_database_index++;
        } else { //generated_messages[generated_index].message_uuid == extracted_messages_from_database[extracted_messages_from_database_index].message_uuid
            extracted_messages_from_database_index++;
            generated_index++;
            success = true;
        }
    }
    return success;
};

struct JoinedLeftUserValues {
    UserAccountDoc user_account;
    long extra_time_to_send_back_messages = 0;
    std::vector<UserChatRoomInfo> chat_rooms;

    struct ChatRoomWithIndex {
        std::string chat_room_id;
        int index_of_chat_room;

        ChatRoomWithIndex() = delete;

        ChatRoomWithIndex(
                std::string _chat_room_id,
                int _index_of_chat_room
        ) :
                chat_room_id(std::move(_chat_room_id)),
                index_of_chat_room(_index_of_chat_room) {}
    };

    //NOTE: These do not include the chat room the user is admin of.
    std::vector<ChatRoomWithIndex> currently_inside_chat_rooms;
    std::vector<ChatRoomWithIndex> currently_not_in_chat_rooms;
    std::vector<ChatRoomWithIndex> currently_in_limbo_chat_rooms;

    //used to lock around currently_inside_chat_rooms, currently_not_in_chat_rooms &
    // currently_in_limbo_chat_rooms
    //NOTE: This is locked INSIDE a times_joined_left_mutex, so make sure to keep
    // order consistent.
    std::mutex currently_mutex;

    JoinedLeftUserValues() = delete;

    explicit JoinedLeftUserValues(
            UserAccountDoc&& _user_account,
            size_t chat_room_size
    ) :
            user_account(std::move(_user_account)),
            chat_rooms(chat_room_size) {}
};

void checkJoinedLeftResponse(
        const google::protobuf::RepeatedPtrField<ChatMessageToClient>& messages_list,
        JoinedLeftUserValues& user_account,
        int current_user_index,
        std::vector<GlobalChatRoomInfo>& chat_rooms,
        std::atomic_int& num_times_duplicate_messages_received,
        const std::function<SimpleMessageInfoFrom(const ChatMessageToClient& msg)>& check_message_from
) {

    for (const auto& msg: messages_list) {

        if (msg.message().message_specifics().message_body_case() ==
            MessageSpecifics::MessageBodyCase::kTextMessage
            || msg.message().message_specifics().message_body_case() ==
               MessageSpecifics::MessageBodyCase::kUserKickedMessage
            || msg.message().message_specifics().message_body_case() ==
               MessageSpecifics::MessageBodyCase::kUserBannedMessage
                ) {

            int index_of_chat_room = 0;
            for (auto& chat_room: user_account.chat_rooms) {
                if (chat_room.chat_room_id ==
                    msg.message().standard_message_info().chat_room_id_message_sent_from()
                    && !chat_room.times_joined_left.empty()) {

                    std::scoped_lock<std::mutex> lock(chat_room.times_joined_left_mutex);
                    if (chat_room.times_joined_left.back().left_time == -1) {
                        auto result = chat_room.times_joined_left.back().messages_during_timeframe.insert(
                                SimpleMessageInfo(
                                        msg.sent_by_account_id(),
                                        msg.message_uuid(),
                                        msg.timestamp_stored(),
                                        msg.message().message_specifics().message_body_case(),
                                        check_message_from(msg)
                                )
                        );

                        num_times_duplicate_messages_received += !result.second;

                        if (chat_room.times_joined_left.back().most_recent_message_stored_time
                            < msg.timestamp_stored()
                                ) {
                            chat_room.times_joined_left.back().most_recent_message_stored_time = msg.timestamp_stored();
                            chat_room.times_joined_left.back().most_recent_uuid = msg.message_uuid();
                        }

                        if (msg.message().message_specifics().message_body_case() ==
                            MessageSpecifics::MessageBodyCase::kUserKickedMessage
                            && msg.message().message_specifics().user_kicked_message().kicked_account_oid() ==
                               user_account.user_account.current_object_oid.to_string()
                            && chat_room.times_joined_left.back().joined_time < msg.timestamp_stored()
                            && result.second) {

                            bool chat_room_in_limbo = false;
                            {
                                std::scoped_lock<std::mutex> currently_mutex_lock(user_account.currently_mutex);
                                for (int i = 0;
                                     i < (int) user_account.currently_in_limbo_chat_rooms.size(); ++i) {
                                    if (chat_room.chat_room_id ==
                                        user_account.currently_in_limbo_chat_rooms[i].chat_room_id) {

                                        user_account.currently_not_in_chat_rooms.emplace_back(
                                                user_account.currently_in_limbo_chat_rooms[i]
                                        );

                                        user_account.currently_in_limbo_chat_rooms.erase(
                                                user_account.currently_in_limbo_chat_rooms.begin() + i
                                        );

                                        chat_room_in_limbo = true;
                                        break;
                                    }
                                }
                            }

                            if (chat_room_in_limbo) {

                                chat_room.times_joined_left.back().left_time =
                                        chat_room.times_joined_left.back().most_recent_message_stored_time == -1 ? 0
                                                                                                                 : chat_room.times_joined_left.back().most_recent_message_stored_time;
                                chat_room.times_joined_left.back().reason_left = ChatRoomJoinedLeftTimes::ReasonLeft::REASON_LEFT_KICKED_CHAT_ROOM;

                                chat_rooms[index_of_chat_room].users_info[
                                        user_account.user_account.current_object_oid.to_string()] = UserInsideChatRoomInfo(
                                        current_user_index,
                                        AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
                                );
                            }
                        }
                    }
                    break;
                }
                index_of_chat_room++;
            }
        } else if (msg.message().message_specifics().message_body_case() ==
                   MessageSpecifics::MessageBodyCase::kThisUserLeftChatRoomMessage
                ) {
            int index_of_chat_room = 0;
            for (auto& chat_room: user_account.chat_rooms) {
                if (chat_room.chat_room_id ==
                    msg.message().standard_message_info().chat_room_id_message_sent_from()) {

                    std::scoped_lock<std::mutex> lock(chat_room.times_joined_left_mutex);
                    if (chat_room.times_joined_left.back().left_time == -1) { //still inside

                        bool chat_room_in_limbo = false;
                        {
                            std::scoped_lock<std::mutex> currently_mutex_lock(user_account.currently_mutex);
                            for (int i = 0;
                                 i < (int) user_account.currently_in_limbo_chat_rooms.size(); ++i) {
                                if (chat_room.chat_room_id ==
                                    user_account.currently_in_limbo_chat_rooms[i].chat_room_id) {

                                    user_account.currently_not_in_chat_rooms.emplace_back(
                                            user_account.currently_in_limbo_chat_rooms[i]
                                    );

                                    user_account.currently_in_limbo_chat_rooms.erase(
                                            user_account.currently_in_limbo_chat_rooms.begin() + i
                                    );

                                    chat_room_in_limbo = true;
                                    break;
                                }
                            }
                        }

                        if (chat_room_in_limbo) {

                            chat_room.times_joined_left.back().left_time =
                                    chat_room.times_joined_left.back().most_recent_message_stored_time == -1 ? 0
                                                                                                             : chat_room.times_joined_left.back().most_recent_message_stored_time;
                            chat_room.times_joined_left.back().reason_left = ChatRoomJoinedLeftTimes::ReasonLeft::REASON_LEFT_CHAT_STREAM_INITIALIZATION;

                            chat_rooms[index_of_chat_room].users_info[
                                    user_account.user_account.current_object_oid.to_string()] = UserInsideChatRoomInfo(
                                    current_user_index,
                                    AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
                            );
                        }

                    }
                    break;
                }
                index_of_chat_room++;
            }
        }
    }
}

/* This test is not very representative of a 'real' situation. Each 'user' must run multiple threads while the device also
 * runs the server and the database. The purpose is more just to do a larger test to find inconsistencies between threads
 * within the server. It can however provide some loose profiling information.
 *
 * Some of the values are printed at the end of the test to see run times.
 *
 * This is not a test of functions like joinChatRoom/leaveChatRoom, they are assumed to work 'correctly'.
 *
 * Skipping testing ban, the permanent nature of it produces complications (such as all users will eventually
 * be banned from all chat rooms). Also it is essentially the same as kicked (the same functions are used).
 *
 * Will 'randomly' choose 1 of 5 actions
 * 1) Send a text message.
 * 2) Join chat rooms.
 * 3) Leave chat rooms.
 * 4) Kick/be kicked from chat rooms.
 * 5) Restart the bi-di stream with a new stream.
*/
TEST_F(ServerStressTest, joined_left_chat_room_test) {

    const std::chrono::milliseconds original_delay_for_message_ordering = chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING;
    const std::chrono::milliseconds original_message_ordering_thread_sleep_time = chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME;
    const std::chrono::milliseconds original_testing_delay_for_messages = testing_delay_for_messages;
    const std::chrono::milliseconds original_time_to_request_previous_messages = chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES;

    const int join_chat_room_without_time_received_index = 0;
    const int join_chat_room_moderate_delay_received_index = 1;

    //If TIME_TO_REQUEST_PREVIOUS_MESSAGES gets too large, it will request ALL messages for the entire timeframe when
    // it restarts the bi-di stream. So setting this down to a more 'reasonable' number for testing.
    chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES = std::chrono::milliseconds{800};

    for (int r = 0; r < 2; ++r) {

        //will only be used when r == join_chat_room_without_time_received_index
        std::chrono::milliseconds max_time_value_for_repeats{450L};

        if (r == join_chat_room_without_time_received_index) {
            //This iteration will allow messages through the change stream in an un-ordered manner. While
            // it is possible during production, it should be rare. This test is an extreme case that
            // should never truly happen. However, it forces some edge cases to occur.
            chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = std::chrono::milliseconds{1L};
            chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = std::chrono::milliseconds{1L};
            testing_delay_for_messages = std::chrono::milliseconds{150};
        } else if (r == join_chat_room_moderate_delay_received_index) {
            //This iteration will set a delay to not allow messages through the change stream in an un-ordered
            // manner. It will also make the messages 'closer' to each other by timestamp_stored. This is because
            // testing_delay_for_messages == -1. This will be similar to a production build. However, the delays
            // will be much shorter, this test will not work well across say a network.
            chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = std::chrono::milliseconds{500L};
            chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = std::chrono::milliseconds{1L};
            testing_delay_for_messages = std::chrono::milliseconds{-1}; //force the messages to be sent using $$NOW
        }

        ChatStreamContainerObject::num_new_objects_created = 0;
        ChatStreamContainerObject::num_objects_deleted = 0;
        ChatStreamContainerObject::num_times_finish_called = 0;

        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        const std::chrono::milliseconds test_start_time = getCurrentTimestamp();

        /** These parameters control the function. **/

        const int num_clients = 50; //number of users/clients (each user will have a thread and a chat room that they are admin of)
        const std::chrono::milliseconds total_time_to_send_messages =
                std::chrono::milliseconds{
                        30 * 1000}; //total time that the users will randomly send messages/run operations
        const std::chrono::milliseconds total_time_to_wait_to_complete =
                chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING +
                chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME + std::chrono::milliseconds{
                        15 * 1000
                        }; //after users have 'finished' running operations, this is how long for the threads to sleep while the server catches up
        const std::chrono::milliseconds total_time_chat_stream_active{
                30 * 60 * 1000}; //temp TIME_CHAT_STREAM_STAYS_ACTIVE variable value
        const std::chrono::milliseconds sleep_time_between_client_actions{15L};

        std::atomic_int num_messages_sent = 0;
        std::atomic_int num_messages_received = 0;
        std::atomic_int num_times_duplicate_messages_received = 0;
        std::atomic_int num_times_stream_restarted = 0;

        std::atomic_int chat_rooms_joined = 0;
        std::atomic_int chat_rooms_left = 0;
        std::atomic_int kicked_members = 0;

        std::vector<std::unique_ptr<std::jthread>> threads(num_clients);
        std::vector<std::unique_ptr<JoinedLeftUserValues>> user_accounts(num_clients);

        std::vector<GlobalChatRoomInfo> chat_rooms(num_clients);
        std::shared_ptr<GrpcServerImpl> grpcServerImpl = std::make_shared<GrpcServerImpl>();

        std::atomic_long total_client_run_time = 0;

        const std::chrono::milliseconds original_time_between_token_verification = general_values::TIME_BETWEEN_TOKEN_VERIFICATION;
        general_values::TIME_BETWEEN_TOKEN_VERIFICATION = std::chrono::milliseconds{30L * 60L * 1000L};

        {
            mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
            mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;
            mongocxx::collection accounts_collection = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME][collection_names::USER_ACCOUNTS_COLLECTION_NAME];

            multiThreadInsertAccounts(
                    num_clients,
                    (int) std::thread::hardware_concurrency() - 1,
                    false
            );

            auto cursor = accounts_collection.find(
                    document{} << finalize
            );

            int i = 0;
            for (const bsoncxx::document::view& doc: cursor) {
                UserAccountDoc account_doc;
                account_doc.convertDocumentToClass(doc);
                user_accounts[i] = std::make_unique<JoinedLeftUserValues>(std::move(account_doc), num_clients);
                i++;
            }
        }

        int number_join_chat_threads = num_clients < (int) std::thread::hardware_concurrency() ? num_clients
                                                                                               : (int) std::thread::hardware_concurrency();
        std::vector<std::jthread> join_chat_threads;

        //skip first account, it is already a part of all chat rooms
        int remainder = num_clients % number_join_chat_threads;
        int total = num_clients / number_join_chat_threads;
        int start = 0;

        //Each client creates a chat room, they will permanently stay in that chat room (the purpose of
        // this test is not to test if the admin properly switches).
        for (int i = 0; i < number_join_chat_threads; i++) {
            int passed_remained = 0;
            if (remainder > 0) {
                passed_remained = 1;
                remainder--;
            }

            join_chat_threads.emplace_back(
                    [&chat_rooms, &user_accounts, start, end = (start + total + passed_remained)] {
                        std::cout << std::string(
                                "join_chat_room start: " + std::to_string(start) + " end: " + std::to_string(end) +
                                "\n");
                        for (int j = start; j < end; j++) {
                            grpc_chat_commands::CreateChatRoomRequest create_chat_room_request;
                            grpc_chat_commands::CreateChatRoomResponse create_chat_room_response;

                            setupUserLoginInfo(
                                    create_chat_room_request.mutable_login_info(),
                                    user_accounts[j]->user_account.current_object_oid,
                                    user_accounts[j]->user_account.logged_in_token,
                                    user_accounts[j]->user_account.installation_ids.front()
                            );

                            createChatRoom(&create_chat_room_request, &create_chat_room_response);
                            ASSERT_EQ(ReturnStatus::SUCCESS, create_chat_room_response.return_status());

                            for (auto& user_account: user_accounts) {
                                user_account->chat_rooms[j].chat_room_id = create_chat_room_response.chat_room_id();
                            }

                            user_accounts[j]->chat_rooms[j].times_joined_left.emplace_back(
                                    create_chat_room_response.last_activity_time_timestamp()
                            );

                            chat_rooms[j].create_chat_room_response = std::move(create_chat_room_response);
                            chat_rooms[j].admin_account_oid = user_accounts[j]->user_account.current_object_oid.to_string();
                        }
                    }
            );

            start += total + passed_remained;
        }

        for (auto& thread: join_chat_threads) {
            thread.join();
        }

        for (int i = 0; i < (int) user_accounts.size(); ++i) {
            for (int j = 0; j < (int) chat_rooms.size(); ++j) {
                if (i != j) {
                    user_accounts[i]->currently_not_in_chat_rooms.emplace_back(
                            chat_rooms[j].create_chat_room_response.chat_room_id(),
                            j
                    );
                }
            }
        }

        join_chat_threads.clear();

        std::jthread server_thread = startServer(grpcServerImpl);

        //Start chat change stream last AFTER chat room has been created and user's joined it. This way
        // it will not get the new chat room message.
        std::jthread chat_change_stream_thread = std::jthread(
                [&]() {
                    beginChatChangeStream();
                }
        );

        const std::chrono::milliseconds original_time_chat_stream_stays_active = chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE;
        const std::chrono::milliseconds original_time_chat_stream_refresh_allowed = chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED;

        chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = total_time_chat_stream_active;
        chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = std::chrono::milliseconds{
                (long) ((double) total_time_chat_stream_active.count() * .95)
        };
        print_stream_stuff = false;

        //allow server and change stream to start
        std::this_thread::sleep_for(std::chrono::milliseconds{50});

        const std::chrono::milliseconds loop_start_time = getCurrentTimestamp();
        const std::chrono::milliseconds end_time = loop_start_time + total_time_to_send_messages;

        std::atomic_int clients_passed_send_loop = 0;

        std::atomic_long time_spent_sending_messages = 0;
        std::atomic_long time_spent_joining_chat_rooms = 0;
        std::atomic_long time_spent_leaving_chat_rooms = 0;
        std::atomic_long time_spent_kicking_from_chat_rooms = 0;
        std::atomic_long time_spent_refreshing_bi_di_stream = 0;
        std::atomic_long num_messages_injected_different_user_joined = 0;
        std::atomic_long time_spent_adding_recent_uuids = 0;

        std::cout << "About to start primary loop." << std::endl;
        for (int k = 0; k < num_clients; ++k) {
            threads[k] = std::make_unique<std::jthread>(
                    [&, k]() {

                        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
                        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;
                        mongocxx::database chat_rooms_db = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME];

                        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

                        std::shared_ptr<TestingClient> client = std::make_shared<TestingClient>();

                        //the user is only in a single chat room at this point
                        std::string chat_rooms_string = std::string(user_accounts[k]->chat_rooms[k].chat_room_id).
                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                append(std::to_string(-1)).
                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                append(std::to_string(-1)).
                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                append(std::to_string(0)).
                                append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

                        std::chrono::milliseconds most_recent_received_timestamp{-1};

                        //ensure only 1 message (or group of messages) can be requested at a time
                        std::atomic_bool outstanding_message_request = false;
                        std::atomic_int number_new_messages_received_or_sent = 0;
                        std::atomic_int num_outstanding_refresh_messages = 0;

                        std::atomic_bool initialization_complete = false;
                        std::atomic_bool completed_stream = false;
                        std::atomic_int current_stream_index;

                        const std::function<void(
                                const grpc_stream_chat::ChatToClientResponse& response)> callback_when_message_received = [&](
                                const grpc_stream_chat::ChatToClientResponse& response) {

                            const std::chrono::milliseconds lambda_start = getCurrentTimestamp();
                            num_messages_received++;
                            if (response.server_response_case() ==
                                grpc_stream_chat::ChatToClientResponse::kReturnNewChatMessage) {

                                checkJoinedLeftResponse(
                                        response.return_new_chat_message().messages_list(),
                                        *user_accounts[k],
                                        k,
                                        chat_rooms,
                                        num_times_duplicate_messages_received,
                                        [&](const ChatMessageToClient& msg) {
                                            if (msg.message().standard_message_info().internal_force_send_message_to_current_user()) {
                                                num_messages_injected_different_user_joined++;
                                                return SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_DIFFERENT_USER_JOINED;
                                            } else {
                                                return SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_NEW_MESSAGE;
                                            }
                                        }
                                );
                            } else if (response.server_response_case() ==
                                       grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesResponse
                                    ) {

                                checkJoinedLeftResponse(
                                        response.initial_connection_messages_response().messages_list(),
                                        *user_accounts[k],
                                        k,
                                        chat_rooms,
                                        num_times_duplicate_messages_received,
                                        [](const ChatMessageToClient&) {
                                            return SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_INITIALIZE;
                                        }

                                );
                            } else if (response.server_response_case() ==
                                       grpc_stream_chat::ChatToClientResponse::kInitialConnectionMessagesCompleteResponse) {
                                initialization_complete = true;
                            }

                            const std::chrono::milliseconds timestamp = getCurrentTimestamp();
                            total_client_run_time += timestamp.count() - lambda_start.count();
                            most_recent_received_timestamp = timestamp;
                        };

                        TestingStreamOptions testingStreamOptions;
                        testingStreamOptions.setSkipStatusCheck();

                        std::shared_ptr<std::jthread> bi_di_thread =
                                std::make_shared<std::jthread>(
                                        startBiDiStream(
                                                client,
                                                user_accounts[k]->user_account,
                                                callback_when_message_received,
                                                chat_rooms_string,
                                                testingStreamOptions,
                                                [
                                                        stream_idx = current_stream_index.load(),
                                                        &current_stream_index,
                                                        &completed_stream,
                                                        user_account_oid = user_accounts[k]->user_account.current_object_oid.to_string()
                                                ] {
                                                    if (!completed_stream
                                                        && current_stream_index == stream_idx) {
                                                        completed_stream = true;
                                                    }
                                                }
                                        )
                                );

                        //the int is the ChatStreamContainerObject ptr, the long is the most recent timestamp it returned
                        tbb::concurrent_unordered_map<unsigned int, long> chat_stream_container_largest_timestamp;
                        std::atomic_long current_largest_returned_timestamp;

                        total_client_run_time += getCurrentTimestamp().count() - current_timestamp.count();

                        if (k == num_clients - 1) {
                            std::cout << "Finished initialization on " << std::to_string(k) << " , took " <<
                                    getCurrentTimestamp().count() - loop_start_time.count() << "ms." << std::endl;
                        }

                        //pause while all threads have their bi-di streams started
                        std::this_thread::sleep_for(
                                std::chrono::milliseconds{num_clients < 100 ? 100 : (long) (num_clients)});
                        const std::chrono::milliseconds operation_loop_start_time = getCurrentTimestamp();
                        for (std::chrono::milliseconds timestamp = getCurrentTimestamp();
                             timestamp < end_time; timestamp = getCurrentTimestamp()) {

                            int choice = rand() % 30;
                            const std::chrono::milliseconds choices_start_time = getCurrentTimestamp();

                            //NOTE: 'join' should be greater than leave+kicked+banned to keep the # joined
                            // at least even (ban removes the ability to join a chat room).
                            // nothing 1, send 9, join 10, leave 4, kicked 5, new stream 1

                            if (choice == 0) {
                                //do nothing
                            } else if (choice < 10) { //send a text message to a chat room

                                //get random chat room that this user is part of
                                std::string chat_room_id;
                                int chat_room_index = -1;

                                {
                                    std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);
                                    if (user_accounts[k]->currently_inside_chat_rooms.empty()) { //if user is ONLY inside 'their' chat room
                                        chat_room_id = user_accounts[k]->chat_rooms[k].chat_room_id;
                                        chat_room_index = k;
                                    } else {
                                        size_t random_num = rand() %
                                                            (user_accounts[k]->currently_inside_chat_rooms.size() + 1);

                                        if (random_num == user_accounts[k]->currently_inside_chat_rooms.size()) {
                                            chat_room_id = user_accounts[k]->chat_rooms[k].chat_room_id;
                                            chat_room_index = k;
                                        } else {
                                            chat_room_id = user_accounts[k]->currently_inside_chat_rooms[random_num].chat_room_id;
                                            chat_room_index = user_accounts[k]->currently_inside_chat_rooms[random_num].index_of_chat_room;
                                        }
                                    }
                                }

                                const std::string text_message_uuid = generateUUID();

                                auto [text_message_request, text_message_response] = generateRandomTextMessage(
                                        user_accounts[k]->user_account.current_object_oid,
                                        user_accounts[k]->user_account.logged_in_token,
                                        user_accounts[k]->user_account.installation_ids.front(),
                                        chat_room_id,
                                        text_message_uuid,
                                        gen_random_alpha_numeric_string(
                                                rand() % 100 +
                                                server_parameter_restrictions::MAXIMUM_NUMBER_BYTES_TRIMMED_TEXT_MESSAGE +
                                                1)
                                );

                                EXPECT_EQ(text_message_response.return_status(), ReturnStatus::SUCCESS);

                                if (text_message_response.return_status() == ReturnStatus::SUCCESS
                                    && !text_message_response.user_not_in_chat_room()) {

                                    chat_rooms[chat_room_index].messages.emplace_back(
                                            user_accounts[k]->user_account.current_object_oid.to_string(),
                                            text_message_uuid,
                                            text_message_response.timestamp_stored(),
                                            MessageSpecifics::MessageBodyCase::kTextMessage,
                                            SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_SENT_MESSAGE
                                    );

                                    num_messages_sent++;
                                    number_new_messages_received_or_sent++;
                                }

                                time_spent_sending_messages += (getCurrentTimestamp() - choices_start_time).count();
                            }
                            else if (choice < 20) { //join chat room

                                JoinedLeftUserValues::ChatRoomWithIndex chat_room_with_index("", -1);
                                //pick chat room
                                {
                                    std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);

                                    if (!user_accounts[k]->currently_not_in_chat_rooms.empty()) {
                                        int random_index =
                                                rand() % user_accounts[k]->currently_not_in_chat_rooms.size();
                                        chat_room_with_index = user_accounts[k]->currently_not_in_chat_rooms[random_index];
                                        user_accounts[k]->currently_not_in_chat_rooms.erase(
                                                user_accounts[k]->currently_not_in_chat_rooms.begin() + random_index);
                                    }
                                }

                                //it is possible user is inside all chat rooms, so chat_room_id being empty is OK
                                if (!chat_room_with_index.chat_room_id.empty()) {

                                    grpc_chat_commands::JoinChatRoomRequest join_chat_room_request;

                                    join_chat_room_request.set_chat_room_id(chat_room_with_index.chat_room_id);
                                    join_chat_room_request.set_chat_room_password(
                                            chat_rooms[chat_room_with_index.index_of_chat_room].create_chat_room_response.chat_room_password());

                                    setupUserLoginInfo(
                                            join_chat_room_request.mutable_login_info(),
                                            user_accounts[k]->user_account.current_object_oid,
                                            user_accounts[k]->user_account.logged_in_token,
                                            user_accounts[k]->user_account.installation_ids.front()
                                    );

                                    auto rw = grpc::testing::MockServerWriterVector<grpc_chat_commands::JoinChatRoomResponse>();

                                    int times_joined_left_index;
                                    //Must be pushed BEFORE the join, otherwise messages could be missed that are injected into
                                    // the chat stream.
                                    {
                                        std::scoped_lock<std::mutex> lock(
                                                user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left_mutex);
                                        user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left.emplace_back(
                                                ChatRoomJoinedLeftTimes(-1)
                                        );
                                        times_joined_left_index =
                                                user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left.size() -
                                                1;
                                    }

                                    joinChatRoom(&join_chat_room_request, &rw);

                                    EXPECT_EQ(rw.write_params.front().msg.messages_list(0).return_status(),
                                              ReturnStatus::SUCCESS);

                                    //std::cout << "rw.write_params.front().msg.chat_room_status(): " << rw.write_params.front().msg.chat_room_status() << '\n';
                                    EXPECT_TRUE(rw.write_params.front().msg.chat_room_status() ==
                                                grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED
                                                || rw.write_params.front().msg.chat_room_status() ==
                                                   grpc_chat_commands::ChatRoomStatus::ACCOUNT_WAS_BANNED
                                    );

                                    if (!(rw.write_params.front().msg.chat_room_status() ==
                                          grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED
                                          || rw.write_params.front().msg.chat_room_status() ==
                                             grpc_chat_commands::ChatRoomStatus::ACCOUNT_WAS_BANNED)) {
                                        std::cout << "rw.write_params.front().msg: " << rw.write_params.front().msg.DebugString() << '\n';
                                    }

                                    if (rw.write_params.front().msg.messages_list(0).return_status() ==
                                        ReturnStatus::SUCCESS
                                        && rw.write_params.front().msg.chat_room_status() ==
                                           grpc_chat_commands::ChatRoomStatus::SUCCESSFULLY_JOINED) {

                                        chat_rooms_joined++;

                                        std::vector<SimpleMessageInfo> messages_during_timeframe;

                                        std::string most_recent_message_uuid;
                                        long most_recent_stored_timestamp = -1;

                                        //Can not be kicked until the chat room is inserted back into user_accounts[k]->currently_inside_chat_rooms. So
                                        // this is free to download.
                                        for (const auto& write_result: rw.write_params) {
                                            for (const auto& msg: write_result.msg.messages_list()) {
                                                if (msg.message().message_specifics().message_body_case() ==
                                                    MessageSpecifics::MessageBodyCase::kThisUserJoinedChatRoomStartMessage) {
                                                    std::scoped_lock<std::mutex> lock(
                                                            user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left_mutex);
                                                    user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left[times_joined_left_index].joined_time =
                                                            msg.message().message_specifics().this_user_joined_chat_room_start_message().chat_room_info().time_joined();
                                                } else if (
                                                        msg.message().message_specifics().message_body_case() ==
                                                        MessageSpecifics::MessageBodyCase::kTextMessage
                                                        || msg.message().message_specifics().message_body_case() ==
                                                           MessageSpecifics::MessageBodyCase::kUserKickedMessage
                                                        || msg.message().message_specifics().message_body_case() ==
                                                           MessageSpecifics::MessageBodyCase::kUserBannedMessage
                                                        ) {

                                                    messages_during_timeframe.emplace_back(
                                                            SimpleMessageInfo(
                                                                    msg.sent_by_account_id(),
                                                                    msg.message_uuid(),
                                                                    msg.timestamp_stored(),
                                                                    msg.message().message_specifics().message_body_case(),
                                                                    SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_JOIN_CHAT_ROOM
                                                            )
                                                    );

                                                    if (most_recent_stored_timestamp < msg.timestamp_stored()) {
                                                        most_recent_stored_timestamp = msg.timestamp_stored();
                                                        most_recent_message_uuid = msg.message_uuid();
                                                    }
                                                }
                                            }
                                        }

                                        //Moving these as a group to avoid locking the mutex on every emplace_back.
                                        {
                                            std::scoped_lock<std::mutex> lock(
                                                    user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left_mutex);
                                            std::set<SimpleMessageInfo>& stored_messages =
                                                    user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left[times_joined_left_index].messages_during_timeframe;

                                            size_t original_size = stored_messages.size();
                                            size_t messages_to_move = messages_during_timeframe.size();

                                            stored_messages.insert(
                                                    std::make_move_iterator(messages_during_timeframe.begin()),
                                                    std::make_move_iterator(messages_during_timeframe.end())
                                            );

                                            size_t num_messages_inserted = stored_messages.size() - original_size;

                                            //any messages that were not moved already existed inside the set and are therefore duplicates
                                            num_times_duplicate_messages_received += (int) (messages_to_move -
                                                                                            num_messages_inserted);

                                            if (user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left[times_joined_left_index].most_recent_message_stored_time <
                                                most_recent_stored_timestamp
                                                    ) {
                                                user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left[times_joined_left_index].most_recent_message_stored_time = most_recent_stored_timestamp;
                                                user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left[times_joined_left_index].most_recent_uuid = most_recent_message_uuid;
                                            }

                                        }

                                        {
                                            std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);

                                            user_accounts[k]->currently_inside_chat_rooms.emplace_back(
                                                    chat_room_with_index);

                                            //Make sure this is updated AFTER the rest of the join chat room has completed. This way
                                            // the user cannot be kicked before they have finished the join.
                                            chat_rooms[chat_room_with_index.index_of_chat_room].users_info[
                                                    user_accounts[k]->user_account.current_object_oid.to_string()] = UserInsideChatRoomInfo(
                                                    k,
                                                    AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM
                                            );
                                        }

                                    } else {
                                        std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);
                                        user_accounts[k]->currently_not_in_chat_rooms.emplace_back(
                                                chat_room_with_index);
                                    }
                                }

                                time_spent_joining_chat_rooms += (getCurrentTimestamp() - choices_start_time).count();
                            }
                            else if (choice < 24) { //leave chat room

                                //pick chat room
                                JoinedLeftUserValues::ChatRoomWithIndex chat_room_with_index("", -1);
                                {
                                    std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);

                                    if (!user_accounts[k]->currently_inside_chat_rooms.empty()) {
                                        int random_index =
                                                rand() % user_accounts[k]->currently_inside_chat_rooms.size();
                                        chat_room_with_index = user_accounts[k]->currently_inside_chat_rooms[random_index];
                                        user_accounts[k]->currently_inside_chat_rooms.erase(
                                                user_accounts[k]->currently_inside_chat_rooms.begin() + random_index);

                                        //Change this as soon as the currently_inside_chat_rooms value is extracted. This way 'kicked' will not
                                        // attempt to kick this user (it will fail w/o currently_inside_chat_rooms, however it will waste a cycle).
                                        chat_rooms[chat_room_with_index.index_of_chat_room].users_info[
                                                user_accounts[k]->user_account.current_object_oid.to_string()] = UserInsideChatRoomInfo(
                                                k,
                                                AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM
                                        );
                                    }
                                }

                                //It is possible user is only inside the chat room they are admin of, so chat_room_id
                                // being empty is OK.
                                if (!chat_room_with_index.chat_room_id.empty()) {

                                    int left_index_times_joined_left = -1;
                                    {
                                        std::scoped_lock<std::mutex> lock(
                                                user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left_mutex);
                                        if (user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left.back().left_time ==
                                            -1) {
                                            //insert placeholder timestamp, will be updated after leave has completed
                                            user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left.back().left_time = getCurrentTimestamp().count();
                                            user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left.back().reason_left = ChatRoomJoinedLeftTimes::ReasonLeft::REASON_LEFT_LEFT_CHAT_ROOM;
                                            left_index_times_joined_left =
                                                    (int) user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left.size() -
                                                    1;
                                        }
                                    }

                                    if (left_index_times_joined_left != -1) {

                                        grpc_chat_commands::LeaveChatRoomRequest request;
                                        grpc_chat_commands::LeaveChatRoomResponse response;

                                        request.set_chat_room_id(chat_room_with_index.chat_room_id);

                                        setupUserLoginInfo(
                                                request.mutable_login_info(),
                                                user_accounts[k]->user_account.current_object_oid,
                                                user_accounts[k]->user_account.logged_in_token,
                                                user_accounts[k]->user_account.installation_ids.front()
                                        );

                                        leaveChatRoom(&request, &response);

                                        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

                                        if (response.return_status() == ReturnStatus::SUCCESS) {

                                            chat_rooms_left++;

                                            {
                                                std::scoped_lock<std::mutex> lock(
                                                        user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left_mutex);
                                                //insert placeholder timestamp, will be updated after leave has completed
                                                user_accounts[k]->chat_rooms[chat_room_with_index.index_of_chat_room].times_joined_left[left_index_times_joined_left].left_time = response.timestamp_stored();
                                            }

                                            {
                                                //Do currently vector last, after all other operations have completed.
                                                std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);
                                                user_accounts[k]->currently_not_in_chat_rooms.emplace_back(
                                                        chat_room_with_index);
                                            }
                                        }
                                    }
                                }

                                time_spent_leaving_chat_rooms += (getCurrentTimestamp() - choices_start_time).count();
                            }
                            else if (choice < 29) { //kick from chat room

                                std::vector<std::pair<int, std::string>> users_in_chat_room;

                                //Pick a user inside users' chat room and kick em
                                // from global values is important b/c this is updated
                                // after the chat room has been joined by the other user.
                                for (const auto& ptr: chat_rooms[k].users_info) {
                                    if (ptr.second.load().account_state ==
                                        AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM) {
                                        users_in_chat_room.emplace_back(
                                                ptr.second.load().user_index,
                                                ptr.first
                                        );
                                    }
                                }

                                if (!users_in_chat_room.empty()) {

                                    int random_user = rand() % users_in_chat_room.size();
                                    int user_to_be_kicked_index = users_in_chat_room[random_user].first;

                                    if (user_accounts[user_to_be_kicked_index]->user_account.current_object_oid.to_string() !=
                                        users_in_chat_room[random_user].second) {
                                        std::cout << "FAILED TO MATCH USER TO INDEX\n";
                                    }

                                    //pick chat room
                                    JoinedLeftUserValues::ChatRoomWithIndex chat_room_with_index("", -1);
                                    {
                                        std::scoped_lock<std::mutex> lock(
                                                user_accounts[user_to_be_kicked_index]->currently_mutex);

                                        for (int i = 0; i <
                                                        (int) user_accounts[user_to_be_kicked_index]->currently_inside_chat_rooms.size(); i++) {
                                            if (user_accounts[user_to_be_kicked_index]->currently_inside_chat_rooms[i].chat_room_id
                                                == chat_rooms[k].create_chat_room_response.chat_room_id()) {
                                                chat_room_with_index = user_accounts[user_to_be_kicked_index]->currently_inside_chat_rooms[i];
                                                user_accounts[user_to_be_kicked_index]->currently_inside_chat_rooms.erase(
                                                        user_accounts[user_to_be_kicked_index]->currently_inside_chat_rooms.begin() +
                                                        i);
                                                break;
                                            }
                                        }
                                    }

                                    if (!chat_room_with_index.chat_room_id.empty()) {

                                        grpc_chat_commands::RemoveFromChatRoomRequest request;
                                        grpc_chat_commands::RemoveFromChatRoomResponse response;

                                        setupUserLoginInfo(
                                                request.mutable_login_info(),
                                                user_accounts[k]->user_account.current_object_oid,
                                                user_accounts[k]->user_account.logged_in_token,
                                                user_accounts[k]->user_account.installation_ids.front()
                                        );

                                        const std::string kicked_message_uuid = generateUUID();

                                        request.set_message_uuid(kicked_message_uuid);
                                        request.set_kick_or_ban(
                                                grpc_chat_commands::RemoveFromChatRoomRequest_KickOrBan_KICK);
                                        request.set_chat_room_id(user_accounts[k]->chat_rooms[k].chat_room_id);
                                        request.set_account_id_to_remove(users_in_chat_room[random_user].second);

                                        {
                                            std::scoped_lock<std::mutex> lock(
                                                    user_accounts[user_to_be_kicked_index]->currently_mutex);

                                            //needs to be here in case message is received by user before the function actually completes
                                            user_accounts[user_to_be_kicked_index]->currently_in_limbo_chat_rooms.emplace_back(
                                                    chat_room_with_index
                                            );
                                        }

                                        removeFromChatRoom(&request, &response);

                                        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);

                                        EXPECT_FALSE(response.operation_failed());

                                        if (response.return_status() == ReturnStatus::SUCCESS
                                            && !response.operation_failed()) {

                                            chat_rooms[k].messages.emplace_back(
                                                    user_accounts[k]->user_account.current_object_oid.to_string(),
                                                    kicked_message_uuid,
                                                    response.timestamp_stored(),
                                                    MessageSpecifics::MessageBodyCase::kUserKickedMessage,
                                                    SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_SENT_MESSAGE
                                            );

                                            kicked_members++;
                                            num_messages_sent++;
                                            number_new_messages_received_or_sent++;
                                        }
                                    }
                                }

                                time_spent_kicking_from_chat_rooms += (getCurrentTimestamp() -
                                                                       choices_start_time).count();
                            }
                            else if (choice == 29) { //cancel chat stream and start a new one
                                //NOTE: This possibility takes significantly longer to run than the others, so it has the lowest
                                // chance of being run.
                                num_times_stream_restarted++;

                                chat_rooms_string.clear();

                                {
                                    std::scoped_lock<std::mutex> lock(user_accounts[k]->currently_mutex);

                                    std::vector<JoinedLeftUserValues::ChatRoomWithIndex> current_user_vector{
                                            JoinedLeftUserValues::ChatRoomWithIndex(
                                                    user_accounts[k]->chat_rooms[k].chat_room_id,
                                                    k
                                            )
                                    };

                                    std::vector<std::vector<JoinedLeftUserValues::ChatRoomWithIndex>*> chat_room_vectors{
                                            &current_user_vector,
                                            &user_accounts[k]->currently_inside_chat_rooms,
                                            &user_accounts[k]->currently_in_limbo_chat_rooms
                                    };

                                    for(const auto& single_vector : chat_room_vectors) {
                                        for (const auto& chat_room_info: *single_vector) {

                                            long last_updated_time = -1;

                                            std::vector<std::pair<long, std::string>> message_times;
                                            const std::chrono::milliseconds start_sorting = getCurrentTimestamp();

                                            if (!user_accounts[k]->chat_rooms[chat_room_info.index_of_chat_room].times_joined_left.back().messages_during_timeframe.empty()) {

                                                const std::chrono::milliseconds earliest_timestamp_to_request = getCurrentTimestamp() - chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES;

                                                for(const auto& message : user_accounts[k]->chat_rooms[chat_room_info.index_of_chat_room].times_joined_left.back().messages_during_timeframe) {
                                                    if(message.timestamp_stored >= earliest_timestamp_to_request.count()) {
                                                        message_times.emplace_back(
                                                            message.timestamp_stored,
                                                            message.message_uuid
                                                       );
                                                    }
                                                }

                                                std::sort(message_times.begin(),
                                                          message_times.end(),
                                                          [](const std::pair<long, std::string>& lhs, const std::pair<long, std::string>& rhs) {
                                                              if (lhs.first == rhs.first) {
                                                                  return lhs.second < rhs.second;
                                                              }
                                                              return lhs.first < rhs.first;
                                                          });

                                            }

                                            if(!message_times.empty()) {
                                                last_updated_time = message_times.back().first;
                                            }

                                            chat_rooms_string += std::string(chat_room_info.chat_room_id).
                                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                                    append(std::to_string(last_updated_time)).
                                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                                    append(std::to_string(last_updated_time)).
                                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER).
                                                    append(std::to_string(message_times.size())).
                                                    append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);

                                            for(const auto& val : message_times) {
                                                chat_rooms_string
                                                    .append(val.second)
                                                    .append(chat_stream_container::initial_metadata::CHAT_ROOM_VALUES_DELIMITER);
                                            }

                                            time_spent_adding_recent_uuids += (getCurrentTimestamp() - start_sorting).count();
                                        }
                                    }
                                }

                                current_stream_index++;
                                initialization_complete = false;
                                completed_stream = false;

                                std::shared_ptr<TestingClient> client_two = std::make_shared<TestingClient>();

                                std::shared_ptr<std::jthread> bi_di_thread_2 =
                                        std::make_shared<std::jthread>(
                                                startBiDiStream(
                                                        client_two,
                                                        user_accounts[k]->user_account,
                                                        callback_when_message_received,
                                                        chat_rooms_string,
                                                        testingStreamOptions,
                                                        [
                                                                stream_idx = current_stream_index.load(),
                                                                &current_stream_index,
                                                                &completed_stream,
                                                                user_account_oid = user_accounts[k]->user_account.current_object_oid.to_string()
                                                        ] {
                                                            if (!completed_stream
                                                                && current_stream_index == stream_idx) {
                                                                completed_stream = true;
                                                            }
                                                        }
                                                )
                                        );

                                bi_di_thread->join();
                                std::atomic_store(&bi_di_thread, bi_di_thread_2);
                                std::atomic_store(&client, client_two);
                                time_spent_refreshing_bi_di_stream += (getCurrentTimestamp() -
                                                                       choices_start_time).count();
                            }
                            // else {}
                            // Skipping testing these.
                            // Server shutdown (slow, would require coordination between threads and server).
                            // Inject_message (this is already done by the change stream).
                            // Join & leave chat rooms (slow, will need a thread safe local data structure for this).

                            //There are problems if a join happens and a kick is outstanding because the
                            // server can send back a message to leave AFTER the kick is received and
                            // the chat room can join_1->leave_1->join_2->leave_1(server init). This
                            // makes no difference to a 'real' client.
                            for (int i = 0, j = 0; !initialization_complete; ++i, ++j) {
                                if (i == 8) {
                                    static const timespec ns = {0, 1}; //1 nanosecond
                                    nanosleep(&ns, nullptr);
                                    i = 0;
                                }
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds{
                                    rand() % sleep_time_between_client_actions.count() + 10
                            });
                        }

                        int temp_passed_send_loop = clients_passed_send_loop.fetch_add(1);
                        std::cout << std::string(
                                std::to_string(temp_passed_send_loop + 1) + " clients completed\n") << std::flush;
                        if (temp_passed_send_loop == num_clients - 1) {
                            std::cout << "All clients have finished operations. Sleeping for catchup." << std::endl;
                            std::cout << "Operations took " << (getCurrentTimestamp() -
                                                                operation_loop_start_time).count() << "ms to run." << std::endl;
                        }

                        //sleep to make sure all messages are received
                        if (total_time_to_wait_to_complete.count() > 0) {
                            const std::chrono::milliseconds sleep_start_timestamp = getCurrentTimestamp();
                            std::this_thread::sleep_for(total_time_to_wait_to_complete);
                            const std::chrono::milliseconds sleep_stop_timestamp = getCurrentTimestamp();
                            //If a message was received AFTER sleep was started (the messages were lagging).
                            if (sleep_start_timestamp < most_recent_received_timestamp) {
                                user_accounts[k]->extra_time_to_send_back_messages =
                                        most_recent_received_timestamp.count() - sleep_start_timestamp.count();
                            }
                        }

                        client->finishBiDiStream();

                        bi_di_thread->join();
                    }
            );
        }

        std::cout << "small_stress_testing.cpp JOINING client threads\n";
        for (auto& thread: threads) {
            thread->join();
        }
        threads.clear();

        std::cout << "SHUTTING_DOWN" << std::endl;
        grpcServerImpl->blockWhileWaitingForShutdown(std::chrono::milliseconds{200});
        server_thread.join();
        chat_change_stream_thread.join();

        std::unordered_map<std::string, std::vector<SimpleMessageInfo>> extracted_messages_in_chat_rooms;

        mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
        mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

        long total_number_times_joined_left = 0;
        long total_number_messages_inside_times_joined_left = 0;

        for (const auto& chat_room: chat_rooms) {
            //check user account states (admin is different)
            //make sure all messages were collected (differentUserJoined?)

            mongocxx::collection chat_room_collection = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME][
                    collection_names::CHAT_ROOM_ID_ + chat_room.create_chat_room_response.chat_room_id()];

            mongocxx::options::find find_opts;

            find_opts.projection(
                    document{}
                            << "_id" << 1
                            << chat_room_shared_keys::TIMESTAMP_CREATED << 1
                            << chat_room_message_keys::MESSAGE_SENT_BY << 1
                            << chat_room_message_keys::MESSAGE_TYPE << 1
                            << finalize
            );

            auto chat_room_messages = chat_room_collection.find(
                    document{}
                            << "_id" << open_document
                            << "$ne" << chat_room_header_keys::ID
                            << close_document
                            << chat_room_message_keys::MESSAGE_TYPE << open_document
                            << "$in" << open_array
                            //these message types do not return their message uuid to the caller
                            << MessageSpecifics::MessageBodyCase::kTextMessage
                            << MessageSpecifics::MessageBodyCase::kUserKickedMessage
                            << MessageSpecifics::MessageBodyCase::kUserBannedMessage
                            << close_array
                            << close_document
                            << finalize,
                    find_opts
            );

            auto inserted_ele = extracted_messages_in_chat_rooms.insert(
                    std::make_pair(
                            chat_room.create_chat_room_response.chat_room_id(),
                            std::vector<SimpleMessageInfo>{}
                    )
            );

            std::vector<SimpleMessageInfo> extracted_messages;
            for (const bsoncxx::document::view& msg_doc: chat_room_messages) {

                inserted_ele.first->second.emplace_back(
                        msg_doc[chat_room_message_keys::MESSAGE_SENT_BY].get_oid().value.to_string(),
                        msg_doc["_id"].get_string().value.to_string(),
                        msg_doc[chat_room_shared_keys::TIMESTAMP_CREATED].get_date().value.count(),
                        MessageSpecifics::MessageBodyCase(
                                msg_doc[chat_room_message_keys::MESSAGE_TYPE].get_int32().value),
                        SimpleMessageInfoFrom::SIMPLE_MESSAGE_INFO_FROM_MANUALLY_EXTRACTED
                );
            }

            std::sort(inserted_ele.first->second.begin(), inserted_ele.first->second.end(),
                      [](const SimpleMessageInfo& lhs, const SimpleMessageInfo& rhs) {
                          if (lhs.timestamp_stored == rhs.timestamp_stored) {
                              return lhs.message_uuid < rhs.message_uuid;
                          }
                          return lhs.timestamp_stored < rhs.timestamp_stored;
                      });

        }

        size_t total_num_missed_messages = 0;

        //make sure local user chat room list matches
        for (int i = 0; i < (int) user_accounts.size(); ++i) {
            //for (auto& chat_room : user_accounts[i]->chat_rooms) {
            for (int j = 0; j < (int) user_accounts[i]->chat_rooms.size(); ++j) {

                UserChatRoomInfo& chat_room = user_accounts[i]->chat_rooms[j];

                total_number_times_joined_left += (long) chat_room.times_joined_left.size();

                mongocxx::collection chat_room_collection = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME][
                        collection_names::CHAT_ROOM_ID_ + chat_room.chat_room_id];

                std::vector<SimpleMessageInfo>& extracted_from_database_messages = extracted_messages_in_chat_rooms[chat_room.chat_room_id];

                AccountStateInChatRoom final_account_state_in_chat_room = AccountStateInChatRoom::AccountStateInChatRoom_INT_MAX_SENTINEL_DO_NOT_USE_;
                for (int k = 0; k < (int) chat_room.times_joined_left.size(); ++k) {
                    //If no 'left_time' set, user should have every message from chat room (never left) AND it should be the last element
                    //If a 'left_time' was set, just check messages until the final message is received

                    total_number_messages_inside_times_joined_left += (long) chat_room.times_joined_left[k].messages_during_timeframe.size();

                    //copy values to vector for sorting
                    std::vector<SimpleMessageInfo> messages_during_timeframe(
                            chat_room.times_joined_left[k].messages_during_timeframe.begin(),
                            chat_room.times_joined_left[k].messages_during_timeframe.end()
                    );

                    std::sort(messages_during_timeframe.begin(),
                              messages_during_timeframe.end(),
                              [](const SimpleMessageInfo& lhs, const SimpleMessageInfo& rhs) {
                                  if (lhs.timestamp_stored == rhs.timestamp_stored) {
                                      return lhs.message_uuid < rhs.message_uuid;
                                  }
                                  return lhs.timestamp_stored < rhs.timestamp_stored;
                              }
                    );

                    //Trim the final few message because there can (and most likely will) be ordering issues.
                    if (r == join_chat_room_without_time_received_index) {

                        long final_message_time = 0;

                        if (chat_room.times_joined_left[k].left_time == -1) {
                            final_message_time = chat_room.times_joined_left[k].most_recent_message_stored_time;

                        } else if (chat_room.times_joined_left[k].most_recent_message_stored_time == -1) {
                            final_message_time = chat_room.times_joined_left[k].left_time;
                        } else {
                            final_message_time = std::min(
                                    chat_room.times_joined_left[k].most_recent_message_stored_time,
                                    chat_room.times_joined_left[k].left_time);
                        }

                        final_message_time -= max_time_value_for_repeats.count();

                        for (int l = (int) messages_during_timeframe.size() - 1; l >= 0; l--) {
                            if (messages_during_timeframe[l].timestamp_stored >= final_message_time) {
                                messages_during_timeframe.pop_back();
                            } else {
                                break;
                            }
                        }
                    }

                    if (j == i) { //users' chat room
                        EXPECT_EQ(k, chat_room.times_joined_left.size() - 1);
                        final_account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN;
                    } else if (chat_room.times_joined_left[k].left_time == -1) { //still in chat room
                        EXPECT_EQ(k, chat_room.times_joined_left.size() - 1);
                        if (k != (int) chat_room.times_joined_left.size() - 1) {
                            std::cout << "X";
                        }
                        final_account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_IN_CHAT_ROOM;
                    } else { //left chat room
                        if (chat_room.times_joined_left[k].reason_left ==
                            ChatRoomJoinedLeftTimes::REASON_LEFT_BANNED_CHAT_ROOM) {
                            final_account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_BANNED;
                            EXPECT_EQ(k, chat_room.times_joined_left.size() -
                                         1); //banned should be last interaction w/ chat room
                        } else { //left or kicked
                            final_account_state_in_chat_room = AccountStateInChatRoom::ACCOUNT_STATE_NOT_IN_CHAT_ROOM;
                        }
                    }

                    //Make sure there were no gaps in the messages
                    int extracted_from_database_index = 0, messages_during_timeframe_index = 0;
                    while (extracted_from_database_index < (int) extracted_from_database_messages.size()
                           && messages_during_timeframe_index < (int) messages_during_timeframe.size()) {

                        bool result = compareSimpleMessageInfo(
                                messages_during_timeframe,
                                messages_during_timeframe_index,
                                extracted_from_database_messages,
                                extracted_from_database_index,
                                chat_room_collection,
                                user_accounts[i]->user_account.current_object_oid.to_string()
                        );

                        if (!result) {
                            std::cout << "time_message_sent_before_left: " << chat_room.times_joined_left[k].left_time -
                                                                              extracted_from_database_messages[
                                                                                      extracted_from_database_index -
                                                                                      1].timestamp_stored << " ms\n"
                                    << "time_message_sent_before_last_message: " <<
                                    chat_room.times_joined_left[k].most_recent_message_stored_time -
                                    extracted_from_database_messages[extracted_from_database_index -
                                                                     1].timestamp_stored << " ms\n"
                                    << "time_message_sent_before_join: " << chat_room.times_joined_left[k].joined_time -
                                                                            extracted_from_database_messages[
                                                                                    extracted_from_database_index -
                                                                                    1].timestamp_stored << " ms\n"
                                    << "time_client_spent_in_chat_room: " << chat_room.times_joined_left[k].left_time -
                                                                             chat_room.times_joined_left[k].joined_time << " ms\n"
                                    << "delay_for_message_ordering: " << chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING.count() << " ms\n";

                            if (messages_during_timeframe_index > 0) {
                                std::cout
                                        << "previous  message_came_from: " << convertSimpleMessageInfoFromToString(
                                        messages_during_timeframe[messages_during_timeframe_index -
                                                                  1].message_came_from) << '\n'
                                        << "following message_came_from: " << convertSimpleMessageInfoFromToString(
                                        messages_during_timeframe[messages_during_timeframe_index].message_came_from) << '\n';
                            }

                            total_num_missed_messages++;
                        }
                        EXPECT_TRUE(result);
                    }

                    //Generated should be the index to reach max, it is fine if extracted were not all reached. This could
                    // happen if the user left a chat room.
                    EXPECT_EQ(messages_during_timeframe_index, (int) messages_during_timeframe.size());
                }

                //This means that this user joined the chat room at some point.
                if (final_account_state_in_chat_room !=
                    AccountStateInChatRoom::AccountStateInChatRoom_INT_MAX_SENTINEL_DO_NOT_USE_) {

                    mongocxx::options::find find_opts;

                    find_opts.projection(
                            document{}
                                    << "_id" << 0
                                    << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".").append(
                                            chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID) << 1
                                    << std::string(chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM).append(".").append(
                                            chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM) << 1
                                    << finalize
                    );

                    auto chat_room_header_doc = chat_room_collection.find_one(
                            document{}
                                    << "_id" << chat_room_header_keys::ID
                                    << finalize,
                            find_opts
                    );

                    EXPECT_TRUE(chat_room_header_doc);
                    if (chat_room_header_doc) {
                        bool found_user = false;
                        bsoncxx::array::view accounts_arr = chat_room_header_doc->view()[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM].get_array().value;
                        for (const auto& ele: accounts_arr) {
                            bsoncxx::document::view account_doc = ele.get_document().value;

                            std::string user_account_oid = account_doc[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID].get_oid().value.to_string();
                            if (user_account_oid == user_accounts[i]->user_account.current_object_oid.to_string()) {
                                found_user = true;
                                AccountStateInChatRoom extracted_account_state = AccountStateInChatRoom(
                                        account_doc[chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM].get_int32().value);

                                EXPECT_EQ(
                                        extracted_account_state,
                                        final_account_state_in_chat_room
                                );

                                if (extracted_account_state != final_account_state_in_chat_room) {
                                    std::cout << "x\n";
                                }

                                break;
                            }
                        }
                        EXPECT_TRUE(found_user);
                    } else {
                        std::cout << chat_room.chat_room_id << '\n';
                    }
                }
            }
        }

        if (total_num_missed_messages > 0) {
            std::cout << "total_num_missed_messages: " << total_num_missed_messages << '\n';
        }

        EXPECT_EQ(map_of_chat_rooms_to_users.size(), num_clients);
        for (const auto& x: map_of_chat_rooms_to_users) {
            EXPECT_TRUE(x.second.map.empty());
            if (!x.second.map.empty()) {
                std::cout << "chat_room_id: " << x.first << '\n';
                for (const auto& [user_account_oid, index_num]: x.second.map) {
                    std::cout << "   " << x.first << "," << user_account_oid << "," << index_num << '\n';
                    for (int i = 0; i < num_clients; ++i) {
                        if (user_accounts[i]->user_account.current_object_oid.to_string()
                            == user_account_oid) {
                            for (int j = 0; j < num_clients; ++j) {
                                if (user_accounts[i]->chat_rooms[j].chat_room_id
                                    == x.first) {
                                    if (!user_accounts[i]->chat_rooms[j].times_joined_left.empty()) {
                                        std::cout << "   left_time: " << user_accounts[i]->chat_rooms[j].times_joined_left.back().left_time << " reason_for_leaving: " << ChatRoomJoinedLeftTimes::convertReasonLeftToString(
                                                user_accounts[i]->chat_rooms[j].times_joined_left.back().reason_left) << '\n';
                                    }
                                    break;
                                }
                            }

                            break;
                        }
                    }
                }
            }
        }

        EXPECT_EQ(user_open_chat_streams.num_values_stored, 0);

        for (const auto& chat_room: chat_rooms) {
            //check user account states (admin is different)
            //make sure all messages were collected (differentUserJoined?)

            mongocxx::collection chat_room_collection = mongo_cpp_client[database_names::CHAT_ROOMS_DATABASE_NAME][
                    collection_names::CHAT_ROOM_ID_ + chat_room.create_chat_room_response.chat_room_id()];

            auto chat_room_header_doc = chat_room_collection.find_one(
                    document{}
                            << "_id" << chat_room_header_keys::ID
                            << finalize
            );

            //check account states
            EXPECT_TRUE(chat_room_header_doc);
            if (chat_room_header_doc) {
                bsoncxx::array::view accounts_arr = chat_room_header_doc->view()[chat_room_header_keys::ACCOUNTS_IN_CHAT_ROOM].get_array().value;
                size_t num_accounts = std::distance(accounts_arr.begin(), accounts_arr.end());
                EXPECT_EQ(num_accounts, chat_room.users_info.size() + 1); // + 1 is for admin account
                for (const auto& ele: accounts_arr) {
                    bsoncxx::document::view account_doc = ele.get_document().value;

                    std::string extracted_account_oid = account_doc[chat_room_header_keys::accounts_in_chat_room::ACCOUNT_OID].get_oid().value.to_string();
                    AccountStateInChatRoom extracted_account_state = AccountStateInChatRoom(
                            account_doc[chat_room_header_keys::accounts_in_chat_room::STATE_IN_CHAT_ROOM].get_int32().value);

                    if (extracted_account_oid == chat_room.admin_account_oid) {
                        EXPECT_EQ(AccountStateInChatRoom::ACCOUNT_STATE_IS_ADMIN, extracted_account_state);
                    } else {
                        auto user_ptr = chat_room.users_info.find(extracted_account_oid);
                        EXPECT_NE(user_ptr, chat_room.users_info.end());
                        if (user_ptr != chat_room.users_info.end()) {
                            EXPECT_EQ(user_ptr->second.load().account_state, extracted_account_state);
                        }
                    }
                }
            }

            std::vector<SimpleMessageInfo> generated_messages;
            std::vector<SimpleMessageInfo>& extracted_messages = extracted_messages_in_chat_rooms[chat_room.create_chat_room_response.chat_room_id()];

            for (auto& message_info: chat_room.messages) {
                generated_messages.emplace_back(
                        SimpleMessageInfo(
                                message_info.send_by_oid,
                                message_info.message_uuid,
                                message_info.timestamp_stored,
                                message_info.message_type,
                                message_info.message_came_from
                        )
                );
            }

            std::sort(generated_messages.begin(), generated_messages.end(),
                      [](const SimpleMessageInfo& lhs, const SimpleMessageInfo& rhs) {
                          if (lhs.timestamp_stored == rhs.timestamp_stored) {
                              return lhs.message_uuid < rhs.message_uuid;
                          }
                          return lhs.timestamp_stored < rhs.timestamp_stored;
                      });

            EXPECT_EQ(generated_messages.size(), extracted_messages.size());

            int generated_index = 0, extracted_index = 0;
            while (generated_index < (int) generated_messages.size() &&
                   extracted_index < (int) extracted_messages.size()) {
                bool result = compareSimpleMessageInfo(
                        generated_messages,
                        generated_index,
                        extracted_messages,
                        extracted_index,
                        chat_room_collection,
                        ""
                );
                if (!result) {
                    std::cout << "X";
                }
                EXPECT_TRUE(result);
            } //Don't need to check the rest, an error should have already occurred.

        }

        std::cout << "num_messages_sent: " << num_messages_sent << '\n';
        std::cout << "num_messages_received: " << num_messages_received << '\n';
        std::cout << "num_times_stream_restarted: " << num_times_stream_restarted << '\n';
        std::cout << "num_times_duplicate_messages_received: " << num_times_duplicate_messages_received << '\n';
        std::cout << "num_messages_injected_different_user_joined: " << num_messages_injected_different_user_joined << '\n';
        std::cout << "time_spent_adding_recent_uuids: " << time_spent_adding_recent_uuids << " ms\n";

        std::cout << "\ntotal_number_times_joined_left: " << total_number_times_joined_left << '\n';
        std::cout << "total_number_messages_inside_times_joined_left: " << total_number_messages_inside_times_joined_left << '\n';

        std::cout << "\nchat_rooms_joined: " << chat_rooms_joined << '\n';
        std::cout << "chat_rooms_left: " << chat_rooms_left << '\n';
        std::cout << "kicked_members: " << kicked_members << '\n';

        std::cout << "\ntotal_client_run_time: " << total_client_run_time << '\n';
        std::cout << "time_spent_sending_messages: " << time_spent_sending_messages << '\n';
        std::cout << "time_spent_joining_chat_rooms: " << time_spent_joining_chat_rooms << '\n';
        std::cout << "time_spent_leaving_chat_rooms: " << time_spent_leaving_chat_rooms << '\n';
        std::cout << "time_spent_kicking_from_chat_rooms: " << time_spent_kicking_from_chat_rooms << '\n';
        std::cout << "time_spent_refreshing_bi_di_stream: " << time_spent_refreshing_bi_di_stream << '\n';

        long largest_lag_time = 0; //this will probably be user[0], they are a member of every chat room
        long average_lag_time = 0;
        for (const auto& acct: user_accounts) {
            average_lag_time += acct->extra_time_to_send_back_messages;
            largest_lag_time =
                    acct->extra_time_to_send_back_messages > largest_lag_time ? acct->extra_time_to_send_back_messages
                                                                              : largest_lag_time;
        }

        //These values show how long after the messages stopped being sent it took the server to 'catch up'.
        std::cout << "\naverage_lag_time: " << (user_accounts.empty() ? 0 : average_lag_time /
                                                                            user_accounts.size()) << '\n';
        std::cout << "largest_lag_time: " << largest_lag_time << '\n';
        std::cout << "approximate sleep_time: " << total_time_to_wait_to_complete.count() << '\n';

        std::cout << "thread_pool.num_jobs_outstanding: " << thread_pool.num_jobs_outstanding() << '\n';

        std::cout << "total_test_run_time: " << (getCurrentTimestamp() - test_start_time).count() << '\n';

        EXPECT_TRUE(
                ChatStreamContainerObject::num_new_objects_created == ChatStreamContainerObject::num_objects_deleted
                ||
                ChatStreamContainerObject::num_new_objects_created == ChatStreamContainerObject::num_objects_deleted +
                                                                      1 //an object could be created and 'inside' the completion queue that has never been used
        );

        chat_stream_container::TIME_CHAT_STREAM_STAYS_ACTIVE = original_time_chat_stream_stays_active;
        chat_stream_container::TIME_CHAT_STREAM_REFRESH_ALLOWED = original_time_chat_stream_refresh_allowed;
        general_values::TIME_BETWEEN_TOKEN_VERIFICATION = original_time_between_token_verification;
        print_stream_stuff = true;

        clearDatabaseAndGlobalsForTesting();
    }

    chat_change_stream_values::DELAY_FOR_MESSAGE_ORDERING = original_delay_for_message_ordering;
    chat_change_stream_values::MESSAGE_ORDERING_THREAD_SLEEP_TIME = original_message_ordering_thread_sleep_time;
    testing_delay_for_messages = original_testing_delay_for_messages;
    chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES = original_time_to_request_previous_messages;

    //Sleep to allow database to catch up. Otherwise, it could affect other tests
    std::this_thread::sleep_for(std::chrono::seconds{30});
}


