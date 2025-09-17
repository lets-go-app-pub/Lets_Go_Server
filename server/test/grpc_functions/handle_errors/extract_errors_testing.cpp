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

class ExtractErrorsTesting : public ::testing::Test {
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

    handle_errors::ExtractErrorsRequest request;
    grpc::testing::MockServerWriterVector<handle_errors::ExtractErrorsResponse> mock_server_writer;

    bsoncxx::oid error_oid{};

    void setupValidRequest() {
        setupAdminLoginInfo(
                request.mutable_login_info(),
                TEMP_ADMIN_ACCOUNT_NAME,
                TEMP_ADMIN_ACCOUNT_PASSWORD
        );

        auto error_parameters = request.mutable_error_parameters();

        error_parameters->set_error_origin(ErrorOriginType(rand() % (ErrorOriginType_MAX-1) + 1));
        error_parameters->set_version_number(rand() % 100 + 1);
        error_parameters->set_file_name(gen_random_alpha_numeric_string(rand() % 100 + 10));
        error_parameters->set_line_number(rand() % 10000);

        request.set_max_number_bytes(rand() % (error_values::MAXIMUM_NUMBER_ALLOWED_BYTES_TO_REQUEST_ERROR_MESSAGE-1) + 1);
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

        extractErrors(
                send_trailing_meta_data,
                &request,
                &mock_server_writer
        );
    }

    FreshErrorsDoc generateFreshErrorMatchingRequest() {
        FreshErrorsDoc fresh_error;
        fresh_error.generateRandomValues();
        fresh_error.error_origin = request.error_parameters().error_origin();
        fresh_error.version_number = (int)request.error_parameters().version_number();
        fresh_error.file_name = request.error_parameters().file_name();
        fresh_error.line_number = (int)request.error_parameters().line_number();
        fresh_error.setIntoCollection();

        return fresh_error;
    }

    static ErrorMessage convertFreshErrorsDocTogRPCErrorMessage(const FreshErrorsDoc& fresh_error) {
        ErrorMessage error_message;

        //extractErrors() does not store these in the return message for brevity
//        error_message.set_error_origin(fresh_error.error_origin);
//        error_message.set_error_urgency_level(fresh_error.error_urgency);
//        error_message.set_version_number(fresh_error.version_number);
//        error_message.set_file_name(fresh_error.file_name);
//        error_message.set_line_number(fresh_error.line_number);

        error_message.set_stack_trace(fresh_error.stack_trace);

        error_message.set_timestamp_stored(fresh_error.timestamp_stored);
        error_message.set_error_id(fresh_error.current_object_oid.to_string());

        error_message.set_api_number(fresh_error.api_number ? *fresh_error.api_number : 0);
        error_message.set_device_name(fresh_error.device_name ? *fresh_error.device_name : "");

        error_message.set_error_message(fresh_error.error_message);

        return error_message;
    }

