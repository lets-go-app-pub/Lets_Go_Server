
#include <iostream>

#include <mongocxx/instance.hpp>
#include <grpcpp/grpcpp.h>
#include <thread>

#include "server_initialization_functions.h"

//Testing
#include "async_server.h"
#include <Python.h>
#include <server_values.h>
#include <assert_macro.h>
#include <accepted_mime_types.h>
#include <connection_pool_global_variable.h>
#include <grpc_values.h>

#include "chat_change_stream.h"
#include "chat_change_stream_values.h"
#include "server_accepting_connections_bool.h"
#include "generate_multiple_accounts_multi_thread.h"
#include "generate_multiple_random_accounts.h"
#include "store_mongoDB_error_and_exception.h"

bool ipv4IsInvalid(const std::string& ip) {

    if (ip.size() < 7
        || ip.size() > 15
            ) {
        return true;
    }

    const int ip_size = (int) ip.size();
    int chunks = 0;
    for (int i = ip_size; i >= 0;) {
        if (chunks > 4) {
            return true;
        }
        int number = 0, j = i - 1;
        char most_recent_char = '.';
        for (; j >= 0; --j) {
            if (ip[j] == '.') {
                break;
            } else if (isdigit(ip[j])) {
                most_recent_char = ip[j];
                number += (ip[j] - '0') * (int) std::pow(10, i - j - 1);
            } else {
                return true;
            }
        }

        int size_of_chunk = i - j - 1;
        if (size_of_chunk < 1 || 3 < size_of_chunk
            || number < 0 || 255 < number
            || (most_recent_char == '0' && size_of_chunk > 1)) {
            return true;
        }
        i = j;
        chunks++;
    }

    if(chunks != 4) {
        return true;
    }

    return false;
}

bool ipv6IsInvalid(const std::string& ip) {

    if (ip.size() < 15
        || ip.size() > 39
            ) {
        return true;
    }

    const int ip_size = (int) ip.size();
    int chunks = 0;
    for (int i = -1; i < ip_size;) {
        if (chunks > 8) {
            return true;
        }
        int j = i + 1;
        for (; j < ip_size; ++j) {
            if (ip[j] == ':') {
                break;
            } else if (!isxdigit(ip[j])) {
                return true;
            }
        }

        const int size_of_chunk = j - i - 1;
        if (size_of_chunk < 1 || 4 < size_of_chunk) {
            return true;
        }
        i = j;
        chunks++;
    }

    if(chunks != 8) {
        return true;
    }

    return false;
}

bool dnsIsInvalid(const std::string& ip) {
    if (ip.empty()
        || ip.size() > 4096 //Arbitrary number
            ) {
        return true;
    }

    //DNS Can only contain letters, numbers, period and dash.
    return std::any_of(ip.begin(), ip.end(), [](char c){
        return !isalnum(c) && c != '-' && c != '.';
    });
}

enum TypeOfIpvAddress {
    IPV_ADDRESS_TYPE_FOUR,
    IPV_ADDRESS_TYPE_SIX,
    IPV_ADDRESS_TYPE_DNS,
    IPV_ADDRESS_TYPE_UNKNOWN
};

//NOTE: There is a regex solution to the problem of validating ip addresses. However, it is substantially slower. And
// while admittedly this function does not run very much which means performance is not terribly important, this
// function should not be harder to read (probably easier) than the regex solution.
TypeOfIpvAddress validateIpvAddressAndReturnType(const std::string& ip) {
    if(!ipv4IsInvalid(ip)) {
        return IPV_ADDRESS_TYPE_FOUR;
    } else if(!ipv6IsInvalid(ip)) {
        return IPV_ADDRESS_TYPE_SIX;
    } else if(!dnsIsInvalid(ip)) {
        return IPV_ADDRESS_TYPE_DNS;
    }

    return IPV_ADDRESS_TYPE_UNKNOWN;
}

void runServer() {
    grpc_server_impl = std::make_unique<GrpcServerImpl>();
    grpc_server_impl->Run();
}

void initializeThenRunServer() {

    for (const auto& mime_type: accepted_mime_types) {
        if (mime_type.size() > largest_mime_type_size) {
            largest_mime_type_size = mime_type.size();
        }
    }

    _ts* embedded_python_thread_state = setupPythonModules();
    //indexing must come before database docs to enforce unique indexing
    setupMongoDBIndexing();
    setupMandatoryDatabaseDocs();

    std::thread chatChangeStreamThread{beginChatChangeStream};
    chatChangeStreamThread.detach();

    //Spin until the change stream thread begins. This is done for an edge case in which
    // cancelChatChangeStream() is called before beginChatChangeStream() is called. This case
    // would result in a situation where the chat change stream is never cancelled.
    while (!getThreadStarted()) {}

#ifdef _RELEASE
    std::cout << "Sleeping for " << chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES.count()/1000 << "s while cached_messages fills up to allow previous messages to be requested." << std::endl;
    std::this_thread::sleep_for(chat_change_stream_values::TIME_TO_REQUEST_PREVIOUS_MESSAGES);
    std::cout << "Finished sleeping." << std::endl;
#endif

    runServer();

    //NOTE: this must be released AFTER all Python code has completed
    PyEval_RestoreThread(embedded_python_thread_state);
    Py_Finalize();
}

