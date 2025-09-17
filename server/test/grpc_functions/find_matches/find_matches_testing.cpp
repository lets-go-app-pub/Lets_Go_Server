//
// Created by jeremiah on 9/5/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "utility_find_matches_functions.h"
#include "helper_functions/generate_matching_users.h"
#include "find_matches.h"
#include "grpc_mock_stream/mock_stream.h"
#include "setup_login_info.h"
#include "generate_randoms.h"
#include "event_request_message_is_valid.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "helper_functions/find_matches_helper_functions.h"
#include "activities_info_keys.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class FindMatchesTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    findmatches::FindMatchesRequest request;
    grpc::testing::MockServerWriterVector<findmatches::FindMatchesResponse> response;

    enum PlaceMatchExists {
        NOT_YET_MATCHED,
        INSIDE_ALGORITHM_MATCHED,
        INSIDE_OTHER_USERS_MATCHED
    };

    inline static const long DELAY_IN_MILLIS = 200;

    std::vector<std::pair<bsoncxx::oid, PlaceMatchExists>> matching_account_oids;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front()
        );

        request.set_number_messages(matching_algorithm::MAXIMUM_NUMBER_RESPONSE_MESSAGES);
        request.set_client_longitude(user_account_doc.location.longitude);
        request.set_client_latitude(user_account_doc.location.latitude);

        //allow the algorithm to run
        user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{
                getCurrentTimestamp() - matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS * 10};

        //don't let any timeframes that may be trimmed exist
        const std::chrono::milliseconds earliest_timestamp = getCurrentTimestamp() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED + std::chrono::milliseconds{100};
        for(auto& category : user_account_doc.categories) {
            for(auto it = category.time_frames.begin(); it != category.time_frames.end();) {
                if(it->time <= earliest_timestamp.count()) {
                    it = category.time_frames.erase(it);
                } else {
                    break;
                }
            }
        }

        user_account_doc.setIntoCollection();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        //NOTE: Because the algorithm is generally run on the secondaries, and it can take some time for
        // the information to propagate to the secondaries, it is important to sleep here.
        // Also, it guarantees that timestamps are unique.
        std::this_thread::sleep_for(std::chrono::milliseconds{DELAY_IN_MILLIS});

        findMatches(&request, &response);
    }

    void checkNothingChanged() {
        UserAccountDoc extracted_account_doc(user_account_oid);

        EXPECT_EQ(extracted_account_doc, user_account_doc);
    }

    //The idea here is that the value can be put inside matching_account_oids in the order they are expected to
    // be received. Then most of the process can be automated away. So if multiple of this function is run
    // inside a single test, run them in the order they are expected out.
    void buildMatchingUser(PlaceMatchExists place_match_exists) {

        const bsoncxx::oid match_user_account_oid = insertRandomAccounts(1, 0);

        UserAccountDoc match_account_doc(match_user_account_oid);

        bool return_value = buildMatchingUserForPassedAccount(
                user_account_doc,
                match_account_doc,
                user_account_oid,
                match_user_account_oid
        );

        matching_account_oids.emplace_back(
                std::make_pair<bsoncxx::oid, PlaceMatchExists>(bsoncxx::oid{match_user_account_oid},
                                                               PlaceMatchExists(place_match_exists))
        );

        ASSERT_TRUE(return_value);

        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

        switch (place_match_exists) {
            case NOT_YET_MATCHED:
                break;
            case INSIDE_ALGORITHM_MATCHED: {
                MatchingElement inserted_match{
                        match_user_account_oid,
                        1234.56,
                        0.0,
                        bsoncxx::types::b_date{
                                current_timestamp + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES},
                        bsoncxx::types::b_date{current_timestamp},
                        true,
                        bsoncxx::oid{}
                };

                inserted_match.initializeActivityStatistics();

                user_account_doc.algorithm_matched_accounts_list.emplace_back(
                        inserted_match
                );

                user_account_doc.setIntoCollection();
                break;
            }
            case INSIDE_OTHER_USERS_MATCHED: {
                MatchingElement inserted_match{
                        match_user_account_oid,
                        1234.56,
                        0.0,
                        bsoncxx::types::b_date{
                                current_timestamp + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES},
                        bsoncxx::types::b_date{current_timestamp},
                        false,
                        bsoncxx::oid{}
                };

                user_account_doc.other_users_matched_accounts_list.emplace_back(
                        inserted_match
                );

                user_account_doc.setIntoCollection();
                break;
            }
        }

    }

    struct NonMatchingActivityReturnValues {
        const int category_index = 0;
        const int activity_index = 0;

        NonMatchingActivityReturnValues() = delete;

        NonMatchingActivityReturnValues(
                int _category_index,
                int _activity_index
        ) :
                category_index(_category_index),
                activity_index(_activity_index) {}
    };

    NonMatchingActivityReturnValues findNonMatchingActivity() {
        mongocxx::collection activities_collection = accounts_db[collection_names::ACTIVITIES_INFO_COLLECTION_NAME];

        const bsoncxx::stdx::optional<bsoncxx::document::value> doc = activities_collection.find_one(
                document{}
                        << "_id" << activities_info_keys::ID
                        << finalize
        );

        const bsoncxx::document::view doc_view = doc->view();

        const bsoncxx::array::view categories_array = doc_view[activities_info_keys::CATEGORIES].get_array().value;
        const bsoncxx::array::view activities_array = doc_view[activities_info_keys::ACTIVITIES].get_array().value;

        int final_category_index = 0;
        int final_activity_index = 0;
        int activity_index = 0;
        const int categories_start = (int)user_account_doc.categories.size() / 2 - 1;
        for (const auto& x: activities_array) {
            const bsoncxx::document::view activities_doc = x.get_document().value;
            const int category_index = activities_doc[activities_info_keys::activities::CATEGORY_INDEX].get_int32().value;
            const int activity_min_age = activities_doc[activities_info_keys::activities::MIN_AGE].get_int32().value;
            const int category_min_age = categories_array[category_index].get_document().value[activities_info_keys::categories::MIN_AGE].get_int32().value;

            if(activity_index == 0
                || category_index == 0
                || activity_min_age > server_parameter_restrictions::LOWEST_ALLOWED_AGE
                || category_min_age > server_parameter_restrictions::LOWEST_ALLOWED_AGE) {
                activity_index++;
                continue;
            }

            bool category_found = false;
            //iterate categories
            for (int i = categories_start; i < (int) user_account_doc.categories.size(); ++i) {
                if(category_index == user_account_doc.categories[i].index_value) {
                    category_found = true;
                    break;
                }
            }

            if(!category_found) {
                final_category_index = category_index;
                final_activity_index = activity_index;
                break;
            }
            activity_index++;
        }

        return {final_category_index, final_activity_index};
    }

    void buildUserThatDoesNotMatch() {

        const bsoncxx::oid match_user_account_oid = insertRandomAccounts(1, 0);

        UserAccountDoc match_account_doc(match_user_account_oid);

        const bool return_val = buildMatchingUserForPassedAccount(
                user_account_doc,
                match_account_doc,
                user_account_oid,
                match_user_account_oid
        );

        ASSERT_TRUE(return_val);

        const auto non_matching_category_and_activity = findNonMatchingActivity();

        ASSERT_NE(non_matching_category_and_activity.category_index, 0);
        ASSERT_NE(non_matching_category_and_activity.activity_index, 0);

        match_account_doc.categories.clear();
        match_account_doc.categories.emplace_back(
                AccountCategoryType::ACTIVITY_TYPE,
                non_matching_category_and_activity.activity_index
        );

        match_account_doc.categories.emplace_back(
                AccountCategoryType::CATEGORY_TYPE,
                non_matching_category_and_activity.category_index
        );

        match_account_doc.setIntoCollection();
    }

    void makeEventInfoMatchUser(EventRequestMessage& event_info) {
        //Make sure the event matches current user.
        event_info.set_location_longitude(user_account_doc.location.longitude);
        event_info.set_location_latitude(user_account_doc.location.latitude);

        event_info.set_min_allowed_age(user_account_doc.age);
        event_info.set_max_allowed_age(server_parameter_restrictions::HIGHEST_ALLOWED_AGE);

        event_info.clear_allowed_genders();
        event_info.add_allowed_genders(general_values::MALE_GENDER_VALUE);
        event_info.add_allowed_genders(general_values::FEMALE_GENDER_VALUE);

        event_info.mutable_activity()->set_activity_index(user_account_doc.categories.front().index_value);
    }

    void buildMatchingEvent() {
        createAndStoreEventAdminAccount();

        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

        EventRequestMessage event_info = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        makeEventInfoMatchUser(event_info);

        const bsoncxx::oid matching_event_oid =
                bsoncxx::oid{
                        generateRandomEvent(
                                chat_room_admin_info,
                                TEMP_ADMIN_ACCOUNT_NAME,
                                current_timestamp,
                                event_info
                        ).event_account_oid
                };

        matching_account_oids.emplace_back(
                std::make_pair<bsoncxx::oid, PlaceMatchExists>(bsoncxx::oid{matching_event_oid},
                                                               PlaceMatchExists::NOT_YET_MATCHED)
        );
    }

    void buildEventWithoutMatchingActivities() {
        createAndStoreEventAdminAccount();

        const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

        const EventChatRoomAdminInfo chat_room_admin_info(
                UserAccountType::ADMIN_GENERATED_EVENT_TYPE
        );

        EventRequestMessage event_info = generateRandomEventRequestMessage(
                chat_room_admin_info,
                current_timestamp
        );

        makeEventInfoMatchUser(event_info);

        const auto non_matching_category_and_activity = findNonMatchingActivity();

        event_info.mutable_activity()->set_activity_index(non_matching_category_and_activity.activity_index);

        const bsoncxx::oid matching_event_oid =
                bsoncxx::oid{
                        generateRandomEvent(
                                chat_room_admin_info,
                                TEMP_ADMIN_ACCOUNT_NAME,
                                current_timestamp,
                                event_info
                        ).event_account_oid
                };

        matching_account_oids.emplace_back(
                std::make_pair<bsoncxx::oid, PlaceMatchExists>(bsoncxx::oid{matching_event_oid},
                                                               PlaceMatchExists::NOT_YET_MATCHED)
        );
    }

    void setupGeneralValues(
            const std::chrono::milliseconds& used_current_timestamp,
            FinalAlgorithmResults expected_algorithm_results,
            const UserAccountDoc& extracted_user_account_doc,
            bool check_extracted_accounts_size = true
    ) {

        user_account_doc.last_time_find_matches_ran = bsoncxx::types::b_date{used_current_timestamp};

        if (check_extracted_accounts_size) {
            ASSERT_EQ(extracted_user_account_doc.has_been_extracted_accounts_list.size(), matching_account_oids.size());
        }

        switch (expected_algorithm_results) {
            case algorithm_did_not_run:
            case algorithm_cool_down_found:
                break;
            case algorithm_successful:
                user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{used_current_timestamp};
                user_account_doc.last_time_empty_match_returned = bsoncxx::types::b_date{std::chrono::milliseconds{-1}};
                break;
            case set_no_matches_found:
                user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{used_current_timestamp};
                user_account_doc.last_time_empty_match_returned = bsoncxx::types::b_date{used_current_timestamp};
                break;
        }
    }

    void setupMatchInUserAccount(
            const UserAccountDoc& extracted_user_account_doc,
            const int matching_index,
            const findmatches::SingleMatchMessage& received_single_match_message,
            const std::chrono::milliseconds& used_current_timestamp
    ) {

        const auto& [matching_account_oid, place_match_came_from] = matching_account_oids[matching_index];

        UserAccountDoc match_account_doc(matching_account_oids[matching_index].first);

        user_account_doc.number_swipes_remaining--;
        user_account_doc.total_number_matches_drawn++;

        auto match_came_from = FindMatchesArrayNameEnum::other_users_matched_list;

        switch (place_match_came_from) {
            case INSIDE_ALGORITHM_MATCHED:
                user_account_doc.algorithm_matched_accounts_list.erase(
                        user_account_doc.algorithm_matched_accounts_list.begin());
                [[fallthrough]];
            case NOT_YET_MATCHED: {
                if (user_account_doc.int_for_match_list_to_draw_from <
                    matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER) {
                    user_account_doc.int_for_match_list_to_draw_from++;
                }

                match_came_from = FindMatchesArrayNameEnum::algorithm_matched_list;
                break;
            }
            case INSIDE_OTHER_USERS_MATCHED: {
                user_account_doc.int_for_match_list_to_draw_from = 0;

                user_account_doc.other_users_matched_accounts_list.erase(
                        user_account_doc.other_users_matched_accounts_list.begin());

                match_came_from = FindMatchesArrayNameEnum::other_users_matched_list;
                break;
            }
        }

        user_account_doc.has_been_extracted_accounts_list.emplace_back(
                extracted_user_account_doc.has_been_extracted_accounts_list[matching_index]
        );

        user_account_doc.has_been_extracted_accounts_list.back().oid = match_account_doc.current_object_oid;
        user_account_doc.has_been_extracted_accounts_list.back().from_match_algorithm_list =
                match_came_from == FindMatchesArrayNameEnum::algorithm_matched_list;

        findmatches::SingleMatchMessage generated_match_response_message;

        bsoncxx::builder::stream::document matching_user_doc_builder;
        match_account_doc.convertToDocument(matching_user_doc_builder);

        const auto& extracted_account = user_account_doc.has_been_extracted_accounts_list.back();

        const std::chrono::milliseconds swipes_time_before_reset =
                matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED -
                std::chrono::milliseconds{used_current_timestamp} % matching_algorithm::TIME_BETWEEN_SWIPES_UPDATED;

        std::chrono::milliseconds expiration_time = extracted_account.expiration_time.value;

        if (expiration_time >= matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE + used_current_timestamp
                ) { //if the expiration time is larger than the max time allowed on android then change the expiration time
            //set expiration time
            expiration_time = matching_algorithm::MAXIMUM_TIME_MATCH_WILL_STAY_ON_DEVICE + used_current_timestamp;
        }

        bool setup_single_match_message_result = setupSuccessfulSingleMatchMessage(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                matching_user_doc_builder.view(),
                match_account_doc.current_object_oid,
                extracted_account.distance,
                extracted_account.point_value,
                expiration_time,
                swipes_time_before_reset,
                user_account_doc.number_swipes_remaining,
                used_current_timestamp,
                match_came_from,
                &generated_match_response_message
        );

        EXPECT_TRUE(setup_single_match_message_result);

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                generated_match_response_message,
                received_single_match_message
        );

        if (!equivalent) {
            generated_match_response_message.mutable_member_info()->clear_picture();
            generated_match_response_message.mutable_member_info()->clear_account_thumbnail();

            auto copy = received_single_match_message;

            copy.mutable_member_info()->clear_picture();
            copy.mutable_member_info()->clear_account_thumbnail();

            std::cout << "generated_match_response_message\n" << generated_match_response_message.DebugString() << '\n';
            std::cout << "received_single_match_message\n" << copy.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

    void checkThreeReturnMessageTypes(std::chrono::milliseconds& used_current_timestamp) {
        ASSERT_EQ(response.write_params.size(), 4);
        ASSERT_TRUE(response.write_params[0].msg.has_single_match());
        ASSERT_TRUE(response.write_params[1].msg.has_single_match());
        ASSERT_TRUE(response.write_params[2].msg.has_single_match());
        ASSERT_TRUE(response.write_params[3].msg.has_find_matches_cap());

        EXPECT_EQ(response.write_params[3].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
        EXPECT_EQ(response.write_params[3].msg.find_matches_cap().success_type(),
                  findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

        used_current_timestamp = std::chrono::milliseconds{response.write_params[3].msg.find_matches_cap().timestamp()};
    }

};

TEST_F(FindMatchesTesting, invalidLoginInfoPassed) {

    checkLoginInfoClientOnly(
            user_account_oid,
            user_account_doc.logged_in_token,
            user_account_doc.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {
                request.mutable_login_info()->Clear();
                request.mutable_login_info()->CopyFrom(login_info);

                response.write_params.clear();

                findMatches(&request, &response);

                EXPECT_EQ(response.write_params.size(), 1);
                EXPECT_TRUE(response.write_params[0].msg.has_find_matches_cap());

                if (response.write_params.size() == 1
                    && response.write_params[0].msg.has_find_matches_cap()) {
                    return response.write_params[0].msg.find_matches_cap().return_status();
                } else {
                    return ReturnStatus_INT_MIN_SENTINEL_DO_NOT_USE_;
                }
            }
    );

    checkNothingChanged();
}

TEST_F(FindMatchesTesting, invalidLocationPassed) {

    request.set_client_longitude(-2000);
    request.set_client_latitude(15);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params[0].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[0].msg.find_matches_cap().return_status(), ReturnStatus::INVALID_PARAMETER_PASSED);

    checkNothingChanged();
}

TEST_F(FindMatchesTesting, requestZeroMatches) {
    request.set_number_messages(0);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params[0].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[0].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[0].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED);

    checkNothingChanged();
}

