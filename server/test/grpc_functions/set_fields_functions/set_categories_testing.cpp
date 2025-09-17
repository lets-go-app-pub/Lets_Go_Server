//
// Created by jeremiah on 10/7/22.
//

#include <fstream>
#include <account_objects.h>
#include <reports_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include <google/protobuf/util/message_differencer.h>

#include "chat_room_shared_keys.h"
#include "setup_login_info.h"
#include "connection_pool_global_variable.h"
#include "ManageServerCommands.pb.h"
#include "generate_multiple_random_accounts.h"
#include "set_fields_functions.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SetCategoriesTesting : public testing::Test {
protected:

    setfields::SetCategoriesRequest request;
    setfields::SetFieldResponse response;

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account;

    UserAccountStatisticsDoc user_account_statistics;

    inline static const std::string DUMMY_ACTIVITIES_COLLECTION_NAME = "set_cata_dummy_activities_col";
    const std::string previous_activities_info_collection_name = collection_names::ACTIVITIES_INFO_COLLECTION_NAME;

    ActivitiesInfoDoc activities_info_doc;

    void setupValidRequest() {
        setupUserLoginInfo(
                request.mutable_login_info(),
                user_account_oid,
                user_account.logged_in_token,
                user_account.installation_ids.front()
        );

        auto* category = request.add_category();
        category->set_activity_index(1);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        //Generate the user account BEFORE changing the activities collection name. Otherwise, it will attempt
        // to access invalid activities and seg fault.
        user_account_oid = insertRandomAccounts(1, 0);

        user_account.getFromCollection(user_account_oid);
        user_account_statistics.getFromCollection(user_account_oid);

        //don't pollute the actual collection
        collection_names::ACTIVITIES_INFO_COLLECTION_NAME = DUMMY_ACTIVITIES_COLLECTION_NAME;

        activities_info_doc.categories.emplace_back(
                "Unknown",
                "Unknown",
                bsoncxx::types::b_double{0},
                server_parameter_restrictions::HIGHEST_ALLOWED_AGE + 1,
                "unknown",
                "#000000"
        );

        activities_info_doc.activities.emplace_back(
                "Unknown",
                "Unknown",
                server_parameter_restrictions::LOWEST_ALLOWED_AGE,
                "unknown",
                0,
                0
        );

        //Contains
        // The 'Unknown' category and activity as index 0.
        activities_info_doc.setIntoCollection();

        setupValidRequest();
    }

    void TearDown() override {
        //Must be set back BEFORE clearDatabaseAndGlobalsForTesting() is run, otherwise this collection will not
        // be cleared.
        collection_names::ACTIVITIES_INFO_COLLECTION_NAME = previous_activities_info_collection_name;

        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction() {
        setCategories(&request, &response);
    }

    static std::pair<std::string, std::string> generateRandomNameAndStoredName() {
        //name length must be
        //server_parameter_restrictions::MINIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE <= name <= server_parameter_restrictions::MAXIMUM_ACTIVITY_OR_CATEGORY_NAME_SIZE
        const std::string name = gen_random_alpha_numeric_string(15);
        std::string stored_name;
        for(char c : name) {
            if(isalpha(c) && isupper(c)) {
                stored_name += (char)tolower(c);
            } else {
                stored_name += c;
            }
        }

        return {name, stored_name};
    }

    void addRandomCategories(int number) {
        for(int i = 0; i < number; ++i) {
            auto [name, stored_name] = generateRandomNameAndStoredName();

            //generate a random color code
            std::stringstream color_code_ss;
            color_code_ss
                    << "#"
                    << std::setfill('0') << std::setw(6)
                    << std::hex << (rand() % (long) std::pow(2, 24));

            activities_info_doc.categories.emplace_back(
                    name,
                    name,
                    (double)(rand() % 100 + 1),
                    server_parameter_restrictions::LOWEST_ALLOWED_AGE,
                    stored_name,
                    color_code_ss.str()
            );
        }
    }

    void addRandomActivities(int number) {
        for(int i = 0; i < number; ++i) {
            auto [name, stored_name] = generateRandomNameAndStoredName();

            activities_info_doc.activities.emplace_back(
                    name,
                    name,
                    server_parameter_restrictions::LOWEST_ALLOWED_AGE,
                    stored_name,
                    (rand() % (activities_info_doc.categories.size()-1)) + 1, //don't allow category 0
                    1
            );
        }
    }

    static void sortCategoriesArray(std::vector<TestCategory>& passed_user_account) {
        std::sort(
                passed_user_account.begin(),
                passed_user_account.end(),
                [](
                        const TestCategory& l, const TestCategory& r
                        ){
                    //sort by type first with activity first
                    if(l.type != r.type) {
                        return l.type < r.type;
                    }
                    //sort by index afterwords
                    return l.index_value < r.index_value;
                }
        );
    }

    static void sortTimeframesArray(
            std::vector<TestTimeframe>& time_frame_array
            ) {
        std::sort(
                time_frame_array.begin(),
                time_frame_array.end(),
                [](const TestTimeframe& l , const TestTimeframe& r){
                    return l.time < r.time;
                }
        );
    }

    void compareUserAccounts() {
        UserAccountDoc extracted_user_account(user_account_oid);

        //sort category arrays, this way they are equal if the elements are all present
        sortCategoriesArray(user_account.categories);
        sortCategoriesArray(extracted_user_account.categories);

        EXPECT_EQ(user_account, extracted_user_account);
    }

    void compareUserStatisticsAccounts() {
        UserAccountStatisticsDoc extracted_user_account_statistics(user_account_oid);

        //sort category arrays, this way they are equal if the elements are all present
        sortCategoriesArray(user_account_statistics.categories.back().categories);
        sortCategoriesArray(extracted_user_account_statistics.categories.back().categories);

        EXPECT_EQ(user_account_statistics, extracted_user_account_statistics);
    }

    void compareFailure(ReturnStatus return_status) {
        EXPECT_EQ(response.return_status(), return_status);

        //should be no changes
        compareUserAccounts();
        compareUserStatisticsAccounts();
    }

    void compareSuccess(
            int categories_loop_start_index,
            int categories_loop_stop_index
            ) {
        compareExpectedResponseAndUserCategories(
                categories_loop_start_index,
                categories_loop_stop_index
        );

        compareAndSetFinalSuccessValues();
    }

    void compareExpectedResponseAndUserCategories(
            int categories_loop_start_index,
            int categories_loop_stop_index
            ) {
        EXPECT_EQ(response.return_status(), ReturnStatus::SUCCESS);
        EXPECT_GT(response.timestamp(), 0);

        updateUserAccountCategories(
                categories_loop_start_index,
                categories_loop_stop_index
        );
    }

    void compareAndSetFinalSuccessValues() {
        user_account.categories_timestamp = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};
        user_account.last_time_displayed_info_updated = bsoncxx::types::b_date{std::chrono::milliseconds{response.timestamp()}};

        compareUserAccounts();

        user_account_statistics.categories.emplace_back(
                user_account.categories,
                user_account.categories_timestamp
        );

        compareUserStatisticsAccounts();
    }

    void updateUserAccountCategories(
            int loop_start_index,
            int loop_stop_index
            ) {
        user_account.categories.clear();

        std::vector<TestCategory> categories;

        for(size_t i = loop_start_index; i < (size_t)loop_stop_index; ++i) {
            user_account.categories.emplace_back(
                    AccountCategoryType::ACTIVITY_TYPE, i
            );

            bool exists = false;

            for(const auto& category : categories) {
                if(activities_info_doc.activities[i].category_index == category.index_value) {
                    exists = true;
                    break;
                }
            }

            if(!exists) {
                categories.emplace_back(
                        AccountCategoryType::CATEGORY_TYPE, activities_info_doc.activities[i].category_index
                );
            }
        }

        std::move(categories.begin(), categories.end(), std::back_inserter(user_account.categories));
    }

    void addSingleTimeframeForSingleCategory(
            long start_time,
            long stop_time
            ) {

        if(start_time != -1) {
            user_account.categories[0].time_frames.emplace_back(
                    start_time,
                    1
            );
        }

        user_account.categories[0].time_frames.emplace_back(
                stop_time,
                -1
        );

        if(start_time != -1) {
            user_account.categories[1].time_frames.emplace_back(
                    start_time,
                    1
            );
        }

        user_account.categories[1].time_frames.emplace_back(
                stop_time,
                -1
        );
    }
};

