//
// Created by jeremiah on 10/5/22.
//

#include <fstream>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"

#include "setup_login_info.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "errors_objects.h"
#include "RequestStatistics.pb.h"
#include "request_statistics.h"
#include "compare_equivalent_messages.h"
#include "request_statistics_values.h"
#include "info_for_statistics_objects.h"
#include "match_algorithm_results_keys.h"
#include "user_account_keys.h"
#include "generate_randoms.h"
#include "generate_multiple_random_accounts.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class MatchingActivityStatisticsTesting : public testing::Test {
protected:

    request_statistics::MatchingActivityStatisticsRequest request;
    request_statistics::MatchingActivityStatisticsResponse response;

    request_statistics::MatchingActivityStatisticsResponse generated_response;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    int number_days_to_search = request_statistics_values::MAXIMUM_NUMBER_DAYS_TO_SEARCH;
    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();
    const long current_timestamp_day = current_timestamp.count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_number_days_to_search(number_days_to_search);
        request.set_include_categories(true);
        request.set_yes_yes(true);
        request.set_yes_no(true);
        request.set_yes_block_and_report(true);
        request.set_yes_incomplete(true);
        request.set_no(true);
        request.set_block_and_report(true);
        request.set_incomplete(true);

        request.add_user_account_types(UserAccountType::USER_ACCOUNT_TYPE);
        request.add_user_account_types(UserAccountType::USER_GENERATED_EVENT_TYPE);
        request.add_user_account_types(UserAccountType::ADMIN_GENERATED_EVENT_TYPE);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    static void sortResponse(request_statistics::MatchingActivityStatisticsResponse& passed_response) {
        std::sort(passed_response.mutable_activity_statistics()->begin(),
                  passed_response.mutable_activity_statistics()->end(),
                  [](const auto& l, const auto& r) {
                      if (l.account_category_type() != r.account_category_type()) {
                          //make categories come first
                          return l.account_category_type() > r.account_category_type();
                      }
                      return l.activity_or_category_index() < r.activity_or_category_index();
                  });
    }

    void runFunctionAndSortResponses(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        runMatchingActivityStatistics(
                &request, &response
        );

        sortResponse(response);
        sortResponse(generated_response);
    }

    void resetAllBoolsToFalse() {
        request.set_include_categories(false);
        request.set_yes_yes(false);
        request.set_yes_no(false);
        request.set_yes_block_and_report(false);
        request.set_yes_incomplete(false);
        request.set_no(false);
        request.set_block_and_report(false);
        request.set_incomplete(false);
    }

    void checkFunctionFailed() {
        EXPECT_FALSE(response.error_msg().empty());
        EXPECT_FALSE(response.success());
    }

    //This 'expects' the response variable to be set up before it is called.
    template <bool use_current_timestamp_day = true>
    void addMatchesAndGeneratedResponse(long final_day_to_generate_for = -1) {
        generated_response.Clear();
        generated_response.set_success(true);

        struct CategoryValueTotalSwipes {
            //will be set to 0 when initialized
            long category_swipes = -1;
            long activity_swipes = -1;
        };

        if(use_current_timestamp_day == true) {
            final_day_to_generate_for = current_timestamp_day;
        }

        std::map<int, CategoryValueTotalSwipes> number_swipes;
        for (long i = current_timestamp_day - number_days_to_search; i < final_day_to_generate_for; ++i) {

            MatchAlgorithmResultsDoc header_match_algorithm_doc;
            header_match_algorithm_doc.generated_for_day = i;
            header_match_algorithm_doc.account_type = UserAccountType(match_algorithm_results_keys::ACCOUNT_TYPE_KEY_HEADER_VALUE);
            header_match_algorithm_doc.categories_type = AccountCategoryType(match_algorithm_results_keys::TYPE_KEY_HEADER_VALUE);
            header_match_algorithm_doc.categories_value = match_algorithm_results_keys::VALUE_KEY_HEADER_VALUE;
            header_match_algorithm_doc.setIntoCollection();

            if (rand() % 2 == 0) {

                MatchAlgorithmResultsDoc match_algorithm_doc;
                match_algorithm_doc.generated_for_day = i;
                match_algorithm_doc.account_type = generateRandomValidUserAccountType();
                match_algorithm_doc.categories_type = AccountCategoryType(rand() % (AccountCategoryType_MAX + 1));
                match_algorithm_doc.categories_value = rand() % 100 + 1;
                match_algorithm_doc.num_yes_yes = rand() % 50;
                match_algorithm_doc.num_yes_no = rand() % 50;
                match_algorithm_doc.num_yes_block_and_report = rand() % 50;
                match_algorithm_doc.num_yes_incomplete = rand() % 50;
                match_algorithm_doc.num_no = rand() % 50;
                match_algorithm_doc.num_block_and_report = rand() % 50;
                match_algorithm_doc.num_incomplete = rand() % 50;
                match_algorithm_doc.setIntoCollection();

                const bool account_type_exists_in_request = std::find(request.user_account_types().begin(), request.user_account_types().end(), match_algorithm_doc.account_type) != request.user_account_types().end();

                if (
                    (
                        request.include_categories()
                        || (
                            !request.include_categories()
                            && match_algorithm_doc.categories_type == ACTIVITY_TYPE
                        )
                    )
                    && account_type_exists_in_request
                ) {

                    const long total_num_swipes =
                            (request.yes_yes() ? match_algorithm_doc.num_yes_yes : 0) +
                            (request.yes_no() ? match_algorithm_doc.num_yes_no : 0) +
                            (request.yes_block_and_report() ? match_algorithm_doc.num_yes_block_and_report : 0) +
                            (request.yes_incomplete() ? match_algorithm_doc.num_yes_incomplete : 0) +
                            (request.no() ? match_algorithm_doc.num_no : 0) +
                            (request.block_and_report() ? match_algorithm_doc.num_block_and_report : 0) +
                            (request.incomplete() ? match_algorithm_doc.num_incomplete : 0);

                    if (match_algorithm_doc.categories_type == ACTIVITY_TYPE) {
                        if(number_swipes[match_algorithm_doc.categories_value].activity_swipes == -1) {
                            number_swipes[match_algorithm_doc.categories_value].activity_swipes++;
                        }
                        number_swipes[match_algorithm_doc.categories_value].activity_swipes += total_num_swipes;
                    } else {
                        if(number_swipes[match_algorithm_doc.categories_value].category_swipes == -1) {
                            number_swipes[match_algorithm_doc.categories_value].category_swipes++;
                        }
                        number_swipes[match_algorithm_doc.categories_value].category_swipes += total_num_swipes;
                    }
                }
            }
        }

        for (const auto& val: number_swipes) {
            if (val.second.activity_swipes != -1) {
                auto* activity_statistics = generated_response.add_activity_statistics();
                activity_statistics->set_account_category_type(AccountCategoryType::ACTIVITY_TYPE);
                activity_statistics->set_activity_or_category_index(val.first);
                activity_statistics->set_number_times_swiped(val.second.activity_swipes);
            }
            if (val.second.category_swipes != -1) {
                auto* activity_statistics = generated_response.add_activity_statistics();
                activity_statistics->set_account_category_type(AccountCategoryType::CATEGORY_TYPE);
                activity_statistics->set_activity_or_category_index(val.first);
                activity_statistics->set_number_times_swiped(val.second.category_swipes);
            }
        }
    }

    void addIndividualMatchStatisticsToGeneratedResponse(long number_days_skipped) {
        for(long i = current_timestamp_day - number_days_skipped; i < current_timestamp_day; ++i) {
            IndividualMatchStatisticsDoc individual_match_statistics_doc;
            individual_match_statistics_doc.generateRandomValues(bsoncxx::oid{});

            individual_match_statistics_doc.day_timestamp = i;

            individual_match_statistics_doc.user_extracted_list_element_document.activity_statistics = std::make_shared<bsoncxx::builder::basic::array>(bsoncxx::builder::basic::array{});

            auto generated_type(AccountCategoryType(rand() % (AccountCategoryType_MAX + 1)));
            int generated_value = rand() % 100 + 1;

            individual_match_statistics_doc.user_extracted_list_element_document.activity_statistics->append(
                    document{}
                            << user_account_keys::categories::TYPE << generated_type
                            << user_account_keys::categories::INDEX_VALUE << generated_value
                            << finalize
            );
            const bsoncxx::oid user_account_oid = insertRandomAccounts(1, 0);
            individual_match_statistics_doc.matched_user_account_document.getFromCollection(user_account_oid);
            individual_match_statistics_doc.matched_user_account_document.account_type = generateRandomValidUserAccountType();
            individual_match_statistics_doc.setIntoCollection();

            bool added = false;
            for(auto& element : *generated_response.mutable_activity_statistics()) {
                if(element.account_category_type() == generated_type && element.activity_or_category_index() == generated_value) {
                    element.set_number_times_swiped(element.number_times_swiped() + 1);
                    added = true;
                    break;
                }
            }

            if(!added) {
                auto* activity_statistics = generated_response.add_activity_statistics();
                activity_statistics->set_account_category_type(generated_type);
                activity_statistics->set_activity_or_category_index(generated_value);
                activity_statistics->set_number_times_swiped(1);
            }
        }
    }

};

