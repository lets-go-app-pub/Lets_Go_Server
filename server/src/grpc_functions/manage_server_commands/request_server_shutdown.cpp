//
// Created by jeremiah on 10/26/21.
//

#include <handle_function_operation_exception.h>
#include <utility_general_functions.h>
#include <AdminLevelEnum.grpc.pb.h>
#include <admin_privileges_vector.h>
#include <server_values.h>
#include <server_accepting_connections_bool.h>
#include "manage_server_commands.h"


#include "database_names.h"
#include "collection_names.h"
#include "admin_account_keys.h"

void requestServerShutdownImplementation(
        const manage_server_commands::RequestServerShutdownRequest* request,
        manage_server_commands::RequestServerShutdownResponse* response
);

void requestServerShutdown(
        const manage_server_commands::RequestServerShutdownRequest* request,
        manage_server_commands::RequestServerShutdownResponse* response
) {
    handleFunctionOperationException(
            [&] {
                requestServerShutdownImplementation(request, response);
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::DATABASE_DOWN));
            },
            [&] {
                response->set_successful(false);
                response->set_error_message(ReturnStatus_Name(ReturnStatus::LG_ERROR));
            },
            __LINE__, __FILE__, request
    );
}

void setError(
        manage_server_commands::RequestServerShutdownResponse* response,
        const std::string& error_message
);

void requestServerShutdownImplementation(
        const manage_server_commands::RequestServerShutdownRequest* request,
        manage_server_commands::RequestServerShutdownResponse* response
) {

    static std::mutex shutdown_server_mutex;

    bsoncxx::stdx::optional<bsoncxx::document::value> admin_info_doc_value;

    {

        std::string error_message;
        std::string user_account_oid_str;
        std::string login_token_str;
        std::string installation_id;

        auto store_error_message = [&](bsoncxx::stdx::optional<bsoncxx::document::value>& returned_admin_info_doc,
                                       const std::string& passed_error_message) {
            error_message = passed_error_message;
            admin_info_doc_value = std::move(returned_admin_info_doc);
        };

        ReturnStatus basic_info_return_status = isLoginToServerBasicInfoValid(
                LoginTypesAccepted::LOGIN_TYPE_ONLY_ADMIN,
                request->login_info(),
                user_account_oid_str,
                login_token_str,
                installation_id,
                store_error_message
        );

        if (basic_info_return_status != ReturnStatus::SUCCESS) {
            response->set_successful(false);
            response->set_error_message(
                    "ReturnStatus: " + ReturnStatus_Name(basic_info_return_status) + " " + error_message);
            return;
        } else if (!admin_info_doc_value) {
            response->set_successful(false);
            response->set_error_message("Could not find admin document.");
            return;
        }

    }

    const bsoncxx::document::view admin_info_doc_view = admin_info_doc_value->view();
    AdminLevelEnum admin_level;

    auto admin_privilege_element = admin_info_doc_view[admin_account_key::PRIVILEGE_LEVEL];
    if (admin_privilege_element &&
        admin_privilege_element.type() == bsoncxx::type::k_int32) { //if element exists and is type int32
        admin_level = AdminLevelEnum(admin_privilege_element.get_int32().value);
    } else { //if element does not exist or is not type int32
        logElementError(
                __LINE__, __FILE__,
                admin_privilege_element, admin_info_doc_view,
                bsoncxx::type::k_int32, admin_account_key::PRIVILEGE_LEVEL,
                database_names::ACCOUNTS_DATABASE_NAME, collection_names::ADMIN_ACCOUNTS_COLLECTION_NAME
        );
        setError(response, "Error stored on server.");
        return;
    }

    if (!admin_privileges[admin_level].run_server_shutdowns()) {
        setError(response, std::string("Admin level ")
                .append(AdminLevelEnum_Name(admin_level))
                .append(" does not have 'update_single_users' access."));
        return;
    }

    if (request->server_address().size() > MAXIMUM_NUMBER_ALLOWED_CHARS_ADDRESS) {
        setError(response, "Error stored on server.");
        return;
    }

    std::unique_lock<std::mutex> lock(shutdown_server_mutex, std::defer_lock);

    if (lock.try_lock()) {

        std::cout << "current_server_address: " << current_server_address << '\n' << " request->server_address(): " << request->server_address() << '\n';
        std::cout << "grpc_server_impl == nullptr: " << (grpc_server_impl==nullptr?"true":"false") << '\n';
        std::cout << "server_accepting_connections: " << server_accepting_connections << '\n';
        //NOTE: Checking if the server address matches is in here to avoid potential bugs
        // on the desktop interface of shutting down the wrong server.
        if (current_server_address == request->server_address()
            && grpc_server_impl != nullptr
            && server_accepting_connections) {
            SHUTDOWN_RETURN_CODE = SERVER_SHUTDOWN_REQUESTED_RETURN_CODE;
            grpc_server_impl->beginShutdownOnSeparateThread();
        } else if (current_server_address != request->server_address()) {
            setError(response,
                     std::string("Requested incorrect server address.\nCurrent address: ")
                             .append(current_server_address)
                             .append(".\nRequested address: ")
                             .append(request->server_address())
                             .append(".")
            );
            return;
        } else if (grpc_server_impl == nullptr) {
            setError(response, "Server is not set up (_server_impl == nullptr).");
            return;
        } else { //server is no longer accepting connections
            setError(response, "Server is no longer accepting connections (server_accepting_connections == false).");
            return;
        }

    } else {
        setError(response, "Server is already attempting to shut down (server_accepting_connections == false).");
        return;
    }

    response->set_successful(true);
}

void setError(manage_server_commands::RequestServerShutdownResponse* response,
              const std::string& error_message) {
    response->set_successful(false);
    response->set_error_message(error_message);
}