    //Will generate three fresh errors that match, calculate the total size in bytes of these
    // three message (size of the documents stored in the database). Then return the size.
    //Will also generate four fresh errors that do not match.
    long generateFreshErrors(std::vector<ErrorMessage>& fresh_errors) {
        fresh_errors.emplace_back(convertFreshErrorsDocTogRPCErrorMessage(generateFreshErrorMatchingRequest()));
        fresh_errors.emplace_back(convertFreshErrorsDocTogRPCErrorMessage(generateFreshErrorMatchingRequest()));
        fresh_errors.emplace_back(convertFreshErrorsDocTogRPCErrorMessage(generateFreshErrorMatchingRequest()));

        //return the list in descending order
        std::sort(fresh_errors.begin(), fresh_errors.end(), [](const ErrorMessage& l, const ErrorMessage& r){
            return l.timestamp_stored() > r.timestamp_stored();
        });

        mongocxx::pipeline pipe;

        pipe.project(
                document{}
                    << "val" << open_document
                        << "$toLong" << open_document
                            << "$bsonSize" << "$$ROOT"
                        << close_document
                    << close_document
                << finalize
        );

        auto cursor = fresh_errors_collection.aggregate(pipe);

        long max_size_extracted_docs = 0;

        for(const auto& doc : cursor) {
            max_size_extracted_docs += doc["val"].get_int64().value;
        }

        //Making sure non-matching errors are not extracted
        FreshErrorsDoc origin_fresh_error = generateFreshErrorMatchingRequest();
        origin_fresh_error.error_origin = ErrorOriginType(((int)request.error_parameters().error_origin() + 1) % ErrorOriginType_MAX);
        origin_fresh_error.setIntoCollection();

        FreshErrorsDoc version_fresh_error = generateFreshErrorMatchingRequest();
        version_fresh_error.version_number = (int)request.error_parameters().version_number() + 1;
        version_fresh_error.setIntoCollection();

        FreshErrorsDoc file_name_fresh_error = generateFreshErrorMatchingRequest();
        file_name_fresh_error.file_name = request.error_parameters().file_name() + 'a';
        file_name_fresh_error.setIntoCollection();

        FreshErrorsDoc line_number_fresh_error = generateFreshErrorMatchingRequest();
        line_number_fresh_error.version_number = (int)request.error_parameters().line_number() + 1;
        line_number_fresh_error.setIntoCollection();

        return max_size_extracted_docs;
    }

    void compareReturnedMessages(const std::vector<ErrorMessage>& fresh_errors) {
        ASSERT_EQ(mock_server_writer.write_params.size(), fresh_errors.size());

        for(size_t i = 0; i < fresh_errors.size(); ++i) {

            const auto& returned_err = mock_server_writer.write_params[i].msg.error_message();
            const auto& generated_err = fresh_errors[i];

            bool equivalent = google::protobuf::util::MessageDifferencer::Equivalent(
                    returned_err,
                    generated_err
            );

            if(!equivalent) {
                std::cout << "returned_err\n" << returned_err.DebugString() << '\n';
                std::cout << "generated_err\n" << generated_err.DebugString() << '\n';
            }

            EXPECT_TRUE(equivalent);
            EXPECT_TRUE(mock_server_writer.write_params[i].msg.error_msg().empty());
            EXPECT_TRUE(mock_server_writer.write_params[i].msg.success());
        }
    }

    template <bool expect_less_than_extracted_bytes>
    void compareMetadataReturned(
            const long total_bytes,
            const long extracted_bytes,
            const long total_docs,
            const long extracted_docs
            ) {
        ASSERT_EQ(meta_data.size(), 4);
        EXPECT_EQ(meta_data[0], Metadata(error_values::EXTRACT_ERRORS_TOTAL_BYTES_KEY, std::to_string(total_bytes)));

        if(expect_less_than_extracted_bytes) {
            EXPECT_EQ(meta_data[1].key, error_values::EXTRACT_ERRORS_EXTRACTED_BYTES_KEY);

            try {
                EXPECT_LT(std::stol(meta_data[1].value), extracted_bytes);
            } catch (const std::exception& e) {
                //print error string and show error
                EXPECT_EQ(std::string(e.what()), "%#N3");
            }
        } else {
            EXPECT_EQ(meta_data[1], Metadata(error_values::EXTRACT_ERRORS_EXTRACTED_BYTES_KEY, std::to_string(extracted_bytes)));
        }

        EXPECT_EQ(meta_data[2], Metadata(error_values::EXTRACT_ERRORS_TOTAL_DOCS_KEY, std::to_string(total_docs)));
        EXPECT_EQ(meta_data[3], Metadata(error_values::EXTRACT_ERRORS_EXTRACTED_DOCS_KEY, std::to_string(extracted_docs)));
    }
};

