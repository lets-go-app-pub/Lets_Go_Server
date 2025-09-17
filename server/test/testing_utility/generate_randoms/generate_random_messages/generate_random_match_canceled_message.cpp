//
// Created by jeremiah on 6/20/22.
//

#include <chat_room_commands.h>
#include <gtest/gtest.h>
#include "generate_random_messages.h"
#include "setup_login_info.h"

grpc_chat_commands::UnMatchRequest generateRandomMatchCanceledMessage(
        const bsoncxx::oid& account_oid,
        const std::string& logged_in_token,
        const std::string& installation_id,
        const std::string& chat_room_id,
        const std::string& matched_account_oid
        ) {

    grpc_chat_commands::UnMatchRequest un_match_request;
    grpc_chat_commands::UnMatchResponse un_match_response;

    setupUserLoginInfo(
            un_match_request.mutable_login_info(),
            account_oid,
            logged_in_token,
            installation_id
            );

    un_match_request.set_chat_room_id(chat_room_id);
    un_match_request.set_matched_account_oid(matched_account_oid);

    unMatch(&un_match_request, &un_match_response);

    EXPECT_EQ(un_match_response.return_status(), ReturnStatus::SUCCESS);

    return un_match_request;
}