//
// Created by jeremiah on 9/16/22.
//

#include <fstream>
#include <account_objects.h>
#include <chat_room_commands.h>
#include "gtest/gtest.h"
#include "clear_database_for_testing.h"
#include "connection_pool_global_variable.h"

#include "helper_functions/algorithm_pipeline_helper_functions/algorithm_pipeline_helper_functions.h"
#include <google/protobuf/util/message_differencer.h>

#include "extract_data_from_bsoncxx.h"
#include "generate_multiple_random_accounts.h"
#include "setup_login_info.h"
#include "HandleErrors.pb.h"
#include "generate_temp_admin_account/generate_temp_admin_account.h"
#include "handle_errors.h"
#include "errors_objects.h"
#include "grpc_mock_stream/mock_stream.h"
#include "error_values.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

class SearchErrorsTesting : public ::testing::Test {
protected:

    struct Metadata {
        std::string key;
        std::string value;

        Metadata() = delete;

        Metadata(
                std::string _key,
                std::string _value
        ) : key(std::move(_key)),
            value(std::move(_value)) {}

        bool operator==(const Metadata& rhs) const {
            return key == rhs.key &&
                   value == rhs.value;
        }

        bool operator!=(const Metadata& rhs) const {
            return !(rhs == *this);
        }
    };

    std::vector<Metadata> meta_data;

    const std::function<void(const std::string& /*key*/, const std::string& /*value*/)> send_trailing_meta_data = [this](
            const std::string& key, const std::string& value
    ){
        meta_data.emplace_back(key, value);
    };

    bsoncxx::oid user_account_oid;

    UserAccountDoc user_account_doc;

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongo_cpp_client = *mongocxx_pool_entry;

    mongocxx::database errors_db = mongo_cpp_client[database_names::ERRORS_DATABASE_NAME];
    mongocxx::collection fresh_errors_collection = errors_db[collection_names::FRESH_ERRORS_COLLECTION_NAME];

