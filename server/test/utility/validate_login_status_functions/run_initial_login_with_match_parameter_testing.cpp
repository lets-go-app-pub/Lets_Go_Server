//
// Created by jeremiah on 9/15/22.
//

#include <gtest/gtest.h>
#include <bsoncxx/builder/stream/document.hpp>
#include <clear_database_for_testing.h>
#include <generate_multiple_random_accounts.h>
#include <account_objects.h>
#include <utility_general_functions.h>
#include <general_values.h>
#include <user_account_keys.h>

#include "grpc_function_server_template.h"
#include "../../grpc_functions/find_matches/helper_functions/generate_matching_users.h"
#include "run_initial_login_with_match_parameter.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class RunInitialLoginWithMatchParameterTemplate : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;
    bsoncxx::oid match_account_oid;

    UserAccountDoc user_account_doc;
    UserAccountDoc match_account_doc;

    bsoncxx::builder::stream::document match_parameters_to_query;
    bsoncxx::builder::stream::document fields_to_update_document;
    bsoncxx::builder::stream::document fields_to_project;

    inline const static int NEW_MAX_DISTANCE = -1;
    inline const static int MAX_DISTANCE_GREATER_THAN = 0;

    MatchingElement matching_element;

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    int num_times_set_return_status_ran = 0;
    int num_times_set_success_ran = 0;
    int num_times_save_statistics_ran = 0;

    const std::function<void(const ReturnStatus& /*return_status*/, const std::string& /*error_message*/)> set_return_status = [&](
            const ReturnStatus&, const std::string&
    ) {
        num_times_set_return_status_ran++;
    };

    const std::function<void()> set_success = [&]() {
        num_times_set_success_ran++;
    };

    const std::function<void(mongocxx::client&, mongocxx::database& )> save_statistics = [&](
            mongocxx::client& /*mongo_cpp_client*/, mongocxx::database& /*accounts_db*/
    ) {
        num_times_save_statistics_ran++;
    };

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);
        user_account_doc.getFromCollection(user_account_oid);

        match_account_oid = insertRandomAccounts(1, 0);
        match_account_doc.getFromCollection(match_account_oid);

        ASSERT_TRUE(
                buildMatchingUserForPassedAccount(
                        user_account_doc,
                        match_account_doc,
                        user_account_oid,
                        match_account_oid
                )
        );

        user_account_doc.algorithm_matched_accounts_list.clear();
        user_account_doc.other_users_matched_accounts_list.clear();

        user_account_doc.setIntoCollection();

        match_parameters_to_query
                << user_account_keys::MAX_DISTANCE << open_document
                << "$gte" << MAX_DISTANCE_GREATER_THAN
                << close_document;

        fields_to_update_document
                << user_account_keys::MAX_DISTANCE << NEW_MAX_DISTANCE;

        matching_element.generateRandomValues();
        matching_element.oid = match_account_oid;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(bool admin_info_used) {
        bool return_value = grpcValidateLoginFunctionWithMatchParametersTemplate<false>(
                user_account_oid.to_string(),
                user_account_doc.logged_in_token,
                user_account_doc.installation_ids.front(),
                admin_info_used,
                current_timestamp,
                match_parameters_to_query.view(),
                fields_to_update_document.view(),
                set_return_status,
                set_success,
                save_statistics
        );

        EXPECT_TRUE(return_value);
    }

    void checkFunctionSuccessful() {
        EXPECT_EQ(num_times_set_return_status_ran, 0);
        EXPECT_EQ(num_times_set_success_ran, 1);
        EXPECT_EQ(num_times_save_statistics_ran, 1);

        user_account_doc.max_distance = NEW_MAX_DISTANCE;

        UserAccountDoc extracted_user_account_doc(user_account_oid);
        EXPECT_EQ(user_account_doc, extracted_user_account_doc);
    }
};

TEST_F(RunInitialLoginWithMatchParameterTemplate, userInfoUsed) {
    runFunction(false);

    checkFunctionSuccessful();
}

TEST_F(RunInitialLoginWithMatchParameterTemplate, adminInfoUsed) {
    runFunction(true);

    checkFunctionSuccessful();
}

TEST_F(RunInitialLoginWithMatchParameterTemplate, elementRemoved) {
    //make sure removed
    match_account_doc.max_distance = MAX_DISTANCE_GREATER_THAN - 1;
    match_account_doc.setIntoCollection();

    user_account_doc.algorithm_matched_accounts_list.emplace_back(
            matching_element
    );
    user_account_doc.setIntoCollection();

    runFunction(true);

    user_account_doc.algorithm_matched_accounts_list.pop_back();

    checkFunctionSuccessful();
}

TEST_F(RunInitialLoginWithMatchParameterTemplate, elementNotRemoved) {
    //make sure NOT removed
    match_account_doc.max_distance = MAX_DISTANCE_GREATER_THAN;
    match_account_doc.setIntoCollection();

    user_account_doc.algorithm_matched_accounts_list.emplace_back(
            matching_element
    );
    user_account_doc.setIntoCollection();

    runFunction(true);

    checkFunctionSuccessful();
}
