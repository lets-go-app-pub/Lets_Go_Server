//
// Created by jeremiah on 5/31/22.
//

#pragma once

#include <mongocxx/client.hpp>

void checkMoveUserAccountStatisticsDocumentResult(
        const bsoncxx::oid& account_oid,
        const std::chrono::milliseconds& currentTimestamp,
        const UserAccountStatisticsDoc& user_account_statistics,
        bool create_new_statistics_document
        );