TEST_F(FindMatchesTesting, singleMatchReturned_fromAlgorithmRun) {

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params[1].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp{response.write_params[1].msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, singleMatchReturned_fromAlgorithmList) {

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params[1].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp{response.write_params[1].msg.find_matches_cap().timestamp()};

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

}

TEST_F(FindMatchesTesting, singleMatchReturned_fromOtherUserSwipedYesList) {

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params[1].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp{response.write_params[1].msg.find_matches_cap().timestamp()};

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, singleMatchReturned_largestPossibleUserReturned) {

    user_account_doc.genders_range.clear();
    user_account_doc.genders_range.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    user_account_doc.setIntoCollection();

    const bsoncxx::oid match_user_account_oid = insertRandomAccounts(1, 0);

    UserAccountDoc match_account_doc(match_user_account_oid);

    generateRandomLargestPossibleUserAccount(
            match_account_doc,
            getCurrentTimestamp() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED +
            std::chrono::milliseconds{24L * 60L * 60L * 1000L}
    );

    bool return_value = buildMatchingUserForPassedAccount(
            user_account_doc,
            match_account_doc,
            user_account_oid,
            match_user_account_oid
    );

    matching_account_oids.emplace_back(
            std::make_pair<bsoncxx::oid, PlaceMatchExists>(bsoncxx::oid{match_user_account_oid},
                                                           PlaceMatchExists::NOT_YET_MATCHED)
    );

    ASSERT_TRUE(return_value);

    match_account_doc.genders_range.clear();
    match_account_doc.genders_range.emplace_back(general_values::MATCH_EVERYONE_GENDER_RANGE_VALUE);
    match_account_doc.setIntoCollection();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params[1].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp{response.write_params[1].msg.find_matches_cap().timestamp()};

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoRun_otherYes_otherYes) {

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoRun_algoList_otherYes) {
    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    //When the algorithm runs it will std::reverse the list for storage purposes.
    iter_swap(matching_account_oids.begin(), matching_account_oids.begin() + 1);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoRun_otherYes_algoList) {

    user_account_doc.int_for_match_list_to_draw_from =
            matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER - 1;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    //When the algorithm runs it will std::reverse the list for storage purposes.
    iter_swap(matching_account_oids.begin(), matching_account_oids.begin() + 2);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoRun_algoList_algoList) {

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    //When the algorithm runs it will std::reverse the list for storage purposes.
    std::reverse(matching_account_oids.begin(), matching_account_oids.end());

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_algoRun_otherYes) {

    user_account_doc.int_for_match_list_to_draw_from = matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_algoRun_otherYes) {

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_algoRun_algoList) {

    user_account_doc.int_for_match_list_to_draw_from = matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    //When the algorithm runs it will std::reverse the list for storage purposes.
    iter_swap(matching_account_oids.begin() + 1, matching_account_oids.begin() + 2);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_algoRun_algoList) {

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    //When the algorithm runs it will std::reverse the list for storage purposes.
    iter_swap(matching_account_oids.begin() + 1, matching_account_oids.begin() + 2);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_otherYes_algoRun) {
    //This combination is not possible. It would require the other user yes list to be drawn
    // from twice in a row while a potential match exists from the algorithm.
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_otherYes_algoRun) {
    user_account_doc.int_for_match_list_to_draw_from =
            matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER - 1;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_algoList_algoRun) {
    user_account_doc.int_for_match_list_to_draw_from = matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_algoList_algoRun) {

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::NOT_YET_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_otherYes_otherYes) {

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_otherYes_algoList) {
    //This combination is not possible. It would require the other user yes list to be drawn
    // from twice in a row while a potential match exists from the algorithm.
}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_algoList_otherYes) {

    user_account_doc.int_for_match_list_to_draw_from = matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_otherYes_algoList_algoList) {
    user_account_doc.int_for_match_list_to_draw_from = matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_otherYes_otherYes) {
    user_account_doc.int_for_match_list_to_draw_from =
            matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER - 1;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_otherYes_algoList) {
    user_account_doc.int_for_match_list_to_draw_from =
            matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER - 1;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_algoList_otherYes) {
    user_account_doc.int_for_match_list_to_draw_from =
            matching_algorithm::NUMBER_OF_ALGORITHM_MATCHES_TO_DRAW_BEFORE_OTHER_USER - 2;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_OTHER_USERS_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, returnCombinations_algoList_algoList_algoList) {

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    runFunction();

    std::chrono::milliseconds used_current_timestamp{-1};
    checkThreeReturnMessageTypes(used_current_timestamp);

    ASSERT_NE(used_current_timestamp.count(), -1);

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, allRequestedReturnsFound) {

    const int number_matches = matching_algorithm::MAXIMUM_NUMBER_RESPONSE_MESSAGES;

    for (int i = 0; i < number_matches; ++i) {
        buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);
    }

    runFunction();

    ASSERT_EQ(response.write_params.size(), number_matches + 1);

    for (int i = 0; i < number_matches; ++i) {
        ASSERT_TRUE(response.write_params[i].msg.has_single_match());
    }

    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_SUCCESSFULLY_EXTRACTED);

    const std::chrono::milliseconds used_current_timestamp = std::chrono::milliseconds{
            response.write_params.back().msg.find_matches_cap().timestamp()};

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_did_not_run,
            extracted_user_account_doc
    );

    for (int i = 0; i < (int) matching_account_oids.size(); ++i) {
        setupMatchInUserAccount(
                extracted_user_account_doc,
                i,
                response.write_params[i].msg.single_match(),
                used_current_timestamp
        );
    }

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, algorithmOnCooldown_emptyMatchReturned) {

    user_account_doc.last_time_empty_match_returned = bsoncxx::types::b_date{
            getCurrentTimestamp() + std::chrono::milliseconds{DELAY_IN_MILLIS}};
    user_account_doc.setIntoCollection();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_MATCH_ALGORITHM_ON_COOL_DOWN);
    EXPECT_GT(response.write_params.back().msg.find_matches_cap().cool_down_on_match_algorithm(),
              (matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS_WHEN_NO_MATCHES_FOUND -
               std::chrono::milliseconds{100}).count());

    const std::chrono::milliseconds used_current_timestamp = std::chrono::milliseconds{
            response.write_params.back().msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_cool_down_found,
            extracted_user_account_doc
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

}

