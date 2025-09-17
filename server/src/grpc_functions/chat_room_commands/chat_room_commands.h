//
// Created by jeremiah on 3/20/21.
//
#pragma once

#include <ChatRoomCommands.grpc.pb.h>
#include <utility_testing_functions.h>

//primary function for ClientMessageToServerRPC, called from gRPC server implementation
void clientMessageToServer(grpc_chat_commands::ClientMessageToServerRequest* request,
                           grpc_chat_commands::ClientMessageToServerResponse* response);

//primary function for CreateChatRoomRPC, called from gRPC server implementation
void createChatRoom(const grpc_chat_commands::CreateChatRoomRequest* request,
                    grpc_chat_commands::CreateChatRoomResponse* response);

//primary function for JoinChatRoomRPC, called from gRPC server implementation
void joinChatRoom(const grpc_chat_commands::JoinChatRoomRequest* request, grpc::ServerWriterInterface<grpc_chat_commands::JoinChatRoomResponse>* responseStream);

//primary function for LeaveChatRoomRPC, called from gRPC server implementation
void leaveChatRoom(const grpc_chat_commands::LeaveChatRoomRequest* request, grpc_chat_commands::LeaveChatRoomResponse* response);

//primary function for RemoveFromChatRoomRPC, called from gRPC server implementation
void removeFromChatRoom(const grpc_chat_commands::RemoveFromChatRoomRequest* request, grpc_chat_commands::RemoveFromChatRoomResponse* response);

//primary function for UnMatchRPC, called from gRPC server implementation
void unMatch(const grpc_chat_commands::UnMatchRequest* request, grpc_chat_commands::UnMatchResponse* response);

//primary function for BlockAndReportChatRoomRPC, called from gRPC server implementation
void blockAndReportChatRoom(const grpc_chat_commands::BlockAndReportChatRoomRequest* request, UserMatchOptionsResponse* response);

//primary function for UnblockOtherUserRPC, called from gRPC server implementation
void unblockOtherUser(const grpc_chat_commands::UnblockOtherUserRequest* request, grpc_chat_commands::UnblockOtherUserResponse* response);

//primary function for GetSingleChatImageRPC, called from gRPC server implementation
//void getSingleChatImage(const grpc_chat_commands::GetSingleChatImageRequest* request, grpc_chat_commands::GetSingleChatImageResponse* response);

//primary function for PromoteNewAdminRPC, called from gRPC server implementation
void promoteNewAdmin(const grpc_chat_commands::PromoteNewAdminRequest* request, grpc_chat_commands::PromoteNewAdminResponse* response);

//primary function for UpdateChatRoomInfoRPC, called from gRPC server implementation
void updateChatRoomInfo(const grpc_chat_commands::UpdateChatRoomInfoRequest* request, grpc_chat_commands::UpdateChatRoomInfoResponse* response);

//primary function for SetPinnedLocationRPC, called from gRPC server implementation
void setPinnedLocation(const grpc_chat_commands::SetPinnedLocationRequest* request, grpc_chat_commands::SetPinnedLocationResponse* response);

//primary function for UpdateSingleChatRoomMembers, called from gRPC server implementation
void updateSingleChatRoomMember(const grpc_chat_commands::UpdateSingleChatRoomMemberRequest* request,
                                UpdateOtherUserResponse* response);

//primary function for updateChatRoom, called from gRPC server implementation
void updateChatRoom(const grpc_chat_commands::UpdateChatRoomRequest* request,
                    grpc::ServerWriterInterface<grpc_chat_commands::UpdateChatRoomResponse>* responseStream);