//Process Entry Point
int main(int argc, char* argv[]) {
#ifdef LG_TESTING
    std::cout << "LG_TESTING IS STILL ENABLED, THIS CHANGES SOME GLOBAL CONSTANTS AS WELL AS SERVER_CREDENTIALS!\n";
#endif // TESTING

    logErrorToFile("Server initializing.");

    {

        //Make sure environment variables have been set.
        assert_msg(
                general_values::ERROR_LOG_OUTPUT != ENVIRONMENT_VARIABLE_FAILED,
                std::string("Environment variable ERROR_LOG_OUTPUT_DIRECTORY_LETS_GO has not been set. Please set it inside /etc/environment.")
        );

        assert_msg(
                general_values::PYTHON_PATH_STRING != ENVIRONMENT_VARIABLE_FAILED,
                std::string("Environment variable PYTHON_FILES_DIRECTORY_LETS_GO has not been set. Please set it inside /etc/environment.")
        );

        assert_msg(
                general_values::MONGODB_URI_STRING != ENVIRONMENT_VARIABLE_FAILED,
                std::string("Environment variable MONGODB_REPLICA_SET_URI_LETS_GO has not been set. Please set it inside /etc/environment.")
        );

        assert_msg(
                general_values::SSL_FILE_DIRECTORY != ENVIRONMENT_VARIABLE_FAILED,
                std::string("Environment variable GRPC_SSL_KEY_DIRECTORY_LETS_GO has not been set. Please set it inside /etc/environment.")
        );

        std::string address;
        int port;

        //NOTE: This if statement is mostly for testing. Passing the address and port as command line arguments is
        // useful for starting multiple servers on a single device.
        if(argc >= 3) { //If command line arguments were passed, use them.
            address = std::string(argv[1]);
            port = std::stoi(std::string(argv[2]));
        } else { //If no command line arguments were passed, check for environment variables set.
            address = get_environment_variable("CURRENT_SERVER_IP_ADDRESS_LETS_GO");

            assert_msg(
                    address != ENVIRONMENT_VARIABLE_FAILED,
                    std::string("Environment variable CURRENT_SERVER_IP_ADDRESS_LETS_GO has not been set. Please set it inside /etc/environment. Alternatively, command line arguments for port and address can be used.")
            );

            const std::string port_string = get_environment_variable("CURRENT_SERVER_GRPC_PORT_LETS_GO");

            assert_msg(
                    port_string != ENVIRONMENT_VARIABLE_FAILED,
                    std::string("Environment variable CURRENT_SERVER_GRPC_PORT_LETS_GO has not been set. Please set it inside /etc/environment. Alternatively, command line arguments for port and address can be used.")
            );

            port = std::stoi(port_string);
        }

        const TypeOfIpvAddress address_type = validateIpvAddressAndReturnType(address);

        assert_msg(address_type != TypeOfIpvAddress::IPV_ADDRESS_TYPE_UNKNOWN,
                   "Invalid 'current_server_address' passed.");
        assert_msg(port > 0, "Invalid port passed.");

        // 0.0.0.0 represents 10.0.2.2 on android (and inside GRPC_SERVER_ADDRESSES_URI)
        bool address_found = false;
        for (const ServerAddressList& address_and_port: GRPC_SERVER_ADDRESSES_URI) {
            if (
                    (address_and_port.ADDRESS == address
                     || (address == "0.0.0.0"
                         && address_and_port.ADDRESS == "10.0.2.2")
                     || (address == "10.0.2.2"
                         && address_and_port.ADDRESS == "0.0.0.0")
                    )
                    && address_and_port.PORT == port
                    ) {
                address_found = true;
                break;
            }
        }

        assert_msg(address_found, std::string("Passed address not inside GRPC_SERVER_ADDRESSES_URI.\nAddress: " + address + ':' + std::to_string(port)));

        current_server_address = address;

        //NOTE: Include square brackets [] around the address, ipv6 format is [::]:<port>.
        current_server_uri =
                (address_type == TypeOfIpvAddress::IPV_ADDRESS_TYPE_SIX
                 ? "[" + address + "]"
                 : address)
                + ":"
                + std::to_string(port);

        const auto processor_count = std::thread::hardware_concurrency();

        std::cout << "Server Starting!\n";
        std::cout << "Core Number: " << processor_count << '\n';
    }

    [[maybe_unused]] mongocxx::instance mongoCppInstance{}; // This should be done only once.

    mongocxx_client_pool.init();

    initializeThenRunServer();

    logErrorToFile("Server shutting down with code: " + std::to_string(SHUTDOWN_RETURN_CODE));

    return SHUTDOWN_RETURN_CODE;
}