TEST_F(FindMatchesTesting, algorithmOnCooldown_matchAlgorithmRanRecently) {
    user_account_doc.last_time_match_algorithm_ran = bsoncxx::types::b_date{
            getCurrentTimestamp() + std::chrono::milliseconds{DELAY_IN_MILLIS}};
    user_account_doc.setIntoCollection();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_MATCH_ALGORITHM_ON_COOL_DOWN);
    EXPECT_GT(response.write_params.back().msg.find_matches_cap().cool_down_on_match_algorithm(),
              (matching_algorithm::TIME_BETWEEN_ALGORITHM_RUNS - std::chrono::milliseconds{100}).count());

    const std::chrono::milliseconds used_current_timestamp = std::chrono::milliseconds{
            response.write_params.back().msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_cool_down_found,
            extracted_user_account_doc
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, noSwipesRemaing_onInitialCall) {
    user_account_doc.number_swipes_remaining = 0;
    user_account_doc.setIntoCollection();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING);

    const std::chrono::milliseconds used_current_timestamp = std::chrono::milliseconds{
            response.write_params.back().msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_did_not_run,
            extracted_user_account_doc
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, noSwipesRemaing_ranOutDuringExtraction) {
    user_account_doc.number_swipes_remaining = 1;
    user_account_doc.setIntoCollection();

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    buildMatchingUser(PlaceMatchExists::INSIDE_ALGORITHM_MATCHED);

    runFunction();

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_SWIPES_REMAINING);

    UserAccountDoc extracted_user_account_doc(user_account_oid);
    const std::chrono::milliseconds used_current_timestamp = std::chrono::milliseconds{
            response.write_params.back().msg.find_matches_cap().timestamp()};

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_did_not_run,
            extracted_user_account_doc,
            false
    );

    ASSERT_EQ(extracted_user_account_doc.has_been_extracted_accounts_list.size(), 1);

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, findMatchesTimestampLock_locked) {

    //make sure the timestamp is 'locked'
    user_account_doc.find_matches_timestamp_lock = bsoncxx::types::b_date{
            getCurrentTimestamp() + matching_algorithm::TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS +
            std::chrono::milliseconds{DELAY_IN_MILLIS}};
    user_account_doc.setIntoCollection();

    std::cout << user_account_doc.last_time_find_matches_ran.value.count() << '\n';

    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_MATCH_ALGORITHM_ON_COOL_DOWN);
    EXPECT_GT(response.write_params.back().msg.find_matches_cap().cool_down_on_match_algorithm(),
              (matching_algorithm::TIME_TO_LOCK_FIND_MATCHES_TO_PREVENT_DOUBLE_RUNS -
               std::chrono::milliseconds{100}).count());

    UserAccountDoc extracted_user_account_doc(user_account_oid);

    ASSERT_EQ(extracted_user_account_doc.has_been_extracted_accounts_list.size(), 0);

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

