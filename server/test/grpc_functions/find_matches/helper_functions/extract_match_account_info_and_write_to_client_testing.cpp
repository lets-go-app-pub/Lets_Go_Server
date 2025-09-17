//
// Created by jeremiah on 9/3/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "chat_room_shared_keys.h"
#include "find_matches_helper_objects.h"
#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "helper_functions/find_matches_helper_functions.h"
#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "utility_find_matches_functions.h"
#include "info_for_statistics_objects.h"
#include "request_statistics_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class ExtractMatchAccountInfoAndWriteToClientTesting : public ::testing::Test {
protected:

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    bsoncxx::builder::stream::document user_account_doc_builder;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database accounts_db = mongo_cpp_client[database_names::ACCOUNTS_DATABASE_NAME];
    mongocxx::collection user_accounts_collection = accounts_db[collection_names::USER_ACCOUNTS_COLLECTION_NAME];

    const std::chrono::milliseconds current_timestamp{rand() % 1000000};

    const double point_value = 123.45;
    const double match_distance = 14.44;
    const std::chrono::milliseconds expiration_time{4444};
    const bsoncxx::oid statistics_document_oid{};
    const std::chrono::milliseconds swipes_time_before_reset = std::chrono::milliseconds{rand() % 1000};

    MatchInfoStruct match_info;

    bsoncxx::builder::basic::array activity_statistics_array;

    std::vector<bsoncxx::document::value> insert_statistics_documents;
    bsoncxx::builder::basic::array update_statistics_documents_query;

    bsoncxx::array::view activity_statistics_array_view = activity_statistics_array.view();

    FindMatchesArrayNameEnum array_element_is_from = FindMatchesArrayNameEnum::other_users_matched_list;

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);
        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        user_account_doc.convertToDocument(user_account_doc_builder);

        match_info.match_user_account_doc.emplace(user_account_doc_builder.view());

        match_info.number_swipes_after_extracted = rand() % 100;
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunctionWithChecksForMessage() {

        match_info.matching_element_doc =
                buildMatchedAccountDoc<true>(
                        user_account_oid,
                        point_value,
                        match_distance,
                        expiration_time,
                        current_timestamp,
                        array_element_is_from == FindMatchesArrayNameEnum::algorithm_matched_list,
                        statistics_document_oid,
                        &activity_statistics_array_view
                );
        match_info.array_element_is_from = array_element_is_from;

        findmatches::SingleMatchMessage passed_match_response_message;

        bool return_val = extractMatchAccountInfoForWriteToClient(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                match_info,
                swipes_time_before_reset,
                current_timestamp,
                &passed_match_response_message,
                insert_statistics_documents,
                update_statistics_documents_query
        );

        EXPECT_TRUE(return_val);

        findmatches::SingleMatchMessage generated_match_response_message;

        bool build_match_response_msg_return = setupSuccessfulSingleMatchMessage(
                mongo_cpp_client,
                accounts_db,
                user_accounts_collection,
                match_info.match_user_account_doc->view(),
                user_account_oid,
                match_distance,
                point_value,
                expiration_time,
                swipes_time_before_reset,
                match_info.number_swipes_after_extracted,
                current_timestamp,
                match_info.array_element_is_from,
                &generated_match_response_message
        );

        EXPECT_TRUE(build_match_response_msg_return);

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                generated_match_response_message,
                passed_match_response_message
        );

        if(!equivalent) {
            std::cout << "generated_match_response_message\n" << generated_match_response_message.DebugString() << '\n';
            std::cout << "passed_match_response_message\n" << passed_match_response_message.DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }

};

TEST_F(ExtractMatchAccountInfoAndWriteToClientTesting, other_users_matched_list) {

    match_info.array_element_is_from = FindMatchesArrayNameEnum::other_users_matched_list;

    runFunctionWithChecksForMessage();

    bsoncxx::array::view update_statistics_documents_query_view = update_statistics_documents_query.view();
    ASSERT_EQ(std::distance(update_statistics_documents_query_view.begin(), update_statistics_documents_query_view.end()), 1);
    ASSERT_EQ(update_statistics_documents_query_view.find(0)->type(), bsoncxx::type::k_oid);
    EXPECT_EQ(update_statistics_documents_query_view.find(0)->get_oid().value.to_string(), statistics_document_oid.to_string());

    EXPECT_TRUE(insert_statistics_documents.empty());
}

TEST_F(ExtractMatchAccountInfoAndWriteToClientTesting, algorithm_matched_list) {

    //This is reflected by user_account_keys::accounts_list::ACTIVITY_STATISTICS. It is
    // filled with dummy info for testing.
    activity_statistics_array.append(
            document{}
                << "activity_one" << 1
            << finalize
    );

    activity_statistics_array.append(
            document{}
                    << "activity_two" << 2
            << finalize
    );

    activity_statistics_array_view = activity_statistics_array;

    for(const auto& ele : activity_statistics_array_view) {
        match_info.activity_statistics.append(ele.get_value());
    }

    array_element_is_from = FindMatchesArrayNameEnum::algorithm_matched_list;

    runFunctionWithChecksForMessage();

    bsoncxx::array::view update_statistics_documents_query_view = update_statistics_documents_query.view();
    EXPECT_EQ(std::distance(update_statistics_documents_query_view.begin(), update_statistics_documents_query_view.end()), 0);

    ASSERT_EQ(insert_statistics_documents.size(), 1);

    IndividualMatchStatisticsDoc generated_individual_match;

    generated_individual_match.current_object_oid = insert_statistics_documents.front().view()["_id"].get_oid().value;
    generated_individual_match.sent_timestamp.emplace_back(bsoncxx::types::b_date{current_timestamp});
    generated_individual_match.day_timestamp = current_timestamp.count() / request_statistics_values::MILLISECONDS_IN_ONE_DAY;
    generated_individual_match.user_extracted_list_element_document.oid = user_account_oid;
    generated_individual_match.user_extracted_list_element_document.point_value = point_value;
    generated_individual_match.user_extracted_list_element_document.distance = match_distance;
    generated_individual_match.user_extracted_list_element_document.expiration_time = bsoncxx::types::b_date{expiration_time};
    generated_individual_match.user_extracted_list_element_document.match_timestamp = bsoncxx::types::b_date{current_timestamp};
    generated_individual_match.user_extracted_list_element_document.from_match_algorithm_list = array_element_is_from == FindMatchesArrayNameEnum::algorithm_matched_list;
    generated_individual_match.user_extracted_list_element_document.saved_statistics_oid = statistics_document_oid;

    generated_individual_match.user_extracted_list_element_document.initializeActivityStatistics();
    for(const auto& ele : activity_statistics_array_view) {
        generated_individual_match.user_extracted_list_element_document.activity_statistics->append(ele.get_value());
    }

    generated_individual_match.matched_user_account_document.getFromCollection(user_account_oid);

    bsoncxx::builder::stream::document generated_individual_match_builder;
    generated_individual_match.convertToDocument(generated_individual_match_builder);

    EXPECT_EQ(generated_individual_match_builder.view(), insert_statistics_documents.front());
    if(generated_individual_match_builder.view() != insert_statistics_documents.front()) {
        std::cout << "generated_match_doc\n" << makePrettyJson(generated_individual_match_builder) << '\n';
        std::cout << "insert_statistics_documents\n" << makePrettyJson(insert_statistics_documents.front()) << '\n';
    }
}