    handle_errors::SearchErrorsRequest request;
    handle_errors::SearchErrorsResponse response;

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        request.set_all_error_origin_types(true);
        request.set_all_error_urgency_levels(true);
        request.set_all_version_numbers(true);
        request.set_all_file_names(true);
        request.set_all_line_numbers(true);
        request.set_all_timestamps(true);
        request.set_all_android_api(true);
        request.set_all_device_names(true);
    }

    void SetUp() override {
        bool clear_database_success = clearDatabaseAndGlobalsForTesting();
        ASSERT_EQ(clear_database_success, true);

        user_account_oid = insertRandomAccounts(1, 0);

        user_account_doc.getFromCollection(user_account_oid);

        setupValidRequest();
    }

    void TearDown() override {
        clearDatabaseAndGlobalsForTesting();
    }

    void runFunction(AdminLevelEnum admin_level = AdminLevelEnum::PRIMARY_DEVELOPER) {
        createTempAdminAccount(admin_level);

        searchErrors(
                &request,
                &response
        );
    }

    static void storeUniqueStringsToArray(google::protobuf::RepeatedPtrField<std::basic_string<char>>* array) {
        std::unordered_set<std::string> unique_string_set;

        while(unique_string_set.size() <= 1000) {
            unique_string_set.insert(gen_random_alpha_numeric_string(rand() % 50 + 5));
        }

        for(const auto& str : unique_string_set) {
            array->Add(str.c_str());
        }
    }

    static void insertDifferentFreshErrors(
            size_t num_to_insert,
            std::vector<FreshErrorsDoc>& fresh_errors
            ) {
        for(size_t i = 0; i < num_to_insert; ++i) {
            fresh_errors.emplace_back();
            fresh_errors.back().generateRandomValues();
            fresh_errors.back().line_number = (int)i;
            fresh_errors.back().setIntoCollection();
        }
    }

    static handle_errors::ErrorStatistics convertFreshErrorIntoErrorStatistics(const FreshErrorsDoc& fresh_error) {

        handle_errors::ErrorStatistics error_statistics;

        error_statistics.set_number_times_error_occurred(1);
        error_statistics.set_error_origin(fresh_error.error_origin);
        error_statistics.set_error_urgency_level(fresh_error.error_urgency);
        error_statistics.set_version_number(fresh_error.version_number);
        error_statistics.set_file_name(fresh_error.file_name);
        error_statistics.set_line_number(fresh_error.line_number);
        error_statistics.set_most_recent_timestamp(fresh_error.timestamp_stored);

        if(fresh_error.device_name) {
            auto device_name = error_statistics.add_device_names();
            device_name->set_device_name(*fresh_error.device_name);
            device_name->set_number_times_device_name_found(1);
        }

        if(fresh_error.api_number) {
            auto device_name = error_statistics.add_api_numbers();
            device_name->set_api_number(*fresh_error.api_number);
            device_name->set_number_times_api_number_found(1);
        }

        return error_statistics;
    }

    void compareReturnedErrors(std::vector<FreshErrorsDoc>& fresh_errors) {
        ASSERT_EQ(response.error_message_statistics().size(), fresh_errors.size());

        std::sort(response.mutable_error_message_statistics()->begin(), response.mutable_error_message_statistics()->end(), [](
                const handle_errors::ErrorStatistics& l, const handle_errors::ErrorStatistics& r
                ){
            if(l.most_recent_timestamp() == r.most_recent_timestamp()) {
                return l.line_number() < r.line_number();
            }
            return l.most_recent_timestamp() < r.most_recent_timestamp();
        });

        std::sort(fresh_errors.begin(), fresh_errors.end(), [](
                const FreshErrorsDoc& l, const FreshErrorsDoc& r
        ){
            if(l.timestamp_stored == r.timestamp_stored) {
                return l.line_number < r.line_number;
            }
            return l.timestamp_stored < r.timestamp_stored;
        });

        handle_errors::ErrorStatistics error_statistics;
        for(size_t i = 0; i < fresh_errors.size(); ++i) {
            error_statistics.Clear();
            error_statistics = convertFreshErrorIntoErrorStatistics(fresh_errors[i]);

            bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                    error_statistics,
                    response.error_message_statistics((int)i)
            );

            if(!equivalent) {
                std::cout << "error_statistics\n" << error_statistics.DebugString() << '\n';
                std::cout << "response.error_message_statistics\n" << response.error_message_statistics(i).DebugString() << '\n';
            }

            EXPECT_TRUE(equivalent);
        }
    }
};

TEST_F(SearchErrorsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
            TEMP_ADMIN_ACCOUNT_NAME,
            TEMP_ADMIN_ACCOUNT_PASSWORD,
            [&](const LoginToServerBasicInfo& login_info) -> bool {
                request.mutable_login_info()->CopyFrom(login_info);

                response.Clear();
                runFunction();

                EXPECT_FALSE(response.error_msg().empty());
                return response.success();
            }
    );
}

TEST_F(SearchErrorsTesting, invalidParam_originType) {
    request.set_all_error_origin_types(false);
    request.clear_types();

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, invalidParam_maxVersionSmallerThanMinVersion) {
    request.set_all_version_numbers(false);
    request.set_min_version_number(10);
    request.set_max_version_number(9);

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, invalidParam_tooManyFileNamesPassed) {
    request.set_all_file_names(false);

    storeUniqueStringsToArray(request.mutable_file_names());

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, invalidParam_tooManyLineNumbersPassed) {
    request.set_all_line_numbers(false);

    for(size_t i = 0; i < 1001; ++i) {
        request.add_line_numbers(i);
    }

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, invalidParam_urgencyLevel) {
    request.set_all_error_urgency_levels(false);
    request.clear_levels();

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, invalidParam_tooManyDeviceNamesPassed) {
    request.set_all_device_names(false);

    storeUniqueStringsToArray(request.mutable_device_names());

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, invalidParam_maxAndroidApiSmallerThanMinAndroidApi) {
    request.set_all_android_api(false);
    request.set_min_api_number(10);
    request.set_max_api_number(9);

    runFunction();

    EXPECT_FALSE(response.error_msg().empty());
    EXPECT_FALSE(response.success());
}