TEST_F(FindMatchesTesting, matchingEventFound_fromAlgorithm) {
    buildMatchingEvent();

    runFunction();

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params[1].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp{response.write_params[1].msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);

    EXPECT_FALSE(algorithm_ran_only_match_with_events);
}

TEST_F(FindMatchesTesting, noMatchesFound_noEventsFound) {
    runFunction();

    ASSERT_EQ(response.write_params.size(), 1);
    ASSERT_TRUE(response.write_params.back().msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params.back().msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp = std::chrono::milliseconds{
            response.write_params.back().msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::set_no_matches_found,
            extracted_user_account_doc
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}

//This test will make sure that non-matching users are NOT found and that non-matching events ARE found.
TEST_F(FindMatchesTesting, noMatchesFound_eventFound) {
    buildUserThatDoesNotMatch();

    buildEventWithoutMatchingActivities();

    runFunction();

    EXPECT_TRUE(algorithm_ran_only_match_with_events);

    ASSERT_EQ(response.write_params.size(), 2);
    ASSERT_TRUE(response.write_params[0].msg.has_single_match());
    ASSERT_TRUE(response.write_params[1].msg.has_find_matches_cap());

    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().return_status(), ReturnStatus::SUCCESS);
    EXPECT_EQ(response.write_params[1].msg.find_matches_cap().success_type(),
              findmatches::FindMatchesCapMessage_SuccessTypes_NO_MATCHES_FOUND);

    const std::chrono::milliseconds used_current_timestamp{response.write_params[1].msg.find_matches_cap().timestamp()};
    UserAccountDoc extracted_user_account_doc(user_account_oid);

    setupGeneralValues(
            used_current_timestamp,
            FinalAlgorithmResults::algorithm_successful,
            extracted_user_account_doc
    );

    setupMatchInUserAccount(
            extracted_user_account_doc,
            0,
            response.write_params[0].msg.single_match(),
            used_current_timestamp
    );

    EXPECT_EQ(user_account_doc, extracted_user_account_doc);
}