TEST_F(SetCategoriesTesting, invalidLoginInfo) {
    checkLoginInfoClientOnly(
            user_account_oid,
            user_account.logged_in_token,
            user_account.installation_ids.front(),
            [&](const LoginToServerBasicInfo& login_info) -> ReturnStatus {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();

                runFunction();

                return response.return_status();
            }
    );

    //should be no changes
    compareUserAccounts();
    compareUserStatisticsAccounts();
}

TEST_F(SetCategoriesTesting, categoryArrayEmpty) {
    request.clear_category();

    runFunction();

    compareFailure(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetCategoriesTesting, categoryArrayTooLarge) {

    //NOTE: This test case should just trim the extra activities.

    addRandomCategories(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT + 5);
    addRandomActivities(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT + 5);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    //index 0 is 'Unknown' so start at 1
    for(size_t i = 1; i < activities_info_doc.activities.size(); ++i) {
        auto* activity = request.add_category();
        activity->set_activity_index((int)i);
    }

    runFunction();

    compareSuccess(
            1,
            server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT+1
    );
}

TEST_F(SetCategoriesTesting, activityIndexDoesNotExist) {

    addRandomCategories(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT);
    addRandomActivities(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const int stop_index = 3;

    //insert valid activities
    for(size_t i = 1; i < stop_index; ++i) {
        auto* activity = request.add_category();
        activity->set_activity_index((int)i);
    }

    //insert invalid activity
    auto* activity = request.add_category();
    activity->set_activity_index(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT + 1);

    runFunction();

    compareSuccess(
            1,
            stop_index
    );
}

TEST_F(SetCategoriesTesting, allActivitiesDoNotExist) {
    addRandomCategories(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT);
    addRandomActivities(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    //activity zero should be ignored
    CategoryActivityMessage* activity = request.add_category();
    activity->set_activity_index(0);

    //insert invalid activities
    for(int i = 1; i < 4; ++i) {
        activity = request.add_category();
        activity->set_activity_index(server_parameter_restrictions::NUMBER_ACTIVITIES_STORED_PER_ACCOUNT + i);
    }

    runFunction();

    compareFailure(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetCategoriesTesting, overMinAgeRequirement_onlyCategoryNotActivity) {
    addRandomCategories(1);
    activities_info_doc.categories.back().min_age = user_account.age + 1;
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    CategoryActivityMessage* activity = request.add_category();
    activity->set_activity_index(1);

    runFunction();

    compareFailure(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetCategoriesTesting, overMinAgeRequirement_onlyActivityNotCategory) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.activities.back().min_age = user_account.age + 1;
    activities_info_doc.setIntoCollection();

    request.clear_category();

    CategoryActivityMessage* activity = request.add_category();
    activity->set_activity_index(1);

    runFunction();

    compareFailure(ReturnStatus::INVALID_PARAMETER_PASSED);
}

TEST_F(SetCategoriesTesting, validTimeframePassed) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const std::chrono::milliseconds first_start_timeframe = getCurrentTimestamp() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED + std::chrono::milliseconds{5 * 1000L};

    auto* activity = request.add_category();
    activity->set_activity_index(1);
    auto* time_frame_array = activity->add_time_frame_array();
    time_frame_array->set_start_time_frame(first_start_timeframe.count());
    time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame() + 5 * 1000);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    addSingleTimeframeForSingleCategory(
            time_frame_array->start_time_frame(),
            time_frame_array->stop_time_frame()
    );

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, onlyStartTimeTooEarly) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const std::chrono::milliseconds first_start_timeframe = getCurrentTimestamp();

    auto* activity = request.add_category();
    activity->set_activity_index(1);
    auto* time_frame_array = activity->add_time_frame_array();
    time_frame_array->set_start_time_frame(first_start_timeframe.count());
    time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count() + 5 * 1000);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    addSingleTimeframeForSingleCategory(
            -1,
            time_frame_array->stop_time_frame()
    );

    compareAndSetFinalSuccessValues();

}