TEST_F(SearchErrorsTesting, searchFor_multipleOriginTypes_withDuplicates) {

    const ErrorOriginType first_error_origin = ErrorOriginType::ERROR_ORIGIN_SERVER;
    const ErrorOriginType second_error_origin = ErrorOriginType::ERROR_ORIGIN_ANDROID;
    const ErrorOriginType not_stored_error_origin = ErrorOriginType::ERROR_ORIGIN_DESKTOP_INTERFACE;

    request.set_all_error_origin_types(false);
    request.add_types(first_error_origin);
    request.add_types(second_error_origin);
    request.add_types(second_error_origin);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].error_origin = first_error_origin;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].error_origin = second_error_origin;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].error_origin = not_stored_error_origin;
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove element that was not returned
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_minLessThanMaxVersion) {
    const int start_version_number = 5;
    const int middle_version_number = 8;
    const int end_version_number = 10;
    const int before_start_version_number = start_version_number - 1;
    const int after_end_version_number = end_version_number + 1;

    request.set_all_version_numbers(false);
    request.set_min_version_number(start_version_number);
    request.set_max_version_number(end_version_number);

    const size_t num_to_insert = 5;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].version_number = start_version_number;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].version_number = middle_version_number;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].version_number = end_version_number;
    fresh_errors[2].setIntoCollection();

    fresh_errors[3].version_number = before_start_version_number;
    fresh_errors[3].setIntoCollection();

    fresh_errors[4].version_number = after_end_version_number;
    fresh_errors[4].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_minVersionSameAsMaxVersion) {
    const int start_version_number = 5;
    const int end_version_number = start_version_number;
    const int before_start_version_number = start_version_number - 1;
    const int after_end_version_number = end_version_number + 1;

    request.set_all_version_numbers(false);
    request.set_min_version_number(start_version_number);
    request.set_max_version_number(end_version_number);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].version_number = start_version_number;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].version_number = before_start_version_number;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].version_number = after_end_version_number;
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_multipleFileNames_withDuplicates) {
    const std::string first_file_name = gen_random_alpha_numeric_string(rand() % 50 + 5);
    const std::string second_file_name = first_file_name + 'a';
    const std::string not_used_file_name = first_file_name + 'b';

    request.set_all_file_names(false);
    request.add_file_names(first_file_name);
    request.add_file_names(second_file_name);
    request.add_file_names(second_file_name);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].file_name = first_file_name;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].file_name = second_file_name;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].file_name = not_used_file_name;
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_multipleLineNumbers_withDuplicates) {
    const int first_line_number = 1;
    const int second_line_number = 2;
    const int not_used_line_number = 3;

    request.set_all_line_numbers(false);
    request.add_line_numbers(first_line_number);
    request.add_line_numbers(second_line_number);
    request.add_line_numbers(second_line_number);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    //Must set version number because insertDifferentFreshErrors() makes the errors unique using
    // line numbers.
    //NOTE: FRESH_ERRORS_COLLECTION has a validator on it, VERSION_NUMBER must be larger than 0.
    fresh_errors[0].version_number = 1;
    fresh_errors[0].line_number = first_line_number;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].version_number = 2;
    fresh_errors[1].line_number = second_line_number;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].version_number = 3;
    fresh_errors[2].line_number = not_used_line_number;
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_multipleUrgencyLevels_withDuplicates) {
    const ErrorUrgencyLevel first_urgency_level = ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_LOW;
    const ErrorUrgencyLevel second_urgency_level = ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_HIGH;
    const ErrorUrgencyLevel not_used_urgency_level = ErrorUrgencyLevel::ERROR_URGENCY_LEVEL_MAX;

    request.set_all_error_urgency_levels(false);
    request.add_levels(first_urgency_level);
    request.add_levels(second_urgency_level);
    request.add_levels(second_urgency_level);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].error_urgency = first_urgency_level;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].error_urgency = second_urgency_level;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].error_urgency = not_used_urgency_level;
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_multipleDeviceNames_withDuplicates) {
    const std::string first_device_name = gen_random_alpha_numeric_string(rand() % 50 + 10);
    const std::string second_device_name = first_device_name + '1';
    const std::string not_used_device_name = first_device_name + '2';

    request.set_all_device_names(false);
    request.add_device_names(first_device_name);
    request.add_device_names(second_device_name);
    request.add_device_names(second_device_name);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].device_name = std::make_unique<std::string>(first_device_name);
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].device_name = std::make_unique<std::string>(second_device_name);
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].device_name = std::make_unique<std::string>(not_used_device_name);
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_before_timestamp) {
    const std::chrono::milliseconds before_timestamp{1};
    const std::chrono::milliseconds at_timestamp{2};
    const std::chrono::milliseconds after_timestamp{3};

    request.set_all_timestamps(false);
    request.set_before_timestamp(true);
    request.set_search_timestamp(at_timestamp.count());

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].timestamp_stored = bsoncxx::types::b_date{before_timestamp};
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].timestamp_stored = bsoncxx::types::b_date{at_timestamp};
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].timestamp_stored = bsoncxx::types::b_date{after_timestamp};
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_after_timestamp) {
    const std::chrono::milliseconds before_timestamp{1};
    const std::chrono::milliseconds at_timestamp{2};
    const std::chrono::milliseconds after_timestamp{3};

    request.set_all_timestamps(false);
    request.set_before_timestamp(false);
    request.set_search_timestamp(at_timestamp.count());

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].timestamp_stored = bsoncxx::types::b_date{after_timestamp};
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].timestamp_stored = bsoncxx::types::b_date{at_timestamp};
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].timestamp_stored = bsoncxx::types::b_date{before_timestamp};
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_minAndroidApi_lessThan_maxAndroidApi) {
    const int start_android_api = 5;
    const int middle_android_api = 8;
    const int end_android_api = 10;
    const int before_start_android_api = start_android_api - 1;
    const int after_end_android_api = end_android_api + 1;

    request.set_all_android_api(false);
    request.set_min_api_number(start_android_api);
    request.set_max_api_number(end_android_api);

    const size_t num_to_insert = 5;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].api_number = std::make_unique<int>(start_android_api);
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].api_number = std::make_unique<int>(middle_android_api);
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].api_number = std::make_unique<int>(end_android_api);
    fresh_errors[2].setIntoCollection();

    fresh_errors[3].api_number = std::make_unique<int>(before_start_android_api);
    fresh_errors[3].setIntoCollection();

    fresh_errors[4].api_number = std::make_unique<int>(after_end_android_api);
    fresh_errors[4].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_minAndroidApi_sameAs_maxAndroidApi) {
    const int start_android_api = 5;
    const int end_android_api = start_android_api;
    const int before_start_android_api = start_android_api - 1;
    const int after_end_android_api = end_android_api + 1;

    request.set_all_version_numbers(false);
    request.set_min_version_number(start_android_api);
    request.set_max_version_number(end_android_api);

    const size_t num_to_insert = 3;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    fresh_errors[0].version_number = start_android_api;
    fresh_errors[0].setIntoCollection();

    fresh_errors[1].version_number = before_start_android_api;
    fresh_errors[1].setIntoCollection();

    fresh_errors[2].version_number = after_end_android_api;
    fresh_errors[2].setIntoCollection();

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    //remove elements that were not returned
    fresh_errors.pop_back();
    fresh_errors.pop_back();

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, searchFor_allErrors) {
    const size_t num_to_insert = 10;
    std::vector<FreshErrorsDoc> fresh_errors;

    insertDifferentFreshErrors(
            num_to_insert,
            fresh_errors
    );

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    compareReturnedErrors(fresh_errors);
}

