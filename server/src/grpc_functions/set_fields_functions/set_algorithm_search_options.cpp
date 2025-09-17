//
// Created by jeremiah on 3/16/22.
//

#include <bsoncxx/builder/stream/document.hpp>
#include <grpc_function_server_template.h>
#include <handle_function_operation_exception.h>
#include <admin_functions_for_set_values.h>
#include <store_info_to_user_statistics.h>

#include "set_fields_functions.h"
#include "utility_general_functions.h"
#include "user_account_keys.h"
#include "user_account_statistics_keys.h"

using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void setAlgorithmSearchOptionsImplementation(
        const setfields::SetAlgorithmSearchOptionsRequest* request,
        setfields::SetFieldResponse* response
);

void setAlgorithmSearchOptions(
        const setfields::SetAlgorithmSearchOptionsRequest* request,
        setfields::SetFieldResponse* response
) {

    handleFunctionOperationException(
            [&] {
                setAlgorithmSearchOptionsImplementation(request, response);
                },
                [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
                },
                [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
                },
                __LINE__, __FILE__, request
    );
}

void setAlgorithmSearchOptionsImplementation(
        const setfields::SetAlgorithmSearchOptionsRequest* request,
        setfields::SetFieldResponse* response
) {

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;
    std::string user_account_oid_str;
    std::string login_token_str;
    std::string installation_id;

    const ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request->login_info(),
            user_account_oid_str,
            login_token_str,
            installation_id
    );

    if (basic_info_return_status != ReturnStatus::SUCCESS) {
        response->set_return_status(basic_info_return_status);
        return;
    }

    const AlgorithmSearchOptions algorithm_search_options = request->matching_status();

    if (!AlgorithmSearchOptions_IsValid(algorithm_search_options)) {
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default

    const std::chrono::milliseconds current_timestamp = getCurrentTimestamp();

    const bsoncxx::document::value merge_document = document{}
        << user_account_keys::SEARCH_BY_OPTIONS << bsoncxx::types::b_int32{algorithm_search_options}
    << finalize;

    auto store_info_to_user_statistics = [&](
            mongocxx::client& mongo_cpp_client,
            mongocxx::database& accounts_db
    ) {
        auto push_update_doc = document{}
            << user_account_statistics_keys::ACCOUNT_SEARCH_BY_OPTIONS << open_document
                << user_account_statistics_keys::search_by_options::NEW_SEARCH_BY_OPTIONS << bsoncxx::types::b_int32{algorithm_search_options}
                << user_account_statistics_keys::search_by_options::TIMESTAMP << bsoncxx::types::b_date{current_timestamp}
            << close_document
        << finalize;

        storeInfoToUserStatistics(
                mongo_cpp_client,
                accounts_db,
                bsoncxx::oid{user_account_oid_str},
                push_update_doc,
                current_timestamp
        );
    };

    auto set_return_status = [&](const ReturnStatus& return_status) {
        response->set_return_status(return_status);
    };

    auto set_success = [&](
            const bsoncxx::document::view& /*user_account_doc_view*/
    ) {
        response->set_timestamp(current_timestamp.count());
        response->set_return_status(ReturnStatus::SUCCESS);
    };

    grpcValidateLoginFunctionTemplate<true>(
            user_account_oid_str,
            login_token_str,
            installation_id,
            current_timestamp,
            merge_document,
            set_return_status,
            set_success,
            nullptr,
            store_info_to_user_statistics
    );
}