TEST_F(MatchingActivityStatisticsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {

                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunctionAndSortResponses();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(MatchingActivityStatisticsTesting, noAdminPriveledge) {
    runFunctionAndSortResponses(AdminLevelEnum::NO_ADMIN_ACCESS);

    checkFunctionFailed();
}

TEST_F(MatchingActivityStatisticsTesting, zeroDaysSearched) {
    request.set_number_days_to_search(0);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    checkFunctionFailed();
}

TEST_F(MatchingActivityStatisticsTesting, tooManyDaysSearched) {
    request.set_number_days_to_search(request_statistics_values::MAXIMUM_NUMBER_DAYS_TO_SEARCH + 1);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    checkFunctionFailed();
}

TEST_F(MatchingActivityStatisticsTesting, noAccountTypesSearched) {
    request.clear_user_account_types();
    request.mutable_login_info()->set_lets_go_version(2);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    checkFunctionFailed();
}

TEST_F(MatchingActivityStatisticsTesting, noMatchesPresent) {
    runFunctionAndSortResponses();

    generated_response.set_success(true);

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, allBooleansAreSetToFalse) {
    resetAllBoolsToFalse();

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    checkFunctionFailed();
}

TEST_F(MatchingActivityStatisticsTesting, doNotIncludeCategories) {
    request.set_include_categories(false);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestSingleAccountType_userAccountType) {
    request.clear_user_account_types();
    request.add_user_account_types(UserAccountType::USER_ACCOUNT_TYPE);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestSingleAccountType_userGeneratedEventType) {
    request.clear_user_account_types();
    request.add_user_account_types(UserAccountType::USER_GENERATED_EVENT_TYPE);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestSingleAccountType_adminGeneratedEventType) {
    request.clear_user_account_types();
    request.add_user_account_types(UserAccountType::ADMIN_GENERATED_EVENT_TYPE);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyYesYes) {
    resetAllBoolsToFalse();
    request.set_yes_yes(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyYesNo) {
    resetAllBoolsToFalse();
    request.set_yes_no(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyYesBlockAndReport) {
    resetAllBoolsToFalse();
    request.set_yes_block_and_report(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyYesIncomplete) {
    resetAllBoolsToFalse();
    request.set_yes_incomplete(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyNo) {
    resetAllBoolsToFalse();
    request.set_no(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyBlockAndReport) {
    resetAllBoolsToFalse();
    request.set_block_and_report(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

TEST_F(MatchingActivityStatisticsTesting, requestOnlyIncomplete) {
    resetAllBoolsToFalse();
    request.set_incomplete(true);

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

///Implementation Detail Test (can delete test if necessary)
TEST_F(MatchingActivityStatisticsTesting, createsValuesForMissedDays) {
    const int number_days_skipped = 24;

    //Final 'number_days_to_skip' days need to be generated.
    addMatchesAndGeneratedResponse<false>(current_timestamp_day - number_days_skipped);

    addIndividualMatchStatisticsToGeneratedResponse(number_days_skipped);

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );

}

///Implementation Detail Test (can delete test if necessary)
TEST_F(MatchingActivityStatisticsTesting, properlyHandlesADayNoSwipesOccurred) {
    const int number_days_skipped = 2;

    addMatchesAndGeneratedResponse<false>(current_timestamp_day - number_days_skipped);

    //skip a day
    addIndividualMatchStatisticsToGeneratedResponse(number_days_skipped - 1);

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}

///Implementation Detail Test (can delete test if necessary)
TEST_F(MatchingActivityStatisticsTesting, properlyHandlesRequestingBeforeAnyStatisticsStored) {

    //Will request request_statistics_values::MAXIMUM_NUMBER_DAYS_TO_SEARCH days.
    number_days_to_search = request_statistics_values::MAXIMUM_NUMBER_DAYS_TO_SEARCH/2;

    addMatchesAndGeneratedResponse();

    runFunctionAndSortResponses();

    compareEquivalentMessages<request_statistics::MatchingActivityStatisticsResponse>(
            generated_response,
            response
    );
}