TEST_F(SearchErrorsTesting, noErrorsFound) {
    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());
    EXPECT_TRUE(response.error_message_statistics().empty());
}

TEST_F(SearchErrorsTesting, multipleOfSameMessageTypeGrouped) {
    const size_t num_to_insert = rand() % 10 + 1;
    std::vector<FreshErrorsDoc> fresh_errors;

    fresh_errors.emplace_back();
    fresh_errors.back().generateRandomValues();
    fresh_errors.back().timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{100}};
    fresh_errors.back().api_number = nullptr;
    fresh_errors.back().device_name = nullptr;

    fresh_errors.back().setIntoCollection();

    for(size_t i = 1; i < num_to_insert; ++i) {
        fresh_errors.emplace_back();
        fresh_errors.back().generateRandomValues();
        //guarantee errors match
        fresh_errors.back().error_origin = fresh_errors[0].error_origin;
        fresh_errors.back().version_number = fresh_errors[0].version_number;
        fresh_errors.back().file_name = fresh_errors[0].file_name;
        fresh_errors.back().line_number = fresh_errors[0].line_number;
        fresh_errors.back().error_urgency = fresh_errors[0].error_urgency;
        fresh_errors.back().timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 100}};
        fresh_errors.back().api_number = nullptr;
        fresh_errors.back().device_name = nullptr;
        fresh_errors.back().setIntoCollection();
    }

    ASSERT_EQ(fresh_errors.size(), num_to_insert);

    runFunction();

    EXPECT_TRUE(response.error_msg().empty());
    EXPECT_TRUE(response.success());

    ASSERT_EQ(response.error_message_statistics().size(), 1);

    handle_errors::ErrorStatistics error_statistics = convertFreshErrorIntoErrorStatistics(fresh_errors[0]);
    error_statistics.set_number_times_error_occurred(num_to_insert);

    bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
            error_statistics,
            response.error_message_statistics(0)
    );

    if(!equivalent) {
        std::cout << "error_statistics\n" << error_statistics.DebugString() << '\n';
        std::cout << "response.error_message_statistics\n" << response.error_message_statistics(0).DebugString() << '\n';
    }

    EXPECT_TRUE(equivalent);
}