TEST_F(SetCategoriesTesting, stopTimeTooEarly) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const std::chrono::milliseconds first_start_timeframe = getCurrentTimestamp();

    auto* activity = request.add_category();
    activity->set_activity_index(1);
    auto* time_frame_array = activity->add_time_frame_array();
    time_frame_array->set_start_time_frame(first_start_timeframe.count() - 5 * 1000);
    time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame());

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, startTimeTooLate) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const std::chrono::milliseconds first_start_timeframe = getCurrentTimestamp();

    auto* activity = request.add_category();
    activity->set_activity_index(1);
    auto* time_frame_array = activity->add_time_frame_array();
    time_frame_array->set_start_time_frame(first_start_timeframe.count() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count() + 5 * 1000);
    time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame() + 5 * 1000);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, onlyStopTimeTooLate) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const std::chrono::milliseconds first_start_timeframe = getCurrentTimestamp() + matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED + std::chrono::milliseconds{5 * 1000L};

    auto* activity = request.add_category();
    activity->set_activity_index(1);
    auto* time_frame_array = activity->add_time_frame_array();
    time_frame_array->set_start_time_frame(first_start_timeframe.count());
    time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count() + 5 * 1000);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    addSingleTimeframeForSingleCategory(
            first_start_timeframe.count(),
            response.timestamp() + general_values::TIME_AVAILABLE_TO_SELECT_TIME_FRAMES.count()
    );

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, stopTimeBeforeStartTime) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    const std::chrono::milliseconds first_start_timeframe = getCurrentTimestamp() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED;

    auto* activity = request.add_category();
    activity->set_activity_index(1);
    auto* time_frame_array = activity->add_time_frame_array();
    time_frame_array->set_start_time_frame(first_start_timeframe.count());
    time_frame_array->set_stop_time_frame(time_frame_array->start_time_frame() - matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count());

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, timeFrameArrayTooLarge) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    long current_timeframe_time = getCurrentTimestamp().count() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count();
    auto* activity = request.add_category();
    activity->set_activity_index(1);

    //create all valid timeframes, just too many
    for(int i = 0; i < server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY + 1; ++i) {
        auto* time_frame_array = activity->add_time_frame_array();
        time_frame_array->set_start_time_frame(current_timeframe_time);
        current_timeframe_time += 5L * 60L * 1000L;
        time_frame_array->set_stop_time_frame(current_timeframe_time);
        current_timeframe_time += 5L * 60L * 1000L;
    }

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    for(int i = 0; i < server_parameter_restrictions::NUMBER_TIME_FRAMES_STORED_PER_ACTIVITY; ++i) {
        addSingleTimeframeForSingleCategory(
                request.category(0).time_frame_array(i).start_time_frame(),
                request.category(0).time_frame_array(i).stop_time_frame()
        );
    }

    ASSERT_EQ(user_account.categories.size(), 2);

    sortTimeframesArray(user_account.categories[0].time_frames);
    sortTimeframesArray(user_account.categories[1].time_frames);

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, timeFrameArray_outOfOrder) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    long first_start_timeframe_time = getCurrentTimestamp().count() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count();
    long first_stop_timeframe_time = first_start_timeframe_time +5L * 60L * 1000L;
    long second_start_timeframe_time = first_stop_timeframe_time + 5L * 60L * 1000L;
    long second_stop_timeframe_time = second_start_timeframe_time + 5L * 60L * 1000L;

    auto* activity = request.add_category();
    activity->set_activity_index(1);

    //add later timeframe first
    auto* late_time_frame_array = activity->add_time_frame_array();
    late_time_frame_array->set_start_time_frame(second_start_timeframe_time);
    late_time_frame_array->set_stop_time_frame(second_stop_timeframe_time);

    auto* early_time_frame_array = activity->add_time_frame_array();
    early_time_frame_array->set_start_time_frame(first_start_timeframe_time);
    early_time_frame_array->set_stop_time_frame(first_stop_timeframe_time);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    addSingleTimeframeForSingleCategory(
            first_start_timeframe_time,
            first_stop_timeframe_time
    );

    addSingleTimeframeForSingleCategory(
            second_start_timeframe_time,
            second_stop_timeframe_time
    );

    ASSERT_EQ(user_account.categories.size(), 2);

    sortTimeframesArray(user_account.categories[0].time_frames);
    sortTimeframesArray(user_account.categories[1].time_frames);

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, timeFrameArray_nestedTimeframe) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    //second timeframe is completely nesting inside first
    long first_start_timeframe_time = getCurrentTimestamp().count() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count();
    long second_start_timeframe_time = first_start_timeframe_time + 5L * 60L * 1000L;
    long second_stop_timeframe_time = second_start_timeframe_time + 5L * 60L * 1000L;
    long first_stop_timeframe_time = second_stop_timeframe_time + 5L * 60L * 1000L;

    auto* activity = request.add_category();
    activity->set_activity_index(1);

    auto* late_time_frame_array = activity->add_time_frame_array();
    late_time_frame_array->set_start_time_frame(first_start_timeframe_time);
    late_time_frame_array->set_stop_time_frame(first_stop_timeframe_time);

    auto* early_time_frame_array = activity->add_time_frame_array();
    early_time_frame_array->set_start_time_frame(second_start_timeframe_time);
    early_time_frame_array->set_stop_time_frame(second_stop_timeframe_time);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    addSingleTimeframeForSingleCategory(
            first_start_timeframe_time,
            first_stop_timeframe_time
    );

    ASSERT_EQ(user_account.categories.size(), 2);

    sortTimeframesArray(user_account.categories[0].time_frames);
    sortTimeframesArray(user_account.categories[1].time_frames);

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, timeFrameArray_overlappingTimeFrames) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    //first timeframe overlaps with second
    long first_start_timeframe_time = getCurrentTimestamp().count() + 2 * matching_algorithm::TIME_BEFORE_EXPIRED_TIME_MATCH_WILL_BE_RETURNED.count();
    long second_start_timeframe_time = first_start_timeframe_time + 5L * 60L * 1000L;
    long first_stop_timeframe_time = second_start_timeframe_time + 5L * 60L * 1000L;
    long second_stop_timeframe_time = first_stop_timeframe_time + 5L * 60L * 1000L;

    auto* activity = request.add_category();
    activity->set_activity_index(1);

    auto* late_time_frame_array = activity->add_time_frame_array();
    late_time_frame_array->set_start_time_frame(first_start_timeframe_time);
    late_time_frame_array->set_stop_time_frame(first_stop_timeframe_time);

    auto* early_time_frame_array = activity->add_time_frame_array();
    early_time_frame_array->set_start_time_frame(second_start_timeframe_time);
    early_time_frame_array->set_stop_time_frame(second_stop_timeframe_time);

    runFunction();

    compareExpectedResponseAndUserCategories(
            1,
            2
    );

    addSingleTimeframeForSingleCategory(
            first_start_timeframe_time,
            second_stop_timeframe_time
    );

    ASSERT_EQ(user_account.categories.size(), 2);

    sortTimeframesArray(user_account.categories[0].time_frames);
    sortTimeframesArray(user_account.categories[1].time_frames);

    compareAndSetFinalSuccessValues();
}

TEST_F(SetCategoriesTesting, matchingInfoUpdated) {
    addRandomCategories(1);
    addRandomActivities(1);
    activities_info_doc.setIntoCollection();

    request.clear_category();

    auto* activity = request.add_category();
    activity->set_activity_index(1);

    {
        MatchingElement staying_matching_element;
        staying_matching_element.generateRandomValues();
        staying_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};
        staying_matching_element.point_value = (double) matching_algorithm::ACTIVITY_MATCH_WEIGHT * 0.7 + 100000;

        MatchingElement removed_matching_element;
        removed_matching_element.generateRandomValues();
        removed_matching_element.expiration_time = bsoncxx::types::b_date{getCurrentTimestamp() + std::chrono::milliseconds{15L * 60L * 1000L}};
        removed_matching_element.point_value = (double) matching_algorithm::ACTIVITY_MATCH_WEIGHT * 0.7 - 100;

        user_account.other_users_matched_accounts_list.emplace_back(staying_matching_element);
        user_account.other_users_matched_accounts_list.emplace_back(removed_matching_element);
    }
    user_account.setIntoCollection();

    runFunction();

    user_account.other_users_matched_accounts_list.pop_back();

    compareSuccess(
            1,
            2
    );
}
