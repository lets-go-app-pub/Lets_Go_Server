//
// Created by jeremiah on 3/20/21.
//
#include <mongocxx/client.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/logic_error.hpp>

#include <bsoncxx/builder/stream/document.hpp>
#include "extract_thumbnail_from_verified_doc.h"
#include "global_bsoncxx_docs.h"
#include "utility_chat_functions.h"
#include "handle_function_operation_exception.h"
#include "connection_pool_global_variable.h"

#include "chat_room_commands.h"
#include "utility_general_functions.h"
#include "store_mongoDB_error_and_exception.h"

//mongoDB
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

void getSingleChatImageImplementation(const grpc_chat_commands::GetSingleChatImageRequest* request,
                                      grpc_chat_commands::GetSingleChatImageResponse* response);

//primary function for GetSingleChatImageRPC, called from gRPC server implementation
void getSingleChatImage(const grpc_chat_commands::GetSingleChatImageRequest* request,
                        grpc_chat_commands::GetSingleChatImageResponse* response) {

    handleFunctionOperationException(
            [&] {
                getSingleChatImageImplementation(request, response);
            },
            [&] {
                response->set_return_status(ReturnStatus::DATABASE_DOWN);
            },
            [&] {
                response->set_return_status(ReturnStatus::LG_ERROR);
            });
}

void getSingleChatImageImplementation(const grpc_chat_commands::GetSingleChatImageRequest* request,
                                      grpc_chat_commands::GetSingleChatImageResponse* response
) {
    std::string userAccountOIDStr;
    std::string loginTokenStr;
    std::string installationID;

    ReturnStatus basicInfoReturnStatus = isLoginToServerBasicInfoValid(
            request->login_info(), userAccountOIDStr, loginTokenStr, installationID
    );

    if (basicInfoReturnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(basicInfoReturnStatus);
        return;
    }

    const std::string& pictureOIDString = request->picture_oid(); //check for valid match oid

    if (isInvalidOIDString(pictureOIDString)) { //check if picture oid is a proper oid
        response->set_timestamp(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    const std::string& chatRoomId = request->chat_room_id();

    if (isInvalidChatRoomId(chatRoomId)) {
        response->set_timestamp(-2L);
        response->set_return_status(ReturnStatus::INVALID_PARAMETER_PASSED);
        return;
    }

    mongocxx::pool::entry mongocxx_pool_entry = mongocxx_client_pool.acquire();
    mongocxx::client& mongoCppClient = *mongocxx_pool_entry;

    mongocxx::database accountsDB = mongoCppClient[ACCOUNTS_DATABASE_NAME];
    mongocxx::collection userAccountsCollection = accountsDB[USER_ACCOUNTS_COLLECTION_NAME];
    mongocxx::database chatRoomDB = mongoCppClient[CHAT_ROOMS_DATABASE_NAME];

    //CHAT_ROOM_COMMANDS_GET_SINGLE_CHAT_IMAGE_COLLECTION_NAME

    response->set_timestamp(-1L); //setting value for 'unset'
    response->set_return_status(ReturnStatus::UNKNOWN); //setting error as default
    const std::chrono::milliseconds currentTimestamp = getCurrentTimestamp();

    const bsoncxx::oid userAccountOID{userAccountOIDStr};

    std::shared_ptr<std::vector<bsoncxx::document::value>> appendToAndStatementDoc = std::make_shared<std::vector<bsoncxx::document::value>>();

    //checks if user is a member of chat room
    bsoncxx::document::value userRequiredInChatRoomCondition = buildUserRequiredInChatRoomCondition(chatRoomId);
    appendToAndStatementDoc->emplace_back(userRequiredInChatRoomCondition.view());

    bsoncxx::document::value mergeDocument = document{} << finalize;

    bsoncxx::document::value projectionDocument = document{}
            << ACCOUNT_FIRST_NAME_KEY << 1
            << ACCOUNT_PICTURES_KEY << 1
            << finalize;

    bsoncxx::document::value loginDocument = getLoginDocument(loginTokenStr, installationID,
                                                              mergeDocument,
                                                              currentTimestamp, false,
                                                              appendToAndStatementDoc);

    bsoncxx::stdx::optional<bsoncxx::document::value> findUserAccount;

    if (!runInitialLoginOperation(findUserAccount, userAccountsCollection,
                                  userAccountOID, mergeDocument, loginDocument,
                                  projectionDocument)
            ) {
        response->set_return_status(ReturnStatus::LG_ERROR);
        return;
    }

    ReturnStatus returnStatus = checkForValidLoginToken(findUserAccount, userAccountOIDStr);

    if (returnStatus == ReturnStatus::UNKNOWN) { //user not found inside chat room

        std::string errStr = "User was not part of a chat room they requested an image from.";

        std::optional<std::string> dummyExceptionString;
        storeMongoDBErrorAndException(__LINE__, __FILE__,
                                      dummyExceptionString, errStr,
                                      "database", ACCOUNTS_DATABASE_NAME,
                                      "collection", USER_ACCOUNTS_COLLECTION_NAME,
                                      "oid_used", userAccountOID,
                                      "chat_room_id", chatRoomId,
                                      "picture_oid", pictureOIDString);

        response->set_success_status(
                grpc_chat_commands::GetSingleChatImageResponse_GetSinglePictureStatus_USER_NOT_FOUND_OR_NOT_IN_CHAT_ROOM);
        response->set_return_status(ReturnStatus::SUCCESS);

    } else if (returnStatus != ReturnStatus::SUCCESS) {
        response->set_return_status(returnStatus);
    } else {

        auto successful = [&](const long& timestamp, const int pictureSize,
                              const int pictureHeight, const int pictureWidth, std::string& pictureByteString
        ) {
            response->mutable_picture()->set_timestamp_picture_last_updated(timestamp);
            response->mutable_picture()->set_file_size(pictureSize);
            response->mutable_picture()->set_pic_height(pictureHeight);
            response->mutable_picture()->set_pic_width(pictureWidth);
            response->mutable_picture()->set_index_number(-1);
            response->mutable_picture()->set_file_in_bytes(std::move(pictureByteString));

            response->set_success_status(
                    grpc_chat_commands::GetSingleChatImageResponse_GetSinglePictureStatus_SUCCESS);
            response->set_return_status(SUCCESS);
        };

        auto pictureCorrupt = [&]() {
            response->set_success_status(
                    grpc_chat_commands::GetSingleChatImageResponse_GetSinglePictureStatus_PICTURE_NOT_FOUND);
            response->set_return_status(SUCCESS);
        };

        if (!extractChatPicture(
                mongoCppClient,
                pictureOIDString,
                chatRoomId, userAccountOID,
                successful,
                pictureCorrupt)
                ) { //if error occurred
            response->set_return_status(LG_ERROR);
        }

    }

}