TEST_F(SearchErrorsTesting, apiCount_and_deviceCount_set) {

    const size_t num_values_from_lookup = 3;

    std::vector<handle_errors::ApiNumberSearchResults> api_number_lookup(num_values_from_lookup);
    api_number_lookup[0].set_api_number(12);
    api_number_lookup[1].set_api_number(15);
    api_number_lookup[2].set_api_number(22);

    std::vector<handle_errors::DeviceNameSearchResults> device_name_lookup(num_values_from_lookup);
    device_name_lookup[0].set_device_name("123");
    device_name_lookup[1].set_device_name("234");
    device_name_lookup[2].set_device_name("345");

    for(int l = 0; l < 20; ++l) {

        if(l > 0) {
            for(auto& a : api_number_lookup) {
                a.set_number_times_api_number_found(0);
            }

            for(auto& d : device_name_lookup) {
                d.set_number_times_device_name_found(0);
            }

            response.Clear();
            bool clear_database_success = clearDatabaseAndGlobalsForTesting();
            ASSERT_EQ(clear_database_success, true);
        }

        const size_t num_to_insert = rand() % 10 + 1;
        std::vector<FreshErrorsDoc> fresh_errors;

        {
            int random_api_index = rand() % num_values_from_lookup;
            int random_device_index = rand() % num_values_from_lookup;

            auto& api_ptr = api_number_lookup[random_api_index];
            auto& device_ptr = device_name_lookup[random_device_index];

            fresh_errors.emplace_back();
            fresh_errors.back().generateRandomValues();
            fresh_errors.back().timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{100}};
            fresh_errors.back().api_number = std::make_unique<int>(api_ptr.api_number());
            fresh_errors.back().device_name = std::make_unique<std::string>(device_ptr.device_name());
            fresh_errors.back().setIntoCollection();

            api_ptr.set_number_times_api_number_found(
                    api_ptr.number_times_api_number_found() + 1
            );

            device_ptr.set_number_times_device_name_found(
                    device_ptr.number_times_device_name_found() + 1
            );

        }

        for (size_t i = 1; i < num_to_insert; ++i) {

            int random_api_index = rand() % num_values_from_lookup;
            int random_device_index = rand() % num_values_from_lookup;

            auto& api_ptr = api_number_lookup[random_api_index];
            auto& device_ptr = device_name_lookup[random_device_index];

            fresh_errors.emplace_back();
            fresh_errors.back().generateRandomValues();
            //guarantee errors match
            fresh_errors.back().error_origin = fresh_errors[0].error_origin;
            fresh_errors.back().version_number = fresh_errors[0].version_number;
            fresh_errors.back().file_name = fresh_errors[0].file_name;
            fresh_errors.back().line_number = fresh_errors[0].line_number;
            fresh_errors.back().error_urgency = fresh_errors[0].error_urgency;
            fresh_errors.back().timestamp_stored = bsoncxx::types::b_date{std::chrono::milliseconds{rand() % 100}};

            fresh_errors.back().api_number = std::make_unique<int>(api_ptr.api_number());
            fresh_errors.back().device_name = std::make_unique<std::string>(device_ptr.device_name());

            api_ptr.set_number_times_api_number_found(
                    api_ptr.number_times_api_number_found() + 1
            );

            device_ptr.set_number_times_device_name_found(
                    device_ptr.number_times_device_name_found() + 1
            );

            fresh_errors.back().setIntoCollection();
        }

        ASSERT_EQ(fresh_errors.size(), num_to_insert);

        runFunction();

        EXPECT_TRUE(response.error_msg().empty());
        EXPECT_TRUE(response.success());

        ASSERT_EQ(response.error_message_statistics().size(), 1);

        handle_errors::ErrorStatistics error_statistics = convertFreshErrorIntoErrorStatistics(fresh_errors[0]);
        error_statistics.set_number_times_error_occurred(num_to_insert);

        error_statistics.clear_api_numbers();
        for (const auto& api: api_number_lookup) {
            if (api.number_times_api_number_found() > 0) {
                error_statistics.add_api_numbers()->CopyFrom(api);
            }
        }

        error_statistics.clear_device_names();
        for (const auto& device: device_name_lookup) {
            if (device.number_times_device_name_found() > 0) {
                error_statistics.add_device_names()->CopyFrom(device);
            }
        }

        std::sort(error_statistics.mutable_api_numbers()->begin(), error_statistics.mutable_api_numbers()->end(), [](
                const handle_errors::ApiNumberSearchResults& l, const handle_errors::ApiNumberSearchResults& r
        ) {
            return l.api_number() < r.api_number();
        });

        std::sort(error_statistics.mutable_device_names()->begin(), error_statistics.mutable_device_names()->end(), [](
                const handle_errors::DeviceNameSearchResults& l, const handle_errors::DeviceNameSearchResults& r
        ) {
            return l.device_name() < r.device_name();
        });

        std::sort(response.mutable_error_message_statistics(0)->mutable_api_numbers()->begin(),
                  response.mutable_error_message_statistics(0)->mutable_api_numbers()->end(), [](
                        const handle_errors::ApiNumberSearchResults& l, const handle_errors::ApiNumberSearchResults& r
                ) {
                    return l.api_number() < r.api_number();
                });

        std::sort(response.mutable_error_message_statistics(0)->mutable_device_names()->begin(),
                  response.mutable_error_message_statistics(0)->mutable_device_names()->end(), [](
                        const handle_errors::DeviceNameSearchResults& l, const handle_errors::DeviceNameSearchResults& r
                ) {
                    return l.device_name() < r.device_name();
                });

        bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                error_statistics,
                response.error_message_statistics(0)
        );

        if (!equivalent) {
            std::cout << "error_statistics\n" << error_statistics.DebugString() << '\n';
            std::cout << "response.error_message_statistics\n" << response.error_message_statistics(
                    0).DebugString() << '\n';
        }

        EXPECT_TRUE(equivalent);
    }
}