TEST_F(ExtractErrorsTesting, invalidLoginInfo) {
    checkLoginInfoAdminOnly(
        TEMP_ADMIN_ACCOUNT_NAME,
        TEMP_ADMIN_ACCOUNT_PASSWORD,
        [&](const LoginToServerBasicInfo& login_info) -> bool {

            request.mutable_login_info()->CopyFrom(login_info);

            mock_server_writer.write_params.clear();
            runFunction();

            EXPECT_TRUE(meta_data.empty());
            EXPECT_EQ(mock_server_writer.write_params.size(), 1);
            if(mock_server_writer.write_params.size() == 1) {
                EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
                return mock_server_writer.write_params[0].msg.success();
            } else {
                return true;
            }
        }
    );
}

TEST_F(ExtractErrorsTesting, invalidParam_errorOrigin) {
    request.mutable_error_parameters()->set_error_origin(ErrorOriginType(-1));

    runFunction();

    EXPECT_TRUE(meta_data.empty());
    ASSERT_EQ(mock_server_writer.write_params.size(), 1);
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.success());
}

TEST_F(ExtractErrorsTesting, invalidParam_fileName) {
    request.mutable_error_parameters()->set_file_name(gen_random_alpha_numeric_string(server_parameter_restrictions::MAXIMUM_NUMBER_ALLOWED_BYTES + 1));

    runFunction();

    EXPECT_TRUE(meta_data.empty());
    ASSERT_EQ(mock_server_writer.write_params.size(), 1);
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.success());
}

TEST_F(ExtractErrorsTesting, invalidParam_maxNumberBytes_tooSmall) {
    request.set_max_number_bytes(-1);

    runFunction();

    EXPECT_TRUE(meta_data.empty());
    ASSERT_EQ(mock_server_writer.write_params.size(), 1);
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.success());
}

TEST_F(ExtractErrorsTesting, invalidParam_maxNumberBytes_tooLarge) {
    request.set_max_number_bytes(error_values::MAXIMUM_NUMBER_ALLOWED_BYTES_TO_REQUEST_ERROR_MESSAGE + 1);

    runFunction();

    EXPECT_TRUE(meta_data.empty());
    ASSERT_EQ(mock_server_writer.write_params.size(), 1);
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.success());
}

TEST_F(ExtractErrorsTesting, invalidAdminPrivilegeLevel) {
    runFunction(AdminLevelEnum::NO_ADMIN_ACCESS);

    EXPECT_TRUE(meta_data.empty());
    ASSERT_EQ(mock_server_writer.write_params.size(), 1);
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.success());
}

TEST_F(ExtractErrorsTesting, noErrorsFound) {
    request.set_max_number_bytes(error_values::MAXIMUM_NUMBER_ALLOWED_BYTES_TO_REQUEST_ERROR_MESSAGE);

    runFunction();

    EXPECT_TRUE(meta_data.empty());
    ASSERT_EQ(mock_server_writer.write_params.size(), 1);
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.error_msg().empty());
    EXPECT_FALSE(mock_server_writer.write_params[0].msg.success());
}

TEST_F(ExtractErrorsTesting, totalErrorsSize_under_maxNumberBytes) {
    std::vector<ErrorMessage> fresh_errors;
    const long max_number_bytes = generateFreshErrors(fresh_errors);

    request.set_max_number_bytes(max_number_bytes + 1024);

    runFunction();

    compareReturnedMessages(fresh_errors);

    compareMetadataReturned<false>(
            max_number_bytes,
            max_number_bytes,
            (long)fresh_errors.size(),
            (long)fresh_errors.size()
    );
}

TEST_F(ExtractErrorsTesting, totalErrorsSize_over_maxNumberBytes) {
    std::vector<ErrorMessage> fresh_errors;
    const long max_number_bytes = generateFreshErrors(fresh_errors);

    request.set_max_number_bytes(max_number_bytes - 1);

    runFunction();

    //It should not return the earliest fresh error (they are sorted in order of timestamp_stored).
    fresh_errors.pop_back();

    compareReturnedMessages(fresh_errors);

    compareMetadataReturned<true>(
            max_number_bytes,
            max_number_bytes,
            (long)fresh_errors.size() + 1L,
            (long)fresh_errors.size()
    );
}