class ChatRoomCommandsImpl final : public grpc_chat_commands::ChatRoomMessagesService::Service {

public:
    grpc::Status ClientMessageToServerRPC(grpc::ServerContext* context [[maybe_unused]],
                                          const grpc_chat_commands::ClientMessageToServerRequest* request,
                                          grpc_chat_commands::ClientMessageToServerResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting ClientMessageToServerRPC Function...\n";
        std::cout << "Message Type: " << convertMessageBodyTypeToString(request->message().message_specifics().message_body_case()) << '\n';
        std::cout << "Message Size: " << (request->ByteSizeLong())/1024 << "Kb\n";
#endif // DEBUG

        //in order to move things like the byte arrays for thumbnails, the request cannot be const, HOWEVER
        // the grpc function implementation IS const, so casting it to non_const here
        //I posted a question about using const_cast on stack overflow, check it
        // https://stackoverflow.com/questions/69993061/why-is-the-generated-request-const-moving-a-protobuf-string
        // ALSO search for the other const_cast and undo it if I can't do this for whatever reason
        // the testing note is to make sure to test this
        clientMessageToServer(const_cast<grpc_chat_commands::ClientMessageToServerRequest*> (request), response);

#ifdef _DEBUG
        std::cout << "Finishing ClientMessageToServerRPC Function...\n";
        std::cout << "Message Type: " << convertReturnStatusToString(response->return_status()) << " Return Status: " << ReturnStatus_Name(response->return_status()) << '\n';
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status CreateChatRoomRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::CreateChatRoomRequest* request,
                                   grpc_chat_commands::CreateChatRoomResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting CreateChatRoomRPC Function...\n";
#endif // DEBUG

        createChatRoom(request, response);

#ifdef _DEBUG
        std::cout << "Finishing CreateChatRoomRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status JoinChatRoomRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::JoinChatRoomRequest* request,
                                 grpc::ServerWriter<grpc_chat_commands::JoinChatRoomResponse>* responseStream) override {
#ifdef _DEBUG
        std::cout << "Starting JoinChatRoomRPC Function...\n";
#endif // DEBUG

        joinChatRoom(request, responseStream);

#ifdef _DEBUG
        std::cout << "Finishing JoinChatRoomRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status LeaveChatRoomRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::LeaveChatRoomRequest* request,
                                  grpc_chat_commands::LeaveChatRoomResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting LeaveChatRoomRPC Function...\n";
#endif // DEBUG

        leaveChatRoom(request, response);

#ifdef _DEBUG
        std::cout << "Finishing LeaveChatRoomRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status RemoveFromChatRoomRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::RemoveFromChatRoomRequest* request,
                                       grpc_chat_commands::RemoveFromChatRoomResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting RemoveFromChatRoomRPC Function...\n";
#endif // DEBUG

        removeFromChatRoom(request, response);

#ifdef _DEBUG
        std::cout << "Finishing RemoveFromChatRoomRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status UnMatchRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::UnMatchRequest* request,
                            grpc_chat_commands::UnMatchResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting UnMatchRPC Function...\n";
#endif // DEBUG

        unMatch(request, response);

#ifdef _DEBUG
        std::cout << "Finishing UnMatchRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status BlockAndReportChatRoomRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::BlockAndReportChatRoomRequest* request,
                                           UserMatchOptionsResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting BlockAndReportChatRoomRPC Function...\n";
#endif // DEBUG

        blockAndReportChatRoom(request, response);

#ifdef _DEBUG
        std::cout << "Finishing BlockAndReportChatRoomRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status UnblockOtherUserRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::UnblockOtherUserRequest* request,
                                     grpc_chat_commands::UnblockOtherUserResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting UnblockOtherUserRPC Function...\n";
#endif // DEBUG

        unblockOtherUser(request, response);

#ifdef _DEBUG
        std::cout << "Finishing UnblockOtherUserRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

//    grpc::Status GetSingleChatImageRPC(grpc::ServerContext* context, const grpc_chat_commands::GetSingleChatImageRequest* request,
//                                       grpc_chat_commands::GetSingleChatImageResponse* response) override {
//#ifdef _DEBUG
//        std::cout << "Starting GetSingleChatImageRPC Function...\n";
//#endif // DEBUG
//
//        getSingleChatImage(request, response);
//
//#ifdef _DEBUG
//        std::cout << "Finishing GetSingleChatImageRPC Function...\n";
//#endif // DEBUG
//
//        return grpc::Status::OK;
//    }

grpc::Status PromoteNewAdminRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::PromoteNewAdminRequest* request,
                                    grpc_chat_commands::PromoteNewAdminResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting PromoteNewAdminRPC Function...\n";
#endif // DEBUG

        promoteNewAdmin(request, response);

#ifdef _DEBUG
        std::cout << "Finishing PromoteNewAdminRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status UpdateChatRoomInfoRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::UpdateChatRoomInfoRequest* request,
                                       grpc_chat_commands::UpdateChatRoomInfoResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting UpdateChatRoomInfoRPC Function...\n";
#endif // DEBUG

        updateChatRoomInfo(request, response);

#ifdef _DEBUG
        std::cout << "Finishing UpdateChatRoomInfoRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status SetPinnedLocationRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::SetPinnedLocationRequest* request,
                                       grpc_chat_commands::SetPinnedLocationResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting SetPinnedLocationRPC Function...\n";
#endif // DEBUG

        setPinnedLocation(request, response);

#ifdef _DEBUG
        std::cout << "Finishing SetPinnedLocationRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status UpdateChatRoomRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::UpdateChatRoomRequest* request,
                                             grpc::ServerWriter<grpc_chat_commands::UpdateChatRoomResponse>* responseStream) override {
#ifdef _DEBUG
        std::cout << "Starting UpdateChatRoomRPC Function...\n";
#endif // DEBUG

        updateChatRoom(request, responseStream);

#ifdef _DEBUG
        std::cout << "Finishing UpdateChatRoomRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

    grpc::Status UpdateSingleChatRoomMemberRPC(grpc::ServerContext* context [[maybe_unused]], const grpc_chat_commands::UpdateSingleChatRoomMemberRequest* request,
                                               UpdateOtherUserResponse* response) override {
#ifdef _DEBUG
        std::cout << "Starting UpdateSingleChatRoomMemberRPC Function...\n";
#endif // DEBUG

        updateSingleChatRoomMember(request, response);

#ifdef _DEBUG
        std::cout << "Finishing UpdateSingleChatRoomMemberRPC Function...\n";
#endif // DEBUG

        return grpc::Status::OK;
    }

};