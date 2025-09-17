//
// Created by jeremiah on 3/20/21.
//

#include "chat_message_stream.h"
#include "grpc_function_server_template.h"
#include "utility_general_functions.h"

std::string checkLoginToken(
        const grpc_stream_chat::InitialLoginMessageRequest& request,
        ReturnStatus& return_status,
        std::chrono::milliseconds& currentTimestamp
) {

    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationID;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            LoginTypesAccepted::LOGIN_TYPE_ONLY_CLIENT,
            request.login_info(),
            userAccountOIDStr,
            loginTokenStr,
            installationID
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        return_status = basicInfoReturnStatus;
        return "";
    }

    return_status = ReturnStatus::UNKNOWN;

    bsoncxx::document::value mergeDocument = bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize;

    auto setReturnStatus = [&return_status](const ReturnStatus& returnStatus){
        return_status = returnStatus;
    };

    std::string returnString;

    auto setSuccess = [&returnString, &return_status, &userAccountOIDStr](const bsoncxx::document::view& userAccountDocView [[maybe_unused]]){
        returnString = userAccountOIDStr;
        return_status = ReturnStatus::SUCCESS;
    };

    grpcValidateLoginFunctionTemplate<true>(
            userAccountOIDStr,
            loginTokenStr,
            installationID,
            currentTimestamp,
            mergeDocument,
            setReturnStatus,
            setSuccess
    );

    return returnString;
}